#include "Game/Elevator/DigetherElevatorRules.h"

namespace
{
	bool IsPositiveFinite(float Value)
	{
		return FMath::IsFinite(Value) && Value > 0.f;
	}

	bool IsNonNegativeFinite(float Value)
	{
		return FMath::IsFinite(Value) && Value >= 0.f;
	}

	bool IsValidTripLimit(int32 TripLimit)
	{
		return TripLimit == 1 || TripLimit == 2;
	}

	bool IsValidLoadKind(EDigetherElevatorLoadKind Kind)
	{
		switch (Kind)
		{
		case EDigetherElevatorLoadKind::Player:
		case EDigetherElevatorLoadKind::Corpse:
		case EDigetherElevatorLoadKind::CarriedItem:
		case EDigetherElevatorLoadKind::Cargo:
			return true;
		default:
			return false;
		}
	}

	bool HasOnlyValidLoadKinds(const TArray<FDigetherElevatorLoadEntry>& Entries)
	{
		for (const FDigetherElevatorLoadEntry& Entry : Entries)
		{
			if (!IsValidLoadKind(Entry.Kind))
			{
				return false;
			}
		}
		return true;
	}

	bool IsValidPhase(EDigetherElevatorPhase Phase)
	{
		switch (Phase)
		{
		case EDigetherElevatorPhase::HubWaiting:
		case EDigetherElevatorPhase::InitialDescent:
		case EDigetherElevatorPhase::BottomWaiting:
		case EDigetherElevatorPhase::Ascending:
		case EDigetherElevatorPhase::UpperStop:
		case EDigetherElevatorPhase::ReturnDescent:
		case EDigetherElevatorPhase::FinalAscent:
		case EDigetherElevatorPhase::Finished:
			return true;
		default:
			return false;
		}
	}

	bool IsValidState(const FDigetherElevatorState& State, int32 TripLimit, float TravelSeconds,
		const FDigetherElevatorTuning& Tuning)
	{
		if (!IsNonNegativeFinite(State.PhaseElapsedSeconds)
			|| State.CompletedAscents < 0 || State.CompletedAscents > TripLimit)
		{
			return false;
		}

		switch (State.Phase)
		{
		case EDigetherElevatorPhase::HubWaiting:
			return State.CompletedAscents == 0 && State.PhaseElapsedSeconds == 0.f;
		case EDigetherElevatorPhase::InitialDescent:
			return State.CompletedAscents == 0 && State.PhaseElapsedSeconds < TravelSeconds;
		case EDigetherElevatorPhase::BottomWaiting:
			return State.CompletedAscents < TripLimit && State.PhaseElapsedSeconds == 0.f;
		case EDigetherElevatorPhase::Ascending:
			return State.CompletedAscents + 1 < TripLimit && State.PhaseElapsedSeconds < TravelSeconds;
		case EDigetherElevatorPhase::FinalAscent:
			return State.CompletedAscents + 1 == TripLimit && State.PhaseElapsedSeconds < TravelSeconds;
		case EDigetherElevatorPhase::UpperStop:
			return State.CompletedAscents > 0 && State.CompletedAscents < TripLimit
				&& State.PhaseElapsedSeconds < Tuning.UpperStopSeconds;
		case EDigetherElevatorPhase::ReturnDescent:
			return State.CompletedAscents > 0 && State.CompletedAscents < TripLimit
				&& State.PhaseElapsedSeconds < TravelSeconds;
		case EDigetherElevatorPhase::Finished:
			return State.CompletedAscents == TripLimit && State.PhaseElapsedSeconds == 0.f;
		default:
			return false;
		}
	}

	bool IsValidLeverKind(EDigetherElevatorLeverKind Kind)
	{
		return Kind == EDigetherElevatorLeverKind::Ascent || Kind == EDigetherElevatorLeverKind::Departure;
	}

