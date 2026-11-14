#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Data/DigetherTuningProfile.h"
#include "Game/Economy/DigetherEconomyTuning.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherEconomyEfficiencyTest,
	"DIGETHER.Game.Economy.Efficiency", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherEconomyEfficiencyTest::RunTest(const FString& Parameters)
{
	const FDigetherEconomyTuning Tuning;
	// 기본값 없는 런 스냅샷이라 시험 입력 명시
	FDigetherRunTuning RunTuning;
	RunTuning.DeathLossPercent = 100;
	RunTuning.RecoveredDeathLossPercent = 50;
	RunTuning.InitialQuotaCredits = 100;
	// 2026-11-04 기획 답변 초안 임시값(PAY_DAY_EFFICIENCY), 최종 기획 값 아님
	TestEqual(TEXT("1일차 임시 효율"), Tuning.DayOneEfficiencyPercent, 30);
	TestEqual(TEXT("2일차 임시 효율"), Tuning.DayTwoEfficiencyPercent, 50);
	TestEqual(TEXT("3일차 임시 효율"), Tuning.DayThreeEfficiencyPercent, 70);
	TestEqual(TEXT("4일차 확정 효율"), Tuning.DayFourEfficiencyPercent, 100);
	TestEqual(TEXT("런 스냅샷 첫 목표"), RunTuning.InitialQuotaCredits, 100);
	TestEqual(TEXT("임시 고정 증가량"), Tuning.TargetIncreaseCredits, 100);
	TestEqual(TEXT("런 스냅샷 사망 손실률"), RunTuning.DeathLossPercent, 100);
	TestEqual(TEXT("런 스냅샷 시체 회수 손실률"), RunTuning.RecoveredDeathLossPercent, 50);
	TestEqual(TEXT("미사용 세션 수명(저장 호환)"), Tuning.DepositSessionLifetimeSeconds, 10.0);
	TestEqual(TEXT("임시 붓기 기준액(PAY_UNIT_AMOUNT)"), Tuning.PourUnitCredits, 10);

	// 한 주기 붓기 금액별 같은 효율 규칙 확인, 기준액은 시험 입력이며 기획 값 아님
	const auto WithUnit = [&Tuning](const int32 UnitCredits)
	{
		FDigetherEconomyTuning Unit = Tuning;
		Unit.PourUnitCredits = UnitCredits;
		return Unit;
	};
	FDigetherWalletState Wallet;
	Wallet.BalanceCredits = 1000;
	Wallet.LastRunPayoutCredits = 13;
	Wallet.HiddenLifetimeCredits = 17;
	Wallet.TotalDeathLossCredits = 40;
	Wallet.LastPreDeathBalanceCredits = 42;
	const FDigetherQuotaState Initial = FDigetherEconomyRules::MakeInitialQuotaState(RunTuning);
	TestEqual(TEXT("새 목표는 1일차"), Initial.Day, 1);
	TestEqual(TEXT("새 목표는 100"), Initial.TargetCredits, 100);
	TestEqual(TEXT("새 누적액 0"), Initial.DepositedCredits, 0);
	TestEqual(TEXT("새 기간 세대 0"), Initial.PeriodGeneration, 0);

	const auto CheckPreservedWalletRecords = [this, &Wallet](const FDigetherWalletState& Actual)
	{
		TestTrue(TEXT("붓기는 다른 지갑 기록을 보존"), Actual.LastRunPayoutCredits == Wallet.LastRunPayoutCredits &&
			Actual.HiddenLifetimeCredits == Wallet.HiddenLifetimeCredits &&
			Actual.TotalDeathLossCredits == Wallet.TotalDeathLossCredits &&
			Actual.LastPreDeathBalanceCredits == Wallet.LastPreDeathBalanceCredits);
	};
	const auto CheckRejected = [this, &CheckPreservedWalletRecords](const FDigetherPourTickResult& Actual,
		const EDigetherEconomyResult ExpectedResult, const FDigetherWalletState& BeforeWallet,
		const FDigetherQuotaState& BeforeQuota)
	{
		TestEqual(TEXT("붓기 거절 사유"), Actual.Result, ExpectedResult);
		TestEqual(TEXT("거절 시 차감액 0"), Actual.SpentCredits, 0);
		TestEqual(TEXT("거절 시 반영액 0"), Actual.CreditedCredits, 0);
		TestFalse(TEXT("거절 시 목표 완료 없음"), Actual.bTargetCompleted);
		TestEqual(TEXT("거절 시 잔고 보존"), Actual.WalletState.BalanceCredits, BeforeWallet.BalanceCredits);
		CheckPreservedWalletRecords(Actual.WalletState);
		TestTrue(TEXT("거절 시 전체 목표 보존"), Actual.QuotaState.Day == BeforeQuota.Day &&
			Actual.QuotaState.TargetCredits == BeforeQuota.TargetCredits &&
			Actual.QuotaState.DepositedCredits == BeforeQuota.DepositedCredits &&
			Actual.QuotaState.PeriodGeneration == BeforeQuota.PeriodGeneration);
	};
	struct FDigetherEfficiencyCase
	{
		int32 Day;
		int32 Percent;
		int32 ElevenCredits;
		int32 MaximumCredits;
	};
	const FDigetherEfficiencyCase Cases[] =
	{
		{ 1, 30, 3, 644245094 },
		{ 2, 50, 5, 1073741823 },
		{ 3, 70, 7, 1503238552 },
		{ 4, 100, 11, MAX_int32 }
	};
	for (const FDigetherEfficiencyCase& Case : Cases)
	{
		int32 Percent = -1;
		TestEqual(TEXT("일차 효율 조회"), FDigetherEconomyRules::GetDepositEfficiencyPercent(Case.Day, Tuning, Percent),
			EDigetherEconomyResult::Success);
		TestEqual(TEXT("일차별 고정 효율"), Percent, Case.Percent);
		FDigetherQuotaState Quota = Initial;
		Quota.Day = Case.Day;
		Quota.DepositedCredits = 5;
		const FDigetherPourTickResult Partial = FDigetherEconomyRules::CalculatePourTick(Wallet, Quota, WithUnit(11), false);
		TestEqual(TEXT("11원 주기 성공"), Partial.Result, EDigetherEconomyResult::Success);
		TestEqual(TEXT("효율과 무관하게 부은 금액 전체 차감"), Partial.SpentCredits, 11);
		TestEqual(TEXT("반영액은 내림"), Partial.CreditedCredits, Case.ElevenCredits);
		TestEqual(TEXT("지갑에서는 11원 전체 차감"), Partial.WalletState.BalanceCredits, 989);
		TestEqual(TEXT("미달 주기는 바닥 금화 없음"), Partial.FloorCredits, 0);
		CheckPreservedWalletRecords(Partial.WalletState);
		TestEqual(TEXT("기존 누적액에 반영액만 추가"), Partial.QuotaState.DepositedCredits, 5 + Case.ElevenCredits);
		TestFalse(TEXT("미달 주기는 기간 전환 없음"), Partial.bTargetCompleted);
		TestEqual(TEXT("미달 주기는 일차 보존"), Partial.QuotaState.Day, Case.Day);
		TestEqual(TEXT("미달 주기는 목표 보존"), Partial.QuotaState.TargetCredits, 100);
		TestEqual(TEXT("미달 주기는 세대 보존"), Partial.QuotaState.PeriodGeneration, 0);

		FDigetherWalletState MaximumWallet = Wallet;
		MaximumWallet.BalanceCredits = MAX_int32;
		Quota.TargetCredits = MAX_int32;
		Quota.DepositedCredits = 0;
		const FDigetherPourTickResult Maximum = FDigetherEconomyRules::CalculatePourTick(
			MaximumWallet, Quota, WithUnit(MAX_int32), false);
		TestEqual(TEXT("최대 금액 주기 성공"), Maximum.Result, EDigetherEconomyResult::Success);
		TestEqual(TEXT("최대 금액 전액 차감"), Maximum.SpentCredits, MAX_int32);
		TestEqual(TEXT("64비트 곱셈 후 반영액"), Maximum.CreditedCredits, Case.MaximumCredits);
		TestEqual(TEXT("최대 금액 차감 후 잔고 0"), Maximum.WalletState.BalanceCredits, 0);
		TestEqual(TEXT("자료형 상한 목표 유지·성장 포화"), Maximum.QuotaState.TargetCredits, MAX_int32);
		TestEqual(TEXT("100%일 때만 최대 목표 달성"), Maximum.bTargetCompleted, Case.Day == 4);
		TestEqual(TEXT("최대 금액 누적 또는 다음 기간 초기화"), Maximum.QuotaState.DepositedCredits,
			Case.Day == 4 ? 0 : Case.MaximumCredits);
		TestEqual(TEXT("최대 금액 일차 또는 다음 기간 1일차"), Maximum.QuotaState.Day, Case.Day == 4 ? 1 : Case.Day);
		TestEqual(TEXT("최대 금액 기간 세대"), Maximum.QuotaState.PeriodGeneration, Case.Day == 4 ? 1 : 0);
	}

	FDigetherQuotaState NearTarget = Initial;
	NearTarget.DepositedCredits = 50;
	// 200원의 30% 반영 60이 남은 목표 50 초과, 목표에는 N = ceil(50 * 100 / 30) = 167만 사용
	// 지갑은 200원 차감, 남은 33원은 바닥 금화(PQ-34 안 A, 임시)
	const FDigetherPourTickResult Reduced = FDigetherEconomyRules::CalculatePourTick(Wallet, NearTarget, WithUnit(200), false);
	TestEqual(TEXT("초과 반영 주기 성공"), Reduced.Result, EDigetherEconomyResult::Success);
	TestEqual(TEXT("부은 200원 전부 지갑 차감"), Reduced.SpentCredits, 200);
	TestEqual(TEXT("남은 목표 50·30%의 최소 실제 금액은 167"), Reduced.ConsumedCredits, 167);
	TestEqual(TEXT("남는 33원은 바닥 금화"), Reduced.FloorCredits, 33);
	TestEqual(TEXT("남은 목표 50만 반영"), Reduced.CreditedCredits, 50);
	TestEqual(TEXT("200원 차감 후 잔고"), Reduced.WalletState.BalanceCredits, 800);
	TestTrue(TEXT("반영액 기준 목표 완료"), Reduced.bTargetCompleted);
	TestEqual(TEXT("달성 즉시 누적액 초기화(초과분은 새 목표에 넣지 않음)"), Reduced.QuotaState.DepositedCredits, 0);
	TestEqual(TEXT("달성 후 목표 200"), Reduced.QuotaState.TargetCredits, 200);
	TestEqual(TEXT("달성 후 1일차"), Reduced.QuotaState.Day, 1);
	TestEqual(TEXT("달성 후 세대 증가"), Reduced.QuotaState.PeriodGeneration, 1);
	const FDigetherPourTickResult BelowMinimum = FDigetherEconomyRules::CalculatePourTick(Wallet, NearTarget, WithUnit(166), false);
	TestEqual(TEXT("최소액보다 1원 적으면 반영 49"), BelowMinimum.CreditedCredits, 49);
	TestEqual(TEXT("최소액 미달이면 부은 166원 차감"), BelowMinimum.SpentCredits, 166);
	TestEqual(TEXT("최소액 미달이면 누적 99"), BelowMinimum.QuotaState.DepositedCredits, 99);
	TestFalse(TEXT("최소액 미달이면 목표 미완료"), BelowMinimum.bTargetCompleted);
	const FDigetherPourTickResult ExactMinimum = FDigetherEconomyRules::CalculatePourTick(Wallet, NearTarget, WithUnit(167), false);
	TestEqual(TEXT("정확한 최소액 차감"), ExactMinimum.SpentCredits, 167);
	TestEqual(TEXT("정확한 최소액은 바닥 금화 없음"), ExactMinimum.FloorCredits, 0);
	TestEqual(TEXT("정확한 최소액 반영"), ExactMinimum.CreditedCredits, 50);
	TestTrue(TEXT("정확한 최소액 목표 달성"), ExactMinimum.bTargetCompleted);

	// 잔고가 기준액 미만이면 잔고 전부 붓기, 167원이면 남은 목표 정확히 충족
	FDigetherWalletState SmallWallet = Wallet;
	SmallWallet.BalanceCredits = 167;
	const FDigetherPourTickResult SmallBalance = FDigetherEconomyRules::CalculatePourTick(SmallWallet, NearTarget, WithUnit(200), false);
	TestEqual(TEXT("잔고 전부 붓기"), SmallBalance.SpentCredits, 167);
	TestTrue(TEXT("잔고 전부로 목표 달성"), SmallBalance.bTargetCompleted);
	TestEqual(TEXT("잔고 0"), SmallBalance.WalletState.BalanceCredits, 0);
	for (const int32 Day : { 1, 2, 3 })
	{
		FDigetherQuotaState Quota = Initial;
		Quota.Day = Day;
		CheckRejected(FDigetherEconomyRules::CalculatePourTick(Wallet, Quota, WithUnit(1), false),
			EDigetherEconomyResult::DepositCreditTooSmall, Wallet, Quota);
	}
	// 1일차 30%: 3원까지 반영 0으로 거절, 4원부터 1원 반영
	CheckRejected(FDigetherEconomyRules::CalculatePourTick(Wallet, Initial, WithUnit(3), false),
		EDigetherEconomyResult::DepositCreditTooSmall, Wallet, Initial);
	const FDigetherPourTickResult SmallestCredited = FDigetherEconomyRules::CalculatePourTick(Wallet, Initial, WithUnit(4), false);
	TestEqual(TEXT("1일차 4원 주기 성공"), SmallestCredited.Result, EDigetherEconomyResult::Success);
	TestEqual(TEXT("1일차 4원은 1원 반영"), SmallestCredited.CreditedCredits, 1);
	TestEqual(TEXT("1일차 4원 전액 차감"), SmallestCredited.SpentCredits, 4);
	for (const int32 Day : { -1, 0, 5, MAX_int32 })
	{
		int32 Percent = 123;
		TestEqual(TEXT("범위 밖 일차 효율 거절"), FDigetherEconomyRules::GetDepositEfficiencyPercent(Day, Tuning, Percent),
			EDigetherEconomyResult::InvalidInput);
		TestEqual(TEXT("효율 조회 실패는 이전 출력 제거"), Percent, 0);
		FDigetherQuotaState Quota = Initial;
		Quota.Day = Day;
		CheckRejected(FDigetherEconomyRules::CalculatePourTick(Wallet, Quota, Tuning, false),
			EDigetherEconomyResult::InvalidInput, Wallet, Quota);
	}
	for (const int32 InvalidPercent : { 0, -1, 101, MAX_int32 })
	{
		FDigetherEconomyTuning InvalidTuning = Tuning;
		InvalidTuning.DayOneEfficiencyPercent = InvalidPercent;
		int32 Percent = 123;
		TestEqual(TEXT("손상 효율 설정 조회 거절"),
			FDigetherEconomyRules::GetDepositEfficiencyPercent(1, InvalidTuning, Percent), EDigetherEconomyResult::InvalidInput);
		TestEqual(TEXT("손상 효율 조회 출력 0"), Percent, 0);
		CheckRejected(FDigetherEconomyRules::CalculatePourTick(Wallet, Initial, InvalidTuning, false),
			EDigetherEconomyResult::InvalidInput, Wallet, Initial);
	}

	FDigetherQuotaState LargeTarget = Initial;
	LargeTarget.TargetCredits = MAX_int32;
	LargeTarget.DepositedCredits = MAX_int32 - 50;
	FDigetherWalletState MaximumWallet = Wallet;
	MaximumWallet.BalanceCredits = MAX_int32;
	const FDigetherPourTickResult Saturated = FDigetherEconomyRules::CalculatePourTick(
		MaximumWallet, LargeTarget, WithUnit(MAX_int32), false);
	TestEqual(TEXT("큰 주기도 목표에는 최소 167원만 사용"), Saturated.ConsumedCredits, 167);
	TestEqual(TEXT("큰 주기의 나머지는 바닥 금화"), Saturated.FloorCredits, MAX_int32 - 167);
	TestEqual(TEXT("큰 목표의 잔여 50만 반영"), Saturated.CreditedCredits, 50);
	TestEqual(TEXT("큰 잔고에서 부은 금액 전부 차감"), Saturated.WalletState.BalanceCredits, 0);
	TestEqual(TEXT("목표 증가는 포화"), Saturated.QuotaState.TargetCredits, MAX_int32);
	TestTrue(TEXT("큰 목표 달성 성공"), Saturated.bTargetCompleted);

	// 1일차 30%로 두 목표(100·200) 연속 충족, 목표에 쓴 금액 334 + 667, 나머지는 바닥 금화
	FDigetherWalletState ChainWallet = Wallet;
	ChainWallet.BalanceCredits = 2000;
	const FDigetherPourTickResult First = FDigetherEconomyRules::CalculatePourTick(ChainWallet, Initial, WithUnit(400), false);
	TestEqual(TEXT("첫 목표 100 달성 성공"), First.Result, EDigetherEconomyResult::Success);
	TestEqual(TEXT("첫 목표 100에 쓰인 1일차 실제 금액"), First.ConsumedCredits, 334);
	TestEqual(TEXT("첫 목표 초과 바닥 금화"), First.FloorCredits, 66);
	TestEqual(TEXT("첫 목표 반영 100"), First.CreditedCredits, 100);
	TestEqual(TEXT("100 달성 후 목표 200"), First.QuotaState.TargetCredits, 200);
	const FDigetherPourTickResult Second = FDigetherEconomyRules::CalculatePourTick(
		First.WalletState, First.QuotaState, WithUnit(700), false);
	TestEqual(TEXT("두 번째 목표 달성 성공"), Second.Result, EDigetherEconomyResult::Success);
	TestEqual(TEXT("두 번째 목표 200에 쓰인 실제 금액"), Second.ConsumedCredits, 667);
	TestEqual(TEXT("두 번째 목표 초과 바닥 금화"), Second.FloorCredits, 33);
	TestEqual(TEXT("두 번째 목표 반영 200"), Second.CreditedCredits, 200);
	TestEqual(TEXT("200 달성 후 목표 300"), Second.QuotaState.TargetCredits, 300);
	TestEqual(TEXT("두 번 달성 후 누적액 0"), Second.QuotaState.DepositedCredits, 0);
	TestEqual(TEXT("두 번 달성 후 세대 2"), Second.QuotaState.PeriodGeneration, 2);
	TestEqual(TEXT("두 번 달성 후 일차 1"), Second.QuotaState.Day, 1);
	TestEqual(TEXT("두 번 부은 금액 전체 차감"), Second.WalletState.BalanceCredits, 900);
	TestEqual(TEXT("목표 증가 함수도 포화"), FDigetherEconomyRules::CalculateNextTargetCredits(MAX_int32 - 1, Tuning), MAX_int32);
	TestEqual(TEXT("입력 지갑은 불변"), Wallet.BalanceCredits, 1000);
	TestEqual(TEXT("입력 목표는 불변"), Initial.TargetCredits, 100);

	// 첫 목표는 런 스냅샷, 달성 후 증가량은 경제 튜닝에서 받음
	FDigetherRunTuning SeparateRunTuning = RunTuning;
	SeparateRunTuning.InitialQuotaCredits = 250;
	const FDigetherQuotaState SeparateInitial = FDigetherEconomyRules::MakeInitialQuotaState(SeparateRunTuning);
	TestEqual(TEXT("별도 런 스냅샷의 첫 목표 250"), SeparateInitial.TargetCredits, 250);
	const FDigetherPourTickResult SeparateFirst = FDigetherEconomyRules::CalculatePourTick(
		Wallet, SeparateInitial, WithUnit(900), false);
	TestEqual(TEXT("별도 스냅샷 첫 목표 달성 성공"), SeparateFirst.Result, EDigetherEconomyResult::Success);
	TestEqual(TEXT("첫 목표 250에 쓰인 1일차 실제 금액"), SeparateFirst.ConsumedCredits, 834);
	TestEqual(TEXT("별도 스냅샷 첫 목표 반영 250"), SeparateFirst.CreditedCredits, 250);
	TestTrue(TEXT("별도 스냅샷 첫 목표 완료"), SeparateFirst.bTargetCompleted);
	TestEqual(TEXT("첫 목표 250에 경제 증가량 100을 더한 다음 목표 350"), SeparateFirst.QuotaState.TargetCredits, 350);
	TestTrue(TEXT("별도 초기 목표 입력은 불변"), SeparateInitial.Day == 1 && SeparateInitial.TargetCredits == 250 &&
		SeparateInitial.DepositedCredits == 0 && SeparateInitial.PeriodGeneration == 0);
	TestTrue(TEXT("별도 런 스냅샷 입력은 불변"), SeparateRunTuning.InitialQuotaCredits == 250 &&
		SeparateRunTuning.DeathLossPercent == 100 && SeparateRunTuning.RecoveredDeathLossPercent == 50);
	TestEqual(TEXT("다른 첫 목표를 적용해도 경제 증가량 입력은 불변"), Tuning.TargetIncreaseCredits, 100);
	FDigetherRunTuning NegativeRunTuning = RunTuning;
	NegativeRunTuning.InitialQuotaCredits = -1;
	const FDigetherQuotaState NegativeInitial = FDigetherEconomyRules::MakeInitialQuotaState(NegativeRunTuning);
	TestEqual(TEXT("음수 스냅샷 첫 목표는 0으로 보정"), NegativeInitial.TargetCredits, 0);
	TestTrue(TEXT("음수 첫 목표도 일차·누적액·세대 초기화"), NegativeInitial.Day == 1 &&
		NegativeInitial.DepositedCredits == 0 && NegativeInitial.PeriodGeneration == 0);
	TestEqual(TEXT("음수 첫 목표 보정은 원본 스냅샷 불변"), NegativeRunTuning.InitialQuotaCredits, -1);
	return true;
}

#endif
