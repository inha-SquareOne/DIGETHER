#include "Game/Run/DigetherRunRules.h"

#include "Game/Economy/DigetherEconomyTypes.h"
#include "Game/Run/DigetherRunRuleTuning.h"

namespace
{
	bool IsValidLife(const EDigetherLifeState Life)
	{
		return Life == EDigetherLifeState::Active || Life == EDigetherLifeState::Dead
			|| Life == EDigetherLifeState::Escaped;
	}

	bool IsValidLocation(const EDigetherRunPlayerLocation Location)
	{
		return Location == EDigetherRunPlayerLocation::Elevator
			|| Location == EDigetherRunPlayerLocation::MineOrDungeon || Location == EDigetherRunPlayerLocation::Base;
	}

	bool IsValidTerminationInput(const FDigetherRunTerminationInput& Input)
	{
		if (Input.Trigger != EDigetherRunTerminationTrigger::TimeExpired
			&& Input.Trigger != EDigetherRunTerminationTrigger::FinalAscentCompleted)
		{
			return false;
		}
		if (Input.ElevatorState != EDigetherRunElevatorState::StoppedInMine
			&& Input.ElevatorState != EDigetherRunElevatorState::InTransit
			&& Input.ElevatorState != EDigetherRunElevatorState::AtBase)
		{
			return false;
		}

		TSet<FString> PlayerKeys;
		for (const FDigetherRunPlayerInput& Player : Input.Players)
		{
			if (Player.PlayerKey.TrimStartAndEnd().IsEmpty() || PlayerKeys.Contains(Player.PlayerKey)
				|| !IsValidLife(Player.Life) || !IsValidLocation(Player.Location))
			{
				return false;
			}
			PlayerKeys.Add(Player.PlayerKey);
		}
		return true;
	}
}

bool FDigetherRunRules::IsValidPublicState(const FDigetherRunPublicState& State)
{
	return (State.Phase == EDigetherRunPhase::NotStarted || State.Phase == EDigetherRunPhase::InProgress
		|| State.Phase == EDigetherRunPhase::Ended) && State.AftershockStage >= 0;
}

bool FDigetherRunRules::IsValidRuleTuning(const FDigetherRunRuleTuning& Tuning)
{
	return Tuning.AftershockStageCount > 0 && FMath::IsFinite(Tuning.AftershockMaxIntervalSeconds)
		&& FMath::IsFinite(Tuning.AftershockMinIntervalSeconds) && Tuning.AftershockMinIntervalSeconds > 0.0
		&& Tuning.AftershockMaxIntervalSeconds >= Tuning.AftershockMinIntervalSeconds;
}

FDigetherRunTimeResult FDigetherRunRules::CalculateRemainingTime(const double RunSeconds,
	const double ElapsedSeconds, const int32 ConfirmedAscentCount, const double AscentPenaltySeconds)
{
	FDigetherRunTimeResult Result;
	if (!FMath::IsFinite(RunSeconds) || !FMath::IsFinite(ElapsedSeconds)
		|| !FMath::IsFinite(AscentPenaltySeconds) || RunSeconds < 0.0 || ElapsedSeconds < 0.0
		|| ConfirmedAscentCount < 0 || AscentPenaltySeconds < 0.0)
	{
		return Result;
	}

	Result.Result = EDigetherRunResult::Success;
	const double BeforePenaltySeconds = FMath::Max(0.0, RunSeconds - ElapsedSeconds);
	// 확실히 초과할 때만 먼저 만료시켜 나눗셈 반올림이 양수 잔여 시간을 없애지 않게 한다.
	if (ConfirmedAscentCount > 0 && AscentPenaltySeconds > 0.0
		&& AscentPenaltySeconds > BeforePenaltySeconds / ConfirmedAscentCount)
	{
		Result.RemainingSeconds = 0.0;
	}
	else
	{
		Result.RemainingSeconds = FMath::Max(0.0, BeforePenaltySeconds - ConfirmedAscentCount * AscentPenaltySeconds);
	}
	Result.bExpired = Result.RemainingSeconds == 0.0;
	return Result;
}