	bool IsValidGaugeState(const FDigetherElevatorGaugeState& State)
	{
		if (!IsNonNegativeFinite(State.ElapsedSeconds) || !IsNonNegativeFinite(State.DurationSeconds)
			|| State.DepartureDayNumber < 0)
		{
			return false;
		}

		const FDigetherElevatorTuning BoardDefaults;
		switch (State.Phase)
		{
		case EDigetherElevatorGaugePhase::Idle:
			return !State.PullerId.IsValid() && State.ElapsedSeconds == 0.f && State.DurationSeconds == 0.f
				&& State.DepartureDayNumber == 0 && !State.bWarningActive;
		case EDigetherElevatorGaugePhase::Pulling:
			return State.PullerId.IsValid() && State.DurationSeconds > 0.f
				&& State.ElapsedSeconds < State.DurationSeconds
				&& State.bWarningActive == (State.DepartureDayNumber == BoardDefaults.WarningDayNumber);
		case EDigetherElevatorGaugePhase::Confirmed:
			return State.PullerId.IsValid() && State.DurationSeconds > 0.f
				&& State.ElapsedSeconds == State.DurationSeconds && !State.bWarningActive;
		default:
			return false;
		}
	}

	bool IsMatchingLeverKind(const FDigetherElevatorGaugeState& State, EDigetherElevatorLeverKind Kind)
	{
		return State.Phase == EDigetherElevatorGaugePhase::Idle
			|| (State.DepartureDayNumber > 0) == (Kind == EDigetherElevatorLeverKind::Departure);
	}

	bool HasValidAscentParameters(const FDigetherElevatorPullContext& Context)
	{
		return IsValidTripLimit(Context.TripLimit) && Context.CompletedAscents >= 0
			&& Context.CompletedAscents < Context.TripLimit && IsPositiveFinite(Context.CapacityKg);
	}

	FDigetherElevatorGaugeUpdate MakeGaugeUpdate(const FDigetherElevatorGaugeState& State,
		EDigetherElevatorLeverResult Result, bool bWarning = false, bool bRattle = false)
	{
		FDigetherElevatorGaugeUpdate Update;
		Update.State = State;
		Update.Feedback.Result = Result;
		Update.Feedback.bWarning = bWarning;
		Update.Feedback.bRattle = bRattle;
		return Update;
	}
}

namespace DigetherElevatorRules
{
	bool IsValidTuning(const FDigetherElevatorTuning& Tuning)
	{
		const FDigetherElevatorTuning BoardDefaults;
		return IsPositiveFinite(Tuning.LightMaxCapacityRatio)
			&& FMath::IsFinite(Tuning.MediumMaxCapacityRatio)
			&& Tuning.LightMaxCapacityRatio < Tuning.MediumMaxCapacityRatio
			&& Tuning.MediumMaxCapacityRatio < 1.f
			&& IsPositiveFinite(Tuning.UpperStopSeconds)
			&& IsPositiveFinite(Tuning.AscentHoldSeconds)
			&& IsPositiveFinite(Tuning.DepartureHoldSeconds)
			&& Tuning.WarningDayNumber == BoardDefaults.WarningDayNumber
			&& Tuning.WarningDayHoldMultiplier == BoardDefaults.WarningDayHoldMultiplier;
	}

	bool IsValidRunParameters(float CapacityKg, int32 TripLimit, float TravelSeconds)
	{
		return IsPositiveFinite(CapacityKg) && IsValidTripLimit(TripLimit) && IsPositiveFinite(TravelSeconds);
	}

	float SanitizeWeight(const FDigetherElevatorLoadEntry& Entry)
	{
		switch (Entry.Kind)
		{
		case EDigetherElevatorLoadKind::CarriedItem:
		case EDigetherElevatorLoadKind::Cargo:
			return IsPositiveFinite(Entry.WeightKg) ? Entry.WeightKg : 0.f;
		default:
			return 0.f;
		}
	}

	double SumWeight(const TArray<FDigetherElevatorLoadEntry>& Entries)
	{
		double TotalWeightKg = 0.0;
		for (const FDigetherElevatorLoadEntry& Entry : Entries)
		{
			TotalWeightKg += static_cast<double>(SanitizeWeight(Entry));
		}
		return TotalWeightKg;
	}

	bool IsOverCapacity(double TotalWeightKg, float CapacityKg)
	{
		return FMath::IsFinite(TotalWeightKg) && TotalWeightKg >= 0.0 && IsPositiveFinite(CapacityKg)
			&& TotalWeightKg > static_cast<double>(CapacityKg);
	}

