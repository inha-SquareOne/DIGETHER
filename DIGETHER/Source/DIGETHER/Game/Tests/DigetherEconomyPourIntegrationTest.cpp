#if WITH_DEV_AUTOMATION_TESTS

// 동시 붓기 납부(기획 S24) 서버 통합 검사, 금액·위치·간격·ID는 테스트 픽스처이며 기획 값 아님
// 붓기 시작·멈춤·주기 처리·줍기·레버는 제품 GameMode 함수와 Tick 직접 호출

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "Game/Economy/DigetherDepositStationActor.h"
#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Economy/DigetherFloorCoinActor.h"
#include "Game/Economy/DigetherQuotaStateComponent.h"
#include "Game/Economy/DigetherWalletComponent.h"
#include "Game/Elevator/DigetherElevatorActor.h"
#include "Game/Elevator/DigetherElevatorComponent.h"
#include "Game/Elevator/DigetherElevatorLeverActor.h"
#include "Game/Elevator/DigetherElevatorLeverComponent.h"
#include "Game/Elevator/DigetherElevatorRules.h"
#include "Game/Elevator/DigetherElevatorTuning.h"
#include "Game/Framework/DigetherGameplayGameMode.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Framework/DigetherTestPawn.h"
#include "Game/Hub/DigetherHubCatalog.h"
#include "Game/Hub/DigetherHubStageAnchor.h"
#include "Game/Hub/DigetherHubStateActor.h"
#include "Game/Run/DigetherRunCoordinatorComponent.h"
#include "Game/Save/DigetherSaveRules.h"
#include "Game/Save/DigetherSaveStorage.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "OnlineSubsystemTypes.h"
#include "UObject/UnrealType.h"

// 준비·관찰만 노출, 붓기·줍기·레버·저장 캡처/복원은 제품 함수 직접 호출
class FDigetherPourIntegrationAccess
{
public:
	static bool Prepare(ADigetherGameplayGameMode& Mode, const FDigetherRunTuning& Tuning)
	{
		Mode.RunTuning = Tuning;
		Mode.GameId = FGuid::NewGuid();
		Mode.CurrentRunId = FGuid::NewGuid();
		Mode.bRunReady = true;
		Mode.bHasSaveStartupAttempted = true;
		const ADigetherGameplayGameState* State = Mode.GetGameState<ADigetherGameplayGameState>();
		return IsValid(State)
			&& State->GetQuotaComponent()->SetSignalTuning(Mode.EconomyTuning) == EDigetherEconomyResult::Success
			&& State->GetQuotaComponent()->SetQuotaState(FDigetherEconomyRules::MakeInitialQuotaState(Tuning), false)
				== EDigetherEconomyResult::Success
			&& Mode.InitializeRunActors();
	}
	static void SetPourInterval(ADigetherGameplayGameMode& Mode, const float Seconds) { Mode.RunTuning.DepositIntervalSeconds = Seconds; }
	static int32 PourSessionCount(const ADigetherGameplayGameMode& Mode) { return Mode.PourSessions.CaptureSessions().Num(); }
	static bool Capture(const ADigetherGameplayGameMode& Mode, FDigetherSaveSnapshot& Out) { return Mode.CaptureProgress(Out); }
	static bool Restore(ADigetherGameplayGameMode& Mode, const FDigetherSaveSnapshot& Snapshot) { return Mode.RestoreProgress(Snapshot); }
	static void ClearFloorCoins(ADigetherGameplayGameMode& Mode) { Mode.ClearFloorCoins(); }
	// 게임 오버 귀환 확정 때 거점 거래 종료(바닥 금화·붓기 정리), 슬롯 초기화는 크레딧 뒤 새 게임 담당
	static void CloseHubTradingForGameOver(ADigetherGameplayGameMode& Mode) { Mode.CloseHubTradingForGameOver(); }
	// 시험 월드는 타이머 미진행, 남은 재시도 예약이 다음 시험에 남지 않게 정리
	static void ClearPendingAutosave(ADigetherGameplayGameMode& Mode) { Mode.ClearPendingAutosave(); }
	static bool HasPendingAutosave(const ADigetherGameplayGameMode& Mode)
	{
		return Mode.PendingAutosave.IsSet() || Mode.bAutosaveRecapturePending;
	}
};

namespace
{
	const FName PourStationId(TEXT("PourStation"));
	const FVector PourStationLocation(1000.0, 0.0, 0.0);
	// 투입구(정산기 앞면 +X 50cm, 높이 100cm) 앞 100cm에서 -X를 보는 자리
	const FVector PourerOffsetA(150.0, 0.0, 100.0);
	const FVector PourerOffsetB(150.0, 60.0, 100.0);
	const FVector PourerOffsetC(150.0, -60.0, 100.0);
	const FRotator FacingStation(0.0, 180.0, 0.0);
	const FVector LeverOperatorLocation(-100.0, 100.0, 100.0);
	constexpr float PourTickSeconds = 0.5f;

	FDigetherRunTuning MakePourTuning()
	{
		FDigetherRunTuning Tuning;
		Tuning.SourceProfile = FPrimaryAssetId(TEXT("DigetherTuningProfile:DA_PourFixture"));
		Tuning.ProfileVersion = 1;
		Tuning.InitialQuotaCredits = 100;
		Tuning.RunSeconds = 120.f;
		Tuning.InteractionDistanceCm = 200.f;
		Tuning.ElevatorCapacityKg = 500.f;
		Tuning.ElevatorTripLimit = 2;
		Tuning.AscentPenaltySeconds = 10.f;
		Tuning.ElevatorTravelSeconds = 2.f;
		Tuning.DepositIntervalSeconds = PourTickSeconds;
		Tuning.EmergencyWeightLimitKg = 3.f;
		Tuning.DeathLossPercent = 50;
		Tuning.RecoveredDeathLossPercent = 50;
		return Tuning;
	}

	struct FPourPlayer
	{
		ADigetherGameplayPlayerController* Controller = nullptr;
		ADigetherGameplayPlayerState* State = nullptr;
		APawn* Pawn = nullptr;
		bool IsReady() const { return IsValid(Controller) && IsValid(State) && IsValid(Pawn); }
		int32 Balance() const { return State->CaptureState().BalanceCredits; }
	};