FDigetherRunAftershockResult FDigetherRunRules::CalculateAftershock(const double RunSeconds,
	const double RemainingSeconds, const FDigetherRunRuleTuning& Tuning)
{
	FDigetherRunAftershockResult Result;
	if (!IsValidRuleTuning(Tuning) || !FMath::IsFinite(RunSeconds) || !FMath::IsFinite(RemainingSeconds)
		|| RunSeconds <= 0.0 || RemainingSeconds < 0.0)
	{
		return Result;
	}

	const double RemainingRatio = FMath::Clamp(RemainingSeconds / RunSeconds, 0.0, 1.0);
	const double ProgressRatio = 1.0 - RemainingRatio;
	// 만료 시 마지막 단계를 명시해 부동소수점 오차로 한 단계 부족해지지 않게 한다.
	Result.Stage = RemainingRatio == 0.0 ? Tuning.AftershockStageCount
		: FMath::FloorToInt32(ProgressRatio * Tuning.AftershockStageCount);
	Result.IntervalSeconds = Tuning.AftershockMinIntervalSeconds
		+ (Tuning.AftershockMaxIntervalSeconds - Tuning.AftershockMinIntervalSeconds) * RemainingRatio;
	Result.Result = EDigetherRunResult::Success;
	return Result;
}

FDigetherRunTerminationResult FDigetherRunRules::CalculateTermination(const FDigetherRunTerminationInput& Input)
{
	FDigetherRunTerminationResult Result;
	if (!IsValidTerminationInput(Input))
	{
		return Result;
	}

	const bool bEjectFromMine = Input.ElevatorState == EDigetherRunElevatorState::StoppedInMine
		&& Input.bElevatorOverweight;
	if (bEjectFromMine)
	{
		Result.ElevatorAction = EDigetherRunElevatorAction::EmptyAscentAfterEjection;
		Result.bEjectCargo = true;
	}
	else if (Input.ElevatorState == EDigetherRunElevatorState::StoppedInMine)
	{
		Result.ElevatorAction = EDigetherRunElevatorAction::NormalReturn;
	}

	Result.Players.Reserve(Input.Players.Num());
	for (const FDigetherRunPlayerInput& Player : Input.Players)
	{
		FDigetherRunPlayerOutcome Outcome;
		Outcome.PlayerKey = Player.PlayerKey;
		if (Player.Life == EDigetherLifeState::Active)
		{
			if (Player.Location == EDigetherRunPlayerLocation::Base)
			{
				Outcome.Outcome = EDigetherRunPlayerOutcome::Returned;
			}
			else if (Player.Location == EDigetherRunPlayerLocation::MineOrDungeon)
			{
				Outcome.Outcome = EDigetherRunPlayerOutcome::MissingDeath;
				Outcome.DeathCause = Input.Trigger == EDigetherRunTerminationTrigger::TimeExpired
					? EDigetherDeathCause::CollapseTimeExpired
					: EDigetherDeathCause::CollapseRemainedAfterFinalAscent;
			}
			else if (Input.ElevatorState == EDigetherRunElevatorState::InTransit)
			{
				// 이미 이동 중인 탑승자의 생존이 과중량 판정보다 우선한다.
				Outcome.Outcome = EDigetherRunPlayerOutcome::SurvivedInTransit;
			}
			else if (bEjectFromMine)
			{
				Outcome.Outcome = EDigetherRunPlayerOutcome::MissingDeath;
				Outcome.DeathCause = EDigetherDeathCause::CollapseEjected;
			}
			else
			{
				// 거점 도착 후 과중량은 다시 사출하지 않는다. 광산 정지 상태만 사출 대상이다.
				Outcome.Outcome = EDigetherRunPlayerOutcome::Returned;
			}
		}
		Result.Players.Add(Outcome);
	}
	Result.Result = EDigetherRunResult::Success;
	return Result;
}

FDigetherRunQuotaResult FDigetherRunRules::EvaluateQuota(const FDigetherQuotaState& QuotaState)
{
	FDigetherRunQuotaResult Result;
	if (QuotaState.Day < 1 || QuotaState.TargetCredits < 0 || QuotaState.DepositedCredits < 0
		|| QuotaState.DepositedCredits > QuotaState.TargetCredits || QuotaState.PeriodGeneration < 0)
	{
		return Result;
	}

	Result.bPaymentDeadline = QuotaState.Day == DigetherRunRules::PaymentDeadlineDay;
	Result.bGameOver = QuotaState.Day > DigetherRunRules::PaymentDeadlineDay
		&& QuotaState.DepositedCredits < QuotaState.TargetCredits;
	Result.Result = EDigetherRunResult::Success;
	return Result;
}
