#if WITH_DEV_AUTOMATION_TESTS

// 아래 숫자는 테스트 픽스처, 기획 값 아님

#include "Blueprint/UserWidget.h"
#include "Blueprint/UserWidgetPool.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Game/Economy/DigetherEconomyTuning.h"
#include "Game/Economy/DigetherQuotaStateComponent.h"
#include "Game/Economy/DigetherWalletComponent.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Run/DigetherRunPublicStateComponent.h"
#include "Game/UI/Hud/DigetherRunHudWidget.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

namespace
{
	FString ReadHudText(UUserWidget& Widget, const TCHAR* Name)
	{
		const UTextBlock* Text = Cast<UTextBlock>(Widget.GetWidgetFromName(Name));
		return Text ? Text->GetText().ToString() : FString(TEXT("<없음>"));
	}

	ADigetherGameplayPlayerState* SpawnHudPlayerState(UWorld& World, APlayerController& Owner, const int32 Balance)
	{
		FActorSpawnParameters Spawn;
		Spawn.Owner = &Owner;
		ADigetherGameplayPlayerState* State = World.SpawnActor<ADigetherGameplayPlayerState>(Spawn);
		if (State)
		{
			FDigetherWalletState Wallet;
			Wallet.BalanceCredits = Balance;
			State->GetWalletComponent()->RestoreState(Wallet, false);
		}
		return State;
	}
}

