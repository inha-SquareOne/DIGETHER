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
		case EDigetherElevatorPhase::DoorClosing:
			return true;
		default:
			return false;
		}
	}

	bool IsDoorOriginPhase(EDigetherElevatorPhase Phase)
	{
		return Phase == EDigetherElevatorPhase::HubWaiting || Phase == EDigetherElevatorPhase::BottomWaiting;
	}

	bool IsValidState(const FDigetherElevatorState& State, int32 TripLimit, float TravelSeconds,
		const FDigetherElevatorTuning& Tuning)
	{
		if (!IsNonNegativeFinite(State.PhaseElapsedSeconds)
			|| State.CompletedAscents < 0 || State.CompletedAscents > TripLimit)
		{
			return false;
		}
		if (State.Phase != EDigetherElevatorPhase::DoorClosing
			&& State.DoorOriginPhase != EDigetherElevatorPhase::HubWaiting)
		{
			return false;
		}

		switch (State.Phase)
		{
		case EDigetherElevatorPhase::DoorClosing:
			// 닫힘 완료(경과 == 닫힘 시간)는 조율자가 목록을 고정할 때까지 유지하는 유효 상태
			if (State.PhaseElapsedSeconds > DigetherElevatorRules::GetDoorCloseSeconds(State.DoorOriginPhase, Tuning))
			{
				return false;
			}
			if (State.DoorOriginPhase == EDigetherElevatorPhase::HubWaiting)
			{
				return State.CompletedAscents == 0;
			}
			return State.DoorOriginPhase == EDigetherElevatorPhase::BottomWaiting
				&& State.CompletedAscents < TripLimit;
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

	bool IsValidGaugeState(const FDigetherElevatorGaugeState& State)
	{
		if (!IsNonNegativeFinite(State.ElapsedSeconds) || !IsNonNegativeFinite(State.DurationSeconds)
			|| State.DepartureDayNumber < 0 || State.DeparturePeriodGeneration < 0
			|| (State.DepartureDayNumber == 0 && State.DeparturePeriodGeneration != 0))
		{
			return false;
		}

		switch (State.Phase)
		{
		case EDigetherElevatorGaugePhase::Idle:
			return !State.PullerId.IsValid() && State.ElapsedSeconds == 0.f && State.DurationSeconds == 0.f
				&& State.DepartureDayNumber == 0 && State.DeparturePeriodGeneration == 0 && !State.bWarningActive;
		case EDigetherElevatorGaugePhase::Pulling:
			return State.PullerId.IsValid() && State.DurationSeconds > 0.f
				&& State.ElapsedSeconds < State.DurationSeconds
				&& State.bWarningActive == (State.DepartureDayNumber == DigetherElevatorBoardValues::WarningDayNumber);
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
		return DigetherElevatorRules::IsValidTripLimit(Context.TripLimit) && Context.CompletedAscents >= 0
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

	// 아래 Transition* 함수는 Transition의 공통 입력·상태 검사 통과 후에만 호출
	// 대기 단계 자체이거나 그 대기 단계에서 시작한 문 닫힘이면 true
	bool IsAtWaitingStop(const FDigetherElevatorState& State, EDigetherElevatorPhase WaitingPhase)
	{
		return State.Phase == WaitingPhase
			|| (State.Phase == EDigetherElevatorPhase::DoorClosing && State.DoorOriginPhase == WaitingPhase);
	}

	// 일반 편은 조율자가 문 닫힘 완료 신호 뒤 호출, 시간 종료 귀환은 닫힘 도중에도 즉시 출발
	FDigetherElevatorTransition TransitionDepartHub(const FDigetherElevatorState& State)
	{
		FDigetherElevatorTransition Update;
		Update.State = State;
		if (!IsAtWaitingStop(State, EDigetherElevatorPhase::HubWaiting))
		{
			Update.Result = EDigetherElevatorResult::InvalidPhase;
			return Update;
		}
		Update.State.Phase = EDigetherElevatorPhase::InitialDescent;
		Update.State.PhaseElapsedSeconds = 0.f;
		Update.State.DoorOriginPhase = EDigetherElevatorPhase::HubWaiting;
		Update.Result = EDigetherElevatorResult::Success;
		return Update;
	}

	FDigetherElevatorTransition TransitionRequestAscent(const FDigetherElevatorState& State, int32 TripLimit)
	{
		FDigetherElevatorTransition Update;
		Update.State = State;
		if (!IsAtWaitingStop(State, EDigetherElevatorPhase::BottomWaiting))
		{
			Update.Result = EDigetherElevatorResult::InvalidPhase;
			return Update;
		}
		Update.State.Phase = State.CompletedAscents + 1 == TripLimit
			? EDigetherElevatorPhase::FinalAscent : EDigetherElevatorPhase::Ascending;
		Update.State.PhaseElapsedSeconds = 0.f;
		Update.State.DoorOriginPhase = EDigetherElevatorPhase::HubWaiting;
		Update.Result = EDigetherElevatorResult::Success;
		return Update;
	}

	FDigetherElevatorTransition TransitionBeginDoorClose(const FDigetherElevatorState& State)
	{
		FDigetherElevatorTransition Update;
		Update.State = State;
		if (!IsDoorOriginPhase(State.Phase))
		{
			Update.Result = EDigetherElevatorResult::InvalidPhase;
			return Update;
		}
		Update.State.Phase = EDigetherElevatorPhase::DoorClosing;
		Update.State.PhaseElapsedSeconds = 0.f;
		Update.State.DoorOriginPhase = State.Phase;
		Update.Result = EDigetherElevatorResult::Success;
		return Update;
	}

	// 다시 연 시도는 운행 횟수 제외, 시작했던 대기 단계로 복귀
	FDigetherElevatorTransition TransitionReopenDoor(const FDigetherElevatorState& State)
	{
		FDigetherElevatorTransition Update;
		Update.State = State;
		if (State.Phase != EDigetherElevatorPhase::DoorClosing)
		{
			Update.Result = EDigetherElevatorResult::InvalidPhase;
			return Update;
		}
		Update.State.Phase = State.DoorOriginPhase;
		Update.State.PhaseElapsedSeconds = 0.f;
		Update.State.DoorOriginPhase = EDigetherElevatorPhase::HubWaiting;
		Update.Result = EDigetherElevatorResult::Success;
		return Update;
	}

	// 닫힘 완료 뒤 시간이 더 흘러도 같은 상태 유지, 완료 결과는 1회만
	FDigetherElevatorTransition TransitionAdvanceDoor(const FDigetherElevatorState& State, float DeltaSeconds,
		const FDigetherElevatorTuning& Tuning)
	{
		FDigetherElevatorTransition Update;
		Update.State = State;
		const float DoorCloseSeconds = DigetherElevatorRules::GetDoorCloseSeconds(State.DoorOriginPhase, Tuning);
		if (DeltaSeconds == 0.f || State.PhaseElapsedSeconds >= DoorCloseSeconds)
		{
			return Update;
		}
		Update.State.PhaseElapsedSeconds = static_cast<float>(FMath::Min(
			static_cast<double>(State.PhaseElapsedSeconds) + static_cast<double>(DeltaSeconds),
			static_cast<double>(DoorCloseSeconds)));
		Update.Result = Update.State.PhaseElapsedSeconds < DoorCloseSeconds
			? EDigetherElevatorResult::Success : EDigetherElevatorResult::DoorClosed;
		return Update;
	}

	FDigetherElevatorTransition TransitionAdvanceTime(const FDigetherElevatorState& State, float DeltaSeconds,
		float TravelSeconds, const FDigetherElevatorTuning& Tuning)
	{
		if (State.Phase == EDigetherElevatorPhase::DoorClosing)
		{
			return TransitionAdvanceDoor(State, DeltaSeconds, Tuning);
		}
		FDigetherElevatorTransition Update;
		Update.State = State;
		if ((!DigetherElevatorRules::IsMoving(State.Phase) && State.Phase != EDigetherElevatorPhase::UpperStop)
			|| DeltaSeconds == 0.f)
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

		// 초과 시간 폐기, 한 호출에서 여러 운행 단계 건너뛰기 방지
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
}

namespace DigetherElevatorRules
{
	bool IsValidTuning(const FDigetherElevatorTuning& Tuning)
	{
		return IsPositiveFinite(Tuning.LightMaxCapacityRatio)
			&& FMath::IsFinite(Tuning.MediumMaxCapacityRatio)
			&& Tuning.LightMaxCapacityRatio < Tuning.MediumMaxCapacityRatio
			&& Tuning.MediumMaxCapacityRatio < 1.f
			&& IsPositiveFinite(Tuning.UpperStopSeconds)
			&& IsPositiveFinite(Tuning.AscentHoldSeconds)
			&& IsPositiveFinite(Tuning.DepartureHoldSeconds)
			&& IsPositiveFinite(Tuning.HubDoorCloseSeconds)
			&& IsPositiveFinite(Tuning.MineDoorCloseSeconds)
			&& IsNonNegativeFinite(Tuning.ChainSummonSeconds)
			&& Tuning.WarningDayNumber == DigetherElevatorBoardValues::WarningDayNumber
			&& Tuning.WarningDayHoldMultiplier == DigetherElevatorBoardValues::WarningDayHoldMultiplier;
	}

	bool IsValidTripLimit(int32 TripLimit)
	{
		return TripLimit >= MinTripLimit && TripLimit <= MaxTripLimit;
	}

	bool IsValidRunParameters(float CapacityKg, int32 TripLimit, float TravelSeconds)
	{
		return IsPositiveFinite(CapacityKg) && IsValidTripLimit(TripLimit) && IsPositiveFinite(TravelSeconds);
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

	bool IsValidLeverKind(EDigetherElevatorLeverKind Kind)
	{
		return Kind == EDigetherElevatorLeverKind::Ascent || Kind == EDigetherElevatorLeverKind::Departure;
	}

	bool IsValidLeverLayout(const int32 DepartureLevers, const int32 AscentInsideLevers, const int32 AscentOutsideLevers)
	{
		return DepartureLevers == 1 && AscentInsideLevers == 1 && AscentOutsideLevers == 1;
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
		case EDigetherElevatorPhase::DoorClosing:
			// 문 닫힘 중에는 시작한 정지 위치 유지
			return State.DoorOriginPhase == EDigetherElevatorPhase::HubWaiting ? 1.f : 0.f;
		default:
			return 0.f;
		}
	}

	float GetDoorCloseSeconds(const EDigetherElevatorPhase DoorOriginPhase, const FDigetherElevatorTuning& Tuning)
	{
		switch (DoorOriginPhase)
		{
		case EDigetherElevatorPhase::HubWaiting:
			return Tuning.HubDoorCloseSeconds;
		case EDigetherElevatorPhase::BottomWaiting:
			return Tuning.MineDoorCloseSeconds;
		default:
			return 0.f;
		}
	}

	float CalculateDoorCloseProgress(const FDigetherElevatorState& State, const FDigetherElevatorTuning& Tuning)
	{
		const float DoorCloseSeconds = GetDoorCloseSeconds(State.DoorOriginPhase, Tuning);
		if (State.Phase != EDigetherElevatorPhase::DoorClosing || !IsNonNegativeFinite(State.PhaseElapsedSeconds)
			|| !IsPositiveFinite(DoorCloseSeconds))
		{
			return 0.f;
		}
		return static_cast<float>(FMath::Clamp(static_cast<double>(State.PhaseElapsedSeconds)
			/ static_cast<double>(DoorCloseSeconds), 0.0, 1.0));
	}

	bool IsDoorFullyClosed(const FDigetherElevatorState& State, const FDigetherElevatorTuning& Tuning)
	{
		const float DoorCloseSeconds = GetDoorCloseSeconds(State.DoorOriginPhase, Tuning);
		return State.Phase == EDigetherElevatorPhase::DoorClosing && IsPositiveFinite(DoorCloseSeconds)
			&& IsNonNegativeFinite(State.PhaseElapsedSeconds) && State.PhaseElapsedSeconds >= DoorCloseSeconds;
	}

	bool ShouldReopenDoorForOverweight(const FDigetherElevatorState& State, const double TotalWeightKg,
		const float CapacityKg)
	{
		// 거점 출발은 게이지 완료 순간 취소 불가로 확정, 갱도 상승 문만 무게 초과로 재개방
		return State.Phase == EDigetherElevatorPhase::DoorClosing
			&& State.DoorOriginPhase == EDigetherElevatorPhase::BottomWaiting
			&& IsOverCapacity(TotalWeightKg, CapacityKg);
	}

	EDigetherDepartureWarningKind CalculateDepartureWarning(const FDigetherElevatorGaugeState& State)
	{
		if (!IsValidGaugeState(State) || State.Phase != EDigetherElevatorGaugePhase::Pulling
			|| State.DepartureDayNumber <= 0)
		{
			return EDigetherDepartureWarningKind::None;
		}
		// 거점 4일차는 목표 미달일 때만 존재(달성 시 즉시 새 기간 1일차)
		return State.DepartureDayNumber == DigetherElevatorBoardValues::WarningDayNumber
			? EDigetherDepartureWarningKind::Deadline : EDigetherDepartureWarningKind::Standard;
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
		// 새 런 초기화는 손상된 이전 상태도 복구 가능
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
			return TransitionDepartHub(State);
		case EDigetherElevatorCommand::RequestAscent:
			return TransitionRequestAscent(State, TripLimit);
		case EDigetherElevatorCommand::AdvanceTime:
			return TransitionAdvanceTime(State, DeltaSeconds, TravelSeconds, Tuning);
		case EDigetherElevatorCommand::BeginDoorClose:
			return TransitionBeginDoorClose(State);
		case EDigetherElevatorCommand::ReopenDoor:
			return TransitionReopenDoor(State);
		default:
			Update.Result = EDigetherElevatorResult::InvalidInput;
			return Update;
		}
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
		// 결과 크레딧 중 출발 차단은 게임 오버 단계(RunPublicState.bGameOver)가 연결층에서 처리(S33)
		return EDigetherElevatorLeverResult::Started;
	}

	float CalculateDepartureDuration(int32 DayNumber, const FDigetherElevatorTuning& Tuning)
	{
		if (DayNumber <= 0 || !IsValidTuning(Tuning))
		{
			return 0.f;
		}
		// IsValidTuning 통과 튜닝의 경고 일차·배수는 보드 확정값과 같음
		const double HoldMultiplier = DayNumber == DigetherElevatorBoardValues::WarningDayNumber
			? static_cast<double>(DigetherElevatorBoardValues::WarningDayHoldMultiplier) : 1.0;
		const double DurationSeconds = static_cast<double>(Tuning.DepartureHoldSeconds) * HoldMultiplier;
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
		// S15 안팎 레버 동시 입력은 먼저 시작한 레버 한 건만 수락, 이 레버는 Idle 유지
		// 기획 확인 항목: 다른 레버 점유 중이면 무게 초과여도 경고·덜컹 대신 Busy 우선
		if (Context.bOtherLeverOccupied)
		{
			return MakeGaugeUpdate(State, EDigetherElevatorLeverResult::Busy);
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
			StartedState.DeparturePeriodGeneration = Context.PeriodGeneration;
			StartedState.bWarningActive = Context.DayNumber == DigetherElevatorBoardValues::WarningDayNumber;
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
		const float ElapsedSeconds = static_cast<float>(FMath::Min(static_cast<double>(State.ElapsedSeconds)
			+ static_cast<double>(DeltaSeconds), static_cast<double>(State.DurationSeconds)));
		if (Kind == EDigetherElevatorLeverKind::Departure)
		{
			if (CanUseDeparture(Context) != EDigetherElevatorLeverResult::Started || Context.PeriodGeneration < 0)
			{
				return MakeGaugeUpdate(FDigetherElevatorGaugeState(), EDigetherElevatorLeverResult::InterruptedConditions);
			}
			// 목표 달성은 일차가 아닌 기간 세대 번호로 판정(같은 일차 번호로 새 기간이 될 수 있음)
			// 이번 호출에서 게이지 완료와 납부가 겹치면 납부 선반영으로 간주하고 완료된 레버로 새 기간 1일차 출발
			// 그 전에 달성했으면 레버 입력 취소
			const bool bPeriodChanged = Context.PeriodGeneration != State.DeparturePeriodGeneration;
			const bool bCompletesNow = ElapsedSeconds >= State.DurationSeconds;
			if ((bPeriodChanged && !bCompletesNow)
				|| (!bPeriodChanged && Context.DayNumber != State.DepartureDayNumber))
			{
				return MakeGaugeUpdate(FDigetherElevatorGaugeState(), EDigetherElevatorLeverResult::InterruptedConditions);
			}
		}

		FDigetherElevatorGaugeState NextState = State;
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
