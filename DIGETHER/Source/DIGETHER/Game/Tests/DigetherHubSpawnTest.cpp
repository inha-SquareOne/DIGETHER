#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Economy/DigetherQuotaStateComponent.h"
#include "Game/Elevator/DigetherElevatorActor.h"
#include "Game/Framework/DigetherGameplayGameMode.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Framework/DigetherTestPawn.h"
#include "Game/Run/DigetherRunCoordinatorComponent.h"
#include "Game/Run/DigetherRunPublicStateComponent.h"
#include "Game/Save/DigetherSaveRules.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/AutomationTest.h"
#include "OnlineSubsystemTypes.h"

class FDigetherHubSpawnTestAccess
{
public:
	static bool Resolve(const ADigetherGameplayGameMode& Mode, const APawn* Pawn, const FTransform* Saved,
		const TArray<FTransform>& Reserved, FTransform& Out, bool& bFallback)
	{
		return Mode.ResolveHubSpawnTransform(Pawn, Saved, Reserved, Out, bFallback);
	}
	static void Prepare(ADigetherGameplayGameMode& Mode, ADigetherElevatorActor& Elevator,
		const FDigetherSaveSnapshot& Snapshot)
	{
		Mode.RunTuning = Snapshot.RunTuning;
		Mode.GameId = FGuid::NewGuid();
		Mode.CurrentRunId = FGuid::NewGuid();
		Mode.ElevatorActor = &Elevator;
		Mode.bRunReady = true;
		Mode.RunCoordinatorComponent->SetPublicStateComponent(
			Mode.GetGameState<ADigetherGameplayGameState>()->GetRunPublicStateComponent());
	}
	static bool Restore(ADigetherGameplayGameMode& Mode, const FDigetherSaveSnapshot& Snapshot, const bool bValidateOnly = false)
	{
		return Mode.RestoreProgress(Snapshot, bValidateOnly);
	}
	static FGuid GameId(const ADigetherGameplayGameMode& Mode) { return Mode.GameId; }
	static void RestoreReturning(ADigetherGameplayGameMode& Mode, ADigetherGameplayPlayerState* Player,
		const FDigetherSavePlayerRecord& Record)
	{
		Mode.SavedPlayerRecords = {Record};
		Mode.RestorePlayerPawnTransform(Player);
	}
};

