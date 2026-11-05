#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Economy/DigetherEconomyTuning.h"
#include "Misc/AutomationTest.h"

// 붓기 한 주기(CalculatePourTick)의 값 계산이다. 금액·목표는 테스트 픽스처이며 기획 값이 아니다.
// 초과 처리는 PQ-34 안 A(임시, D27 확정 전)를 따른다: 지갑 차감 S, 인정 C, 목표에 쓴 금액 N, 바닥 금화 F = S - N.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherEconomyDepositTest,
	"DIGETHER.Game.Economy.Deposit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherEconomyDepositTest::RunTest(const FString& Parameters)
{
	const FDigetherEconomyTuning Tuning;
	TestEqual(TEXT("임시 붓기 기준액"), Tuning.PourUnitCredits, 10);
	FDigetherEconomyTuning InvalidTuning = Tuning;
	InvalidTuning.TargetIncreaseCredits = -1;
	FDigetherEconomyTuning ZeroUnitTuning = Tuning;
	ZeroUnitTuning.PourUnitCredits = 0;
	TestFalse(TEXT("붓기 기준액 0은 잘못된 튜닝"), FDigetherEconomyRules::IsValidTuning(ZeroUnitTuning));
	const auto CheckWallet = [this](const TCHAR* Name, const FDigetherWalletState& Actual, const FDigetherWalletState& Expected)
	{
		TestTrue(Name, Actual.BalanceCredits == Expected.BalanceCredits &&
			Actual.LastRunPayoutCredits == Expected.LastRunPayoutCredits &&
			Actual.HiddenLifetimeCredits == Expected.HiddenLifetimeCredits &&
			Actual.TotalDeathLossCredits == Expected.TotalDeathLossCredits &&
			Actual.LastPreDeathBalanceCredits == Expected.LastPreDeathBalanceCredits);
	};
	const auto CheckQuota = [this](const TCHAR* Name, const FDigetherQuotaState& Actual, const FDigetherQuotaState& Expected)
	{
		TestTrue(Name, Actual.Day == Expected.Day && Actual.TargetCredits == Expected.TargetCredits &&
			Actual.DepositedCredits == Expected.DepositedCredits && Actual.PeriodGeneration == Expected.PeriodGeneration
			&& Actual.TotalDay == Expected.TotalDay);
	};
	const auto CheckRejected = [this, &CheckWallet, &CheckQuota](const TCHAR* Name, const FDigetherPourTickResult& Actual,
		EDigetherEconomyResult ExpectedResult, const FDigetherWalletState& Wallet, const FDigetherQuotaState& Quota)
	{
		TestTrue(FString::Printf(TEXT("%s: 실패 사유"), Name), Actual.Result == ExpectedResult);
		TestEqual(FString::Printf(TEXT("%s: 차감 없음"), Name), Actual.SpentCredits, 0);
		TestEqual(FString::Printf(TEXT("%s: 목표 반영 없음"), Name), Actual.CreditedCredits, 0);
		TestEqual(FString::Printf(TEXT("%s: 바닥 금화 없음"), Name), Actual.FloorCredits, 0);
		TestFalse(FString::Printf(TEXT("%s: 목표 완료 없음"), Name), Actual.bTargetCompleted);
		CheckWallet(TEXT("실패 시 모든 개인 지갑 값 보존"), Actual.WalletState, Wallet);
		CheckQuota(TEXT("실패 시 모든 목표 값 보존"), Actual.QuotaState, Quota);
	};
	const auto CheckAccepted = [this, &CheckWallet, &CheckQuota](const TCHAR* Name, const FDigetherPourTickResult& Actual,
		int32 Spent, int32 Credited, int32 Floor, bool bCompleted, const FDigetherWalletState& Wallet,
		const FDigetherQuotaState& Quota)
	{
		TestTrue(FString::Printf(TEXT("%s: 성공"), Name), Actual.Result == EDigetherEconomyResult::Success);
		TestEqual(FString::Printf(TEXT("%s: 지갑 차감 S"), Name), Actual.SpentCredits, Spent);
		TestEqual(FString::Printf(TEXT("%s: 인정 C"), Name), Actual.CreditedCredits, Credited);
		TestEqual(FString::Printf(TEXT("%s: 바닥 금화 F"), Name), Actual.FloorCredits, Floor);
		TestEqual(FString::Printf(TEXT("%s: N + F = S"), Name), Actual.ConsumedCredits + Actual.FloorCredits, Actual.SpentCredits);
		TestEqual(FString::Printf(TEXT("%s: 목표 완료 여부"), Name), Actual.bTargetCompleted, bCompleted);
		CheckWallet(TEXT("붓기 후 지갑 값"), Actual.WalletState, Wallet);
		CheckQuota(TEXT("붓기 후 목표 값"), Actual.QuotaState, Quota);
	};
	FDigetherWalletState Wallet;
	Wallet.BalanceCredits = 50;
	Wallet.LastRunPayoutCredits = 13;
	Wallet.HiddenLifetimeCredits = 17;
	Wallet.TotalDeathLossCredits = 40;
	Wallet.LastPreDeathBalanceCredits = 42;
	FDigetherQuotaState Quota;
	Quota.Day = 4;
	Quota.TargetCredits = 100;
	Quota.DepositedCredits = 20;
	Quota.PeriodGeneration = 7;
	// 전체 일차는 기간 일차와 다른 값으로 둔다. 목표 달성(새 기간)에도 바뀌지 않아야 한다(기획 02 개요 5·6행).
	Quota.TotalDay = 11;

	FDigetherWalletState ExpectedWallet = Wallet;
	ExpectedWallet.BalanceCredits = 40;
	FDigetherQuotaState ExpectedQuota = Quota;
	ExpectedQuota.DepositedCredits = 30;
	CheckAccepted(TEXT("4일차 한 주기"), FDigetherEconomyRules::CalculatePourTick(Wallet, Quota, Tuning, false),
		10, 10, 0, false, ExpectedWallet, ExpectedQuota);

	FDigetherWalletState SmallWallet = Wallet;
	SmallWallet.BalanceCredits = 7;
	ExpectedWallet = SmallWallet;
	ExpectedWallet.BalanceCredits = 0;
	ExpectedQuota = Quota;
	ExpectedQuota.DepositedCredits = 27;
	CheckAccepted(TEXT("잔고가 기준액보다 적으면 잔고 전부"), FDigetherEconomyRules::CalculatePourTick(SmallWallet, Quota, Tuning, false),
		7, 7, 0, false, ExpectedWallet, ExpectedQuota);

	FDigetherQuotaState ExactTarget = Quota;
	ExactTarget.DepositedCredits = 90;
	ExpectedWallet = Wallet;
	ExpectedWallet.BalanceCredits = 40;
	ExpectedQuota = ExactTarget;
	ExpectedQuota.Day = 1;
	ExpectedQuota.TargetCredits = 200;
	ExpectedQuota.DepositedCredits = 0;
	ExpectedQuota.PeriodGeneration = 8;
	CheckAccepted(TEXT("정확히 남은 목표를 채우면 새 기간"), FDigetherEconomyRules::CalculatePourTick(Wallet, ExactTarget, Tuning, false),
		10, 10, 0, true, ExpectedWallet, ExpectedQuota);

	// 남은 목표 5에 4일차 10원을 부으면 N = 5만 목표에 쓰고, 지갑에서는 10원을 빼며 F = 5가 바닥에 떨어진다.
	FDigetherQuotaState NearTarget = Quota;
	NearTarget.DepositedCredits = 95;
	ExpectedQuota = NearTarget;
	ExpectedQuota.Day = 1;
	ExpectedQuota.TargetCredits = 200;
	ExpectedQuota.DepositedCredits = 0;
	ExpectedQuota.PeriodGeneration = 8;
	const FDigetherPourTickResult Overflow = FDigetherEconomyRules::CalculatePourTick(Wallet, NearTarget, Tuning, false);
	CheckAccepted(TEXT("초과분은 바닥 금화이며 새 목표에 넣지 않음"), Overflow, 10, 5, 5, true, ExpectedWallet, ExpectedQuota);
	TestEqual(TEXT("목표에 쓴 금액 N"), Overflow.ConsumedCredits, 5);

	// 바닥 금화를 만들지 못하면 F를 지갑에 되돌려 차감을 N으로 줄인다(돈 소멸 방지).
	const FDigetherPourTickResult Reclaimed = FDigetherEconomyRules::ReclaimFloorCredits(Overflow);
	ExpectedWallet = Wallet;
	ExpectedWallet.BalanceCredits = 45;
	CheckAccepted(TEXT("바닥 금화 실패 시 차감을 N으로 축소"), Reclaimed, 5, 5, 0, true, ExpectedWallet, ExpectedQuota);
	const FDigetherPourTickResult NoFloor = FDigetherEconomyRules::ReclaimFloorCredits(
		FDigetherEconomyRules::CalculatePourTick(Wallet, Quota, Tuning, false));
	TestEqual(TEXT("바닥 금화가 없으면 되돌릴 것 없음"), NoFloor.WalletState.BalanceCredits, 40);
	const FDigetherPourTickResult FailedReclaim = FDigetherEconomyRules::ReclaimFloorCredits(FDigetherPourTickResult());
	TestTrue(TEXT("실패 결과는 그대로"), FailedReclaim.Result == EDigetherEconomyResult::InvalidInput && FailedReclaim.FloorCredits == 0);

	// 1일차 30%: 10원은 3원 인정이다. 남은 목표 2면 N = ceil(200 / 30) = 7, F = 3이다.
	FDigetherQuotaState DayOneNear = Quota;
	DayOneNear.Day = 1;
	DayOneNear.DepositedCredits = 98;
	const FDigetherPourTickResult DayOneOverflow = FDigetherEconomyRules::CalculatePourTick(Wallet, DayOneNear, Tuning, false);
	TestEqual(TEXT("1일차 초과 인정은 남은 목표"), DayOneOverflow.CreditedCredits, 2);
	TestEqual(TEXT("1일차 초과 N은 올림"), DayOneOverflow.ConsumedCredits, 7);
	TestEqual(TEXT("1일차 초과 F"), DayOneOverflow.FloorCredits, 3);
	TestEqual(TEXT("1일차 효율 기록"), DayOneOverflow.EfficiencyPercent, 30);
	TestTrue(TEXT("1일차 초과 달성"), DayOneOverflow.bTargetCompleted);

	// 효율 내림으로 인정이 0이면 차감 없이 거절한다.
	FDigetherQuotaState DayOne = Quota;
	DayOne.Day = 1;
	FDigetherWalletState TinyWallet = Wallet;
	TinyWallet.BalanceCredits = 3;
	CheckRejected(TEXT("1일차 3원은 인정 0"), FDigetherEconomyRules::CalculatePourTick(TinyWallet, DayOne, Tuning, false),
		EDigetherEconomyResult::DepositCreditTooSmall, TinyWallet, DayOne);

	CheckRejected(TEXT("납부 마감"), FDigetherEconomyRules::CalculatePourTick(Wallet, Quota, Tuning, true),
		EDigetherEconomyResult::DepositClosed, Wallet, Quota);
	FDigetherWalletState EmptyWallet = Wallet;
	EmptyWallet.BalanceCredits = 0;
	CheckRejected(TEXT("마감이 잔고 0보다 우선"), FDigetherEconomyRules::CalculatePourTick(EmptyWallet, Quota, Tuning, true),
		EDigetherEconomyResult::DepositClosed, EmptyWallet, Quota);
	CheckRejected(TEXT("잔고 0"), FDigetherEconomyRules::CalculatePourTick(EmptyWallet, Quota, Tuning, false),
		EDigetherEconomyResult::InsufficientFunds, EmptyWallet, Quota);
	CheckRejected(TEXT("음수 목표 증가량"), FDigetherEconomyRules::CalculatePourTick(Wallet, Quota, InvalidTuning, false),
		EDigetherEconomyResult::InvalidInput, Wallet, Quota);
	CheckRejected(TEXT("붓기 기준액 0"), FDigetherEconomyRules::CalculatePourTick(Wallet, Quota, ZeroUnitTuning, false),
		EDigetherEconomyResult::InvalidInput, Wallet, Quota);

	FDigetherQuotaState Full = Quota;
	Full.DepositedCredits = Full.TargetCredits;
	CheckRejected(TEXT("이미 완료된 목표"), FDigetherEconomyRules::CalculatePourTick(Wallet, Full, Tuning, false),
		EDigetherEconomyResult::QuotaAlreadySatisfied, Wallet, Full);
	FDigetherQuotaState ZeroTarget = Quota;
	ZeroTarget.TargetCredits = 0;
	ZeroTarget.DepositedCredits = 0;
	CheckRejected(TEXT("목표 0은 이미 완료된 목표"), FDigetherEconomyRules::CalculatePourTick(Wallet, ZeroTarget, Tuning, false),
		EDigetherEconomyResult::QuotaAlreadySatisfied, Wallet, ZeroTarget);

	using FDigetherWalletCreditMember = int32 FDigetherWalletState::*;
	const FDigetherWalletCreditMember WalletMembers[] =
	{
		&FDigetherWalletState::BalanceCredits,
		&FDigetherWalletState::LastRunPayoutCredits,
		&FDigetherWalletState::HiddenLifetimeCredits,
		&FDigetherWalletState::TotalDeathLossCredits,
		&FDigetherWalletState::LastPreDeathBalanceCredits
	};
	for (const FDigetherWalletCreditMember Member : WalletMembers)
	{
		FDigetherWalletState InvalidWallet = Wallet;
		InvalidWallet.*Member = -1;
		CheckRejected(TEXT("음수 저장 지갑"), FDigetherEconomyRules::CalculatePourTick(InvalidWallet, Quota, Tuning, false),
			EDigetherEconomyResult::InvalidInput, InvalidWallet, Quota);
	}
	using FDigetherQuotaMember = int32 FDigetherQuotaState::*;
	const FDigetherQuotaMember QuotaMembers[] =
	{
		&FDigetherQuotaState::Day,
		&FDigetherQuotaState::TargetCredits,
		&FDigetherQuotaState::DepositedCredits,
		&FDigetherQuotaState::PeriodGeneration
	};
	for (const FDigetherQuotaMember Member : QuotaMembers)
	{
		FDigetherQuotaState InvalidQuota = Quota;
		InvalidQuota.*Member = -1;
		CheckRejected(TEXT("음수 저장 목표"), FDigetherEconomyRules::CalculatePourTick(Wallet, InvalidQuota, Tuning, false),
			EDigetherEconomyResult::InvalidInput, Wallet, InvalidQuota);
		const FDigetherDayAdvanceResult Advance = FDigetherEconomyRules::AdvanceDay(InvalidQuota);
		TestTrue(TEXT("음수 저장 목표의 일차 전환 거절"), Advance.Result == EDigetherEconomyResult::InvalidInput);
		CheckQuota(TEXT("일차 전환 실패는 모든 목표 값 보존"), Advance.State, InvalidQuota);
	}
	FDigetherQuotaState Uninitialized = Quota;
	Uninitialized.Day = 0;
	TestTrue(TEXT("초기화 전 일차 0은 저장 가능한 값"), FDigetherEconomyRules::IsValidQuotaState(Uninitialized));
	CheckRejected(TEXT("일차 0에서는 붓기 불가"), FDigetherEconomyRules::CalculatePourTick(Wallet, Uninitialized, Tuning, false),
		EDigetherEconomyResult::InvalidInput, Wallet, Uninitialized);
	const FDigetherDayAdvanceResult UninitializedAdvance = FDigetherEconomyRules::AdvanceDay(Uninitialized);
	TestTrue(TEXT("일차 0에서는 일차 전환 불가"), UninitializedAdvance.Result == EDigetherEconomyResult::InvalidInput);
	CheckQuota(TEXT("초기화 전 일차 전환 실패는 상태 보존"), UninitializedAdvance.State, Uninitialized);
	FDigetherQuotaState OverTarget = Quota;
	OverTarget.DepositedCredits = OverTarget.TargetCredits + 1;
	CheckRejected(TEXT("목표액을 넘은 저장 진행량"), FDigetherEconomyRules::CalculatePourTick(Wallet, OverTarget, Tuning, false),
		EDigetherEconomyResult::InvalidInput, Wallet, OverTarget);
	const FDigetherDayAdvanceResult OverTargetAdvance = FDigetherEconomyRules::AdvanceDay(OverTarget);
	TestTrue(TEXT("진행량이 손상된 목표의 일차 전환 거절"), OverTargetAdvance.Result == EDigetherEconomyResult::InvalidInput);
	CheckQuota(TEXT("진행량 손상 상태는 일차 전환 실패 후 보존"), OverTargetAdvance.State, OverTarget);

	FDigetherQuotaState LargeTarget = Quota;
	LargeTarget.TargetCredits = MAX_int32 - 2;
	LargeTarget.DepositedCredits = MAX_int32 - 3;
	ExpectedQuota = LargeTarget;
	ExpectedQuota.Day = 1;
	ExpectedQuota.TargetCredits = MAX_int32;
	ExpectedQuota.DepositedCredits = 0;
	ExpectedQuota.PeriodGeneration = 8;
	ExpectedWallet = Wallet;
	ExpectedWallet.BalanceCredits = 40;
	CheckAccepted(TEXT("목표 증가량 합계 포화"), FDigetherEconomyRules::CalculatePourTick(Wallet, LargeTarget, Tuning, false),
		10, 1, 9, true, ExpectedWallet, ExpectedQuota);
	FDigetherQuotaState LastGeneration = Quota;
	LastGeneration.PeriodGeneration = MAX_int32;
	ExpectedQuota = LastGeneration;
	ExpectedQuota.DepositedCredits = 30;
	CheckAccepted(TEXT("최대 세대에서 목표 미완료 붓기 허용"), FDigetherEconomyRules::CalculatePourTick(Wallet, LastGeneration, Tuning, false),
		10, 10, 0, false, ExpectedWallet, ExpectedQuota);
	LastGeneration.DepositedCredits = 99;
	CheckRejected(TEXT("세대 재사용을 막기 위해 최대 세대의 목표 완료 거절"),
		FDigetherEconomyRules::CalculatePourTick(Wallet, LastGeneration, Tuning, false),
		EDigetherEconomyResult::InvalidInput, Wallet, LastGeneration);
	FDigetherEconomyTuning LargeUnitTuning = Tuning;
	LargeUnitTuning.PourUnitCredits = MAX_int32;
	LargeUnitTuning.TargetIncreaseCredits = 0;
	FDigetherWalletState MaximumWallet = Wallet;
	MaximumWallet.BalanceCredits = MAX_int32;
	LargeTarget.TargetCredits = MAX_int32;
	LargeTarget.DepositedCredits = 0;
	ExpectedWallet = MaximumWallet;
	ExpectedWallet.BalanceCredits = 0;
	ExpectedQuota = LargeTarget;
	ExpectedQuota.Day = 1;
	ExpectedQuota.DepositedCredits = 0;
	ExpectedQuota.PeriodGeneration = 8;
	CheckAccepted(TEXT("최대 잔고 전액 한 주기와 목표 증가 0 허용"),
		FDigetherEconomyRules::CalculatePourTick(MaximumWallet, LargeTarget, LargeUnitTuning, false),
		MAX_int32, MAX_int32, 0, true, ExpectedWallet, ExpectedQuota);

	FDigetherQuotaState ThirdDay = Quota;
	ThirdDay.Day = 3;
	const FDigetherDayAdvanceResult NextDay = FDigetherEconomyRules::AdvanceDay(ThirdDay);
	ExpectedQuota = ThirdDay;
	ExpectedQuota.Day = 4;
	// 작업리스트 1.1.0: 탐사 종료 확정 시 기간 일차와 전체 일차가 함께 하루 증가한다.
	ExpectedQuota.TotalDay = ThirdDay.TotalDay + 1;
	TestTrue(TEXT("일차 전환 성공"), NextDay.Result == EDigetherEconomyResult::Success);
	CheckQuota(TEXT("일차만 증가하고 목표·진행량·세대 유지"), NextDay.State, ExpectedQuota);
	const FDigetherDayAdvanceResult BeyondFourthDay = FDigetherEconomyRules::AdvanceDay(NextDay.State);
	ExpectedQuota.Day = 5;
	ExpectedQuota.TotalDay = ThirdDay.TotalDay + 2;
	TestTrue(TEXT("4일차 뒤 게임 오버를 순수 경제 함수가 확정하지 않음"), BeyondFourthDay.Result == EDigetherEconomyResult::Success);
	CheckQuota(TEXT("4일차 뒤도 상한 없이 일차만 증가"), BeyondFourthDay.State, ExpectedQuota);
	FDigetherQuotaState MaximumDay = Quota;
	MaximumDay.Day = MAX_int32;
	const FDigetherDayAdvanceResult SaturatedDay = FDigetherEconomyRules::AdvanceDay(MaximumDay);
	TestTrue(TEXT("자료형 최대 일차 전환 성공"), SaturatedDay.Result == EDigetherEconomyResult::Success);
	FDigetherQuotaState ExpectedSaturated = MaximumDay;
	ExpectedSaturated.TotalDay = MaximumDay.TotalDay + 1;
	CheckQuota(TEXT("최대 일차는 자료형 상한에서 포화"), SaturatedDay.State, ExpectedSaturated);
	FDigetherQuotaState MaximumTotalDay = Quota;
	MaximumTotalDay.TotalDay = MAX_int32;
	const FDigetherDayAdvanceResult SaturatedTotalDay = FDigetherEconomyRules::AdvanceDay(MaximumTotalDay);
	TestEqual(TEXT("최대 전체 일차는 자료형 상한에서 포화"), SaturatedTotalDay.State.TotalDay, MAX_int32);
	return true;
}

#endif