	// 새 GameInstance의 임시 월드, 독립 실행 월드는 모든 PlayerController가 로컬이라 호스트 판정
	// 할당량 달성 자동 저장이 실제 슬롯에 기록되므로 플레이어별 고유 ID 사용, 종료 시 슬롯 삭제
	class FPourWorld
	{
	public:
		explicit FPourWorld(FAutomationTestBase& InTest) : Test(InTest)
		{
			if (!GEngine) return;
			Instance = NewObject<UGameInstance>(GEngine);
			Instance->AddToRoot();
			Instance->InitializeStandalone();
			World = Instance->GetWorld();
			const FURL URL(nullptr, TEXT("/Engine/Maps/Entry?game=/Script/DIGETHER.DigetherGameplayGameMode"), TRAVEL_Absolute);
			if (!World || !World->SetGameMode(URL)) return;
			World->InitializeActorsForPlay(URL);
			Mode = World->GetAuthGameMode<ADigetherGameplayGameMode>();
			GameState = World->GetGameState<ADigetherGameplayGameState>();
			if (!IsValid(Mode) || !IsValid(GameState)) return;
			Elevator = World->SpawnActor<ADigetherElevatorActor>();
			Departure = AddLever(EDigetherElevatorLeverKind::Departure, FVector(-100, 0, 0));
			AddLever(EDigetherElevatorLeverKind::Ascent, FVector(100, 0, 0));
			AddLever(EDigetherElevatorLeverKind::Ascent, FVector(400, 0, 0), EDigetherElevatorLeverPlacement::Outside);
			if (!IsValid(Elevator) || !IsValid(Departure)
				|| !FDigetherPourIntegrationAccess::Prepare(*Mode, MakePourTuning())) return;
			Hub = World->SpawnActor<ADigetherHubStateActor>();
			if (!IsValid(Hub)) return;
			UDigetherHubCatalog* Catalog = NewObject<UDigetherHubCatalog>(Hub);
			Catalog->Offers = {{FName(TEXT("Wood")), FText::FromString(TEXT("Fixture wood")), 10, 1}};
			Catalog->Stages = {{FName(TEXT("Mine")), FText::FromString(TEXT("Fixture mine")), 0, true}};
			Catalog->TrialStarterCredits = 0;
			Hub->Catalog = Catalog;
			if (!Hub->RestoreProgress(FDigetherHubProgress())) return;
			ADigetherHubStageAnchor* Anchor = World->SpawnActor<ADigetherHubStageAnchor>();
			if (!IsValid(Anchor)) return;
			Anchor->StageId = TEXT("Mine");
			Anchor->SetActorLocation(FVector(2000, 0, 0));
			Station = World->SpawnActor<ADigetherDepositStationActor>(PourStationLocation, FRotator::ZeroRotator);
			FNameProperty* StationIdProperty = FindFProperty<FNameProperty>(ADigetherDepositStationActor::StaticClass(), TEXT("StationId"));
			if (!IsValid(Station) || StationIdProperty == nullptr) return;
			*StationIdProperty->ContainerPtrToValuePtr<FName>(Station) = PourStationId;
			bReady = true;
		}

