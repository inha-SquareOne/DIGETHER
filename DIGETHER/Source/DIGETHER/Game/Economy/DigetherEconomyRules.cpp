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
		&& State.TotalDeathLossCredits >= 0 && State.LastPreDeathBalanceCredits >= 0
		&& State.LifetimeEarnedCredits >= 0 && State.LifetimeCorpseRefundCredits >= 0;
}

bool FDigetherEconomyRules::IsValidQuotaState(const FDigetherQuotaState& State)
{
	// 전체·기간 일차 관계(전체 >= 기간)는 비강제, 값 형태만 검사(헤더 주석 참고)
	return State.Day >= 0 && State.TargetCredits >= 0 && State.DepositedCredits >= 0
		&& State.DepositedCredits <= State.TargetCredits && State.PeriodGeneration >= 0 && State.TotalDay >= 0;
}

bool FDigetherEconomyRules::IsValidPublicState(const FDigetherQuotaPublicState& State)
{
	if (!IsValidQuotaState(State.Quota))
	{
		return false;
	}

	TSet<FString> PlayerKeys;
	PlayerKeys.Reserve(State.HiddenReveals.Num());
	for (const FDigetherHiddenRevealEntry& Entry : State.HiddenReveals)
	{
		if (Entry.PlayerKey.IsEmpty() || Entry.HiddenLifetimeCredits < 0)
		{
			return false;
		}
		bool bDuplicateKey = false;
		PlayerKeys.Add(Entry.PlayerKey, &bDuplicateKey);
		if (bDuplicateKey)
		{
			return false;
		}
	}
	return true;
}

bool FDigetherEconomyRules::IsValidTuning(const FDigetherEconomyTuning& Tuning)
{
	const int32 Percentages[] = { Tuning.DayOneEfficiencyPercent, Tuning.DayTwoEfficiencyPercent,
		Tuning.DayThreeEfficiencyPercent, Tuning.DayFourEfficiencyPercent };
	for (const int32 Percent : Percentages)
	{
		if (Percent < 1 || Percent > PercentDenominator)
		{
			return false;
		}
	}
	return Tuning.TargetIncreaseCredits >= 0 && FMath::IsFinite(Tuning.DepositSessionLifetimeSeconds)
		&& Tuning.DepositSessionLifetimeSeconds > 0.0 && Tuning.PourUnitCredits >= 1
		&& FMath::IsFinite(Tuning.PaymentSignalOrangeRatio) && FMath::IsFinite(Tuning.PaymentSignalGreenRatio)
		&& Tuning.PaymentSignalOrangeRatio > 0.0 && Tuning.PaymentSignalOrangeRatio < Tuning.PaymentSignalGreenRatio
		&& Tuning.PaymentSignalGreenRatio <= 1.0;
}

EDigetherPaymentSignal FDigetherEconomyRules::CalculatePaymentSignal(const FDigetherQuotaState& State,
	const FDigetherEconomyTuning& Tuning)
{
	if (!IsValidQuotaState(State) || State.Day <= 0 || State.TargetCredits <= 0 || !IsValidTuning(Tuning))
	{
		return EDigetherPaymentSignal::None;
	}
	const double Ratio = static_cast<double>(State.DepositedCredits) / static_cast<double>(State.TargetCredits);
	if (Ratio >= Tuning.PaymentSignalGreenRatio)
	{
		return EDigetherPaymentSignal::Green;
	}
	return Ratio >= Tuning.PaymentSignalOrangeRatio ? EDigetherPaymentSignal::Orange : EDigetherPaymentSignal::Red;
}

FDigetherQuotaReplicatedState FDigetherEconomyRules::MakeReplicatedQuotaState(const FDigetherQuotaPublicState& State,
	const FDigetherEconomyTuning& Tuning)
{
	FDigetherQuotaReplicatedState Replicated;
	Replicated.Day = State.Quota.Day;
	Replicated.TargetCredits = State.Quota.TargetCredits;
	Replicated.PeriodGeneration = State.Quota.PeriodGeneration;
	Replicated.PaymentSignal = CalculatePaymentSignal(State.Quota, Tuning);
	Replicated.TotalDay = State.Quota.TotalDay;
	Replicated.HiddenReveals = State.HiddenReveals;
	return Replicated;
}

