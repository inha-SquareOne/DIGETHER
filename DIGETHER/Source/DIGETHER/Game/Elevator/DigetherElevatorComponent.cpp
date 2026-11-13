#include "Game/Elevator/DigetherElevatorComponent.h"

#include "Game/Core/DigetherAuthority.h"
#include "Game/Data/DigetherTuningProfile.h"
#include "Game/Elevator/DigetherElevatorLog.h"
#include "Game/Elevator/DigetherElevatorRules.h"
#include "Net/UnrealNetwork.h"

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
	if (DigetherAuthority::HasOwnerAuthority(*this))
	{
		++NotificationBatchDepth;
	}
}

void UDigetherElevatorComponent::EndNotificationBatch()
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

	const bool bShouldNotify = bStateNotificationPending;
	// 콜백에서 새 묶음을 시작해도 이전 알림이 다시 남지 않도록 먼저 비움
	bStateNotificationPending = false;
	if (bShouldNotify)
	{
		const FDigetherElevatorPublicState StateSnapshot = PublicState;
		OnStateChanged.Broadcast(StateSnapshot);
	}
}

EDigetherElevatorResult UDigetherElevatorComponent::InitializeForRun(const FDigetherRunTuning& RunTuning,
	const FDigetherElevatorTuning& Tuning)
{
	if (!DigetherAuthority::HasOwnerAuthority(*this))
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
#if !UE_BUILD_SHIPPING
	LastLoadWeightKg = 0.0;
#endif

	UE_LOG(LogDigetherElevator, Verbose, TEXT("Elevator initialized for a run."));
	CommitState(EDigetherElevatorWeightStage::Empty);
	return EDigetherElevatorResult::Success;
}

EDigetherElevatorResult UDigetherElevatorComponent::DepartHub()
{
	if (!DigetherAuthority::HasOwnerAuthority(*this))
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
	CommitState(PublicState.WeightStage);
	return Update.Result;
}

EDigetherElevatorResult UDigetherElevatorComponent::ConfirmAscent(const FDigetherElevatorAscentConfirmed& Ascent)
{
	if (!DigetherAuthority::HasOwnerAuthority(*this))
	{
		return EDigetherElevatorResult::NotAuthority;
	}
	if (!bInitialized)
	{
		return EDigetherElevatorResult::NotInitialized;
	}
	const bool bAtBottomStop = State.Phase == EDigetherElevatorPhase::BottomWaiting
		|| (State.Phase == EDigetherElevatorPhase::DoorClosing
			&& State.DoorOriginPhase == EDigetherElevatorPhase::BottomWaiting);
	if (!bAtBottomStop)
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
	// 상태 알림 콜백이 컴포넌트를 다시 바꿔도 이번 확정 매니페스트를 전달하도록 먼저 복사
	const FDigetherElevatorAscentConfirmed AscentSnapshot = LastAscent;
	UE_LOG(LogDigetherElevator, Verbose, TEXT("Elevator ascent confirmed: trip %d, final %d."),
		AscentSnapshot.TripNumber, AscentSnapshot.bFinalAscent);
	CommitState(CanonicalAscent.WeightStage);
	OnAscentConfirmed.Broadcast(AscentSnapshot);
	return Update.Result;
}

EDigetherElevatorResult UDigetherElevatorComponent::UpdateLoad(const TArray<FDigetherElevatorLoadEntry>& LoadEntries)
{
	if (!DigetherAuthority::HasOwnerAuthority(*this))
	{
		return EDigetherElevatorResult::NotAuthority;
	}
	if (!bInitialized)
	{
		return EDigetherElevatorResult::NotInitialized;
	}
	if (!DigetherElevatorRules::HasOnlyValidLoadKinds(LoadEntries))
	{
		return EDigetherElevatorResult::InvalidInput;
	}

	const FDigetherElevatorWeightSummary Weight = DigetherElevatorRules::EvaluateLoad(LoadEntries, CapacityKg,
		ElevatorTuning);
#if !UE_BUILD_SHIPPING
	LastLoadWeightKg = Weight.TotalWeightKg;
#endif
	// 기획 A3: 갱도 상승 문이 닫히는 중 무게 초과면 문 재개방, 이번 시도는 편으로 세지 않음
	// 거점 출발 문은 게이지 완료로 취소 불가가 확정되어 무게로 재개방하지 않음
	if (DigetherElevatorRules::ShouldReopenDoorForOverweight(State, Weight.TotalWeightKg, CapacityKg))
	{
		const EDigetherElevatorPhase OriginPhase = State.DoorOriginPhase;
		const FDigetherElevatorTransition Reopened = DigetherElevatorRules::Transition(State,
			EDigetherElevatorCommand::ReopenDoor, 0.f, TripLimit, TravelSeconds, ElevatorTuning);
		if (Reopened.Result == EDigetherElevatorResult::Success)
		{
			State = Reopened.State;
			UE_LOG(LogDigetherElevator, Verbose, TEXT("Elevator door reopened by overweight."));
			CommitState(Weight.Stage);
			OnDoorReopenedByOverweight.Broadcast(OriginPhase);
			return EDigetherElevatorResult::Success;
		}
	}
	return CommitState(Weight.Stage) ? EDigetherElevatorResult::Success : EDigetherElevatorResult::NoChange;
}

