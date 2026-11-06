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

namespace
{
	// 시선 변화 추적 주기(초)
	constexpr float InteractionRefreshSeconds = 0.1f;

	void SetQuotaText(UTextBlock* TextDay, UTextBlock* TextQuota, const FDigetherQuotaState& Quota,
		const EDigetherPaymentSignal Signal)
	{
		if (TextDay)
		{
			TextDay->SetText(Quota.Day > 0
				? FText::Format(LOCTEXT("DayValue", "{0}일차"), FText::AsNumber(Quota.Day))
				: LOCTEXT("DayPending", "일차 준비 중"));
		}
		if (TextQuota)
		{
			// 공동 인정액 비공개 유지용 신호기 단계 표시
			FText SignalText = LOCTEXT("SignalNone", "-");
			switch (Signal)
			{
			case EDigetherPaymentSignal::Red: SignalText = LOCTEXT("SignalRed", "빨강"); break;
			case EDigetherPaymentSignal::Orange: SignalText = LOCTEXT("SignalOrange", "주황"); break;
			case EDigetherPaymentSignal::Green: SignalText = LOCTEXT("SignalGreen", "초록"); break;
			default: break;
			}
			TextQuota->SetText(Quota.Day > 0
				? FText::Format(LOCTEXT("QuotaSignalValue", "목표 {0} · 신호기 {1}"),
					FText::AsNumber(Quota.TargetCredits), SignalText)
				: LOCTEXT("QuotaPending", "목표 준비 중"));
		}
	}

	FText MakeRunPhaseText(const FDigetherRunPublicState* RunState)
	{
		if (RunState == nullptr)
		{
			return LOCTEXT("RunPending", "준비 중");
		}
		if (RunState->bGameOver)
		{
			return LOCTEXT("GameOver", "게임 오버");
		}
		if (RunState->Phase == EDigetherRunPhase::InProgress)
		{
			return LOCTEXT("RunInProgress", "탐사 중");
		}
		if (RunState->Phase == EDigetherRunPhase::Ended)
		{
			return LOCTEXT("RunEnded", "탐사 종료");
		}
		return LOCTEXT("RunPending", "준비 중");
	}

	FText MakeBalanceText(const FDigetherWalletState* Wallet)
	{
		return Wallet
			? FText::Format(LOCTEXT("BalanceValue", "잔고 {0}"), FText::AsNumber(Wallet->BalanceCredits))
			: LOCTEXT("BalancePending", "잔고 확인 중");
	}
}

void UDigetherRunHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// 재표시 시 현재 복제 상태부터 반영, 이후 변경 알림만 수신
	if (UWorld* World = GetWorld())
	{
		BoundWorld = World;
		GameStateSetHandle = World->GameStateSetEvent.AddUObject(this, &ThisClass::HandleGameStateSet);
		BindGameState(World->GetGameState());
	}
	else
	{
		BindGameState(nullptr);
	}
	if (ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(GetOwningPlayer()))
	{
		BoundController = Controller;
		PlayerStateChangedHandle = Controller->OnGameplayPlayerStateChanged.AddUObject(this, &ThisClass::HandlePlayerStateChanged);
	}
	BindWallet();
	if (Text_Interaction)
	{
		RefreshInteraction();
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(InteractionTimer, this, &ThisClass::RefreshInteraction,
				InteractionRefreshSeconds, true);
		}
	}
}

void UDigetherRunHudWidget::NativeDestruct()
{
	// 제거된 HUD의 갱신 호출 차단
	if (UWorld* World = BoundWorld.Get())
	{
		World->GetTimerManager().ClearTimer(InteractionTimer);
		World->GameStateSetEvent.Remove(GameStateSetHandle);
	}
	BoundWorld.Reset();
	GameStateSetHandle.Reset();
	UnbindGameState();
	if (ADigetherGameplayPlayerController* Controller = BoundController.Get())
	{
		Controller->OnGameplayPlayerStateChanged.Remove(PlayerStateChangedHandle);
	}
	BoundController.Reset();
	PlayerStateChangedHandle.Reset();
	UnbindWallet();
	Super::NativeDestruct();
}

void UDigetherRunHudWidget::HandleGameStateSet(AGameStateBase* GameState)
{
	BindGameState(GameState);
}