FDigetherQuotaPublicState FDigetherEconomyRules::MakeClientQuotaPublicState(const FDigetherQuotaReplicatedState& State)
{
	FDigetherQuotaPublicState Public;
	Public.Quota.Day = State.Day;
	Public.Quota.TargetCredits = State.TargetCredits;
	// 클라이언트는 정확한 누적 인정액 미수신, 진행도는 PaymentSignal로만 표시
	Public.Quota.DepositedCredits = 0;
	Public.Quota.PeriodGeneration = State.PeriodGeneration;
	Public.Quota.TotalDay = State.TotalDay;
	Public.HiddenReveals = State.HiddenReveals;
	return Public;
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
	// 새 게임은 전체·기간 모두 1일차
	State.TotalDay = FirstDay;
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
	// 전멸·출발 0이면 몫 0, 분모는 생존 인원이 아닌 출발 인원
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
		else if (Player.bDied && Player.bCorpseRecovered)
		{
			// D25 확정: 전멸(생존 귀환 0명)이어도 유효하게 회수된 시체의 복구금 지급
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
	// 기획 S33: 결과 통계는 지급마다 누적, 복구금은 새로 번 돈이 아니므로 획득액과 따로 누적
	Result.State.LifetimeEarnedCredits = AddCredits(State.LifetimeEarnedCredits, Settlement.PayoutCredits);
	Result.State.LifetimeCorpseRefundCredits = AddCredits(State.LifetimeCorpseRefundCredits,
		Settlement.CorpseRefundCredits);
	Result.Result = EDigetherEconomyResult::Success;
	return Result;
}

FDigetherPourTickResult FDigetherEconomyRules::CalculatePourTick(const FDigetherWalletState& WalletState,
	const FDigetherQuotaState& QuotaState, const FDigetherEconomyTuning& Tuning, const bool bDepositClosed)
{
	FDigetherPourTickResult Result;
	Result.WalletState = WalletState;
	Result.QuotaState = QuotaState;
	if (!IsValidWalletState(WalletState) || !IsValidQuotaState(QuotaState))
	{
		return Result;
	}
	int32 EfficiencyPercent = 0;
	if (GetDepositEfficiencyPercent(QuotaState.Day, Tuning, EfficiencyPercent) != EDigetherEconomyResult::Success
		|| Tuning.TargetIncreaseCredits < 0 || Tuning.PourUnitCredits < 1)
	{
		return Result;
	}
	if (bDepositClosed)
	{
		Result.Result = EDigetherEconomyResult::DepositClosed;
		return Result;
	}
	if (WalletState.BalanceCredits <= 0)
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

	const int32 SpentCredits = FMath::Min(Tuning.PourUnitCredits, WalletState.BalanceCredits);
	const int64 CandidateCredits = static_cast<int64>(SpentCredits) * EfficiencyPercent / PercentDenominator;
	if (CandidateCredits == 0)
	{
		// 효율 내림으로 인정 0인 주기는 돈만 사라지므로 차감 없음
		Result.Result = EDigetherEconomyResult::DepositCreditTooSmall;
		return Result;
	}
	const bool bTargetCompleted = CandidateCredits >= RemainingCredits;
	// 세대 재사용 시 이전 기간 붓기와 구별 불가, 변경 전 거절
	if (bTargetCompleted && QuotaState.PeriodGeneration == MAX_int32)
	{
		return Result;
	}
	int32 ConsumedCredits = SpentCredits;
	int32 CreditedCredits = static_cast<int32>(CandidateCredits);
	if (CandidateCredits > RemainingCredits)
	{
		// 남은 목표를 채우는 최소 실제 금액 N = ceil(100R / e), floor(S·e/100) > R이므로 N <= S 보장
		ConsumedCredits = static_cast<int32>((static_cast<int64>(RemainingCredits) * PercentDenominator
			+ EfficiencyPercent - 1) / EfficiencyPercent);
		CreditedCredits = RemainingCredits;
	}

	Result.SpentCredits = SpentCredits;
	Result.CreditedCredits = CreditedCredits;
	Result.ConsumedCredits = ConsumedCredits;
	Result.FloorCredits = SpentCredits - ConsumedCredits;
	Result.EfficiencyPercent = EfficiencyPercent;
	Result.bTargetCompleted = bTargetCompleted;
	Result.WalletState.BalanceCredits = ClampCredits(static_cast<int64>(WalletState.BalanceCredits) - SpentCredits);
	Result.QuotaState.DepositedCredits = AddCredits(QuotaState.DepositedCredits, CreditedCredits);
	if (bTargetCompleted && !TryMakeCompletedQuotaState(QuotaState, Tuning, Result.QuotaState))
	{
		// 위 세대·입력 검사를 통과했으므로 도달 불가, 실패해도 변경 전 거절
		Result = FDigetherPourTickResult();
		Result.WalletState = WalletState;
		Result.QuotaState = QuotaState;
		return Result;
	}
	Result.Result = EDigetherEconomyResult::Success;
	return Result;
}

bool FDigetherEconomyRules::TryMakeCompletedQuotaState(const FDigetherQuotaState& QuotaState,
	const FDigetherEconomyTuning& Tuning, FDigetherQuotaState& OutState)
{
	// 세대 재사용 시 이전 기간 붓기와 구별 불가, 변경 전 거절
	if (!IsValidQuotaState(QuotaState) || Tuning.TargetIncreaseCredits < 0 || QuotaState.PeriodGeneration == MAX_int32)
	{
		return false;
	}
	FDigetherQuotaState Completed = QuotaState;
	Completed.TargetCredits = CalculateNextTargetCredits(QuotaState.TargetCredits, Tuning);
	Completed.DepositedCredits = 0;
	// 새 기간은 기간 일차만 1로 복귀, 탐사 종료가 아니므로 전체 일차 유지(기획 02 개요 5·6행)
	Completed.Day = FirstDay;
	Completed.PeriodGeneration = static_cast<int32>(static_cast<int64>(QuotaState.PeriodGeneration) + 1);
	OutState = Completed;
	return true;
}

FDigetherPourTickResult FDigetherEconomyRules::ReclaimFloorCredits(const FDigetherPourTickResult& Result)
{
	FDigetherPourTickResult Reclaimed = Result;
	if (Result.Result != EDigetherEconomyResult::Success || Result.FloorCredits <= 0)
	{
		return Reclaimed;
	}
	Reclaimed.WalletState.BalanceCredits = AddCredits(Result.WalletState.BalanceCredits, Result.FloorCredits);
	Reclaimed.SpentCredits = Result.ConsumedCredits;
	Reclaimed.FloorCredits = 0;
	return Reclaimed;
}

FDigetherDayAdvanceResult FDigetherEconomyRules::AdvanceDay(const FDigetherQuotaState& State)
{
	FDigetherDayAdvanceResult Result;
	Result.State = State;
	if (!IsValidQuotaState(State) || State.Day < FirstDay)
	{
		return Result;
	}
	// 게임상 일차 상한·게임 오버는 판정하지 않고 자료형 상한만 포화
	// 탐사 종료 확정 1회에 기간·전체 일차 각각 +1(공유 사양 "일차 증가")
	Result.State.Day = ClampCredits(static_cast<int64>(State.Day) + 1);
	Result.State.TotalDay = ClampCredits(static_cast<int64>(State.TotalDay) + 1);
	Result.Result = EDigetherEconomyResult::Success;
	return Result;
}