		~FPourWorld()
		{
			if (IsValid(Mode))
			{
				FDigetherPourIntegrationAccess::ClearPendingAutosave(*Mode);
			}
			if (Instance) Instance->Shutdown();
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine) GEngine->DestroyWorldContext(World);
			}
			if (Instance) Instance->RemoveFromRoot();
			for (const FString& Slot : TrackedSlots)
			{
				if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
				{
					Test.TestTrue(TEXT("붓기 시험 임시 슬롯 삭제"), UGameplayStatics::DeleteGameInSlot(Slot, 0));
				}
			}
		}

		FPourWorld(const FPourWorld&) = delete;
		FPourWorld& operator=(const FPourWorld&) = delete;

		bool IsReady() const { return bReady; }
		UWorld* GetWorld() const { return World; }
		ADigetherGameplayGameMode* GetMode() const { return Mode; }
		ADigetherDepositStationActor* GetStation() const { return Station; }
		ADigetherElevatorActor* GetElevator() const { return Elevator; }
		ADigetherElevatorLeverActor* GetDeparture() const { return Departure; }
		UDigetherQuotaStateComponent* GetQuota() const { return GameState->GetQuotaComponent(); }
		FDigetherQuotaState QuotaState() const { return GetQuota()->CaptureState().Quota; }

		void SetQuota(const int32 Day, const int32 Target, const int32 Deposited)
		{
			FDigetherQuotaState Quota = QuotaState();
			Quota.Day = Day;
			Quota.TargetCredits = Target;
			Quota.DepositedCredits = Deposited;
			Test.TestTrue(TEXT("시험 목표 설정"), GetQuota()->SetQuotaState(Quota) == EDigetherEconomyResult::Success);
		}

		FPourPlayer AddPlayer(const FVector& Location, const FRotator& Rotation, const int32 Credits)
		{
			FPourPlayer Player;
			Player.Controller = World->SpawnActor<ADigetherGameplayPlayerController>();
			if (!IsValid(Player.Controller)) return Player;
			// 저장 스냅샷에 호스트 계정 키 필요, 첫 플레이어를 호스트(로컬 컨트롤러)로 지정(SaveIntegration 픽스처와 동일)
			if (!bHostAssigned)
			{
				Player.Controller->SetAsLocalPlayerController();
				bHostAssigned = true;
			}
			Player.State = Player.Controller->GetPlayerState<ADigetherGameplayPlayerState>();
			if (!IsValid(Player.State)) Player.State = World->SpawnActor<ADigetherGameplayPlayerState>();
			if (!IsValid(Player.State)) return Player;
			Player.State->SetOwner(Player.Controller);
			Player.Controller->SetPlayerState(Player.State);
			GameState->AddPlayerState(Player.State);
			const FUniqueNetIdRepl Id(FUniqueNetIdRef(FUniqueNetIdString::Create(FGuid::NewGuid().ToString(), FName(TEXT("NULL")))));
			Player.State->SetUniqueId(Id);
			TrackSlots(Id);
			Player.State->InitializeGameplayIdentity();
			Player.State->MarkGameplayInitialized();
			FDigetherWalletState Wallet;
			Wallet.BalanceCredits = Credits;
			Player.State->RestoreState(Wallet, false);
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player.Pawn = World->SpawnActor<ADigetherTestPawn>(Location, Rotation, Spawn);
			if (IsValid(Player.Pawn))
			{
				Player.Controller->Possess(Player.Pawn);
				// 시험 월드는 카메라 미갱신, 시선 기준은 보는 대상(Pawn)의 위치·회전
				Player.Controller->SetViewTarget(Player.Pawn);
			}
			return Player;
		}

		FPourPlayer AddPourer(const FVector& Offset, const int32 Credits)
		{
			return AddPlayer(PourStationLocation + Offset, FacingStation, Credits);
		}

		// 자동 저장 요청 시 슬롯 기록(성공) 또는 재시도 대기(실패), 독립 실행 월드는 모든 컨트롤러가 로컬이라
		// 캡처·쓰기의 호스트 선택이 달라 쓰기 실패 가능, 둘 중 하나로 요청 여부 판정
		bool WasAutosaveRequested() const
		{
			return AnyTrackedSlotExists() || FDigetherPourIntegrationAccess::HasPendingAutosave(*Mode);
		}

		bool AnyTrackedSlotExists() const
		{
			for (const FString& Slot : TrackedSlots)
			{
				if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
				{
					return true;
				}
			}
			return false;
		}

		TArray<ADigetherFloorCoinActor*> FloorCoins() const
		{
			TArray<ADigetherFloorCoinActor*> Coins;
			for (TActorIterator<ADigetherFloorCoinActor> It(World); It; ++It)
			{
				if (IsValid(*It) && !It->IsActorBeingDestroyed())
				{
					Coins.Add(*It);
				}
			}
			return Coins;
		}

		void Tick(const float Seconds = PourTickSeconds) { Mode->Tick(Seconds); }

	private:
		ADigetherElevatorLeverActor* AddLever(EDigetherElevatorLeverKind Kind, const FVector& Location,
			EDigetherElevatorLeverPlacement Placement = EDigetherElevatorLeverPlacement::Inside)
		{
			ADigetherElevatorLeverActor* Lever = World->SpawnActor<ADigetherElevatorLeverActor>();
			if (!IsValid(Lever)) return nullptr;
			FEnumProperty* KindProperty = FindFProperty<FEnumProperty>(Lever->GetClass(), TEXT("LeverKind"));
			FEnumProperty* PlacementProperty = FindFProperty<FEnumProperty>(Lever->GetClass(), TEXT("Placement"));
			FObjectPropertyBase* ElevatorProperty = FindFProperty<FObjectPropertyBase>(Lever->GetClass(), TEXT("ElevatorActor"));
			if (!KindProperty || !PlacementProperty || !ElevatorProperty) return nullptr;
			*KindProperty->ContainerPtrToValuePtr<EDigetherElevatorLeverKind>(Lever) = Kind;
			*PlacementProperty->ContainerPtrToValuePtr<EDigetherElevatorLeverPlacement>(Lever) = Placement;
			ElevatorProperty->SetObjectPropertyValue_InContainer(Lever, Elevator);
			Lever->SetActorLocation(Location);
			return Lever;
		}

		void TrackSlots(const FUniqueNetIdRepl& Id)
		{
			FString Key;
			if (DigetherSaveRules::MakeAuthenticatedPlayerKey(Id, Key) == EDigetherSaveResult::Success)
			{
				const FDigetherSaveSlotNames Names = DigetherSaveStorage::MakeSlotNames(Key);
				TrackedSlots.Add(Names.Primary);
				TrackedSlots.Add(Names.Backup);
			}
		}

		FAutomationTestBase& Test;
		UGameInstance* Instance = nullptr;
		UWorld* World = nullptr;
		ADigetherGameplayGameMode* Mode = nullptr;
		ADigetherGameplayGameState* GameState = nullptr;
		ADigetherElevatorActor* Elevator = nullptr;
		bool bHostAssigned = false;
		ADigetherElevatorLeverActor* Departure = nullptr;
		ADigetherHubStateActor* Hub = nullptr;
		ADigetherDepositStationActor* Station = nullptr;
		TArray<FString> TrackedSlots;
		bool bReady = false;
	};

	bool BeginPour(FPourWorld& Fixture, const FPourPlayer& Player)
	{
		return Fixture.GetMode()->BeginPour(Player.Controller, Fixture.GetStation()).Result == EDigetherEconomyResult::Success;
	}
}

