#include "Game/UI/Hud/DigetherRunHudWidget.h"

#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Game/Economy/DigetherQuotaStateComponent.h"
#include "Game/Economy/DigetherWalletComponent.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Run/DigetherRunPublicStateComponent.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "DigetherRunHud"

void UDigetherRunHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// 재표시 시 현재 복제 상태부터 반영
	RefreshText();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimer, this, &ThisClass::RefreshText, 0.1f, true);
	}
}

void UDigetherRunHudWidget::NativeDestruct()
{
	// 제거된 HUD의 갱신 호출 차단
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	Super::NativeDestruct();
}

void UDigetherRunHudWidget::RefreshText()
{
	if (Text_Interaction)
	{
		const ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(GetOwningPlayer());
		Text_Interaction->SetText(Controller ? Controller->GetInteractionHint() : FText::GetEmpty());
	}
	const UWorld* World = GetWorld();
	const ADigetherGameplayGameState* GameState = World ? World->GetGameState<ADigetherGameplayGameState>() : nullptr;
	const UDigetherQuotaStateComponent* QuotaComponent = GameState ? GameState->GetQuotaComponent() : nullptr;
	const UDigetherRunPublicStateComponent* RunComponent = GameState ? GameState->GetRunPublicStateComponent() : nullptr;
	const ADigetherGameplayPlayerState* PlayerState = GetOwningPlayerState<ADigetherGameplayPlayerState>();
	const UDigetherWalletComponent* WalletComponent = PlayerState ? PlayerState->GetWalletComponent() : nullptr;

	if (Text_Day || Text_Quota)
	{
		const FDigetherQuotaState Quota = QuotaComponent ? QuotaComponent->CaptureState().Quota : FDigetherQuotaState();
		if (Text_Day)
		{
			Text_Day->SetText(Quota.Day > 0
				? FText::Format(LOCTEXT("DayValue", "{0}일차"), FText::AsNumber(Quota.Day))
				: LOCTEXT("DayPending", "일차 준비 중"));
		}
		if (Text_Quota)
		{
			// 공동 인정액 비공개 유지용 신호기 단계 표시
			FText SignalText = LOCTEXT("SignalNone", "-");
			switch (QuotaComponent ? QuotaComponent->GetPaymentSignal() : EDigetherPaymentSignal::None)
			{
			case EDigetherPaymentSignal::Red: SignalText = LOCTEXT("SignalRed", "빨강"); break;
			case EDigetherPaymentSignal::Orange: SignalText = LOCTEXT("SignalOrange", "주황"); break;
			case EDigetherPaymentSignal::Green: SignalText = LOCTEXT("SignalGreen", "초록"); break;
			default: break;
			}
			Text_Quota->SetText(Quota.Day > 0
				? FText::Format(LOCTEXT("QuotaSignalValue", "목표 {0} · 신호기 {1}"),
					FText::AsNumber(Quota.TargetCredits), SignalText)
				: LOCTEXT("QuotaPending", "목표 준비 중"));
		}
	}

	if (Text_Balance)
	{
		Text_Balance->SetText(WalletComponent
			? FText::Format(LOCTEXT("BalanceValue", "잔고 {0}"),
				FText::AsNumber(WalletComponent->CaptureState().BalanceCredits))
			: LOCTEXT("BalancePending", "잔고 확인 중"));
	}

	if (Text_RunPhase)
	{
		FText PhaseText = LOCTEXT("RunPending", "준비 중");
		if (RunComponent)
		{
			const FDigetherRunPublicState RunState = RunComponent->CaptureState();
			if (RunState.bGameOver)
			{
				PhaseText = LOCTEXT("GameOver", "게임 오버");
			}
			else if (RunState.Phase == EDigetherRunPhase::InProgress)
			{
				PhaseText = LOCTEXT("RunInProgress", "탐사 중");
			}
			else if (RunState.Phase == EDigetherRunPhase::Ended)
			{
				PhaseText = LOCTEXT("RunEnded", "탐사 종료");
			}
		}
		Text_RunPhase->SetText(PhaseText);
	}
}

#undef LOCTEXT_NAMESPACE