EDigetherElevatorResult UDigetherElevatorComponent::AdvanceTravel(const float DeltaSeconds)
{
	if (!DigetherAuthority::HasOwnerAuthority(*this))
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
		&& Update.Result != EDigetherElevatorResult::ExplorationFinished
		&& Update.Result != EDigetherElevatorResult::DoorClosed)
	{
		return Update.Result;
	}

	const bool bExplorationFinished = State.Phase != EDigetherElevatorPhase::Finished
		&& Update.State.Phase == EDigetherElevatorPhase::Finished;
	const bool bDoorClosed = Update.Result == EDigetherElevatorResult::DoorClosed;
	State = Update.State;
	// 상태 알림 콜백이 컴포넌트를 다시 바꿔도 종료 신호에는 이번 마지막 매니페스트 전달
	const FDigetherElevatorAscentConfirmed FinalAscentSnapshot = LastAscent;
	const EDigetherElevatorPhase DoorOriginSnapshot = State.DoorOriginPhase;
	CommitState(PublicState.WeightStage);
	if (bExplorationFinished)
	{
		UE_LOG(LogDigetherElevator, Verbose, TEXT("Elevator exploration finished."));
		OnExplorationFinished.Broadcast(FinalAscentSnapshot);
	}
	if (bDoorClosed)
	{
		UE_LOG(LogDigetherElevator, Verbose, TEXT("Elevator door fully closed."));
		OnDoorClosed.Broadcast(DoorOriginSnapshot);
	}
	return Update.Result;
}

EDigetherElevatorResult UDigetherElevatorComponent::BeginDoorClose()
{
	if (!DigetherAuthority::HasOwnerAuthority(*this))
	{
		return EDigetherElevatorResult::NotAuthority;
	}
	if (!bInitialized)
	{
		return EDigetherElevatorResult::NotInitialized;
	}
	if (PublicState.WeightStage == EDigetherElevatorWeightStage::Over)
	{
		return EDigetherElevatorResult::OverCapacity;
	}

	const FDigetherElevatorTransition Update = DigetherElevatorRules::Transition(State,
		EDigetherElevatorCommand::BeginDoorClose, 0.f, TripLimit, TravelSeconds, ElevatorTuning);
	if (Update.Result != EDigetherElevatorResult::Success)
	{
		return Update.Result;
	}

	State = Update.State;
	UE_LOG(LogDigetherElevator, Verbose, TEXT("Elevator door closing started."));
	CommitState(PublicState.WeightStage);
	return Update.Result;
}

EDigetherElevatorResult UDigetherElevatorComponent::ReopenDoor()
{
	if (!DigetherAuthority::HasOwnerAuthority(*this))
	{
		return EDigetherElevatorResult::NotAuthority;
	}
	if (!bInitialized)
	{
		return EDigetherElevatorResult::NotInitialized;
	}

	const FDigetherElevatorTransition Update = DigetherElevatorRules::Transition(State,
		EDigetherElevatorCommand::ReopenDoor, 0.f, TripLimit, TravelSeconds, ElevatorTuning);
	if (Update.Result != EDigetherElevatorResult::Success)
	{
		return Update.Result;
	}

	State = Update.State;
	UE_LOG(LogDigetherElevator, Verbose, TEXT("Elevator door reopened."));
	CommitState(PublicState.WeightStage);
	return Update.Result;
}

FDigetherElevatorPublicState UDigetherElevatorComponent::GetPublicState() const
{
	return PublicState;
}

bool UDigetherElevatorComponent::TryGetLastAscent(FDigetherElevatorAscentConfirmed& OutAscent) const
{
	OutAscent = FDigetherElevatorAscentConfirmed();
	if (!DigetherAuthority::HasOwnerAuthority(*this) || !bInitialized || !bHasLastAscent)
	{
		return false;
	}
	OutAscent = LastAscent;
	return true;
}

bool UDigetherElevatorComponent::TryGetServerState(FDigetherElevatorState& OutState) const
{
	OutState = FDigetherElevatorState();
	if (!DigetherAuthority::HasOwnerAuthority(*this) || !bInitialized)
	{
		return false;
	}
	OutState = State;
	return true;
}

bool UDigetherElevatorComponent::RefreshPublicState(const EDigetherElevatorWeightStage WeightStage)
{
	FDigetherElevatorPublicState UpdatedState;
	UpdatedState.Phase = State.Phase;
	UpdatedState.CompletedAscents = State.CompletedAscents;
	UpdatedState.TravelProgress = DigetherElevatorRules::CalculateTravelProgress(State, TravelSeconds);
	UpdatedState.WeightStage = WeightStage;
	UpdatedState.DoorCloseProgress = DigetherElevatorRules::CalculateDoorCloseProgress(State, ElevatorTuning);
	if (PublicState.Phase == UpdatedState.Phase && PublicState.CompletedAscents == UpdatedState.CompletedAscents
		&& PublicState.TravelProgress == UpdatedState.TravelProgress && PublicState.WeightStage == UpdatedState.WeightStage
		&& PublicState.DoorCloseProgress == UpdatedState.DoorCloseProgress)
	{
		return false;
	}
	PublicState = UpdatedState;
	return true;
}

bool UDigetherElevatorComponent::CommitState(const EDigetherElevatorWeightStage WeightStage)
{
	const bool bPublicStateChanged = RefreshPublicState(WeightStage);
	if (bPublicStateChanged)
	{
		// 수신 콜백이 PublicState를 바꿔도 이번 값 전달
		const FDigetherElevatorPublicState StateSnapshot = PublicState;
		DispatchStateNotification(StateSnapshot);
	}
	return bPublicStateChanged;
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
