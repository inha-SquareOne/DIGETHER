#include "Game/Elevator/DigetherElevatorComponent.h"

#include "Game/Data/DigetherTuningProfile.h"
#include "Game/Elevator/DigetherElevatorLog.h"
#include "Game/Elevator/DigetherElevatorRules.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

namespace
{
	bool AreValidLoadKinds(const TArray<FDigetherElevatorLoadEntry>& LoadEntries)
	{
		for (const FDigetherElevatorLoadEntry& Entry : LoadEntries)
		{
			switch (Entry.Kind)
			{
			case EDigetherElevatorLoadKind::Player:
			case EDigetherElevatorLoadKind::Corpse:
			case EDigetherElevatorLoadKind::CarriedItem:
			case EDigetherElevatorLoadKind::Cargo:
				break;
			default:
				return false;
			}
		}
		return true;
	}
}

UDigetherElevatorComponent::UDigetherElevatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDigetherElevatorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDigetherElevatorComponent, PublicState);
}

void UDigetherElevatorComponent::BeginNotificationBatch()
{
	if (HasServerAuthority())
	{
		++NotificationBatchDepth;
	}
}

void UDigetherElevatorComponent::EndNotificationBatch()
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

	const bool bShouldNotify = bStateNotificationPending;
	const FDigetherElevatorPublicState StateSnapshot = PublicState;
	// 콜백에서 새 묶음을 시작해도 이전 알림이 다시 남지 않도록 먼저 비운다.
	bStateNotificationPending = false;
	if (bShouldNotify)
	{
		OnStateChanged.Broadcast(StateSnapshot);
	}
}

EDigetherElevatorResult UDigetherElevatorComponent::InitializeForRun(const FDigetherRunTuning& RunTuning,
	const FDigetherElevatorTuning& Tuning)
{
	if (!HasServerAuthority())
	{
		return EDigetherElevatorResult::NotAuthority;
	}
	if (!DigetherElevatorRules::IsValidRunParameters(RunTuning.ElevatorCapacityKg, RunTuning.ElevatorTripLimit,
		RunTuning.ElevatorTravelSeconds) || !DigetherElevatorRules::IsValidTuning(Tuning))
	{
		return EDigetherElevatorResult::InvalidInput;
	}

	CapacityKg = RunTuning.ElevatorCapacityKg;
	TripLimit = RunTuning.ElevatorTripLimit;
	TravelSeconds = RunTuning.ElevatorTravelSeconds;
	ElevatorTuning = Tuning;
	State = FDigetherElevatorState();
	LastAscent = FDigetherElevatorAscentConfirmed();
	bHasLastAscent = false;
	bInitialized = true;

	const bool bPublicStateChanged = RefreshPublicState(EDigetherElevatorWeightStage::Empty);
	const FDigetherElevatorPublicState StateSnapshot = PublicState;
	UE_LOG(LogDigetherElevator, Verbose, TEXT("Elevator initialized for a run."));
	if (bPublicStateChanged)
	{
		DispatchStateNotification(StateSnapshot);
	}
	return EDigetherElevatorResult::Success;
}

EDigetherElevatorResult UDigetherElevatorComponent::DepartHub()
{
	if (!HasServerAuthority())
	{
		return EDigetherElevatorResult::NotAuthority;
	}
	if (!bInitialized)
	{
		return EDigetherElevatorResult::NotInitialized;
	}

	const FDigetherElevatorTransition Update = DigetherElevatorRules::Transition(State,
		EDigetherElevatorCommand::DepartHub, 0.f, TripLimit, TravelSeconds, ElevatorTuning);
	if (Update.Result != EDigetherElevatorResult::Success)
	{
		return Update.Result;
	}

	State = Update.State;
	const bool bPublicStateChanged = RefreshPublicState(PublicState.WeightStage);
	const FDigetherElevatorPublicState StateSnapshot = PublicState;
	if (bPublicStateChanged)
	{
		DispatchStateNotification(StateSnapshot);
	}
	return Update.Result;
}

EDigetherElevatorResult UDigetherElevatorComponent::ConfirmAscent(const FDigetherElevatorAscentConfirmed& Ascent)
{
	if (!HasServerAuthority())
	{
		return EDigetherElevatorResult::NotAuthority;
	}
	if (!bInitialized)
	{
		return EDigetherElevatorResult::NotInitialized;
	}
	if (State.Phase != EDigetherElevatorPhase::BottomWaiting)
	{
		return EDigetherElevatorResult::InvalidPhase;
	}

	const int32 ExpectedTripNumber = State.CompletedAscents + 1;
	const bool bExpectedFinalAscent = ExpectedTripNumber == TripLimit;
	if (Ascent.TripNumber != ExpectedTripNumber || Ascent.bFinalAscent != bExpectedFinalAscent)
	{
		return EDigetherElevatorResult::InvalidInput;
	}

	const FDigetherElevatorAscentConfirmed CanonicalAscent = DigetherElevatorRules::CaptureAscent(
		State.CompletedAscents, TripLimit, Ascent.LoadEntries, CapacityKg, ElevatorTuning);
	if (CanonicalAscent.TripNumber != ExpectedTripNumber || CanonicalAscent.bFinalAscent != bExpectedFinalAscent
		|| CanonicalAscent.PassengerIds != Ascent.PassengerIds || CanonicalAscent.TotalWeightKg != Ascent.TotalWeightKg
		|| CanonicalAscent.WeightStage != Ascent.WeightStage)
	{
		return EDigetherElevatorResult::InvalidInput;
	}
	if (DigetherElevatorRules::IsOverCapacity(CanonicalAscent.TotalWeightKg, CapacityKg))
	{
		return EDigetherElevatorResult::OverCapacity;
	}

	const FDigetherElevatorTransition Update = DigetherElevatorRules::Transition(State,
		EDigetherElevatorCommand::RequestAscent, 0.f, TripLimit, TravelSeconds, ElevatorTuning);
	if (Update.Result != EDigetherElevatorResult::Success)
	{
		return Update.Result;
	}

	State = Update.State;
	LastAscent = CanonicalAscent;
	bHasLastAscent = true;
	const bool bPublicStateChanged = RefreshPublicState(CanonicalAscent.WeightStage);
	const FDigetherElevatorPublicState StateSnapshot = PublicState;
	const FDigetherElevatorAscentConfirmed AscentSnapshot = LastAscent;
	UE_LOG(LogDigetherElevator, Verbose, TEXT("Elevator ascent confirmed: trip %d, final %d."),
		AscentSnapshot.TripNumber, AscentSnapshot.bFinalAscent);
	if (bPublicStateChanged)
	{
		DispatchStateNotification(StateSnapshot);
	}
	OnAscentConfirmed.Broadcast(AscentSnapshot);
	return Update.Result;
}

