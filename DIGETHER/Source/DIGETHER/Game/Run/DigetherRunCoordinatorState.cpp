#include "Game/Run/DigetherRunCoordinatorState.h"

namespace
{
	FDigetherRunAdvanceResult AdvanceCandidate(FDigetherRunCoordinatorState& State, const double ServerSeconds)
	{
		FDigetherRunAdvanceResult Result;
		if (!FMath::IsFinite(ServerSeconds) || ServerSeconds < 0.0
			|| ServerSeconds < State.StartedAtSeconds || ServerSeconds < State.LastUpdatedAtSeconds)
		{
			return Result;
		}

		const double ElapsedSeconds = State.bClockStarted ? ServerSeconds - State.StartedAtSeconds : 0.0;
		const FDigetherRunTimeResult Time = FDigetherRunRules::CalculateRemainingTime(
			State.Tuning.RunSeconds, ElapsedSeconds,
			State.ConfirmedAscentCount, State.Tuning.AscentPenaltySeconds);
		if (Time.Result != EDigetherRunResult::Success)
		{
			return Result;
		}
		const FDigetherRunAftershockResult Aftershock = FDigetherRunRules::CalculateAftershock(
			State.Tuning.RunSeconds, Time.RemainingSeconds, State.RuleTuning);
		if (Aftershock.Result != EDigetherRunResult::Success)
		{
			return Result;
		}

		State.LastUpdatedAtSeconds = ServerSeconds;
		State.RemainingSeconds = Time.RemainingSeconds;
		State.AftershockStage = Aftershock.Stage;
		Result.Result = EDigetherRunResult::Success;
		Result.bTimeExpiredNow = Time.bExpired && !State.bTimeExpiredNotified;
		State.bTimeExpiredNotified |= Time.bExpired;
		return Result;
	}

	EDigetherRunResult CheckInProgress(const FDigetherRunCoordinatorState& State)
	{
		if (State.Phase == EDigetherRunPhase::Ended)
		{
			return EDigetherRunResult::AlreadyEnded;
		}
		return State.Phase == EDigetherRunPhase::InProgress
			? EDigetherRunResult::Success : EDigetherRunResult::NotInProgress;
	}
}

FDigetherRunAdvanceResult FDigetherRunCoordinatorRules::StartRun(FDigetherRunCoordinatorState& State,
	const FDigetherRunTuning& Tuning, const FDigetherRunRuleTuning& RuleTuning, const double ServerSeconds,
	const bool bStartClock)
{
	if (State.Phase == EDigetherRunPhase::InProgress)
	{
		FDigetherRunAdvanceResult Result;
		Result.Result = EDigetherRunResult::AlreadyInProgress;
		return Result;
	}
	if (!FMath::IsFinite(ServerSeconds) || ServerSeconds < 0.0)
	{
		return FDigetherRunAdvanceResult();
	}

	FDigetherRunCoordinatorState Candidate;
	Candidate.Tuning = Tuning;
	Candidate.RuleTuning = RuleTuning;
	Candidate.Phase = EDigetherRunPhase::InProgress;
	Candidate.StartedAtSeconds = ServerSeconds;
	Candidate.LastUpdatedAtSeconds = ServerSeconds;
	Candidate.bClockStarted = bStartClock;
	const FDigetherRunAdvanceResult Result = AdvanceCandidate(Candidate, ServerSeconds);
	if (Result.Result == EDigetherRunResult::Success)
	{
		State = Candidate;
	}
	return Result;
}

FDigetherRunAdvanceResult FDigetherRunCoordinatorRules::StartClock(FDigetherRunCoordinatorState& State,
	const double ServerSeconds)
{
	FDigetherRunAdvanceResult Result;
	Result.Result = CheckInProgress(State);
	if (Result.Result != EDigetherRunResult::Success)
	{
		return Result;
	}
	if (State.bClockStarted)
	{
		Result.Result = EDigetherRunResult::AlreadyInProgress;
		return Result;
	}
	if (!FMath::IsFinite(ServerSeconds) || ServerSeconds < State.LastUpdatedAtSeconds)
	{
		return FDigetherRunAdvanceResult();
	}
	// 출발부터 도착까지 확정된 상승 페널티가 없으므로 도착 시각을 새 기준점으로 사용
	FDigetherRunCoordinatorState Candidate = State;
	Candidate.StartedAtSeconds = ServerSeconds;
	Candidate.bClockStarted = true;
	Result = AdvanceCandidate(Candidate, ServerSeconds);
	if (Result.Result == EDigetherRunResult::Success)
	{
		State = Candidate;
	}
	return Result;
}

FDigetherRunAdvanceResult FDigetherRunCoordinatorRules::RecordConfirmedAscent(FDigetherRunCoordinatorState& State)
{
	FDigetherRunAdvanceResult Result;
	Result.Result = CheckInProgress(State);
	if (Result.Result != EDigetherRunResult::Success)
	{
		return Result;
	}
	if (State.ConfirmedAscentCount < 0 || State.ConfirmedAscentCount == MAX_int32)
	{
		Result.Result = EDigetherRunResult::InvalidInput;
		return Result;
	}
	FDigetherRunCoordinatorState Candidate = State;
	++Candidate.ConfirmedAscentCount;
	Result = AdvanceCandidate(Candidate, Candidate.LastUpdatedAtSeconds);
	if (Result.Result == EDigetherRunResult::Success)
	{
		State = Candidate;
	}
	return Result;
}