	EDigetherElevatorWeightStage CalculateWeightStage(double TotalWeightKg, float CapacityKg,
		const FDigetherElevatorTuning& Tuning)
	{
		if (!FMath::IsFinite(TotalWeightKg) || TotalWeightKg <= 0.0 || !IsPositiveFinite(CapacityKg)
			|| !IsValidTuning(Tuning))
		{
			return EDigetherElevatorWeightStage::Empty;
		}
		if (IsOverCapacity(TotalWeightKg, CapacityKg))
		{
			return EDigetherElevatorWeightStage::Over;
		}

		const double CapacityRatio = TotalWeightKg / static_cast<double>(CapacityKg);
		if (CapacityRatio <= static_cast<double>(Tuning.LightMaxCapacityRatio))
		{
			return EDigetherElevatorWeightStage::Light;
		}
		if (CapacityRatio <= static_cast<double>(Tuning.MediumMaxCapacityRatio))
		{
			return EDigetherElevatorWeightStage::Medium;
		}
		return EDigetherElevatorWeightStage::Heavy;
	}

	FDigetherElevatorWeightSummary EvaluateLoad(const TArray<FDigetherElevatorLoadEntry>& Entries,
		float CapacityKg, const FDigetherElevatorTuning& Tuning)
	{
		FDigetherElevatorWeightSummary Summary;
		Summary.TotalWeightKg = SumWeight(Entries);
		Summary.Stage = CalculateWeightStage(Summary.TotalWeightKg, CapacityKg, Tuning);
		return Summary;
	}

	bool IsMoving(EDigetherElevatorPhase Phase)
	{
		return Phase == EDigetherElevatorPhase::InitialDescent || Phase == EDigetherElevatorPhase::Ascending
			|| Phase == EDigetherElevatorPhase::ReturnDescent || Phase == EDigetherElevatorPhase::FinalAscent;
	}

	float CalculateTravelProgress(const FDigetherElevatorState& State, float TravelSeconds)
	{
		if (!IsValidPhase(State.Phase) || !IsNonNegativeFinite(State.PhaseElapsedSeconds)
			|| !IsPositiveFinite(TravelSeconds))
		{
			return 0.f;
		}
		const float TravelFraction = static_cast<float>(FMath::Clamp(static_cast<double>(State.PhaseElapsedSeconds)
			/ static_cast<double>(TravelSeconds), 0.0, 1.0));
		switch (State.Phase)
		{
		case EDigetherElevatorPhase::InitialDescent:
		case EDigetherElevatorPhase::ReturnDescent:
			return 1.f - TravelFraction;
		case EDigetherElevatorPhase::Ascending:
		case EDigetherElevatorPhase::FinalAscent:
			return TravelFraction;
		case EDigetherElevatorPhase::HubWaiting:
		case EDigetherElevatorPhase::UpperStop:
		case EDigetherElevatorPhase::Finished:
			return 1.f;
		default:
			return 0.f;
		}
	}

