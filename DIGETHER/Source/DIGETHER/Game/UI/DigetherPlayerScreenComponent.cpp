#include "Game/UI/DigetherPlayerScreenComponent.h"

#include "Game/Framework/DigetherFrameworkLog.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/UI/Hud/DigetherRunHudWidget.h"
#include "Game/UI/Results/DigetherGameOverCreditsWidget.h"
#include "Game/UI/Results/DigetherRunResultWidget.h"
#include "UObject/Class.h"

namespace
{
	// 화면 겹침 순서: 시설 패널(100) < 검은 정산 화면 < 결과 크레딧
	constexpr int32 RunResultZOrder = 110;
	constexpr int32 GameOverCreditsZOrder = 120;
}

UDigetherPlayerScreenComponent::UDigetherPlayerScreenComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

ADigetherGameplayPlayerController* UDigetherPlayerScreenComponent::GetController() const
{
	return Cast<ADigetherGameplayPlayerController>(GetOwner());
}

void UDigetherPlayerScreenComponent::CreatePersistentWidgets()
{
	ADigetherGameplayPlayerController* Controller = GetController();
	if (!IsValid(Controller) || !Controller->IsLocalController())
	{
		return;
	}
	if (const TSubclassOf<UDigetherRunHudWidget> HudClass = Controller->GetHudWidgetClass())
	{
		HudWidget = CreateWidget<UDigetherRunHudWidget>(Controller, HudClass);
		if (IsValid(HudWidget))
		{
			HudWidget->AddToViewport();
		}
	}
	if (const TSubclassOf<UDigetherGameOverCreditsWidget> CreditsClass = Controller->GetGameOverCreditsWidgetClass())
	{
		// 크레딧 단계 외에는 위젯이 스스로 접힘
		GameOverCreditsWidget = CreateWidget<UDigetherGameOverCreditsWidget>(Controller, CreditsClass);
		if (IsValid(GameOverCreditsWidget) && !GameOverCreditsWidget->AddToPlayerScreen(GameOverCreditsZOrder))
		{
			GameOverCreditsWidget = nullptr;
		}
	}
}

void UDigetherPlayerScreenComponent::ReleaseWidgets()
{
	if (RunResultPlayerState.IsValid())
	{
		RunResultPlayerState->OnRunResultChanged.RemoveAll(this);
	}
	RunResultPlayerState.Reset();
	CloseRunResult();
	if (IsValid(HudWidget))
	{
		HudWidget->RemoveFromParent();
	}
	HudWidget = nullptr;
	if (IsValid(GameOverCreditsWidget))
	{
		GameOverCreditsWidget->RemoveFromParent();
	}
	GameOverCreditsWidget = nullptr;
}

void UDigetherPlayerScreenComponent::RefreshRunResultBinding()
{
	ADigetherGameplayPlayerController* Controller = GetController();
	if (!IsValid(Controller) || !Controller->IsLocalController() || !Controller->HasActorBegunPlay())
	{
		return;
	}
	ADigetherGameplayPlayerState* State = Controller->GetPlayerState<ADigetherGameplayPlayerState>();
	if (RunResultPlayerState.Get() != State)
	{
		if (RunResultPlayerState.IsValid())
		{
			RunResultPlayerState->OnRunResultChanged.RemoveAll(this);
		}
		RunResultPlayerState = State;
		if (IsValid(State))
		{
			State->OnRunResultChanged.AddUObject(this, &ThisClass::HandleRunResultChanged);
		}
	}
	// OnRep가 UI 준비보다 먼저 왔거나 같은 서버 inactive 상태 재빙의여도 현재 스냅샷 적용
	HandleRunResultChanged(IsValid(State) ? State->GetLastRunResult() : FDigetherRunResult());
}

void UDigetherPlayerScreenComponent::HandleRunResultChanged(const FDigetherRunResult& Result)
{
	const ADigetherGameplayPlayerController* Controller = GetController();
	if (!IsValid(Controller) || !Controller->IsLocalController())
	{
		return;
	}
	if (!Result.IsValid())
	{
		CloseRunResult();
		LastPresentedRunResultId.Invalidate();
		return;
	}
	if (LastPresentedRunResultId != Result.RunId)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogDigetherFramework, Display,
			TEXT("Run result ready: Run=%s Outcome=%s Payout=%d FinalBalance=%d"),
			*Result.RunId.ToString(), *UEnum::GetValueAsString(Result.Outcome),
			Result.PayoutCredits, Result.FinalBalanceCredits);
#endif
		ShowLastRunResult();
	}
}

bool UDigetherPlayerScreenComponent::ShowLastRunResult()
{
	ADigetherGameplayPlayerController* Controller = GetController();
	const ADigetherGameplayPlayerState* State = IsValid(Controller)
		? Controller->GetPlayerState<ADigetherGameplayPlayerState>() : nullptr;
	if (!IsValid(Controller) || !Controller->IsLocalController() || !Controller->HasActorBegunPlay() || !IsValid(State)
		|| !Controller->GetRunResultWidgetClass())
	{
		return false;
	}
	const FDigetherRunResult Result = State->GetLastRunResult();
	if (!Result.IsValid())
	{
		return false;
	}
	Controller->ReleaseHeldInputsAndHubPanel();
	if (!IsValid(RunResultWidget))
	{
		RunResultWidget = CreateWidget<UDigetherRunResultWidget>(Controller, Controller->GetRunResultWidgetClass());
		if (!IsValid(RunResultWidget) || !RunResultWidget->AddToPlayerScreen(RunResultZOrder))
		{
			RunResultWidget = nullptr;
			return false;
		}
	}
	RunResultWidget->SetResult(Result);
	if (!bRunResultInputModeApplied)
	{
		bRunResultPreviousCursorVisible = Controller->bShowMouseCursor;
	}
	Controller->SetShowMouseCursor(true);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(RunResultWidget->TakeWidget());
	Controller->SetInputMode(InputMode);
	bRunResultInputModeApplied = true;
	RunResultWidget->FocusCloseButton();
	LastPresentedRunResultId = Result.RunId;
	return true;
}

void UDigetherPlayerScreenComponent::CloseRunResult()
{
	if (IsValid(RunResultWidget))
	{
		RunResultWidget->RemoveFromParent();
	}
	RunResultWidget = nullptr;
	ADigetherGameplayPlayerController* Controller = GetController();
	if (bRunResultInputModeApplied && IsValid(Controller) && Controller->IsLocalController())
	{
		Controller->SetInputMode(FInputModeGameOnly());
		Controller->SetShowMouseCursor(bRunResultPreviousCursorVisible);
	}
	bRunResultInputModeApplied = false;
}
