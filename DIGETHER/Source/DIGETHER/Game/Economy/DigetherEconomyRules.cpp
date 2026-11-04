#include "Game/Economy/DigetherEconomyRules.h"

#include "Game/Data/DigetherTuningProfile.h"
#include "Game/Economy/DigetherEconomyTuning.h"

namespace
{
	constexpr int32 PercentDenominator = 100;
	constexpr int32 FirstDay = 1;

	int32 ClampCredits(int64 Credits)
	{
		return static_cast<int32>(FMath::Clamp<int64>(Credits, 0, MAX_int32));
	}

	int32 CalculatePercent(int32 Credits, int32 Percent)
	{
		const int64 NonNegativeCredits = FMath::Max(0, Credits);
		const int32 ClampedPercent = FMath::Clamp(Percent, 0, PercentDenominator);
		return static_cast<int32>(NonNegativeCredits * ClampedPercent / PercentDenominator);
	}
}

int32 FDigetherEconomyRules::AddCredits(int32 LeftCredits, int32 RightCredits)
{
	return ClampCredits(static_cast<int64>(FMath::Max(0, LeftCredits)) + FMath::Max(0, RightCredits));
}

bool FDigetherEconomyRules::IsValidWalletState(const FDigetherWalletState& State)
{
	return State.BalanceCredits >= 0 && State.LastRunPayoutCredits >= 0 && State.HiddenLifetimeCredits >= 0
		&& State.TotalDeathLossCredits >= 0 && State.LastPreDeathBalanceCredits >= 0;
}

bool FDigetherEconomyRules::IsValidQuotaState(const FDigetherQuotaState& State)
{
	return State.Day >= 0 && State.TargetCredits >= 0 && State.DepositedCredits >= 0
		&& State.DepositedCredits <= State.TargetCredits && State.PeriodGeneration >= 0;
}

bool FDigetherEconomyRules::IsValidPublicState(const FDigetherQuotaPublicState& State)
{
	if (!IsValidQuotaState(State.Quota))
	{
		return false;
	}

	TSet<FString> PlayerKeys;
	for (const FDigetherHiddenRevealEntry& Entry : State.HiddenReveals)
	{
		if (Entry.PlayerKey.IsEmpty() || Entry.HiddenLifetimeCredits < 0 || PlayerKeys.Contains(Entry.PlayerKey))
		{
			return false;
		}
		PlayerKeys.Add(Entry.PlayerKey);
	}
	return true;
}

EDigetherEconomyResult FDigetherEconomyRules::GetDepositEfficiencyPercent(const int32 Day,
	const FDigetherEconomyTuning& Tuning, int32& OutPercent)
{
	OutPercent = 0;
	int32 Percent = 0;
	switch (Day)
	{
	case 1:
		Percent = Tuning.DayOneEfficiencyPercent;
		break;
	case 2:
		Percent = Tuning.DayTwoEfficiencyPercent;
		break;
	case 3:
		Percent = Tuning.DayThreeEfficiencyPercent;
		break;
	case 4:
		Percent = Tuning.DayFourEfficiencyPercent;
		break;
	default:
		return EDigetherEconomyResult::InvalidInput;
	}
	if (Percent < 1 || Percent > PercentDenominator)
	{
		return EDigetherEconomyResult::InvalidInput;
	}
	OutPercent = Percent;
	return EDigetherEconomyResult::Success;
}

FDigetherQuotaState FDigetherEconomyRules::MakeInitialQuotaState(const FDigetherRunTuning& RunTuning)
{
	FDigetherQuotaState State;
	State.Day = FirstDay;
	State.TargetCredits = FMath::Max(0, RunTuning.InitialQuotaCredits);
	State.DepositedCredits = 0;
	State.PeriodGeneration = 0;
	return State;
}

int32 FDigetherEconomyRules::CalculateNextTargetCredits(const int32 CurrentTargetCredits,
	const FDigetherEconomyTuning& Tuning)
{
	return AddCredits(CurrentTargetCredits, Tuning.TargetIncreaseCredits);
}

FDigetherDeathLossResult FDigetherEconomyRules::CalculateDeathLoss(int32 BalanceCredits, int32 DeathLossPercent)
{
	FDigetherDeathLossResult Result;
	Result.PreDeathBalanceCredits = FMath::Max(0, BalanceCredits);
	Result.LossCredits = CalculatePercent(Result.PreDeathBalanceCredits, DeathLossPercent);
	Result.RemainingBalanceCredits = ClampCredits(static_cast<int64>(Result.PreDeathBalanceCredits) - Result.LossCredits);
	return Result;
}