	FDigetherElevatorTransition Transition(const FDigetherElevatorState& State,
		EDigetherElevatorCommand Command, float DeltaSeconds, int32 TripLimit, float TravelSeconds,
		const FDigetherElevatorTuning& Tuning)
	{
		FDigetherElevatorTransition Update;
		Update.State = State;
		if (!IsValidTuning(Tuning) || !IsValidTripLimit(TripLimit) || !IsPositiveFinite(TravelSeconds)
			|| !IsNonNegativeFinite(DeltaSeconds))
		{
			Update.Result = EDigetherElevatorResult::InvalidInput;
			return Update;
		}
		// 새 런 초기화는 손상된 이전 상태도 복구할 수 있다.
		if (Command == EDigetherElevatorCommand::Reset)
		{
			Update.State = FDigetherElevatorState();
			Update.Result = EDigetherElevatorResult::Success;
			return Update;
		}
		if (!IsValidState(State, TripLimit, TravelSeconds, Tuning))
		{
			Update.Result = EDigetherElevatorResult::InvalidInput;
			return Update;
		}

		switch (Command)
		{
		case EDigetherElevatorCommand::DepartHub:
			if (State.Phase != EDigetherElevatorPhase::HubWaiting)
			{
				Update.Result = EDigetherElevatorResult::InvalidPhase;
				return Update;
			}
			Update.State.Phase = EDigetherElevatorPhase::InitialDescent;
			Update.Result = EDigetherElevatorResult::Success;
			return Update;
		case EDigetherElevatorCommand::RequestAscent:
			if (State.Phase != EDigetherElevatorPhase::BottomWaiting)
			{
				Update.Result = EDigetherElevatorResult::InvalidPhase;
				return Update;
			}
			Update.State.Phase = State.CompletedAscents + 1 == TripLimit
				? EDigetherElevatorPhase::FinalAscent : EDigetherElevatorPhase::Ascending;
			Update.Result = EDigetherElevatorResult::Success;
			return Update;
		case EDigetherElevatorCommand::AdvanceTime:
			break;
		default:
			Update.Result = EDigetherElevatorResult::InvalidInput;
			return Update;
		}

		if ((!IsMoving(State.Phase) && State.Phase != EDigetherElevatorPhase::UpperStop) || DeltaSeconds == 0.f)
		{
			return Update;
		}

		const float DurationSeconds = State.Phase == EDigetherElevatorPhase::UpperStop
			? Tuning.UpperStopSeconds : TravelSeconds;
		const float ElapsedSeconds = static_cast<float>(FMath::Min(static_cast<double>(State.PhaseElapsedSeconds)
			+ static_cast<double>(DeltaSeconds), static_cast<double>(DurationSeconds)));
		Update.Result = EDigetherElevatorResult::Success;
		if (ElapsedSeconds < DurationSeconds)
		{
			Update.State.PhaseElapsedSeconds = ElapsedSeconds;
			return Update;
		}

		// 초과 시간은 폐기하여 한 호출에서 여러 운행 단계를 건너뛰지 않는다.
		Update.State.PhaseElapsedSeconds = 0.f;
		switch (State.Phase)
		{
		case EDigetherElevatorPhase::InitialDescent:
		case EDigetherElevatorPhase::ReturnDescent:
			Update.State.Phase = EDigetherElevatorPhase::BottomWaiting;
			break;
		case EDigetherElevatorPhase::Ascending:
			++Update.State.CompletedAscents;
			Update.State.Phase = EDigetherElevatorPhase::UpperStop;
			break;
		case EDigetherElevatorPhase::UpperStop:
			Update.State.Phase = EDigetherElevatorPhase::ReturnDescent;
			break;
		case EDigetherElevatorPhase::FinalAscent:
			++Update.State.CompletedAscents;
			Update.State.Phase = EDigetherElevatorPhase::Finished;
			Update.Result = EDigetherElevatorResult::ExplorationFinished;
			break;
		default:
			break;
		}
		return Update;
	}

	FDigetherElevatorAscentConfirmed CaptureAscent(int32 CompletedAscents, int32 TripLimit,
		const TArray<FDigetherElevatorLoadEntry>& Entries, float CapacityKg, const FDigetherElevatorTuning& Tuning)
	{
		FDigetherElevatorAscentConfirmed Manifest;
		if (!IsValidTripLimit(TripLimit) || CompletedAscents < 0 || CompletedAscents >= TripLimit
			|| !IsPositiveFinite(CapacityKg) || !IsValidTuning(Tuning) || !HasOnlyValidLoadKinds(Entries))
		{
			return Manifest;
		}

		Manifest.TripNumber = CompletedAscents + 1;
		Manifest.bFinalAscent = Manifest.TripNumber == TripLimit;
		Manifest.LoadEntries = Entries;
		for (FDigetherElevatorLoadEntry& Entry : Manifest.LoadEntries)
		{
			Entry.WeightKg = SanitizeWeight(Entry);
			if (Entry.Kind == EDigetherElevatorLoadKind::Player)
			{
				Manifest.PassengerIds.AddUnique(Entry.EntryId);
			}
		}
		const FDigetherElevatorWeightSummary Summary = EvaluateLoad(Manifest.LoadEntries, CapacityKg, Tuning);
		Manifest.TotalWeightKg = Summary.TotalWeightKg;
		Manifest.WeightStage = Summary.Stage;
		return Manifest;
	}

	EDigetherElevatorLeverResult CanUseDeparture(const FDigetherElevatorPullContext& Context)
	{
		if (!IsValidPhase(Context.ElevatorPhase) || Context.DayNumber <= 0)
		{
			return EDigetherElevatorLeverResult::InvalidInput;
		}
		if (Context.ElevatorPhase != EDigetherElevatorPhase::HubWaiting)
		{
			return EDigetherElevatorLeverResult::InvalidPhase;
		}
		if (Context.bAnyoneWatchingResultCredits)
		{
			return EDigetherElevatorLeverResult::WatchingResultCredits;
		}
		return EDigetherElevatorLeverResult::Started;
	}