#if !UE_BUILD_SHIPPING
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherPourDevTeleportPreparesFourTest,
	"DIGETHER.Game.Economy.PourIntegration.DevTeleportPreparesFourPourers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherPourDevTeleportPreparesFourTest::RunTest(const FString& Parameters)
{
	// 실제 개발 이동 뒤 제품 붓기 판정 검증, 원격 카메라 전송은 별도 M2 대상
	for (const float StationYaw : { 0.f, 90.f })
	{
		FPourWorld Fixture(*this);
		if (!TestTrue(TEXT("개발 이동 시험 월드 준비"), Fixture.IsReady()))
		{
			return false;
		}
		Fixture.GetStation()->SetActorRotation(FRotator(0.f, StationYaw, 0.f));
		Fixture.SetQuota(1, 10000, 0);
		TArray<FPourPlayer> Players;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const FVector StartLocation = PourStationLocation + FVector(600.f + Index * 100.f, 0.f, 100.f);
			FPourPlayer Player = Fixture.AddPlayer(StartLocation, FRotator(0.f, 45.f, 0.f), 100);
			if (!TestTrue(TEXT("개발 이동 플레이어 준비"), Player.IsReady()))
			{
				return false;
			}
			// 개발 RPC 권한을 각 시험 컨트롤러에 명시, 원격 권한은 실제 M2에서 검증
			Player.Controller->SetAsLocalPlayerController();
			Player.Controller->Server_DevTeleportToDeposit_Implementation(PourStationId);
			TestFalse(TEXT("기존 먼 자리에서 이동"), Player.Pawn->GetActorLocation().Equals(StartLocation));
			Players.Add(Player);
		}
		const FTransform Inlet = Fixture.GetStation()->GetPourInletTransform();
		for (int32 Index = 0; Index < Players.Num(); ++Index)
		{
			const FPourPlayer& Player = Players[Index];
			const FVector Location = Player.Pawn->GetActorLocation();
			TestTrue(TEXT("회전된 투입구 앞면에 위치"),
				FVector::DotProduct(Inlet.GetUnitAxis(EAxis::X), Location - Inlet.GetLocation()) > 0.0);
			TestTrue(TEXT("기존 200cm 상호작용 거리 안"), FVector::Distance(Location, Inlet.GetLocation()) <= 200.0);
			TestTrue(TEXT("실제 입력 대상의 원점 거리 200cm 안"),
				FVector::Distance(Location, Fixture.GetStation()->GetActorLocation()) <= 200.0);
			FVector Eye;
			FRotator ViewRotation;
			Player.Controller->GetPlayerViewPoint(Eye, ViewRotation);
			TestTrue(TEXT("개발 이동이 투입구 시선도 정렬"),
				FVector::DotProduct(ViewRotation.Vector(), (Inlet.GetLocation() - Eye).GetSafeNormal()) > 0.99);
			for (int32 OtherIndex = 0; OtherIndex < Index; ++OtherIndex)
			{
				const APawn* OtherPawn = Players[OtherIndex].Pawn;
				TestTrue(TEXT("4명 시험 Pawn의 충돌 구체 비겹침"),
					FVector::Distance(Location, OtherPawn->GetActorLocation())
						> Player.Pawn->GetSimpleCollisionRadius() + OtherPawn->GetSimpleCollisionRadius());
			}
			TestEqual(TEXT("4명이 배치된 뒤에도 기존 붓기 시작 성공"),
				Fixture.GetMode()->BeginPour(Player.Controller, Fixture.GetStation()).Result,
				EDigetherEconomyResult::Success);
		}
		Fixture.Tick();
		for (const FPourPlayer& Player : Players)
		{
			TestEqual(TEXT("각자 붓기 한 주기만 차감"), Player.Balance(), 90);
			Fixture.GetMode()->EndPour(Player.Controller);
		}
		TestEqual(TEXT("4명 인정액을 한 번씩 반영"), Fixture.QuotaState().DepositedCredits, 12);
		TestEqual(TEXT("중지 뒤 붓기 세션 정리"), FDigetherPourIntegrationAccess::PourSessionCount(*Fixture.GetMode()), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherPourDevTeleportFailureTest,
	"DIGETHER.Game.Economy.PourIntegration.DevTeleportFailurePreservesPose",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherPourDevTeleportFailureTest::RunTest(const FString& Parameters)
{
	FPourWorld Fixture(*this);
	if (!TestTrue(TEXT("개발 이동 실패 시험 월드 준비"), Fixture.IsReady()))
	{
		return false;
	}
	const FVector StartLocation = PourStationLocation + FVector(1000.f, 0.f, 100.f);
	const FRotator StartRotation(0.f, 45.f, 0.f);
	FPourPlayer Player = Fixture.AddPlayer(StartLocation, StartRotation, 100);
	if (!TestTrue(TEXT("개발 이동 실패 플레이어 준비"), Player.IsReady()))
	{
		return false;
	}
	Player.Controller->SetControlRotation(StartRotation);
	Player.Controller->Server_DevTeleportToDeposit_Implementation(TEXT("MissingStation"));
	TestTrue(TEXT("미발견 납부대는 위치 보존"), Player.Pawn->GetActorLocation().Equals(StartLocation));
	TestTrue(TEXT("미발견 납부대는 시선 보존"), Player.Controller->GetControlRotation().Equals(StartRotation));

	AActor* Blocker = Fixture.GetWorld()->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("모든 앞자리를 막는 시험 장애물"), Blocker))
	{
		return false;
	}
	UBoxComponent* Box = NewObject<UBoxComponent>(Blocker);
	Blocker->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(400.f));
	Box->SetCollisionProfileName(TEXT("BlockAll"));
	Box->RegisterComponent();
	Blocker->SetActorLocation(Fixture.GetStation()->GetPourInletTransform().GetLocation());
	Player.Controller->Server_DevTeleportToDeposit_Implementation(PourStationId);
	TestTrue(TEXT("빈자리 없으면 기존 위치 보존"), Player.Pawn->GetActorLocation().Equals(StartLocation));
	TestTrue(TEXT("빈자리 없으면 기존 시선 보존"), Player.Controller->GetControlRotation().Equals(StartRotation));
	TestTrue(TEXT("빈자리 없으면 기존 Pawn 회전 보존"), Player.Pawn->GetActorRotation().Equals(StartRotation));
	TestEqual(TEXT("실패한 개발 이동은 잔고 보존"), Player.Balance(), 100);
	return true;
}
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherPourConcurrentTest,
	"DIGETHER.Game.Economy.PourIntegration.ConcurrentAndDisconnect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherPourConcurrentTest::RunTest(const FString& Parameters)
{
	// T17: 두 명 동시 붓기 중 한 명 연결 끊김
	FPourWorld Fixture(*this);
	if (!TestTrue(TEXT("붓기 시험 월드 준비"), Fixture.IsReady())) return false;
	Fixture.SetQuota(4, 100, 0);
	FPourPlayer A = Fixture.AddPourer(PourerOffsetA, 100);
	FPourPlayer B = Fixture.AddPourer(PourerOffsetB, 100);
	if (!TestTrue(TEXT("붓는 두 사람 준비"), A.IsReady() && B.IsReady())) return false;
	if (!TestTrue(TEXT("A 붓기 시작"), BeginPour(Fixture, A)) || !TestTrue(TEXT("B 동시 붓기 시작"), BeginPour(Fixture, B)))
	{
		return false;
	}
	TestEqual(TEXT("누른 순간에는 차감 없음"), A.Balance(), 100);
	Fixture.Tick(PourTickSeconds * 0.5f);
	TestEqual(TEXT("간격 전에는 차감 없음"), A.Balance() + B.Balance(), 200);
	Fixture.Tick(PourTickSeconds * 0.5f);
	TestEqual(TEXT("A 한 번 차감"), A.Balance(), 90);
	TestEqual(TEXT("B 한 번 차감"), B.Balance(), 90);
	TestEqual(TEXT("인정액 중복 없음"), Fixture.QuotaState().DepositedCredits, 20);

	Fixture.GetMode()->NotifyPlayerDisconnecting(B.Controller);
	TestEqual(TEXT("끊긴 B만 멈춤"), FDigetherPourIntegrationAccess::PourSessionCount(*Fixture.GetMode()), 1);
	Fixture.Tick();
	TestEqual(TEXT("A는 계속 붓기"), A.Balance(), 80);
	TestEqual(TEXT("B 확정분 유지·추가 차감 없음"), B.Balance(), 90);
	TestEqual(TEXT("공동 인정액"), Fixture.QuotaState().DepositedCredits, 30);

	TestTrue(TEXT("A 손을 뗌"), Fixture.GetMode()->EndPour(A.Controller).Result == EDigetherEconomyResult::Success);
	Fixture.Tick();
	TestEqual(TEXT("뗀 뒤 차감 없음"), A.Balance(), 80);

	// 거리 이탈 시 그 사람만 멈춤
	TestTrue(TEXT("A 다시 붓기"), BeginPour(Fixture, A));
	A.Pawn->SetActorLocation(PourStationLocation + FVector(800.0, 0.0, 100.0));
	Fixture.Tick();
	TestEqual(TEXT("거리 이탈은 붓기 멈춤"), FDigetherPourIntegrationAccess::PourSessionCount(*Fixture.GetMode()), 0);
	TestEqual(TEXT("거리 이탈 후 차감 없음"), A.Balance(), 80);

	// 투입구를 등지면 시작 불가
	A.Pawn->SetActorLocationAndRotation(PourStationLocation + PourerOffsetA, FRotator::ZeroRotator);
	TestEqual(TEXT("입구를 보지 않으면 거절"), Fixture.GetMode()->BeginPour(A.Controller, Fixture.GetStation()).Result,
		EDigetherEconomyResult::PourInteractionLost);
	// 정산기 뒤는 입구 앞쪽이 아니라 거절
	A.Pawn->SetActorLocationAndRotation(PourStationLocation + FVector(-50.0, 0.0, 100.0), FRotator::ZeroRotator);
	TestEqual(TEXT("정산기 뒤에서 거절"), Fixture.GetMode()->BeginPour(A.Controller, Fixture.GetStation()).Result,
		EDigetherEconomyResult::PourInteractionLost);
	TestEqual(TEXT("거절은 차감 없음"), A.Balance(), 80);
	TestEqual(TEXT("신호기는 목표의 30%로 빨강"), Fixture.GetQuota()->GetPaymentSignal(), EDigetherPaymentSignal::Red);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherPourOverflowTest,
	"DIGETHER.Game.Economy.PourIntegration.OverflowFloorCoinPickup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherPourOverflowTest::RunTest(const FString& Parameters)
{
	// T18·T79: 남은 목표 초과 주기는 목표만큼 인정, 남은 금화는 바닥에 떨어져 누구나 줍기 가능
	FPourWorld Fixture(*this);
	if (!TestTrue(TEXT("붓기 시험 월드 준비"), Fixture.IsReady())) return false;
	Fixture.SetQuota(4, 100, 95);
	const int32 TotalDayBeforeCompletion = Fixture.QuotaState().TotalDay;
	FPourPlayer A = Fixture.AddPourer(PourerOffsetA, 100);
	FPourPlayer B = Fixture.AddPourer(PourerOffsetB, 0);
	FPourPlayer C = Fixture.AddPourer(PourerOffsetC, 0);
	if (!TestTrue(TEXT("세 사람 준비"), A.IsReady() && B.IsReady() && C.IsReady())) return false;
	int32 CompletedCount = 0;
	Fixture.GetQuota()->OnQuotaCompleted.AddLambda([&CompletedCount]() { ++CompletedCount; });
	TestFalse(TEXT("달성 전에는 자동 저장 없음"), Fixture.WasAutosaveRequested());
	TestEqual(TEXT("잔고 0은 붓기 시작 거절"), Fixture.GetMode()->BeginPour(B.Controller, Fixture.GetStation()).Result,
		EDigetherEconomyResult::InsufficientFunds);
	if (!TestTrue(TEXT("A 붓기 시작"), BeginPour(Fixture, A))) return false;
	Fixture.Tick();
	TestEqual(TEXT("부은 10원 전부 차감"), A.Balance(), 90);
	const FDigetherQuotaState NewQuota = Fixture.QuotaState();
	TestEqual(TEXT("다음 목표"), NewQuota.TargetCredits, 200);
	TestEqual(TEXT("달성 즉시 1일차"), NewQuota.Day, 1);
	TestEqual(TEXT("달성은 탐사 종료가 아니므로 전체 일차 유지"), NewQuota.TotalDay, TotalDayBeforeCompletion);
	TestEqual(TEXT("초과분은 새 목표에 넣지 않음"), NewQuota.DepositedCredits, 0);
	TestEqual(TEXT("기간 세대 증가"), NewQuota.PeriodGeneration, 1);
	TestEqual(TEXT("달성 연출 한 번"), CompletedCount, 1);
	TestEqual(TEXT("달성 순간 모든 붓기 끊김"), FDigetherPourIntegrationAccess::PourSessionCount(*Fixture.GetMode()), 0);
	TestTrue(TEXT("달성 시 자동 저장 요청"), Fixture.WasAutosaveRequested());
	TestEqual(TEXT("새 사이클 신호기는 빨강"), Fixture.GetQuota()->GetPaymentSignal(), EDigetherPaymentSignal::Red);
	TArray<ADigetherFloorCoinActor*> Coins = Fixture.FloorCoins();
	if (!TestEqual(TEXT("바닥 금화 더미 하나"), Coins.Num(), 1))
	{
		return false;
	}
	ADigetherFloorCoinActor* Coin = Coins[0];
	TestEqual(TEXT("바닥 금화 F = S - N = 5"), Coin->GetAmountCredits(), 5);
	TestTrue(TEXT("정산기 앞 바닥 위치"), Coin->GetActorLocation().Equals(Fixture.GetStation()->GetCoinDropTransform().GetLocation()));

	// 같은 처리 시점의 두 줍기 요청은 먼저 처리된 한 명만 수령
	const FDigetherDepositResponse First = Fixture.GetMode()->PickupFloorCoin(B.Controller, Coin);
	const FDigetherDepositResponse Second = Fixture.GetMode()->PickupFloorCoin(C.Controller, Coin);
	TestEqual(TEXT("B 줍기 성공"), First.Result, EDigetherEconomyResult::Success);
	TestEqual(TEXT("B가 주운 금액"), First.AcceptedCredits, 5);
	TestEqual(TEXT("C 중복 줍기 거절"), Second.Result, EDigetherEconomyResult::FloorCoinUnavailable);
	TestEqual(TEXT("B 잔고에 들어감"), B.Balance(), 5);
	TestEqual(TEXT("C 잔고 변화 없음"), C.Balance(), 0);
	TestEqual(TEXT("줍기는 런 지급 통계에 넣지 않음"), B.State->CaptureState().LastRunPayoutCredits, 0);
	TestEqual(TEXT("줍기는 히든 통계에 넣지 않음"), B.State->CaptureState().HiddenLifetimeCredits, 0);
	TestEqual(TEXT("주워도 새 목표에 더하지 않음"), Fixture.QuotaState().DepositedCredits, 0);
	TestTrue(TEXT("주운 금화 제거"), Fixture.FloorCoins().IsEmpty());

	// 정산기당 금화 더미 하나에 합산(1일차 30%: 남은 2에 10원 → N 7, F 3)
	Fixture.SetQuota(1, 200, 198);
	if (!TestTrue(TEXT("다시 눌러 새 목표에 붓기"), BeginPour(Fixture, A))) return false;
	Fixture.Tick();
	TestEqual(TEXT("1일차 초과 금화"), Fixture.FloorCoins().Num() == 1 ? Fixture.FloorCoins()[0]->GetAmountCredits() : -1, 3);
	Fixture.SetQuota(4, 300, 295);
	if (!TestTrue(TEXT("세 번째 목표에 붓기"), BeginPour(Fixture, A))) return false;
	Fixture.Tick();
	Coins = Fixture.FloorCoins();
	if (TestEqual(TEXT("두 번 넘쳐도 더미 하나"), Coins.Num(), 1))
	{
		TestEqual(TEXT("금액 합산"), Coins[0]->GetAmountCredits(), 8);
	}
	TestEqual(TEXT("달성 연출 세 번"), CompletedCount, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherPourDayOneAfterCompletionTest,
	"DIGETHER.Game.Economy.PourIntegration.CompletionStartsDayOne",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherPourDayOneAfterCompletionTest::RunTest(const FString& Parameters)
{
	// T58: 3일차 거점 달성 시 즉시 1일차, 추가 붓기는 새 목표에 1일차 비율 반영
	FPourWorld Fixture(*this);
	if (!TestTrue(TEXT("붓기 시험 월드 준비"), Fixture.IsReady())) return false;
	Fixture.SetQuota(3, 100, 95);
	FPourPlayer A = Fixture.AddPourer(PourerOffsetA, 100);
	if (!TestTrue(TEXT("붓는 사람 준비"), A.IsReady()) || !TestTrue(TEXT("3일차 붓기"), BeginPour(Fixture, A))) return false;
	Fixture.Tick();
	TestEqual(TEXT("3일차 70%: 10원 부어 목표 달성"), Fixture.QuotaState().Day, 1);
	TestEqual(TEXT("3일차 초과 금화 F = 10 - ceil(500/70)"), Fixture.FloorCoins().Num() == 1
		? Fixture.FloorCoins()[0]->GetAmountCredits() : -1, 2);
	Fixture.Tick();
	TestEqual(TEXT("달성 뒤에는 다시 누르기 전까지 차감 없음"), A.Balance(), 90);
	if (!TestTrue(TEXT("다시 눌러 새 목표에 붓기"), BeginPour(Fixture, A))) return false;
	Fixture.Tick();
	TestEqual(TEXT("새 목표 붓기 차감"), A.Balance(), 80);
	TestEqual(TEXT("1일차 30% 비율로 반영"), Fixture.QuotaState().DepositedCredits, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherPourDuringLeverTest,
	"DIGETHER.Game.Economy.PourIntegration.CompletionCancelsDepartureLever",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherPourDuringLeverTest::RunTest(const FString& Parameters)
{
	// T62·T63(달성 먼저): 4일차 레버 당기는 중 동료가 목표 충족 시 이전 출발·마감·게이지 취소
	FPourWorld Fixture(*this);
	if (!TestTrue(TEXT("붓기 시험 월드 준비"), Fixture.IsReady())) return false;
	Fixture.SetQuota(4, 100, 95);
	FPourPlayer Operator = Fixture.AddPlayer(LeverOperatorLocation, FRotator::ZeroRotator, 0);
	FPourPlayer A = Fixture.AddPourer(PourerOffsetA, 100);
	if (!TestTrue(TEXT("레버 조작자와 붓는 사람 준비"), Operator.IsReady() && A.IsReady())) return false;
	if (!TestTrue(TEXT("4일차 출발 레버 잡기"), Fixture.GetMode()->BeginLeverPull(Operator.Controller, Fixture.GetDeparture()))
		|| !TestTrue(TEXT("붓기 시작"), BeginPour(Fixture, A)))
	{
		return false;
	}
	Fixture.Tick();
	const UDigetherElevatorLeverComponent* Lever = Fixture.GetDeparture()->GetLeverComponent();
	TestEqual(TEXT("새 목표·1일차"), Fixture.QuotaState().Day, 1);
	TestFalse(TEXT("게이지 취소"), Lever->GetActivePullerId().IsValid());
	TestEqual(TEXT("게이지 0"), Lever->GetPublicState().Progress, 0.f);
	TestFalse(TEXT("이전 기간 마감 없음"), Fixture.GetMode()->GetRunCoordinatorComponent()->CapturePublicState().bPaymentDeadline);
	TestTrue(TEXT("문은 열린 거점 대기"), Fixture.GetElevator()->GetElevatorComponent()->GetPublicState().Phase
		== EDigetherElevatorPhase::HubWaiting);
	TestTrue(TEXT("새 목표 1일차 레버를 다시 당길 수 있음"),
		Fixture.GetMode()->BeginLeverPull(Operator.Controller, Fixture.GetDeparture()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherPourDeadlineFirstTest,
	"DIGETHER.Game.Economy.PourIntegration.DeadlineBeforePourRejects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherPourDeadlineFirstTest::RunTest(const FString& Parameters)
{
	// T63(마감 먼저): 레버 게이지 완료가 먼저면 진행 중 붓기 중단, 이후 붓기는 차감 없이 거절
	FPourWorld Fixture(*this);
	if (!TestTrue(TEXT("붓기 시험 월드 준비"), Fixture.IsReady())) return false;
	Fixture.SetQuota(4, 100, 0);
	// 레버가 차는 동안 붓기 주기가 오지 않게 긴 간격(시험 값)
	FDigetherPourIntegrationAccess::SetPourInterval(*Fixture.GetMode(), 1000.f);
	FPourPlayer Operator = Fixture.AddPlayer(LeverOperatorLocation, FRotator::ZeroRotator, 0);
	FPourPlayer A = Fixture.AddPourer(PourerOffsetA, 100);
	if (!TestTrue(TEXT("레버 조작자와 붓는 사람 준비"), Operator.IsReady() && A.IsReady())) return false;
	if (!TestTrue(TEXT("4일차 출발 레버 잡기"), Fixture.GetMode()->BeginLeverPull(Operator.Controller, Fixture.GetDeparture()))
		|| !TestTrue(TEXT("붓기 시작"), BeginPour(Fixture, A)))
	{
		return false;
	}
	const UDigetherElevatorComponent* Elevator = Fixture.GetElevator()->GetElevatorComponent();
	const int32 MaxTicks = FMath::CeilToInt32(DigetherElevatorRules::CalculateDepartureDuration(4, FDigetherElevatorTuning())
		/ PourTickSeconds) + 2;
	bool bClosing = false;
	for (int32 Count = 0; Count < MaxTicks && !bClosing; ++Count)
	{
		Fixture.Tick();
		bClosing = Elevator->GetPublicState().Phase == EDigetherElevatorPhase::DoorClosing;
	}
	if (!TestTrue(TEXT("4일차 게이지 완료로 문 닫힘"), bClosing)) return false;
	TestTrue(TEXT("미달 상태 4일차 마감 기록"), Fixture.GetMode()->GetRunCoordinatorComponent()->CapturePublicState().bPaymentDeadline);
	TestEqual(TEXT("마감 순간 붓기 멈춤"), FDigetherPourIntegrationAccess::PourSessionCount(*Fixture.GetMode()), 0);
	TestEqual(TEXT("마감 뒤 붓기 시작 거절"), Fixture.GetMode()->BeginPour(A.Controller, Fixture.GetStation()).Result,
		EDigetherEconomyResult::DepositClosed);
	TestEqual(TEXT("마감 뒤 차감 없음"), A.Balance(), 100);
	TestEqual(TEXT("마감 값 유지"), Fixture.QuotaState().DepositedCredits, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherPourSameFrameTest,
	"DIGETHER.Game.Economy.PourIntegration.SameFramePourBeforeLever",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherPourSameFrameTest::RunTest(const FString& Parameters)
{
	// T78: 마지막 붓기 주기와 레버 게이지 완료가 같은 처리 시점이면 붓기 먼저 판정
	const float Duration = DigetherElevatorRules::CalculateDepartureDuration(4, FDigetherElevatorTuning());
	constexpr float FinalStepSeconds = 0.1f;
	for (const bool bReachesTarget : { true, false })
	{
		FPourWorld Fixture(*this);
		if (!TestTrue(TEXT("붓기 시험 월드 준비"), Fixture.IsReady()) || !TestTrue(TEXT("4일차 게이지 길이"), Duration > FinalStepSeconds))
		{
			return false;
		}
		Fixture.SetQuota(4, 100, bReachesTarget ? 95 : 0);
		// 붓기 첫 주기와 게이지 완료를 같은 Tick에 맞추려 간격 = 게이지 길이(시험 값)
		FDigetherPourIntegrationAccess::SetPourInterval(*Fixture.GetMode(), Duration);
		FPourPlayer Operator = Fixture.AddPlayer(LeverOperatorLocation, FRotator::ZeroRotator, 0);
		FPourPlayer A = Fixture.AddPourer(PourerOffsetA, 100);
		if (!TestTrue(TEXT("레버 조작자와 붓는 사람 준비"), Operator.IsReady() && A.IsReady())
			|| !TestTrue(TEXT("4일차 출발 레버 잡기"), Fixture.GetMode()->BeginLeverPull(Operator.Controller, Fixture.GetDeparture()))
			|| !TestTrue(TEXT("붓기 시작"), BeginPour(Fixture, A)))
		{
			return false;
		}
		const UDigetherElevatorComponent* Elevator = Fixture.GetElevator()->GetElevatorComponent();
		Fixture.Tick(Duration - FinalStepSeconds);
		TestEqual(TEXT("완료 직전 Tick에는 차감 없음"), A.Balance(), 100);
		TestTrue(TEXT("완료 직전 Tick에는 문 열림"), Elevator->GetPublicState().Phase == EDigetherElevatorPhase::HubWaiting);
		Fixture.Tick(FinalStepSeconds);
		TestEqual(TEXT("같은 Tick에 붓기 한 번 반영"), A.Balance(), 90);
		TestTrue(TEXT("완료된 레버로 문 닫힘"), Elevator->GetPublicState().Phase == EDigetherElevatorPhase::DoorClosing);
		const FDigetherRunPublicState Run = Fixture.GetMode()->GetRunCoordinatorComponent()->CapturePublicState();
		if (bReachesTarget)
		{
			TestEqual(TEXT("달성하면 새 사이클 1일차로 출발"), Fixture.QuotaState().Day, 1);
			TestFalse(TEXT("달성하면 4일차 마감 없음"), Run.bPaymentDeadline);
		}
		else
		{
			TestEqual(TEXT("미달이면 4일차 유지"), Fixture.QuotaState().Day, 4);
			TestEqual(TEXT("마감 전에 붓기 반영"), Fixture.QuotaState().DepositedCredits, 10);
			TestTrue(TEXT("미달이면 4일차 마감 기록"), Run.bPaymentDeadline);
		}
		TestEqual(TEXT("마감 뒤 도착한 붓기는 차감 없이 거절"),
			Fixture.GetMode()->BeginPour(A.Controller, Fixture.GetStation()).Result, EDigetherEconomyResult::DepositClosed);
		TestEqual(TEXT("거절 뒤 잔고 유지"), A.Balance(), 90);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherPourSignalAndRestoreTest,
	"DIGETHER.Game.Economy.PourIntegration.SignalSaveRestoreAndGameOver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherPourSignalAndRestoreTest::RunTest(const FString& Parameters)
{
	// T79: 신호기는 시험 경계대로 변경, 남은 바닥 금화는 저장 복원(이어하기·공통 로딩 실패 복귀와 같은 경로)으로 복귀
	FPourWorld Fixture(*this);
	if (!TestTrue(TEXT("붓기 시험 월드 준비"), Fixture.IsReady())) return false;
	FPourPlayer A = Fixture.AddPourer(PourerOffsetA, 100);
	FPourPlayer B = Fixture.AddPourer(PourerOffsetB, 0);
	if (!TestTrue(TEXT("두 사람 준비"), A.IsReady() && B.IsReady())) return false;
	Fixture.SetQuota(4, 100, 0);
	struct FSignalCase
	{
		int32 Deposited;
		EDigetherPaymentSignal Expected;
	};
	const FSignalCase Cases[] =
	{
		{ 0, EDigetherPaymentSignal::Red },
		{ 49, EDigetherPaymentSignal::Red },
		{ 50, EDigetherPaymentSignal::Orange },
		{ 79, EDigetherPaymentSignal::Orange },
		{ 80, EDigetherPaymentSignal::Green },
		{ 95, EDigetherPaymentSignal::Green }
	};
	for (const FSignalCase& Case : Cases)
	{
		TestTrue(TEXT("개발 명령으로 공동 인정액 설정"), Fixture.GetMode()->SetQuotaProgressForDev(A.Controller, Case.Deposited));
		TestEqual(*FString::Printf(TEXT("인정액 %d의 신호기"), Case.Deposited), Fixture.GetQuota()->GetPaymentSignal(), Case.Expected);
	}
	TestFalse(TEXT("개발 명령은 목표 이상을 받지 않음"), Fixture.GetMode()->SetQuotaProgressForDev(A.Controller, 100));

	int32 CompletedCount = 0;
	Fixture.GetQuota()->OnQuotaCompleted.AddLambda([&CompletedCount]() { ++CompletedCount; });
	if (!TestTrue(TEXT("초과 붓기 시작"), BeginPour(Fixture, A))) return false;
	Fixture.Tick();
	TArray<ADigetherFloorCoinActor*> Coins = Fixture.FloorCoins();
	if (!TestEqual(TEXT("초과 금화 하나"), Coins.Num(), 1)) return false;
	const FGuid CoinId = Coins[0]->GetCoinId();
	const FVector CoinLocation = Coins[0]->GetActorLocation();
	FDigetherSaveSnapshot Snapshot;
	if (!TestTrue(TEXT("거점 진행 캡처"), FDigetherPourIntegrationAccess::Capture(*Fixture.GetMode(), Snapshot))
		|| !TestEqual(TEXT("저장 값에 바닥 금화"), Snapshot.FloorCoins.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("저장 금액"), Snapshot.FloorCoins[0].AmountCredits, 5);

	// 저장 뒤 주운 금화도 복원 시 저장 시점대로 재생성, 잔고도 저장 시점으로 복귀
	TestEqual(TEXT("저장 뒤 줍기"), Fixture.GetMode()->PickupFloorCoin(B.Controller, Coins[0]).Result, EDigetherEconomyResult::Success);
	TestTrue(TEXT("주운 뒤 금화 없음"), Fixture.FloorCoins().IsEmpty());
	if (!TestTrue(TEXT("저장 진행 복원"), FDigetherPourIntegrationAccess::Restore(*Fixture.GetMode(), Snapshot))) return false;
	Coins = Fixture.FloorCoins();
	if (TestEqual(TEXT("복원된 금화 더미 하나"), Coins.Num(), 1))
	{
		TestTrue(TEXT("같은 금화 ID"), Coins[0]->GetCoinId() == CoinId);
		TestEqual(TEXT("같은 금액"), Coins[0]->GetAmountCredits(), 5);
		TestTrue(TEXT("같은 위치"), Coins[0]->GetActorLocation().Equals(CoinLocation));
	}
	TestEqual(TEXT("복원 잔고"), B.Balance(), 0);
	TestEqual(TEXT("복원된 신호기 색(새 사이클 0)"), Fixture.GetQuota()->GetPaymentSignal(), EDigetherPaymentSignal::Red);
	TestEqual(TEXT("복원은 달성 연출을 다시 재생하지 않음"), CompletedCount, 1);

	// 다른 금화가 있던 상태에 복원해도 저장 내용만 남음
	FDigetherSaveSnapshot Empty = Snapshot;
	Empty.FloorCoins.Reset();
	TestTrue(TEXT("금화 없는 저장 복원"), FDigetherPourIntegrationAccess::Restore(*Fixture.GetMode(), Empty));
	TestTrue(TEXT("기존 금화 제거"), Fixture.FloorCoins().IsEmpty());
	TestTrue(TEXT("금화 저장 다시 복원"), FDigetherPourIntegrationAccess::Restore(*Fixture.GetMode(), Snapshot));

	// 게임 오버 귀환 확정 시 바닥 금화 비움(실패 사건·크레딧 중 줍기·붓기 불가)
	ADigetherFloorCoinActor* Stale = Fixture.FloorCoins().Num() == 1 ? Fixture.FloorCoins()[0] : nullptr;
	FDigetherPourIntegrationAccess::CloseHubTradingForGameOver(*Fixture.GetMode());
	TestTrue(TEXT("게임 오버 확정 뒤 금화 없음"), Fixture.FloorCoins().IsEmpty());
	TestEqual(TEXT("지워진 금화 줍기 거절"), Fixture.GetMode()->PickupFloorCoin(B.Controller, Stale).Result,
		EDigetherEconomyResult::FloorCoinUnavailable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherPourReplicationPrivacyTest,
	"DIGETHER.Game.Economy.PourIntegration.ReplicatedPropertiesHideTotals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherPourReplicationPrivacyTest::RunTest(const FString& Parameters)
{
	// S24: 공동 인정액·바닥 금화 금액은 복제 프로퍼티에 미포함
	const auto ContainsDepositedTotal = [](const FProperty* Property)
	{
		if (Property->GetFName() == TEXT("DepositedCredits"))
		{
			return true;
		}
		const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		return StructProperty != nullptr
			&& FindFProperty<FProperty>(StructProperty->Struct, TEXT("DepositedCredits")) != nullptr;
	};
	int32 QuotaReplicated = 0;
	for (TFieldIterator<FProperty> It(UDigetherQuotaStateComponent::StaticClass()); It; ++It)
	{
		if (It->HasAnyPropertyFlags(CPF_Net) && It->GetOwnerClass() == UDigetherQuotaStateComponent::StaticClass())
		{
			++QuotaReplicated;
			TestFalse(FString::Printf(TEXT("%s에 누적 인정액 없음"), *It->GetName()), ContainsDepositedTotal(*It));
		}
	}
	TestEqual(TEXT("할당량 복제 프로퍼티는 공개 상태와 달성 카운터"), QuotaReplicated, 2);
	for (TFieldIterator<FProperty> It(ADigetherFloorCoinActor::StaticClass()); It; ++It)
	{
		if (It->HasAnyPropertyFlags(CPF_Net) && It->GetOwnerClass() == ADigetherFloorCoinActor::StaticClass())
		{
			AddError(FString::Printf(TEXT("바닥 금화 Actor에 복제 프로퍼티가 생겼습니다: %s"), *It->GetName()));
		}
	}
	TestTrue(TEXT("바닥 금화 금액은 리플렉션 프로퍼티가 아님"),
		FindFProperty<FProperty>(ADigetherFloorCoinActor::StaticClass(), TEXT("AmountCredits")) == nullptr);
	TestTrue(TEXT("바닥 금화 Actor는 존재·위치만 복제"), GetDefault<ADigetherFloorCoinActor>()->GetIsReplicated());
	return true;
}

#endif