void UDigetherRunHudWidget::BindGameState(AGameStateBase* GameState)
{
	const ADigetherGameplayGameState* State = Cast<ADigetherGameplayGameState>(GameState);
	UDigetherQuotaStateComponent* Quota = State ? State->GetQuotaComponent() : nullptr;
	UDigetherRunPublicStateComponent* Run = State ? State->GetRunPublicStateComponent() : nullptr;
	if (BoundQuota.Get() != Quota || BoundRun.Get() != Run)
	{
		UnbindGameState();
		BoundQuota = Quota;
		BoundRun = Run;
		if (Quota)
		{
			QuotaChangedHandle = Quota->OnQuotaStateChanged.AddUObject(this, &ThisClass::HandleQuotaChanged);
		}
		if (Run)
		{
			RunChangedHandle = Run->OnRunPublicStateChanged.AddUObject(this, &ThisClass::HandleRunChanged);
		}
	}
	RefreshQuota();
	RefreshRun();
}

void UDigetherRunHudWidget::UnbindGameState()
{
	if (UDigetherQuotaStateComponent* Quota = BoundQuota.Get())
	{
		Quota->OnQuotaStateChanged.Remove(QuotaChangedHandle);
	}
	if (UDigetherRunPublicStateComponent* Run = BoundRun.Get())
	{
		Run->OnRunPublicStateChanged.Remove(RunChangedHandle);
	}
	BoundQuota.Reset();
	BoundRun.Reset();
	QuotaChangedHandle.Reset();
	RunChangedHandle.Reset();
}

void UDigetherRunHudWidget::HandlePlayerStateChanged()
{
	BindWallet();
}

void UDigetherRunHudWidget::BindWallet()
{
	const ADigetherGameplayPlayerState* PlayerState = GetOwningPlayerState<ADigetherGameplayPlayerState>();
	UDigetherWalletComponent* Wallet = PlayerState ? PlayerState->GetWalletComponent() : nullptr;
	if (BoundWallet.Get() != Wallet)
	{
		UnbindWallet();
		BoundWallet = Wallet;
		if (Wallet)
		{
			WalletChangedHandle = Wallet->OnWalletChanged.AddUObject(this, &ThisClass::HandleWalletChanged);
		}
	}
	RefreshBalance();
}

void UDigetherRunHudWidget::UnbindWallet()
{
	if (UDigetherWalletComponent* Wallet = BoundWallet.Get())
	{
		Wallet->OnWalletChanged.Remove(WalletChangedHandle);
	}
	BoundWallet.Reset();
	WalletChangedHandle.Reset();
}

void UDigetherRunHudWidget::HandleQuotaChanged(const FDigetherQuotaPublicState& State)
{
	const UDigetherQuotaStateComponent* Quota = BoundQuota.Get();
	SetQuotaText(Text_Day, Text_Quota, State.Quota, Quota ? Quota->GetPaymentSignal() : EDigetherPaymentSignal::None);
}

void UDigetherRunHudWidget::HandleRunChanged(const FDigetherRunPublicState& State)
{
	if (Text_RunPhase)
	{
		Text_RunPhase->SetText(MakeRunPhaseText(&State));
	}
}

void UDigetherRunHudWidget::HandleWalletChanged(const FDigetherWalletState& State)
{
	if (Text_Balance)
	{
		Text_Balance->SetText(MakeBalanceText(&State));
	}
}

void UDigetherRunHudWidget::RefreshQuota()
{
	if (!Text_Day && !Text_Quota)
	{
		return;
	}
	const UDigetherQuotaStateComponent* Quota = BoundQuota.Get();
	SetQuotaText(Text_Day, Text_Quota, Quota ? Quota->CaptureState().Quota : FDigetherQuotaState(),
		Quota ? Quota->GetPaymentSignal() : EDigetherPaymentSignal::None);
}

void UDigetherRunHudWidget::RefreshRun()
{
	if (Text_RunPhase)
	{
		const UDigetherRunPublicStateComponent* Run = BoundRun.Get();
		const FDigetherRunPublicState State = Run ? Run->CaptureState() : FDigetherRunPublicState();
		Text_RunPhase->SetText(MakeRunPhaseText(Run ? &State : nullptr));
	}
}

void UDigetherRunHudWidget::RefreshBalance()
{
	if (Text_Balance)
	{
		const UDigetherWalletComponent* Wallet = BoundWallet.Get();
		const FDigetherWalletState State = Wallet ? Wallet->CaptureState() : FDigetherWalletState();
		Text_Balance->SetText(MakeBalanceText(Wallet ? &State : nullptr));
	}
}

void UDigetherRunHudWidget::RefreshInteraction()
{
	const ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(GetOwningPlayer());
	Text_Interaction->SetText(Controller ? Controller->GetInteractionHint() : FText::GetEmpty());
}

#undef LOCTEXT_NAMESPACE