// HUD 일차·목표·잔고·런 단계는 주기 갱신 없이 변경 알림으로 갱신
// GameState 늦은 도착·PlayerState 교체에도 재연결, 닫으면 구독 해제
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherRunHudWidgetEventRefreshTest,
	"DIGETHER.Game.UI.RunHud.EventRefreshAndRebind", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherRunHudWidgetEventRefreshTest::RunTest(const FString& Parameters)
{
	const TSubclassOf<UDigetherRunHudWidget> HudClass = LoadClass<UDigetherRunHudWidget>(nullptr,
		TEXT("/Game/Game/UI/Hub/WBP_HubRunHud.WBP_HubRunHud_C"));
	if (!TestNotNull(TEXT("거점 HUD 위젯 클래스"), HudClass.Get()))
	{
		return false;
	}
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->AddToRoot();
	Instance->InitializeStandalone();
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT
	{
		Instance->Shutdown();
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		Instance->RemoveFromRoot();
	};
	// 스폰 Actor의 PostInitializeComponents(Controller 등록·SetGameState) 경로 활성화, GameMode 없음
	World->InitializeActorsForPlay(FURL());

	ADigetherGameplayPlayerController* Controller = World->SpawnActor<ADigetherGameplayPlayerController>();
	if (!TestNotNull(TEXT("HUD 소유 Controller"), Controller))
	{
		return false;
	}
	Controller->SetAsLocalPlayerController();
	// CreateWidget 소유자 조건(로컬 플레이어)만 연결, 뷰포트·입력 초기화 생략
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	LocalPlayer->PlayerController = Controller;
	Controller->Player = LocalPlayer;
	ON_SCOPE_EXIT
	{
		Controller->Player = nullptr;
	};
	ADigetherGameplayPlayerState* FirstState = SpawnHudPlayerState(*World, *Controller, 50);
	if (!TestNotNull(TEXT("첫 PlayerState"), FirstState))
	{
		return false;
	}
	Controller->SetPlayerState(FirstState);
	TestNull(TEXT("시작 시 GameState 미도착(클라이언트 접속 직후 재현)"), World->GetGameState());

	FUserWidgetPool Pool;
	Pool.SetWorld(World);
	Pool.SetDefaultPlayerController(Controller);
	UDigetherRunHudWidget* Hud = Pool.GetOrCreateInstance(HudClass);
	if (!TestNotNull(TEXT("HUD 생성"), Hud))
	{
		return false;
	}
	TestEqual(TEXT("GameState 전 일차"), ReadHudText(*Hud, TEXT("Text_Day")), FString(TEXT("일차 준비 중")));
	TestEqual(TEXT("GameState 전 목표"), ReadHudText(*Hud, TEXT("Text_Quota")), FString(TEXT("목표 준비 중")));
	TestEqual(TEXT("GameState 전 런 단계"), ReadHudText(*Hud, TEXT("Text_RunPhase")), FString(TEXT("준비 중")));
	TestEqual(TEXT("생성 시 잔고"), ReadHudText(*Hud, TEXT("Text_Balance")), FString(TEXT("잔고 50")));

	// GameState 생성은 엔진 SetGameState → GameStateSetEvent
	ADigetherGameplayGameState* GameState = World->SpawnActor<ADigetherGameplayGameState>();
	if (!TestNotNull(TEXT("늦게 도착한 GameState"), GameState) || !TestEqual(TEXT("월드 GameState 설정"),
		World->GetGameState(), static_cast<AGameStateBase*>(GameState)))
	{
		return false;
	}
	UDigetherQuotaStateComponent* Quota = GameState->GetQuotaComponent();
	TestTrue(TEXT("늦은 GameState 목표 구독"), Quota->OnQuotaStateChanged.IsBoundToObject(Hud));
	TestTrue(TEXT("늦은 GameState 런 구독"), GameState->GetRunPublicStateComponent()->OnRunPublicStateChanged.IsBoundToObject(Hud));

	FDigetherQuotaState QuotaState;
	QuotaState.Day = 2;
	QuotaState.TotalDay = 2;
	QuotaState.TargetCredits = 100;
	QuotaState.DepositedCredits = 60;
	TestEqual(TEXT("목표 변경"), Quota->SetQuotaState(QuotaState), EDigetherEconomyResult::Success);
	TestEqual(TEXT("목표 알림 즉시 일차"), ReadHudText(*Hud, TEXT("Text_Day")), FString(TEXT("2일차")));
	TestEqual(TEXT("목표 알림 즉시 신호기"), ReadHudText(*Hud, TEXT("Text_Quota")), FString(TEXT("목표 100 · 신호기 주황")));

	// 신호기 경계만 바뀌어도 HUD 갱신
	FDigetherEconomyTuning Tuning;
	Tuning.PaymentSignalOrangeRatio = 0.7;
	Tuning.PaymentSignalGreenRatio = 0.9;
	TestEqual(TEXT("신호기 경계 변경"), Quota->SetSignalTuning(Tuning), EDigetherEconomyResult::Success);
	TestEqual(TEXT("신호기 경계 알림"), ReadHudText(*Hud, TEXT("Text_Quota")), FString(TEXT("목표 100 · 신호기 빨강")));
	TestEqual(TEXT("묶음 적용은 알림 보류"), Quota->SetSignalTuning(FDigetherEconomyTuning(), false), EDigetherEconomyResult::Success);
	TestEqual(TEXT("알림 전 표시 유지"), ReadHudText(*Hud, TEXT("Text_Quota")), FString(TEXT("목표 100 · 신호기 빨강")));
	Quota->NotifyStateChanged();
	TestEqual(TEXT("마지막 알림 뒤 반영"), ReadHudText(*Hud, TEXT("Text_Quota")), FString(TEXT("목표 100 · 신호기 주황")));

	FDigetherRunPublicState RunState;
	RunState.Phase = EDigetherRunPhase::InProgress;
	TestEqual(TEXT("런 단계 변경"), GameState->GetRunPublicStateComponent()->RestoreState(RunState), EDigetherRunResult::Success);
	TestEqual(TEXT("런 알림 즉시"), ReadHudText(*Hud, TEXT("Text_RunPhase")), FString(TEXT("탐사 중")));

	FDigetherWalletState Wallet;
	Wallet.BalanceCredits = 60;
	FirstState->GetWalletComponent()->RestoreState(Wallet);
	TestEqual(TEXT("지갑 알림 즉시"), ReadHudText(*Hud, TEXT("Text_Balance")), FString(TEXT("잔고 60")));

	// 재접속 PlayerState 교체: 클라이언트 OnRep_PlayerState 경로
	ADigetherGameplayPlayerState* SecondState = SpawnHudPlayerState(*World, *Controller, 70);
	if (!TestNotNull(TEXT("교체 PlayerState"), SecondState))
	{
		return false;
	}
	Controller->SetPlayerState(SecondState);
	Controller->OnRep_PlayerState();
	TestEqual(TEXT("교체 뒤 새 잔고"), ReadHudText(*Hud, TEXT("Text_Balance")), FString(TEXT("잔고 70")));
	TestFalse(TEXT("이전 지갑 구독 해제"), FirstState->GetWalletComponent()->OnWalletChanged.IsBoundToObject(Hud));
	Wallet.BalanceCredits = 80;
	FirstState->GetWalletComponent()->RestoreState(Wallet);
	TestEqual(TEXT("이전 지갑 변경 무시"), ReadHudText(*Hud, TEXT("Text_Balance")), FString(TEXT("잔고 70")));
	Wallet.BalanceCredits = 90;
	SecondState->GetWalletComponent()->RestoreState(Wallet);
	TestEqual(TEXT("새 지갑 알림"), ReadHudText(*Hud, TEXT("Text_Balance")), FString(TEXT("잔고 90")));

	// 닫으면 모든 구독 해제
	Pool.Release(Hud, true);
	TestFalse(TEXT("닫은 뒤 목표 해제"), Quota->OnQuotaStateChanged.IsBoundToObject(Hud));
	TestFalse(TEXT("닫은 뒤 런 해제"), GameState->GetRunPublicStateComponent()->OnRunPublicStateChanged.IsBoundToObject(Hud));
	TestFalse(TEXT("닫은 뒤 지갑 해제"), SecondState->GetWalletComponent()->OnWalletChanged.IsBoundToObject(Hud));
	TestFalse(TEXT("닫은 뒤 PlayerState 알림 해제"), Controller->OnGameplayPlayerStateChanged.IsBoundToObject(Hud));
	TestFalse(TEXT("닫은 뒤 GameState 대기 해제"), World->GameStateSetEvent.IsBoundToObject(Hud));

	// 같은 인스턴스 재표시 시 현재 값부터 다시 구독
	UDigetherRunHudWidget* Reopened = Pool.GetOrCreateInstance(HudClass);
	TestEqual(TEXT("같은 인스턴스 재사용"), Reopened, Hud);
	TestTrue(TEXT("재표시 목표 구독"), Quota->OnQuotaStateChanged.IsBoundToObject(Reopened));
	TestEqual(TEXT("재표시 잔고"), ReadHudText(*Reopened, TEXT("Text_Balance")), FString(TEXT("잔고 90")));
	Pool.ResetPool();
	return true;
}

#endif
