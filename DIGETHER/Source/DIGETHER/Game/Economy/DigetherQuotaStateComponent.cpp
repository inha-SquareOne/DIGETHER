#include "Game/Economy/DigetherQuotaStateComponent.h"

#include "Game/Core/DigetherAuthority.h"
#include "Game/Economy/DigetherEconomyRules.h"
#include "Net/UnrealNetwork.h"

UDigetherQuotaStateComponent::UDigetherQuotaStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDigetherQuotaStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UDigetherQuotaStateComponent, ReplicatedState, COND_None);
	DOREPLIFETIME_CONDITION(UDigetherQuotaStateComponent, QuotaCompletionCue, COND_None);
}

void UDigetherQuotaStateComponent::BeginPlay()
{
	Super::BeginPlay();
	if (DigetherAuthority::HasOwnerAuthority(*this) && QuotaCompletionCue < 1)
	{
		// 기본값과 다른 시작 값이어야 늦게 들어온 클라이언트도 첫 복제를 받아 기준으로 사용
		QuotaCompletionCue = 1;
	}
}

FDigetherQuotaPublicState UDigetherQuotaStateComponent::CaptureState() const
{
	return PublicState;
}

EDigetherPaymentSignal UDigetherQuotaStateComponent::GetPaymentSignal() const
{
	return ReplicatedState.PaymentSignal;
}

EDigetherEconomyResult UDigetherQuotaStateComponent::RestoreState(const FDigetherQuotaPublicState& State, const bool bNotify)
{
	if (!DigetherAuthority::HasOwnerAuthority(*this))
	{
		return EDigetherEconomyResult::NotAuthority;
	}

	if (!FDigetherEconomyRules::IsValidPublicState(State))
	{
		return EDigetherEconomyResult::InvalidInput;
	}

	PublicState = State;
	ReplicatedState = FDigetherEconomyRules::MakeReplicatedQuotaState(PublicState, SignalTuning);
	if (bNotify)
	{
		NotifyStateChanged();
	}
	return EDigetherEconomyResult::Success;
}

EDigetherEconomyResult UDigetherQuotaStateComponent::SetQuotaState(const FDigetherQuotaState& State, const bool bNotify)
{
	if (!DigetherAuthority::HasOwnerAuthority(*this))
	{
		return EDigetherEconomyResult::NotAuthority;
	}

	if (!FDigetherEconomyRules::IsValidQuotaState(State))
	{
		return EDigetherEconomyResult::InvalidInput;
	}

	FDigetherQuotaPublicState UpdatedState = PublicState;
	UpdatedState.Quota = State;
	return RestoreState(UpdatedState, bNotify);
}

EDigetherEconomyResult UDigetherQuotaStateComponent::SetHiddenReveals(
	const TArray<FDigetherHiddenRevealEntry>& HiddenReveals, const bool bNotify)
{
	if (!DigetherAuthority::HasOwnerAuthority(*this))
	{
		return EDigetherEconomyResult::NotAuthority;
	}

	FDigetherQuotaPublicState UpdatedState = PublicState;
	UpdatedState.HiddenReveals = HiddenReveals;
	return RestoreState(UpdatedState, bNotify);
}

EDigetherEconomyResult UDigetherQuotaStateComponent::NotifyStateChanged()
{
	if (!DigetherAuthority::HasOwnerAuthority(*this))
	{
		return EDigetherEconomyResult::NotAuthority;
	}

	const FDigetherQuotaPublicState State = PublicState;
	OnQuotaStateChanged.Broadcast(State);
	return EDigetherEconomyResult::Success;
}

EDigetherEconomyResult UDigetherQuotaStateComponent::SetSignalTuning(const FDigetherEconomyTuning& Tuning, const bool bNotify)
{
	if (!DigetherAuthority::HasOwnerAuthority(*this))
	{
		return EDigetherEconomyResult::NotAuthority;
	}
	if (!FDigetherEconomyRules::IsValidTuning(Tuning))
	{
		return EDigetherEconomyResult::InvalidInput;
	}
	SignalTuning = Tuning;
	ReplicatedState = FDigetherEconomyRules::MakeReplicatedQuotaState(PublicState, SignalTuning);
	// 신호기 단계 변경을 서버 구독자(리슨 호스트 HUD)에도 전달
	if (bNotify)
	{
		NotifyStateChanged();
	}
	return EDigetherEconomyResult::Success;
}

EDigetherEconomyResult UDigetherQuotaStateComponent::MarkQuotaCompleted()
{
	if (!DigetherAuthority::HasOwnerAuthority(*this))
	{
		return EDigetherEconomyResult::NotAuthority;
	}
	// 정수 범위 끝에서는 2로 순환해 값 변화 유지(0은 기본값, 1은 시작 값이라 제외)
	QuotaCompletionCue = QuotaCompletionCue < 1 || QuotaCompletionCue == MAX_int32 ? 2 : QuotaCompletionCue + 1;
	// 리슨 서버 호스트는 OnRep 미수신, 여기서 바로 알림
	OnQuotaCompleted.Broadcast();
	return EDigetherEconomyResult::Success;
}

void UDigetherQuotaStateComponent::OnRep_QuotaCompletionCue()
{
	if (!bQuotaCompletionCueReceived)
	{
		// 처음 받은 값은 접속 이전 달성 기록, 완료한 할당량 연출 재생 안 함(S24 이어하기)
		bQuotaCompletionCueReceived = true;
		return;
	}
	OnQuotaCompleted.Broadcast();
}

void UDigetherQuotaStateComponent::OnRep_ReplicatedState()
{
	// 서버 원본은 누적 인정액 보존을 위해 덮어쓰지 않음, 클라이언트만 복제 값으로 조회 값 재구성
	if (!DigetherAuthority::HasOwnerAuthority(*this))
	{
		PublicState = FDigetherEconomyRules::MakeClientQuotaPublicState(ReplicatedState);
	}
	const FDigetherQuotaPublicState State = PublicState;
	OnQuotaStateChanged.Broadcast(State);
}