int32 FDigetherEconomyRules::CalculateCorpseRefund(int32 PreDeathBalanceCredits, int32 CorpseRefundPercent)
{
	return CalculatePercent(PreDeathBalanceCredits, CorpseRefundPercent);
}

FDigetherRunSettlementResult FDigetherEconomyRules::CalculateRunSettlement(int32 GeneralCargoCredits,
	int32 DepartureCount, const TArray<FDigetherPlayerSettlementInput>& Players, int32 CorpseRefundPercent)
{
	FDigetherRunSettlementResult Result;
	if (DepartureCount < 0 || Players.Num() > DepartureCount)
	{
		return Result;
	}

	int32 SurvivorCount = 0;
	for (const FDigetherPlayerSettlementInput& Player : Players)
	{
		if ((Player.bSurvived && Player.bDied) || (Player.bCorpseRecovered && !Player.bDied))
		{
			return Result;
		}
		if (Player.bSurvived)
		{
			++SurvivorCount;
		}
	}

	const int32 CargoCredits = FMath::Max(0, GeneralCargoCredits);
	// 전멸·출발 0일 때 몫 0. 살아 돌아온 인원은 분모를 바꾸지 않는다.
	if (DepartureCount > 0 && SurvivorCount > 0)
	{
		Result.GeneralShareCredits = static_cast<int32>(static_cast<int64>(CargoCredits) / DepartureCount);
	}
	Result.LostGeneralCredits = ClampCredits(static_cast<int64>(CargoCredits)
		- static_cast<int64>(Result.GeneralShareCredits) * SurvivorCount);

	Result.Players.Reserve(Players.Num());
	for (const FDigetherPlayerSettlementInput& Player : Players)
	{
		FDigetherPlayerSettlement Payment;
		if (Player.bSurvived)
		{
			Payment.GeneralCredits = Result.GeneralShareCredits;
			Payment.HiddenCredits = FMath::Max(0, Player.ReturnedHiddenCredits);
			Payment.PayoutCredits = AddCredits(Payment.GeneralCredits, Payment.HiddenCredits);
		}
		else if (Player.bDied && Player.bCorpseRecovered && SurvivorCount > 0)
		{
			Payment.CorpseRefundCredits = CalculateCorpseRefund(Player.PreDeathBalanceCredits, CorpseRefundPercent);
		}
		Result.Players.Add(Payment);
	}
	Result.Result = EDigetherEconomyResult::Success;
	return Result;
}

FDigetherWalletUpdateResult FDigetherEconomyRules::ApplyDeathLoss(const FDigetherWalletState& State,
	int32 DeathLossPercent)
{
	FDigetherWalletUpdateResult Result;
	Result.State = State;
	if (!IsValidWalletState(State))
	{
		return Result;
	}

	const FDigetherDeathLossResult Loss = CalculateDeathLoss(State.BalanceCredits, DeathLossPercent);
	Result.State.BalanceCredits = Loss.RemainingBalanceCredits;
	Result.State.LastPreDeathBalanceCredits = Loss.PreDeathBalanceCredits;
	Result.State.TotalDeathLossCredits = AddCredits(State.TotalDeathLossCredits, Loss.LossCredits);
	Result.Result = EDigetherEconomyResult::Success;
	return Result;
}

FDigetherWalletUpdateResult FDigetherEconomyRules::ApplyRunSettlement(const FDigetherWalletState& State,
	const FDigetherPlayerSettlement& Settlement)
{
	FDigetherWalletUpdateResult Result;
	Result.State = State;
	if (!IsValidWalletState(State) || Settlement.GeneralCredits < 0 || Settlement.HiddenCredits < 0
		|| Settlement.PayoutCredits < 0 || Settlement.CorpseRefundCredits < 0
		|| Settlement.PayoutCredits != AddCredits(Settlement.GeneralCredits, Settlement.HiddenCredits)
		|| (Settlement.PayoutCredits > 0 && Settlement.CorpseRefundCredits > 0))
	{
		return Result;
	}

	Result.State.BalanceCredits = AddCredits(AddCredits(State.BalanceCredits, Settlement.PayoutCredits),
		Settlement.CorpseRefundCredits);
	Result.State.LastRunPayoutCredits = Settlement.PayoutCredits;
	Result.State.HiddenLifetimeCredits = AddCredits(State.HiddenLifetimeCredits, Settlement.HiddenCredits);
	Result.Result = EDigetherEconomyResult::Success;
	return Result;
}