namespace
{
	// 물리 쿼리가 있는 독립 월드, 파일·온라인 세션·실사용자 저장 미생성
	class FHubSpawnWorld
	{
	public:
		explicit FHubSpawnWorld(const bool bWithFloor = true)
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &Values);
			if (World == nullptr)
			{
				return;
			}
			if (GEngine != nullptr)
			{
				GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			}
			Mode = World->SpawnActor<ADigetherGameplayGameMode>();
			State = World->SpawnActor<ADigetherGameplayGameState>();
			if (IsValid(Mode) && IsValid(State))
			{
				Mode->GameState = State;
				World->SetGameState(State);
				Mode->bValidateSavedHubSpawn = true;
			}
			if (bWithFloor)
			{
				AddBox(FVector(0.0, 0.0, -10.0), FVector(600.0, 600.0, 10.0));
			}
			for (const FVector& Location : {FVector(-300.0, -300.0, 100.0), FVector(300.0, -300.0, 100.0),
				FVector(-300.0, 300.0, 100.0), FVector(300.0, 300.0, 100.0)})
			{
				APlayerStart* Start = World->SpawnActor<APlayerStart>();
				if (IsValid(Start))
				{
					Start->SetActorLocation(Location);
				}
			}
		}
		~FHubSpawnWorld()
		{
			if (World != nullptr)
			{
				World->DestroyWorld(false);
				if (GEngine != nullptr)
				{
					GEngine->DestroyWorldContext(World);
				}
			}
		}
		FHubSpawnWorld(const FHubSpawnWorld&) = delete;
		FHubSpawnWorld& operator=(const FHubSpawnWorld&) = delete;

		bool IsReady() const { return IsValid(World) && IsValid(Mode) && IsValid(State); }
		UWorld* GetWorld() const { return World; }
		ADigetherGameplayGameMode* GetMode() const { return Mode; }
		ADigetherGameplayGameState* GetState() const { return State; }
		void AddBox(const FVector& Center, const FVector& Extent) const
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
			Actor->AddInstanceComponent(Box);
			Actor->SetRootComponent(Box);
			Box->SetBoxExtent(Extent);
			Box->SetCollisionProfileName(TEXT("BlockAll"));
			Box->SetCollisionObjectType(ECC_WorldStatic);
			Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetMobility(EComponentMobility::Static);
			Box->SetWorldLocation(Center);
			Box->RegisterComponent();
		}
		APawn* AddPawn(const FVector& Location = FVector(150.0, 0.0, 100.0)) const
		{
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<ADigetherTestPawn>(Location, FRotator::ZeroRotator, Spawn);
		}

	private:
		UWorld* World = nullptr;
		ADigetherGameplayGameMode* Mode = nullptr;
		ADigetherGameplayGameState* State = nullptr;
	};

	FDigetherSaveSnapshot MakeSpawnSnapshot(ADigetherGameplayPlayerState& Player)
	{
		FDigetherSaveSnapshot Snapshot;
		Snapshot.GameId = FGuid::NewGuid();
		Snapshot.RunId = FGuid::NewGuid();
		Snapshot.RunTuning.SourceProfile = FPrimaryAssetId(TEXT("DigetherTuningProfile:DA_HubSpawnFixture"));
		Snapshot.RunTuning.ProfileVersion = 1;
		Snapshot.RunTuning.InitialQuotaCredits = 100;
		Snapshot.RunTuning.RunSeconds = 120.f;
		Snapshot.RunTuning.InteractionDistanceCm = 200.f;
		Snapshot.RunTuning.ElevatorCapacityKg = 500.f;
		Snapshot.RunTuning.ElevatorTripLimit = 2;
		Snapshot.RunTuning.AscentPenaltySeconds = 10.f;
		Snapshot.RunTuning.ElevatorTravelSeconds = 2.f;
		Snapshot.RunTuning.DepositIntervalSeconds = .5f;
		Snapshot.RunTuning.EmergencyWeightLimitKg = 3.f;
		Snapshot.RunTuning.DeathLossPercent = 50;
		Snapshot.RunTuning.CorpseRefundPercent = 50;
		Snapshot.Quota.Quota = FDigetherEconomyRules::MakeInitialQuotaState(Snapshot.RunTuning);
		Snapshot.Quota.Quota.DepositedCredits = 17;
		FDigetherSavePlayerRecord Record;
		DigetherSaveRules::MakeAuthenticatedPlayerKey(Player.GetUniqueId(), Record.AuthenticatedPlayerKey);
		Record.GameplayPlayerId = Player.GetGameplayPlayerId();
		Record.Wallet.BalanceCredits = 67;
		Record.bHasHubTransform = true;
		Record.HubTransform.SetLocation(FVector(10000.0, 0.0, 100.0));
		Snapshot.HostAuthenticatedPlayerKey = Record.AuthenticatedPlayerKey;
		Snapshot.Players.Add(Record);
		return Snapshot;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherHubSpawnGeometryTest,
	"DIGETHER.Game.Framework.HubSpawn.Geometry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherHubSpawnGeometryTest::RunTest(const FString& Parameters)
{
	FHubSpawnWorld Fixture;
	if (!TestTrue(TEXT("충돌 바닥 시험 월드"), Fixture.IsReady())) { return false; }
	APawn* Pawn = Fixture.AddPawn();
	if (!TestNotNull(TEXT("실제 구형 충돌 Pawn"), Pawn)) { return false; }
	FTransform Out;
	bool bFallback = true;
	FTransform Saved(FVector(150.0, 0.0, 100.0));
	TestTrue(TEXT("정상 저장 위치 수락"), FDigetherHubSpawnTestAccess::Resolve(*Fixture.GetMode(), Pawn, &Saved, {}, Out, bFallback));
	TestTrue(TEXT("정상 위치 보존"), Out.Equals(Saved));
	TestFalse(TEXT("정상 위치는 대체 아님"), bFallback);
	const USphereComponent* Sphere = Cast<USphereComponent>(Pawn->GetRootComponent());
	if (!TestNotNull(TEXT("구형 Pawn 충돌 컴포넌트"), Sphere)) { return false; }
	Saved.SetLocation(FVector(150.0, 0.0, Sphere->GetScaledSphereRadius()));
	TestTrue(TEXT("바닥에 정확히 접촉한 정상 저장 위치도 보존"),
		FDigetherHubSpawnTestAccess::Resolve(*Fixture.GetMode(), Pawn, &Saved, {}, Out, bFallback) && !bFallback && Out.Equals(Saved));
	Fixture.AddBox(FVector(0.0, 0.0, 100.0), FVector(50.0, 50.0, 100.0));
	for (const FVector& Invalid : {FVector(0.0, 0.0, 100.0), FVector(10000.0, 0.0, 100.0), FVector(800.0, 0.0, 100.0)})
	{
		Saved.SetLocation(Invalid);
		TestTrue(TEXT("벽·구맵 바깥·지지면 없는 위치는 시작점 대체"),
			FDigetherHubSpawnTestAccess::Resolve(*Fixture.GetMode(), Pawn, &Saved, {}, Out, bFallback));
		TestTrue(TEXT("잘못된 좌표를 그대로 적용하지 않음"), bFallback && !Out.GetLocation().Equals(Invalid));
	}
	Fixture.AddBox(FVector(150.0, 0.0, 165.0), FVector(50.0, 50.0, 10.0));
	Saved.SetLocation(FVector(150.0, 0.0, 145.0));
	TestTrue(TEXT("낮은 천장 안의 몸 겹침도 대체"),
		FDigetherHubSpawnTestAccess::Resolve(*Fixture.GetMode(), Pawn, &Saved, {}, Out, bFallback) && bFallback);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherHubSpawnReservationTest,
	"DIGETHER.Game.Framework.HubSpawn.Reservations", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherHubSpawnReservationTest::RunTest(const FString& Parameters)
{
	FHubSpawnWorld Fixture;
	if (!TestTrue(TEXT("여럿 복원 시험 월드"), Fixture.IsReady())) { return false; }
	TArray<FTransform> Reserved;
	const FTransform Saved(FVector(10000.0, 0.0, 100.0));
	for (int32 Index = 0; Index < 4; ++Index)
	{
		FTransform Out;
		bool bFallback = false;
		if (!TestTrue(TEXT("각 플레이어의 빈 시작점 확보"),
			FDigetherHubSpawnTestAccess::Resolve(*Fixture.GetMode(), nullptr, &Saved, Reserved, Out, bFallback)))
		{
			return false;
		}
		TestTrue(TEXT("각 시작점은 서로 다름"), !Reserved.ContainsByPredicate([&Out](const FTransform& Entry) { return Entry.Equals(Out); }));
		Reserved.Add(Out);
	}
	const FTransform Before(FVector(123.0, 456.0, 789.0));
	FTransform Out = Before;
	bool bFallback = false;
	TestFalse(TEXT("안전한 자리가 없으면 거절"), FDigetherHubSpawnTestAccess::Resolve(*Fixture.GetMode(), nullptr, &Saved, Reserved, Out, bFallback));
	TestTrue(TEXT("거절 시 출력 불변"), Out.Equals(Before));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherHubSpawnOptOutTest,
	"DIGETHER.Game.Framework.HubSpawn.OptOut", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherHubSpawnOptOutTest::RunTest(const FString& Parameters)
{
	FHubSpawnWorld Fixture(false);
	if (!TestTrue(TEXT("바닥 없는 기존 시험 월드"), Fixture.IsReady())) { return false; }
	Fixture.GetMode()->bValidateSavedHubSpawn = false;
	const FTransform Saved(FVector(10000.0, 2000.0, 500.0));
	FTransform Out;
	bool bFallback = true;
	TestTrue(TEXT("옵션을 끈 맵은 기존 좌표 계약 유지"),
		FDigetherHubSpawnTestAccess::Resolve(*Fixture.GetMode(), nullptr, &Saved, {}, Out, bFallback));
	TestTrue(TEXT("기존 시험 맵 좌표 보존"), Out.Equals(Saved) && !bFallback);
	TestTrue(TEXT("좌표 없는 기존 진행은 바닥 검사 없이 첫 시작점"),
		FDigetherHubSpawnTestAccess::Resolve(*Fixture.GetMode(), nullptr, nullptr, {}, Out, bFallback));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherHubSpawnRestoreTest,
	"DIGETHER.Game.Framework.HubSpawn.RestoreProgress", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherHubSpawnRestoreTest::RunTest(const FString& Parameters)
{
	for (const bool bWithFloor : {true, false})
	{
		FHubSpawnWorld Fixture(bWithFloor);
		if (!TestTrue(TEXT("진행 보존 시험 월드"), Fixture.IsReady())) { return false; }
		ADigetherGameplayPlayerState* Player = Fixture.GetWorld()->SpawnActor<ADigetherGameplayPlayerState>();
		APawn* Pawn = Fixture.AddPawn();
		ADigetherElevatorActor* Elevator = Fixture.GetWorld()->SpawnActor<ADigetherElevatorActor>();
		if (!TestNotNull(TEXT("복원 PlayerState"), Player) || !TestNotNull(TEXT("복원 Pawn"), Pawn)
			|| !TestNotNull(TEXT("복원 엘리베이터"), Elevator)) { return false; }
		Player->SetUniqueId(FUniqueNetIdRepl(FUniqueNetIdRef(FUniqueNetIdString::Create(TEXT("hub-spawn-fixture"), FName(TEXT("DIGETHER_TEST"))))));
		Player->InitializeGameplayIdentity();
		Player->MarkGameplayInitialized();
		Player->RetainPawn(Pawn);
		Fixture.GetState()->AddPlayerState(Player);
		FDigetherWalletState BeforeWallet;
		BeforeWallet.BalanceCredits = 3;
		Player->RestoreState(BeforeWallet, false);
		const FTransform BeforeTransform = Pawn->GetActorTransform();
		const FDigetherSaveSnapshot Snapshot = MakeSpawnSnapshot(*Player);
		if (!TestTrue(TEXT("시험 스냅샷 유효"), DigetherSaveRules::ValidateSnapshot(Snapshot) == EDigetherSaveResult::Success)) { return false; }
		FDigetherHubSpawnTestAccess::Prepare(*Fixture.GetMode(), *Elevator, Snapshot);
		const FGuid BeforeGameId = FDigetherHubSpawnTestAccess::GameId(*Fixture.GetMode());
		TestEqual(TEXT("사전검사는 지지면 있을 때만 성공"),
			FDigetherHubSpawnTestAccess::Restore(*Fixture.GetMode(), Snapshot, true), bWithFloor);
		TestEqual(TEXT("사전검사 지갑 불변"), Player->CaptureState().BalanceCredits, 3);
		TestTrue(TEXT("사전검사 Pawn 불변"), Pawn->GetActorTransform().Equals(BeforeTransform));
		TestEqual(TEXT("실제복원도 지지면 있을 때만 성공"),
			FDigetherHubSpawnTestAccess::Restore(*Fixture.GetMode(), Snapshot), bWithFloor);
		TestEqual(TEXT("위치 교정 시에도 진행 금액 적용·거절 시 원금 보존"), Player->CaptureState().BalanceCredits, bWithFloor ? 67 : 3);
		TestEqual(TEXT("게임 ID는 성공 때만 복원"), FDigetherHubSpawnTestAccess::GameId(*Fixture.GetMode()), bWithFloor ? Snapshot.GameId : BeforeGameId);
		if (bWithFloor)
		{
			TestEqual(TEXT("할당량 진행 보존"), Fixture.GetState()->GetQuotaComponent()->CaptureState().Quota.DepositedCredits, 17);
			TestTrue(TEXT("구맵 좌표 대신 거점 안으로 복원"), Pawn->GetActorLocation().Size() < 1000.0);
			Pawn->SetActorLocation(BeforeTransform.GetLocation());
			FDigetherHubSpawnTestAccess::RestoreReturning(*Fixture.GetMode(), Player, Snapshot.Players[0]);
			TestTrue(TEXT("재접속 좌표도 같은 안전한 resolver 사용"), !Pawn->GetActorLocation().Equals(BeforeTransform.GetLocation()));
			TestEqual(TEXT("재접속 위치 교정이 지갑을 다시 적용하지 않음"), Player->CaptureState().BalanceCredits, 67);
		}
		else
		{
			TestTrue(TEXT("대체 지점 없는 복원 거절은 Pawn도 보존"), Pawn->GetActorTransform().Equals(BeforeTransform));
		}
	}
	return true;
}

#endif
