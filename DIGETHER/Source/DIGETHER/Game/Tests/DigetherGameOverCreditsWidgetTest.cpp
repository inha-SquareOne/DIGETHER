#if WITH_DEV_AUTOMATION_TESTS

// 아래 숫자는 테스트 픽스처, 기획 값 아님

#include "Blueprint/UserWidgetPool.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Run/DigetherGameOverPublicStateComponent.h"
#include "Game/UI/Results/DigetherGameOverCreditsWidget.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

// Collapsed 위젯은 Slate Tick을 받지 않음, 표시 전환이 Tick에 의존하면 실패
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherGameOverCreditsWidgetPhaseRefreshTest,
	"DIGETHER.Game.UI.GameOverCredits.HiddenPhaseRefreshAndRebind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherGameOverCreditsWidgetPhaseRefreshTest::RunTest(const FString& Parameters)
{
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
	World->InitializeActorsForPlay(FURL());
	ADigetherGameplayPlayerController* Controller = World->SpawnActor<ADigetherGameplayPlayerController>();
	if (!TestNotNull(TEXT("크레딧 소유 Controller"), Controller))
	{
		return false;
	}
	Controller->SetAsLocalPlayerController();
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	LocalPlayer->PlayerController = Controller;
	Controller->Player = LocalPlayer;
	ON_SCOPE_EXIT
	{
		Controller->Player = nullptr;
	};

	FUserWidgetPool Pool;
	Pool.SetWorld(World);
	Pool.SetDefaultPlayerController(Controller);
	UDigetherGameOverCreditsWidget* Widget = Pool.GetOrCreateInstance<UDigetherGameOverCreditsWidget>(
		UDigetherGameOverCreditsWidget::StaticClass());
	if (!TestNotNull(TEXT("크레딧 위젯"), Widget))
	{
		return false;
	}
	TestNull(TEXT("시작 시 GameState 미도착"), World->GetGameState());
	TestEqual(TEXT("크레딧 전 접힘"), Widget->GetVisibility(), ESlateVisibility::Collapsed);

	ADigetherGameplayGameState* State = World->SpawnActor<ADigetherGameplayGameState>();
	if (!TestNotNull(TEXT("늦게 도착한 GameState"), State))
	{
		return false;
	}
	UDigetherGameOverPublicStateComponent* Public = State->GetGameOverPublicStateComponent();
	TestTrue(TEXT("늦은 GameState 단계 구독"), Public->OnGameOverPhaseChanged.IsBound());
	FDigetherGameOverPublicState Credits;
	Credits.Phase = EDigetherGameOverPhase::Credits;
	Credits.PublishedResult.ResultId = FGuid(1, 2, 3, 4);
	Credits.PublishedResult.TotalEarnedCredits = 50;
	TestTrue(TEXT("크레딧 공개 상태 적용"), Public->SetPublicState(Credits));
	// NativeTick 호출 없이 즉시 표시, 숨김 상태에서 시작하는 원인 경로 검사
	TestEqual(TEXT("단계 알림만으로 표시"), Widget->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("공개된 결과 반영"), Widget->GetResult().ResultId, Credits.PublishedResult.ResultId);
	TestEqual(TEXT("공개된 합계 반영"), Widget->GetResult().TotalEarnedCredits, 50);

	FDigetherGameOverPublicState Idle;
	TestTrue(TEXT("새 게임 공개 상태 적용"), Public->SetPublicState(Idle));
	TestEqual(TEXT("새 게임 알림만으로 접힘"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
	TestFalse(TEXT("이전 크레딧 데이터 해제"), Widget->GetResult().ResultId.IsValid());

	// 이미 크레딧이 진행 중일 때 위젯을 재사용해도 Tick 없이 현재 스냅샷 표시
	Pool.Release(Widget, true);
	TestFalse(TEXT("제거 뒤 단계 구독 해제"), Public->OnGameOverPhaseChanged.IsBound());
	TestFalse(TEXT("제거 뒤 GameState 대기 해제"), World->GameStateSetEvent.IsBoundToObject(Widget));
	TestTrue(TEXT("제거 중 크레딧 시작"), Public->SetPublicState(Credits));
	UDigetherGameOverCreditsWidget* Reopened = Pool.GetOrCreateInstance<UDigetherGameOverCreditsWidget>(
		UDigetherGameOverCreditsWidget::StaticClass());
	TestEqual(TEXT("같은 위젯 재사용"), Reopened, Widget);
	TestEqual(TEXT("재표시 때 현재 단계 반영"), Reopened->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("재표시 때 현재 결과 반영"), Reopened->GetResult().ResultId, Credits.PublishedResult.ResultId);

	// 클라이언트 GameState 교체 시 이전 상태 알림은 더 이상 위젯을 갱신하지 않음
	ADigetherGameplayGameState* Replacement = World->SpawnActor<ADigetherGameplayGameState>();
	if (!TestNotNull(TEXT("교체 GameState"), Replacement))
	{
		return false;
	}
	World->SetGameState(Replacement);
	TestFalse(TEXT("이전 GameState 단계 구독 해제"), Public->OnGameOverPhaseChanged.IsBound());
	TestEqual(TEXT("교체된 기본 상태로 접힘"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
	UDigetherGameOverPublicStateComponent* ReplacementPublic = Replacement->GetGameOverPublicStateComponent();
	TestTrue(TEXT("교체 상태 단계 구독"), ReplacementPublic->OnGameOverPhaseChanged.IsBound());
	TestTrue(TEXT("교체 상태 크레딧 적용"), ReplacementPublic->SetPublicState(Credits));
	TestEqual(TEXT("교체 상태 알림으로 표시"), Widget->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	Pool.ResetPool();
	TestFalse(TEXT("최종 단계 구독 해제"), ReplacementPublic->OnGameOverPhaseChanged.IsBound());
	return true;
}

#endif
