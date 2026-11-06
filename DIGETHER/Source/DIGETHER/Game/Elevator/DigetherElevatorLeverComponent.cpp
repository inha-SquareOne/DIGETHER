#include "Game/Elevator/DigetherElevatorLeverComponent.h"

#include "Game/Core/DigetherAuthority.h"
#include "Game/Elevator/DigetherElevatorLog.h"
#include "Game/Elevator/DigetherElevatorRules.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

namespace
{
	bool ShouldNotifyFeedback(const EDigetherElevatorLeverResult Result)
	{
		switch (Result)
		{
		case EDigetherElevatorLeverResult::Started:
		case EDigetherElevatorLeverResult::InProgress:
		case EDigetherElevatorLeverResult::Released:
		case EDigetherElevatorLeverResult::Confirmed:
		case EDigetherElevatorLeverResult::OverweightWarningAndRattle:
		case EDigetherElevatorLeverResult::InterruptedOverweight:
		case EDigetherElevatorLeverResult::InterruptedConditions:
			return true;
		default:
			return false;
		}
	}
}

UDigetherElevatorLeverComponent::UDigetherElevatorLeverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDigetherElevatorLeverComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDigetherElevatorLeverComponent, PublicState);
}

void UDigetherElevatorLeverComponent::BeginNotificationBatch()
{
	if (DigetherAuthority::HasOwnerAuthority(*this))
	{
		++NotificationBatchDepth;
	}
}

void UDigetherElevatorLeverComponent::EndNotificationBatch()
{
	if (!DigetherAuthority::HasOwnerAuthority(*this) || NotificationBatchDepth <= 0)
	{
		return;
	}
	--NotificationBatchDepth;
	if (NotificationBatchDepth > 0)
	{
		return;
	}

	const bool bShouldNotifyPublicState = bPublicStateNotificationPending;
	TArray<FDigetherElevatorLeverFeedback> FeedbackSnapshots = MoveTemp(PendingFeedback);
	// 콜백이 같은 컴포넌트를 다시 변경해도 새 묶음과 이전 피드백을 섞지 않음
	bPublicStateNotificationPending = false;
	PendingFeedback.Reset();
	if (bShouldNotifyPublicState)
	{
		const FDigetherElevatorLeverPublicState StateSnapshot = PublicState;
		OnLeverVisualStateChanged.Broadcast(StateSnapshot);
		OnPublicStateChanged.Broadcast(StateSnapshot);
	}
	for (const FDigetherElevatorLeverFeedback& Feedback : FeedbackSnapshots)
	{
		OnFeedback.Broadcast(Feedback);
	}
}

EDigetherElevatorLeverResult UDigetherElevatorLeverComponent::Configure(const EDigetherElevatorLeverKind Kind,
	const FDigetherElevatorTuning& Tuning)
{
	if (!DigetherAuthority::HasOwnerAuthority(*this))
	{
		return EDigetherElevatorLeverResult::NotAuthority;
	}
	if (!DigetherElevatorRules::IsValidLeverKind(Kind) || !DigetherElevatorRules::IsValidTuning(Tuning))
	{
		return EDigetherElevatorLeverResult::InvalidInput;
	}

	LeverKind = Kind;
	LeverTuning = Tuning;
	GaugeState = FDigetherElevatorGaugeState();
	bInitialized = true;
	if (RefreshPublicState())
	{
		const FDigetherElevatorLeverPublicState StateSnapshot = PublicState;
		DispatchPublicStateNotification(StateSnapshot);
	}
	return EDigetherElevatorLeverResult::Released;
}

EDigetherElevatorLeverResult UDigetherElevatorLeverComponent::TryBeginPull(const FGuid& PullerId,
	const FDigetherElevatorPullContext& Context)
{
	if (!DigetherAuthority::HasOwnerAuthority(*this))
	{
		return EDigetherElevatorLeverResult::NotAuthority;
	}
	if (!bInitialized)
	{
		return EDigetherElevatorLeverResult::NotInitialized;
	}
	return ApplyGaugeUpdate(DigetherElevatorRules::BeginPull(GaugeState, LeverKind, PullerId, Context, LeverTuning));
}