	float CalculateDepartureDuration(int32 DayNumber, const FDigetherElevatorTuning& Tuning)
	{
		if (DayNumber <= 0 || !IsValidTuning(Tuning))
		{
			return 0.f;
		}
		const double DurationSeconds = static_cast<double>(Tuning.DepartureHoldSeconds)
			* (DayNumber == Tuning.WarningDayNumber ? static_cast<double>(Tuning.WarningDayHoldMultiplier) : 1.0);
		return DurationSeconds <= static_cast<double>(MAX_flt) ? static_cast<float>(DurationSeconds) : 0.f;
	}

	float CalculateGaugeProgress(const FDigetherElevatorGaugeState& State)
	{
		if (!IsValidGaugeState(State) || State.Phase == EDigetherElevatorGaugePhase::Idle)
		{
			return 0.f;
		}
		return static_cast<float>(FMath::Clamp(static_cast<double>(State.ElapsedSeconds)
			/ static_cast<double>(State.DurationSeconds), 0.0, 1.0));
	}

	FDigetherElevatorGaugeUpdate BeginPull(const FDigetherElevatorGaugeState& State,
		EDigetherElevatorLeverKind Kind, const FGuid& PullerId, const FDigetherElevatorPullContext& Context,
		const FDigetherElevatorTuning& Tuning)
	{
		if (!IsValidGaugeState(State) || !IsValidLeverKind(Kind) || !PullerId.IsValid() || !IsValidTuning(Tuning)
			|| !IsMatchingLeverKind(State, Kind) || !IsValidPhase(Context.ElevatorPhase)
			|| !HasOnlyValidLoadKinds(Context.LoadEntries))
		{
			return MakeGaugeUpdate(State, EDigetherElevatorLeverResult::InvalidInput);
		}
		if (State.Phase == EDigetherElevatorGaugePhase::Confirmed)
		{
			return MakeGaugeUpdate(State, EDigetherElevatorLeverResult::AlreadyConfirmed);
		}
		if (State.Phase == EDigetherElevatorGaugePhase::Pulling)
		{
			return MakeGaugeUpdate(State, State.PullerId == PullerId
				? EDigetherElevatorLeverResult::InProgress : EDigetherElevatorLeverResult::Busy, State.bWarningActive);
		}

		float DurationSeconds = Tuning.AscentHoldSeconds;
		if (Kind == EDigetherElevatorLeverKind::Ascent)
		{
			if (!HasValidAscentParameters(Context))
			{
				return MakeGaugeUpdate(State, EDigetherElevatorLeverResult::InvalidInput);
			}
			if (Context.ElevatorPhase != EDigetherElevatorPhase::BottomWaiting)
			{
				return MakeGaugeUpdate(State, EDigetherElevatorLeverResult::InvalidPhase);
			}
			if (IsOverCapacity(SumWeight(Context.LoadEntries), Context.CapacityKg))
			{
				return MakeGaugeUpdate(State, EDigetherElevatorLeverResult::OverweightWarningAndRattle, true, true);
			}
		}
		else
		{
			const EDigetherElevatorLeverResult CanUseResult = CanUseDeparture(Context);
			if (CanUseResult != EDigetherElevatorLeverResult::Started)
			{
				return MakeGaugeUpdate(State, CanUseResult);
			}
			DurationSeconds = CalculateDepartureDuration(Context.DayNumber, Tuning);
			if (!IsPositiveFinite(DurationSeconds))
			{
				return MakeGaugeUpdate(State, EDigetherElevatorLeverResult::InvalidInput);
			}
		}

		FDigetherElevatorGaugeState StartedState;
		StartedState.Phase = EDigetherElevatorGaugePhase::Pulling;
		StartedState.PullerId = PullerId;
		StartedState.DurationSeconds = DurationSeconds;
		if (Kind == EDigetherElevatorLeverKind::Departure)
		{
			StartedState.DepartureDayNumber = Context.DayNumber;
			StartedState.bWarningActive = Context.DayNumber == Tuning.WarningDayNumber;
		}
		return MakeGaugeUpdate(StartedState, EDigetherElevatorLeverResult::Started, StartedState.bWarningActive);
	}

