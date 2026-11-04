#include "Game/Elevator/DigetherElevatorLeverComponent.h"

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
	if (HasServerAuthority())
	{
		++NotificationBatchDepth;
	}
}

void UDigetherElevatorLeverComponent::EndNotificationBatch()
{
	if (!HasServerAuthority() || NotificationBatchDepth <= 0)
	{
		return;
	}
	--NotificationBatchDepth;
	if (NotificationBatchDepth > 0)
	{
		return;
	}

	const bool bShouldNotifyPublicState = bPublicStateNotificationPending;
	const FDigetherElevatorLeverPublicState StateSnapshot = PublicState;
	TArray<FDigetherElevatorLeverFeedback> FeedbackSnapshots = MoveTemp(PendingFeedback);
	// 콜백이 같은 컴포넌트를 다시 변경해도 새 묶음과 이전 피드백을 섞지 않는다.
	bPublicStateNotificationPending = false;
	PendingFeedback.Reset();
	if (bShouldNotifyPublicState)
	{
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
	if (!HasServerAuthority())
	{
		return EDigetherElevatorLeverResult::NotAuthority;
	}
	if ((Kind != EDigetherElevatorLeverKind::Ascent && Kind != EDigetherElevatorLeverKind::Departure)
		|| !DigetherElevatorRules::IsValidTuning(Tuning))
	{
		return EDigetherElevatorLeverResult::InvalidInput;
	}

	LeverKind = Kind;
	LeverTuning = Tuning;
	GaugeState = FDigetherElevatorGaugeState();
	bInitialized = true;
	const bool bPublicStateChanged = RefreshPublicState();
	const FDigetherElevatorLeverPublicState StateSnapshot = PublicState;
	if (bPublicStateChanged)
	{
		DispatchPublicStateNotification(StateSnapshot);
	}
	return EDigetherElevatorLeverResult::Released;
}

EDigetherElevatorLeverResult UDigetherElevatorLeverComponent::TryBeginPull(const FGuid& PullerId,
	const FDigetherElevatorPullContext& Context)
{
	if (!HasServerAuthority())
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
	if (!HasServerAuthority())
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
	if (!HasServerAuthority())
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
	if (!HasServerAuthority())
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

bool UDigetherElevatorLeverComponent::TryGetConfirmedAscent(FDigetherElevatorAscentConfirmed& OutAscent) const
{
	OutAscent = FDigetherElevatorAscentConfirmed();
	if (!HasServerAuthority() || !bInitialized || LeverKind != EDigetherElevatorLeverKind::Ascent
		|| GaugeState.Phase != EDigetherElevatorGaugePhase::Confirmed)
	{
		return false;
	}
	OutAscent = GaugeState.Ascent;
	return true;
}

bool UDigetherElevatorLeverComponent::HasServerAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return IsValid(OwnerActor) && OwnerActor->HasAuthority();
}

bool UDigetherElevatorLeverComponent::RefreshPublicState()
{
	FDigetherElevatorLeverPublicState UpdatedState;
	UpdatedState.Progress = DigetherElevatorRules::CalculateGaugeProgress(GaugeState);
	UpdatedState.bWarningActive = GaugeState.bWarningActive;
	if (PublicState.Progress == UpdatedState.Progress && PublicState.bWarningActive == UpdatedState.bWarningActive)
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
	const FDigetherElevatorLeverPublicState StateSnapshot = PublicState;
	const FDigetherElevatorLeverFeedback FeedbackSnapshot = Update.Feedback;
	const FDigetherElevatorAscentConfirmed AscentSnapshot = GaugeState.Ascent;
	const FDigetherElevatorDepartureConfirmed DepartureSnapshot = GaugeState.Departure;
	const EDigetherElevatorLeverKind KindSnapshot = LeverKind;
	if (bPublicStateChanged)
	{
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
	OnPublicStateChanged.Broadcast(StateSnapshot);
}