EDigetherElevatorLeverResult UDigetherElevatorLeverComponent::ReleasePull(const FGuid& PullerId)
{
	if (!DigetherAuthority::HasOwnerAuthority(*this))
	{
		return EDigetherElevatorLeverResult::NotAuthority;
	}
	if (!bInitialized)
	{
		return EDigetherElevatorLeverResult::NotInitialized;
	}
	return ApplyGaugeUpdate(DigetherElevatorRules::ReleasePull(GaugeState, PullerId));
}

EDigetherElevatorLeverResult UDigetherElevatorLeverComponent::AdvancePull(const float DeltaSeconds,
	const FDigetherElevatorPullContext& Context)
{
	if (!DigetherAuthority::HasOwnerAuthority(*this))
	{
		return EDigetherElevatorLeverResult::NotAuthority;
	}
	if (!bInitialized)
	{
		return EDigetherElevatorLeverResult::NotInitialized;
	}
	return ApplyGaugeUpdate(DigetherElevatorRules::AdvancePull(GaugeState, LeverKind, DeltaSeconds, Context, LeverTuning));
}

EDigetherElevatorLeverResult UDigetherElevatorLeverComponent::ResetPull()
{
	if (!DigetherAuthority::HasOwnerAuthority(*this))
	{
		return EDigetherElevatorLeverResult::NotAuthority;
	}
	if (!bInitialized)
	{
		return EDigetherElevatorLeverResult::NotInitialized;
	}

	FDigetherElevatorGaugeUpdate Update;
	Update.Feedback.Result = EDigetherElevatorLeverResult::Released;
	return ApplyGaugeUpdate(Update);
}

FDigetherElevatorLeverPublicState UDigetherElevatorLeverComponent::GetPublicState() const
{
	return PublicState;
}

FText UDigetherElevatorLeverComponent::GetDepartureWarningText(const EDigetherDepartureWarningKind WarningKind)
{
	switch (WarningKind)
	{
	case EDigetherDepartureWarningKind::Standard:
		return NSLOCTEXT("DigetherElevator", "DepartureWarningStandard", "승강기가 곧 출발합니다");
	case EDigetherDepartureWarningKind::Deadline:
		return NSLOCTEXT("DigetherElevator", "DepartureWarningDeadline", "현재 할당량 마감일입니다!");
	default:
		return FText::GetEmpty();
	}
}

FGuid UDigetherElevatorLeverComponent::GetActivePullerId() const
{
	return GetOwner() && GetOwner()->HasAuthority() && GaugeState.Phase == EDigetherElevatorGaugePhase::Pulling
		? GaugeState.PullerId : FGuid();
}

bool UDigetherElevatorLeverComponent::TryGetConfirmedAscent(FDigetherElevatorAscentConfirmed& OutAscent) const
{
	OutAscent = FDigetherElevatorAscentConfirmed();
	if (!DigetherAuthority::HasOwnerAuthority(*this) || !bInitialized
		|| LeverKind != EDigetherElevatorLeverKind::Ascent || GaugeState.Phase != EDigetherElevatorGaugePhase::Confirmed)
	{
		return false;
	}
	OutAscent = GaugeState.Ascent;
	return true;
}

bool UDigetherElevatorLeverComponent::RefreshPublicState()
{
	FDigetherElevatorLeverPublicState UpdatedState;
	UpdatedState.Progress = DigetherElevatorRules::CalculateGaugeProgress(GaugeState);
	UpdatedState.bWarningActive = GaugeState.bWarningActive;
	UpdatedState.WarningKind = DigetherElevatorRules::CalculateDepartureWarning(GaugeState);
	if (PublicState.Progress == UpdatedState.Progress && PublicState.bWarningActive == UpdatedState.bWarningActive
		&& PublicState.WarningKind == UpdatedState.WarningKind)
	{
		return false;
	}
	PublicState = UpdatedState;
	return true;
}