	FDigetherElevatorGaugeUpdate ReleasePull(const FDigetherElevatorGaugeState& State, const FGuid& PullerId)
	{
		if (!IsValidGaugeState(State) || !PullerId.IsValid())
		{
			return MakeGaugeUpdate(State, EDigetherElevatorLeverResult::InvalidInput);
		}
		if (State.Phase == EDigetherElevatorGaugePhase::Confirmed)
		{
			return MakeGaugeUpdate(State, EDigetherElevatorLeverResult::AlreadyConfirmed);
		}
		if (State.Phase != EDigetherElevatorGaugePhase::Pulling)
		{
			return MakeGaugeUpdate(State, EDigetherElevatorLeverResult::NotPulling);
		}
		if (State.PullerId != PullerId)
		{
			return MakeGaugeUpdate(State, EDigetherElevatorLeverResult::NotPuller, State.bWarningActive);
		}
		return MakeGaugeUpdate(FDigetherElevatorGaugeState(), EDigetherElevatorLeverResult::Released);
	}

	FDigetherElevatorGaugeUpdate AdvancePull(const FDigetherElevatorGaugeState& State,
		EDigetherElevatorLeverKind Kind, float DeltaSeconds, const FDigetherElevatorPullContext& Context,
		const FDigetherElevatorTuning& Tuning)
	{
		if (!IsValidGaugeState(State) || !IsValidLeverKind(Kind) || !IsNonNegativeFinite(DeltaSeconds)
			|| !IsValidTuning(Tuning) || !IsMatchingLeverKind(State, Kind)
			|| !IsValidPhase(Context.ElevatorPhase) || !HasOnlyValidLoadKinds(Context.LoadEntries))
		{
			return MakeGaugeUpdate(State, EDigetherElevatorLeverResult::InvalidInput);
		}
		if (State.Phase == EDigetherElevatorGaugePhase::Confirmed)
		{
			return MakeGaugeUpdate(State, EDigetherElevatorLeverResult::AlreadyConfirmed);
		}
		if (State.Phase != EDigetherElevatorGaugePhase::Pulling)
		{
			return MakeGaugeUpdate(State, EDigetherElevatorLeverResult::NotPulling);
		}

		if (Kind == EDigetherElevatorLeverKind::Ascent)
		{
			if (!HasValidAscentParameters(Context) || Context.ElevatorPhase != EDigetherElevatorPhase::BottomWaiting)
			{
				return MakeGaugeUpdate(FDigetherElevatorGaugeState(), EDigetherElevatorLeverResult::InterruptedConditions);
			}
			if (IsOverCapacity(SumWeight(Context.LoadEntries), Context.CapacityKg))
			{
				return MakeGaugeUpdate(FDigetherElevatorGaugeState(), EDigetherElevatorLeverResult::InterruptedOverweight, true, true);
			}
		}
		else if (CanUseDeparture(Context) != EDigetherElevatorLeverResult::Started
			|| Context.DayNumber != State.DepartureDayNumber)
		{
			return MakeGaugeUpdate(FDigetherElevatorGaugeState(), EDigetherElevatorLeverResult::InterruptedConditions);
		}

		FDigetherElevatorGaugeState NextState = State;
		const float ElapsedSeconds = static_cast<float>(FMath::Min(static_cast<double>(State.ElapsedSeconds)
			+ static_cast<double>(DeltaSeconds), static_cast<double>(State.DurationSeconds)));
		if (ElapsedSeconds < State.DurationSeconds)
		{
			NextState.ElapsedSeconds = ElapsedSeconds;
			return MakeGaugeUpdate(NextState, EDigetherElevatorLeverResult::InProgress, NextState.bWarningActive);
		}

		NextState.Phase = EDigetherElevatorGaugePhase::Confirmed;
		NextState.ElapsedSeconds = NextState.DurationSeconds;
		NextState.bWarningActive = false;
		if (Kind == EDigetherElevatorLeverKind::Ascent)
		{
			NextState.Ascent = CaptureAscent(Context.CompletedAscents, Context.TripLimit,
				Context.LoadEntries, Context.CapacityKg, Tuning);
		}
		else
		{
			NextState.Departure.PullerId = State.PullerId;
			NextState.Departure.DayNumber = Context.DayNumber;
		}
		return MakeGaugeUpdate(NextState, EDigetherElevatorLeverResult::Confirmed);
	}
}
