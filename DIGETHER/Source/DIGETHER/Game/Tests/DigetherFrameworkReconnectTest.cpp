#if WITH_DEV_AUTOMATION_TESTS

// 아래 숫자·ID는 테스트 픽스처, 기획 값 아님

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "OnlineSubsystemTypes.h"

namespace
{
	// 인증·실제 접속 없이 서버 액터 인계·수명 검사, 실제 ID 재접속·복제는 M2/M3
	class FDigetherReconnectTestWorld
	{
	public:
		FDigetherReconnectTestWorld()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &Values);
		}

		~FDigetherReconnectTestWorld()
		{
			if (World != nullptr)
			{
				World->DestroyWorld(false);
			}
		}

		FDigetherReconnectTestWorld(const FDigetherReconnectTestWorld&) = delete;
		FDigetherReconnectTestWorld& operator=(const FDigetherReconnectTestWorld&) = delete;

		UWorld* Get() const { return World; }

	private:
		UWorld* World = nullptr;
	};

	template <typename TState>
	bool AreReconnectStatesEqual(const TState& Left, const TState& Right)
	{
		return TState::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
	}

	FDigetherWalletState MakeReconnectWallet()
	{
		FDigetherWalletState Wallet;
		Wallet.BalanceCredits = 75;
		Wallet.LastRunPayoutCredits = 11;
		Wallet.HiddenLifetimeCredits = 13;
		Wallet.TotalDeathLossCredits = 17;
		Wallet.LastPreDeathBalanceCredits = 200;
		return Wallet;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkPlayerStateHandoffTest,
	"DIGETHER.Game.Framework.Reconnect.PlayerStateHandoff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkPlayerStateHandoffTest::RunTest(const FString& Parameters)
{
	FDigetherReconnectTestWorld TestWorld;
	UWorld* World = TestWorld.Get();
	if (!TestNotNull(TEXT("독립 서버 시험 월드"), World))
	{
		return false;
	}
	ADigetherGameplayPlayerState* Source = World->SpawnActor<ADigetherGameplayPlayerState>();
	ADigetherGameplayPlayerState* CopyTarget = World->SpawnActor<ADigetherGameplayPlayerState>();
	ADigetherGameplayPlayerState* OverrideTarget = World->SpawnActor<ADigetherGameplayPlayerState>();
	if (!TestNotNull(TEXT("인계 원본"), Source) || !TestNotNull(TEXT("복사 대상"), CopyTarget)
		|| !TestNotNull(TEXT("복원 대상"), OverrideTarget))
	{
		return false;
	}
	Source->InitializeGameplayIdentity();
	Source->MarkGameplayInitialized();
	const FGuid SourceId = Source->GetGameplayPlayerId();
	const FDigetherWalletState Wallet = MakeReconnectWallet();
	TestEqual(TEXT("시험 지갑 적용"), Source->RestoreState(Wallet), EDigetherEconomyResult::Success);
	TestEqual(TEXT("시험 탈출 적용"), Source->GetLifeComponent()->ApplyEscape(EDigetherEscapeKind::Emergency),
		EDigetherLifeResult::Success);
	const FDigetherLifeComponentState Escaped = Source->GetLifeComponent()->CaptureState();
	APawn* SourcePawn = World->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("인계에서 제외할 원본 Pawn"), SourcePawn))
	{
		return false;
	}
	Source->RetainPawn(SourcePawn);

	Source->CopyProperties(CopyTarget);
	OverrideTarget->OverrideWith(Source);
	for (const ADigetherGameplayPlayerState* Target : { CopyTarget, OverrideTarget })
	{
		TestTrue(TEXT("지갑 장부 전체 인계"), AreReconnectStatesEqual(Target->CaptureState(), Wallet));
		TestTrue(TEXT("탈출·개인 생존 값 전체 인계"),
			AreReconnectStatesEqual(Target->GetLifeComponent()->CaptureState(), Escaped));
		TestEqual(TEXT("서버 플레이어 GUID 인계"), Target->GetGameplayPlayerId(), SourceId);
		TestTrue(TEXT("초기 지급 완료 표식 인계"), Target->IsGameplayInitialized());
		TestNull(TEXT("월드 Pawn 참조는 상태 인계에 포함하지 않음"), Target->GetRetainedPawn());
	}
	TestFalse(TEXT("무효 GUID 복원 거절"), CopyTarget->RestoreGameplayIdentity(FGuid()));
	TestEqual(TEXT("무효 복원 뒤 기존 GUID 유지"), CopyTarget->GetGameplayPlayerId(), SourceId);
	TestTrue(TEXT("동일 GUID 복원 허용"), CopyTarget->RestoreGameplayIdentity(SourceId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkInactiveLatestStateTest,
	"DIGETHER.Game.Framework.Reconnect.InactiveLatestState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkInactiveLatestStateTest::RunTest(const FString& Parameters)
{
	FDigetherReconnectTestWorld TestWorld;
	UWorld* World = TestWorld.Get();
	if (!TestNotNull(TEXT("독립 비활성 시험 월드"), World))
	{
		return false;
	}
	ADigetherGameplayPlayerState* Retained = World->SpawnActor<ADigetherGameplayPlayerState>();
	ADigetherGameplayPlayerState* FreshLogin = World->SpawnActor<ADigetherGameplayPlayerState>();
	if (!TestNotNull(TEXT("보존할 원본 상태"), Retained) || !TestNotNull(TEXT("새 로그인 기본 상태"), FreshLogin))
	{
		return false;
	}
	Retained->InitializeGameplayIdentity();
	Retained->MarkGameplayInitialized();
	Retained->SetIsInactive(true);
	Retained->OnDeactivated();
	if (!TestTrue(TEXT("비활성 원본은 CleanupPlayerState에서 파괴되지 않음"), IsValid(Retained)))
	{
		return false;
	}
	// 끊긴 뒤 사망·정산을 같은 원본에 반영, 접속 시점 스냅샷으로 회귀 금지
	const FDigetherWalletState LatestWallet = MakeReconnectWallet();
	TestEqual(TEXT("끊긴 동안 최신 장부 적용"), Retained->RestoreState(LatestWallet), EDigetherEconomyResult::Success);
	TestEqual(TEXT("끊긴 동안 사망 확정"), Retained->GetLifeComponent()->ApplyDeath(EDigetherDeathCause::CollapseTimeExpired),
		EDigetherLifeResult::Success);
	const FDigetherLifeComponentState LatestLife = Retained->GetLifeComponent()->CaptureState();
	const FGuid RetainedId = Retained->GetGameplayPlayerId();
	Retained->OverrideWith(FreshLogin);
	Retained->OnReactivated();
	TestFalse(TEXT("재활성화 시 inactive 해제"), Retained->IsInactive());
	TestTrue(TEXT("새 로그인 기본 지갑이 최신 장부를 덮지 않음"),
		AreReconnectStatesEqual(Retained->CaptureState(), LatestWallet));
	TestTrue(TEXT("새 로그인 기본 생존 값이 사망을 되돌리지 않음"),
		AreReconnectStatesEqual(Retained->GetLifeComponent()->CaptureState(), LatestLife));
	TestEqual(TEXT("기존 GUID 유지"), Retained->GetGameplayPlayerId(), RetainedId);
	TestTrue(TEXT("지급 완료 표식 유지"), Retained->IsGameplayInitialized());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkPawnDisconnectTest,
	"DIGETHER.Game.Framework.Reconnect.PreservePawnOnDisconnect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkPawnDisconnectTest::RunTest(const FString& Parameters)
{
	FDigetherReconnectTestWorld TestWorld;
	UWorld* World = TestWorld.Get();
	if (!TestNotNull(TEXT("독립 Pawn 시험 월드"), World))
	{
		return false;
	}
	ADigetherGameplayPlayerController* Controller = World->SpawnActor<ADigetherGameplayPlayerController>();
	ADigetherGameplayPlayerState* State = World->SpawnActor<ADigetherGameplayPlayerState>();
	APawn* Pawn = World->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("시험 Controller"), Controller) || !TestNotNull(TEXT("시험 상태"), State)
		|| !TestNotNull(TEXT("시험 Pawn"), Pawn))
	{
		return false;
	}
	State->SetUniqueId(FUniqueNetIdRepl(FUniqueNetIdRef(FUniqueNetIdString::Create(TEXT("fixture-reconnect-id"), FName(TEXT("DIGETHER_TEST"))))));
	State->SetOwner(Controller);
	Controller->SetPlayerState(State);
	USceneComponent* Root = NewObject<USceneComponent>(Pawn);
	Pawn->SetRootComponent(Root);
	Root->RegisterComponent();
	const FVector DisconnectLocation(140.f, 250.f, 360.f);
	Pawn->SetActorLocation(DisconnectLocation);
	Controller->Possess(Pawn);
	Controller->PawnLeavingGame();
	TestTrue(TEXT("끊긴 Pawn 파괴 없음"), IsValid(Pawn));
	TestNull(TEXT("끊긴 Controller 빙의 해제"), Controller->GetPawn());
	TestNull(TEXT("Pawn이 끊긴 Controller를 보관하지 않음"), Pawn->GetController());
	TestEqual(TEXT("원래 Pawn 참조 보관"), State->GetRetainedPawn(), Pawn);
	TestEqual(TEXT("끊긴 Pawn의 PlayerState 연결 유지"), Pawn->GetPlayerState(), static_cast<APlayerState*>(State));
	TestEqual(TEXT("서버 상태에서도 끊긴 Pawn 조회 가능"), State->GetGameplayPawn(), Pawn);
	TestTrue(TEXT("연결 종료 위치 유지"), Pawn->GetActorLocation().Equals(DisconnectLocation));
	Controller->Possess(State->GetRetainedPawn());
	TestTrue(TEXT("새 Pawn 생성 없이 같은 Pawn 재빙의"), Controller->GetPawn() == Pawn);
	TestTrue(TEXT("재빙의 뒤 위치 유지"), Pawn->GetActorLocation().Equals(DisconnectLocation));
	return true;
}

#endif
