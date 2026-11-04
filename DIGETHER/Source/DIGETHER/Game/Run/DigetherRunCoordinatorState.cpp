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

		const FDigetherRunTimeResult Time = FDigetherRunRules::CalculateRemainingTime(
			State.Tuning.RunSeconds, ServerSeconds - State.StartedAtSeconds,
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
	const FDigetherRunTuning& Tuning, const FDigetherRunRuleTuning& RuleTuning, const double ServerSeconds)
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
	const FDigetherRunAdvanceResult Result = AdvanceCandidate(Candidate, ServerSeconds);
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