FDigetherDepositResult FDigetherEconomyRules::CalculateDeposit(const FDigetherWalletState& WalletState,
	const FDigetherQuotaState& QuotaState, const int32 RequestedCredits, const FDigetherEconomyTuning& Tuning,
	const bool bDepositClosed)
{
	FDigetherDepositResult Result;
	Result.WalletState = WalletState;
	Result.QuotaState = QuotaState;
	if (!IsValidWalletState(WalletState) || !IsValidQuotaState(QuotaState))
	{
		return Result;
	}
	int32 EfficiencyPercent = 0;
	if (GetDepositEfficiencyPercent(QuotaState.Day, Tuning, EfficiencyPercent) != EDigetherEconomyResult::Success
		|| Tuning.TargetIncreaseCredits < 0)
	{
		return Result;
	}
	if (bDepositClosed)
	{
		Result.Result = EDigetherEconomyResult::DepositClosed;
		return Result;
	}
	if (RequestedCredits <= 0)
	{
		Result.Result = EDigetherEconomyResult::InvalidAmount;
		return Result;
	}
	// 목표 잔여액으로 줄이기 전에 요청액 자체의 잔고 초과를 거절한다.
	if (RequestedCredits > WalletState.BalanceCredits)
	{
		Result.Result = EDigetherEconomyResult::InsufficientFunds;
		return Result;
	}

	const int32 RemainingCredits = static_cast<int32>(static_cast<int64>(QuotaState.TargetCredits)
		- QuotaState.DepositedCredits);
	if (RemainingCredits <= 0)
	{
		Result.Result = EDigetherEconomyResult::QuotaAlreadySatisfied;
		return Result;
	}

	const int64 RequestedCreditedCredits = static_cast<int64>(RequestedCredits) * EfficiencyPercent / PercentDenominator;
	if (RequestedCreditedCredits == 0)
	{
		Result.Result = EDigetherEconomyResult::DepositCreditTooSmall;
		return Result;
	}

	int32 AcceptedCredits = RequestedCredits;
	if (RequestedCreditedCredits > RemainingCredits)
	{
		// 요청 반영액이 잔여 목표를 넘는 경우에만 역산한다. 이때 필요한 차감액은
		// 요청액 이하이므로 int64 올림 결과도 int32 안에 있음을 보장한다.
		const int64 RequiredCredits = (static_cast<int64>(RemainingCredits) * PercentDenominator
			+ EfficiencyPercent - 1) / EfficiencyPercent;
		AcceptedCredits = static_cast<int32>(RequiredCredits);
	}
	const int32 CreditedCredits = static_cast<int32>(FMath::Min<int64>(
		static_cast<int64>(AcceptedCredits) * EfficiencyPercent / PercentDenominator, RemainingCredits));
	const bool bTargetCompleted = CreditedCredits == RemainingCredits;
	// 세대를 재사용하면 이전 기간의 요청과 구별할 수 없다. 변경 전에 거절한다.
	if (bTargetCompleted && QuotaState.PeriodGeneration == MAX_int32)
	{
		return Result;
	}

	Result.AcceptedCredits = AcceptedCredits;
	Result.CreditedCredits = CreditedCredits;
	Result.bTargetCompleted = bTargetCompleted;
	Result.WalletState.BalanceCredits = ClampCredits(static_cast<int64>(WalletState.BalanceCredits) - AcceptedCredits);
	Result.QuotaState.DepositedCredits = AddCredits(QuotaState.DepositedCredits, CreditedCredits);
	if (bTargetCompleted)
	{
		Result.QuotaState.TargetCredits = CalculateNextTargetCredits(QuotaState.TargetCredits, Tuning);
		Result.QuotaState.DepositedCredits = 0;
		Result.QuotaState.Day = FirstDay;
		Result.QuotaState.PeriodGeneration = static_cast<int32>(static_cast<int64>(QuotaState.PeriodGeneration) + 1);
	}
	Result.Result = EDigetherEconomyResult::Success;
	return Result;
}

FDigetherDayAdvanceResult FDigetherEconomyRules::AdvanceDay(const FDigetherQuotaState& State)
{
	FDigetherDayAdvanceResult Result;
	Result.State = State;
	if (!IsValidQuotaState(State) || State.Day < FirstDay)
	{
		return Result;
	}
	// 게임상 일차 상한·게임 오버는 판정하지 않고 자료형 표현 상한만 포화한다.
	Result.State.Day = ClampCredits(static_cast<int64>(State.Day) + 1);
	Result.Result = EDigetherEconomyResult::Success;
	return Result;
}