void UDigetherElevatorLeverComponent::DispatchPublicStateNotification(
	const FDigetherElevatorLeverPublicState& StateSnapshot)
{
	if (NotificationBatchDepth > 0)
	{
		bPublicStateNotificationPending = true;
		return;
	}
	// native 콜백의 재진입 변경보다 먼저 표현, 이전 스냅샷이 새 표현을 덮지 않게 순서 유지
	OnLeverVisualStateChanged.Broadcast(StateSnapshot);
	OnPublicStateChanged.Broadcast(StateSnapshot);
}

void UDigetherElevatorLeverComponent::DispatchFeedbackNotification(
	const FDigetherElevatorLeverFeedback& FeedbackSnapshot)
{
	if (NotificationBatchDepth > 0)
	{
		if (!PendingFeedback.IsEmpty() && FeedbackSnapshot.Result == EDigetherElevatorLeverResult::Released
			&& PendingFeedback.Last().Result == EDigetherElevatorLeverResult::Released)
		{
			PendingFeedback.Last() = FeedbackSnapshot;
		}
		else
		{
			PendingFeedback.Add(FeedbackSnapshot);
		}
		return;
	}
	OnFeedback.Broadcast(FeedbackSnapshot);
}

EDigetherElevatorLeverResult UDigetherElevatorLeverComponent::ApplyGaugeUpdate(const FDigetherElevatorGaugeUpdate& Update)
{
	const bool bWasConfirmed = GaugeState.Phase == EDigetherElevatorGaugePhase::Confirmed;
	GaugeState = Update.State;
	const bool bPublicStateChanged = RefreshPublicState();
	const bool bNewConfirmation = !bWasConfirmed && GaugeState.Phase == EDigetherElevatorGaugePhase::Confirmed
		&& Update.Feedback.Result == EDigetherElevatorLeverResult::Confirmed;
	const FDigetherElevatorLeverFeedback FeedbackSnapshot = Update.Feedback;
	const EDigetherElevatorLeverKind KindSnapshot = LeverKind;
	// 알림 콜백이 게이지를 다시 바꿔도 이번 확정 값을 보내도록 알림 전에 복사, 매니페스트 배열 복사는 확정 때만
	FDigetherElevatorAscentConfirmed AscentSnapshot;
	FDigetherElevatorDepartureConfirmed DepartureSnapshot;
	if (bNewConfirmation)
	{
		if (KindSnapshot == EDigetherElevatorLeverKind::Ascent)
		{
			AscentSnapshot = GaugeState.Ascent;
		}
		else
		{
			DepartureSnapshot = GaugeState.Departure;
		}
	}
	if (bPublicStateChanged)
	{
		const FDigetherElevatorLeverPublicState StateSnapshot = PublicState;
		DispatchPublicStateNotification(StateSnapshot);
	}
	if (ShouldNotifyFeedback(FeedbackSnapshot.Result))
	{
		DispatchFeedbackNotification(FeedbackSnapshot);
	}
	if (bNewConfirmation)
	{
		UE_LOG(LogDigetherElevator, Verbose, TEXT("Elevator lever confirmed: kind %d."), static_cast<int32>(KindSnapshot));
		if (KindSnapshot == EDigetherElevatorLeverKind::Ascent)
		{
			OnAscentConfirmed.Broadcast(AscentSnapshot);
		}
		else
		{
			OnDepartureConfirmed.Broadcast(DepartureSnapshot);
		}
	}
	return FeedbackSnapshot.Result;
}

void UDigetherElevatorLeverComponent::OnRep_PublicState()
{
	const FDigetherElevatorLeverPublicState StateSnapshot = PublicState;
	OnLeverVisualStateChanged.Broadcast(StateSnapshot);
	OnPublicStateChanged.Broadcast(StateSnapshot);
}