EDigetherElevatorResult UDigetherElevatorComponent::UpdateLoad(const TArray<FDigetherElevatorLoadEntry>& LoadEntries)
{
	if (!HasServerAuthority())
	{
		return EDigetherElevatorResult::NotAuthority;
	}
	if (!bInitialized)
	{
		return EDigetherElevatorResult::NotInitialized;
	}
	if (!AreValidLoadKinds(LoadEntries))
	{
		return EDigetherElevatorResult::InvalidInput;
	}

	const FDigetherElevatorWeightSummary Weight = DigetherElevatorRules::EvaluateLoad(LoadEntries, CapacityKg,
		ElevatorTuning);
	if (!RefreshPublicState(Weight.Stage))
	{
		return EDigetherElevatorResult::NoChange;
	}

	const FDigetherElevatorPublicState StateSnapshot = PublicState;
	DispatchStateNotification(StateSnapshot);
	return EDigetherElevatorResult::Success;
}

EDigetherElevatorResult UDigetherElevatorComponent::AdvanceTravel(const float DeltaSeconds)
{
	if (!HasServerAuthority())
	{
		return EDigetherElevatorResult::NotAuthority;
	}
	if (!bInitialized)
	{
		return EDigetherElevatorResult::NotInitialized;
	}

	const FDigetherElevatorTransition Update = DigetherElevatorRules::Transition(State,
		EDigetherElevatorCommand::AdvanceTime, DeltaSeconds, TripLimit, TravelSeconds, ElevatorTuning);
	if (Update.Result != EDigetherElevatorResult::Success
		&& Update.Result != EDigetherElevatorResult::ExplorationFinished)
	{
		return Update.Result;
	}

	const bool bExplorationFinished = State.Phase != EDigetherElevatorPhase::Finished
		&& Update.State.Phase == EDigetherElevatorPhase::Finished;
	State = Update.State;
	const bool bPublicStateChanged = RefreshPublicState(PublicState.WeightStage);
	const FDigetherElevatorPublicState StateSnapshot = PublicState;
	const FDigetherElevatorAscentConfirmed FinalAscentSnapshot = LastAscent;
	if (bPublicStateChanged)
	{
		DispatchStateNotification(StateSnapshot);
	}
	if (bExplorationFinished)
	{
		UE_LOG(LogDigetherElevator, Verbose, TEXT("Elevator exploration finished."));
		OnExplorationFinished.Broadcast(FinalAscentSnapshot);
	}
	return Update.Result;
}

FDigetherElevatorPublicState UDigetherElevatorComponent::GetPublicState() const
{
	return PublicState;
}

bool UDigetherElevatorComponent::TryGetLastAscent(FDigetherElevatorAscentConfirmed& OutAscent) const
{
	OutAscent = FDigetherElevatorAscentConfirmed();
	if (!HasServerAuthority() || !bInitialized || !bHasLastAscent)
	{
		return false;
	}
	OutAscent = LastAscent;
	return true;
}

bool UDigetherElevatorComponent::TryGetServerState(FDigetherElevatorState& OutState) const
{
	OutState = FDigetherElevatorState();
	if (!HasServerAuthority() || !bInitialized)
	{
		return false;
	}
	OutState = State;
	return true;
}

bool UDigetherElevatorComponent::HasServerAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return IsValid(OwnerActor) && OwnerActor->HasAuthority();
}

bool UDigetherElevatorComponent::RefreshPublicState(const EDigetherElevatorWeightStage WeightStage)
{
	FDigetherElevatorPublicState UpdatedState;
	UpdatedState.Phase = State.Phase;
	UpdatedState.CompletedAscents = State.CompletedAscents;
	UpdatedState.TravelProgress = DigetherElevatorRules::CalculateTravelProgress(State, TravelSeconds);
	UpdatedState.WeightStage = WeightStage;
	if (PublicState.Phase == UpdatedState.Phase && PublicState.CompletedAscents == UpdatedState.CompletedAscents
		&& PublicState.TravelProgress == UpdatedState.TravelProgress && PublicState.WeightStage == UpdatedState.WeightStage)
	{
		return false;
	}
	PublicState = UpdatedState;
	return true;
}

void UDigetherElevatorComponent::DispatchStateNotification(const FDigetherElevatorPublicState& StateSnapshot)
{
	if (NotificationBatchDepth > 0)
	{
		bStateNotificationPending = true;
		return;
	}
	OnStateChanged.Broadcast(StateSnapshot);
}

void UDigetherElevatorComponent::OnRep_PublicState()
{
	const FDigetherElevatorPublicState StateSnapshot = PublicState;
	OnStateChanged.Broadcast(StateSnapshot);
}