FDigetherRunAdvanceResult FDigetherRunCoordinatorRules::UpdateTime(
	FDigetherRunCoordinatorState& State, const double ServerSeconds)
{
	FDigetherRunAdvanceResult Result;
	Result.Result = CheckInProgress(State);
	if (Result.Result != EDigetherRunResult::Success)
	{
		return Result;
	}
	FDigetherRunCoordinatorState Candidate = State;
#if !UE_BUILD_SHIPPING
	if (Candidate.bDevClockPaused)
	{
		DevHoldClock(Candidate, ServerSeconds);
	}
#endif
	Result = AdvanceCandidate(Candidate, ServerSeconds);
	if (Result.Result == EDigetherRunResult::Success)
	{
		State = Candidate;
	}
	return Result;
}

FDigetherRunTerminationResult FDigetherRunCoordinatorRules::CompleteRun(
	FDigetherRunCoordinatorState& State, const FDigetherRunTerminationInput& Input)
{
	FDigetherRunTerminationResult Result;
	Result.Result = CheckInProgress(State);
	if (Result.Result != EDigetherRunResult::Success)
	{
		return Result;
	}
	Result = FDigetherRunRules::CalculateTermination(Input);
	if (Result.Result == EDigetherRunResult::Success)
	{
		State.Phase = EDigetherRunPhase::Ended;
	}
	return Result;
}

FDigetherRunPublicState FDigetherRunCoordinatorRules::MakePublicState(
	const FDigetherRunCoordinatorState& State, const bool bGameOver, const bool bPaymentDeadline)
{
	FDigetherRunPublicState Public;
	Public.Phase = State.Phase;
	Public.AftershockStage = State.AftershockStage;
	Public.bGameOver = bGameOver;
	Public.bPaymentDeadline = bPaymentDeadline;
	return Public;
}

#if !UE_BUILD_SHIPPING
double FDigetherRunCoordinatorRules::DevGetMaxRemainingSeconds(const FDigetherRunCoordinatorState& State)
{
	const double PenaltySeconds = static_cast<double>(FMath::Max(0, State.ConfirmedAscentCount))
		* FMath::Max(0.0, static_cast<double>(State.Tuning.AscentPenaltySeconds));
	return FMath::Max(0.0, static_cast<double>(State.Tuning.RunSeconds) - PenaltySeconds);
}

FDigetherRunAdvanceResult FDigetherRunCoordinatorRules::DevSetRemainingSeconds(FDigetherRunCoordinatorState& State,
	const double RemainingSeconds, const double ServerSeconds)
{
	FDigetherRunAdvanceResult Result;
	Result.Result = CheckInProgress(State);
	if (Result.Result != EDigetherRunResult::Success)
	{
		return Result;
	}
	const double MaxRemainingSeconds = DevGetMaxRemainingSeconds(State);
	if (!State.bClockStarted || State.bTimeExpiredNotified || !FMath::IsFinite(RemainingSeconds)
		|| RemainingSeconds < 0.0 || RemainingSeconds > MaxRemainingSeconds
		|| !FMath::IsFinite(ServerSeconds) || ServerSeconds < State.LastUpdatedAtSeconds)
	{
		return FDigetherRunAdvanceResult();
	}
	// 남은 시간 = 최대 - 경과, 0은 부동소수 오차로 양수가 남지 않게 탐사 시간 전체를 넘겨 확실히 만료
	constexpr double ExpireMarginSeconds = 1.0;
	const double ElapsedSeconds = RemainingSeconds <= 0.0
		? static_cast<double>(State.Tuning.RunSeconds) + ExpireMarginSeconds
		: MaxRemainingSeconds - RemainingSeconds;
	FDigetherRunCoordinatorState Candidate = State;
	Candidate.StartedAtSeconds = ServerSeconds - ElapsedSeconds;
	Result = AdvanceCandidate(Candidate, ServerSeconds);
	if (Result.Result == EDigetherRunResult::Success)
	{
		State = Candidate;
	}
	return Result;
}

void FDigetherRunCoordinatorRules::DevHoldClock(FDigetherRunCoordinatorState& State, const double ServerSeconds)
{
	if (!State.bClockStarted || !FMath::IsFinite(ServerSeconds) || ServerSeconds <= State.LastUpdatedAtSeconds)
	{
		return;
	}
	const double PausedSeconds = ServerSeconds - State.LastUpdatedAtSeconds;
	// 부동소수 오차로 기준 시각이 ServerSeconds를 넘어 단조성 검사에 막히지 않게 상한 고정
	State.StartedAtSeconds = FMath::Min(State.StartedAtSeconds + PausedSeconds, ServerSeconds);
	State.LastUpdatedAtSeconds = ServerSeconds;
}
#endif
