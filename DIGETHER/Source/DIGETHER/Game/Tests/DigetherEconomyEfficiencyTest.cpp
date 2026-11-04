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
	// 기본값 없는 런 스냅샷에 기존 시험 입력을 명시한다.
	FDigetherRunTuning RunTuning;
	RunTuning.DeathLossPercent = 100;
	RunTuning.CorpseRefundPercent = 50;
	RunTuning.InitialQuotaCredits = 100;
	TestEqual(TEXT("1일차 임시 효율"), Tuning.DayOneEfficiencyPercent, 70);
	TestEqual(TEXT("2일차 임시 효율"), Tuning.DayTwoEfficiencyPercent, 80);
	TestEqual(TEXT("3일차 임시 효율"), Tuning.DayThreeEfficiencyPercent, 90);
	TestEqual(TEXT("4일차 확정 효율"), Tuning.DayFourEfficiencyPercent, 100);
	TestEqual(TEXT("런 스냅샷 첫 목표"), RunTuning.InitialQuotaCredits, 100);
	TestEqual(TEXT("임시 고정 증가량"), Tuning.TargetIncreaseCredits, 100);
	TestEqual(TEXT("런 스냅샷 사망 손실률"), RunTuning.DeathLossPercent, 100);
	TestEqual(TEXT("런 스냅샷 시체 복구율"), RunTuning.CorpseRefundPercent, 50);
	TestEqual(TEXT("임시 세션 수명(초)"), Tuning.DepositSessionLifetimeSeconds, 10.0);

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
		TestTrue(TEXT("납부는 다른 지갑 기록을 보존"), Actual.LastRunPayoutCredits == Wallet.LastRunPayoutCredits &&
			Actual.HiddenLifetimeCredits == Wallet.HiddenLifetimeCredits &&
			Actual.TotalDeathLossCredits == Wallet.TotalDeathLossCredits &&
			Actual.LastPreDeathBalanceCredits == Wallet.LastPreDeathBalanceCredits);
	};
	const auto CheckRejected = [this, &CheckPreservedWalletRecords](const FDigetherDepositResult& Actual,
		const EDigetherEconomyResult ExpectedResult, const FDigetherWalletState& BeforeWallet,
		const FDigetherQuotaState& BeforeQuota)
	{
		TestEqual(TEXT("납부 거절 사유"), Actual.Result, ExpectedResult);
		TestEqual(TEXT("거절 시 차감액 0"), Actual.AcceptedCredits, 0);
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
		{ 1, 70, 7, 1503238552 },
		{ 2, 80, 8, 1717986917 },
		{ 3, 90, 9, 1932735282 },
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
		const FDigetherDepositResult Partial = FDigetherEconomyRules::CalculateDeposit(Wallet, Quota, 11, Tuning, false);
		TestEqual(TEXT("11원 납부 성공"), Partial.Result, EDigetherEconomyResult::Success);
		TestEqual(TEXT("효율과 무관하게 낸 금액 전체 수납"), Partial.AcceptedCredits, 11);
		TestEqual(TEXT("반영액은 내림"), Partial.CreditedCredits, Case.ElevenCredits);
		TestEqual(TEXT("지갑에서는 11원 전체 차감"), Partial.WalletState.BalanceCredits, 989);
		CheckPreservedWalletRecords(Partial.WalletState);
		TestEqual(TEXT("기존 누적액에 반영액만 추가"), Partial.QuotaState.DepositedCredits, 5 + Case.ElevenCredits);
		TestFalse(TEXT("미달 납부는 기간 전환 없음"), Partial.bTargetCompleted);
		TestEqual(TEXT("미달 납부는 일차 보존"), Partial.QuotaState.Day, Case.Day);
		TestEqual(TEXT("미달 납부는 목표 보존"), Partial.QuotaState.TargetCredits, 100);
		TestEqual(TEXT("미달 납부는 세대 보존"), Partial.QuotaState.PeriodGeneration, 0);

		FDigetherWalletState MaximumWallet = Wallet;
		MaximumWallet.BalanceCredits = MAX_int32;
		Quota.TargetCredits = MAX_int32;
		Quota.DepositedCredits = 0;
		const FDigetherDepositResult Maximum = FDigetherEconomyRules::CalculateDeposit(
			MaximumWallet, Quota, MAX_int32, Tuning, false);
		TestEqual(TEXT("최대 금액 납부 성공"), Maximum.Result, EDigetherEconomyResult::Success);
		TestEqual(TEXT("최대 금액 전액 차감"), Maximum.AcceptedCredits, MAX_int32);
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
	const FDigetherDepositResult Reduced = FDigetherEconomyRules::CalculateDeposit(Wallet, NearTarget, 100, Tuning, false);
	TestEqual(TEXT("초과 반영 요청을 최소 수납액으로 조정"), Reduced.Result, EDigetherEconomyResult::Success);
	TestEqual(TEXT("남은 목표 50·70%의 최소 수납액은 72"), Reduced.AcceptedCredits, 72);
	TestEqual(TEXT("남은 목표 50만 반영"), Reduced.CreditedCredits, 50);
	TestEqual(TEXT("효율 손실을 포함한 72원 차감"), Reduced.WalletState.BalanceCredits, 928);
	TestTrue(TEXT("반영액 기준 목표 완료"), Reduced.bTargetCompleted);
	TestEqual(TEXT("달성 즉시 누적액 초기화"), Reduced.QuotaState.DepositedCredits, 0);
	TestEqual(TEXT("달성 후 목표 200"), Reduced.QuotaState.TargetCredits, 200);
	TestEqual(TEXT("달성 후 1일차"), Reduced.QuotaState.Day, 1);
	TestEqual(TEXT("달성 후 세대 증가"), Reduced.QuotaState.PeriodGeneration, 1);
	const FDigetherDepositResult BelowMinimum = FDigetherEconomyRules::CalculateDeposit(Wallet, NearTarget, 71, Tuning, false);
	TestEqual(TEXT("최소액보다 1원 적으면 반영 49"), BelowMinimum.CreditedCredits, 49);
	TestEqual(TEXT("최소액 미달이면 낸 금액 71원 수납"), BelowMinimum.AcceptedCredits, 71);
	TestEqual(TEXT("최소액 미달이면 누적 99"), BelowMinimum.QuotaState.DepositedCredits, 99);
	TestFalse(TEXT("최소액 미달이면 목표 미완료"), BelowMinimum.bTargetCompleted);
	const FDigetherDepositResult ExactMinimum = FDigetherEconomyRules::CalculateDeposit(Wallet, NearTarget, 72, Tuning, false);
	TestEqual(TEXT("정확한 최소액 수납"), ExactMinimum.AcceptedCredits, 72);
	TestEqual(TEXT("정확한 최소액 반영"), ExactMinimum.CreditedCredits, 50);
	TestTrue(TEXT("정확한 최소액 목표 달성"), ExactMinimum.bTargetCompleted);

	FDigetherWalletState SmallWallet = Wallet;
	SmallWallet.BalanceCredits = 72;
	CheckRejected(FDigetherEconomyRules::CalculateDeposit(SmallWallet, NearTarget, 100, Tuning, false),
		EDigetherEconomyResult::InsufficientFunds, SmallWallet, NearTarget);
	for (const int32 Day : { 1, 2, 3 })
	{
		FDigetherQuotaState Quota = Initial;
		Quota.Day = Day;
		CheckRejected(FDigetherEconomyRules::CalculateDeposit(Wallet, Quota, 1, Tuning, false),
			EDigetherEconomyResult::DepositCreditTooSmall, Wallet, Quota);
	}
	for (const int32 Day : { -1, 0, 5, MAX_int32 })
	{
		int32 Percent = 123;
		TestEqual(TEXT("범위 밖 일차 효율 거절"), FDigetherEconomyRules::GetDepositEfficiencyPercent(Day, Tuning, Percent),
			EDigetherEconomyResult::InvalidInput);
		TestEqual(TEXT("효율 조회 실패는 이전 출력 제거"), Percent, 0);
		FDigetherQuotaState Quota = Initial;
		Quota.Day = Day;
		CheckRejected(FDigetherEconomyRules::CalculateDeposit(Wallet, Quota, 10, Tuning, false),
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
		CheckRejected(FDigetherEconomyRules::CalculateDeposit(Wallet, Initial, 10, InvalidTuning, false),
			EDigetherEconomyResult::InvalidInput, Wallet, Initial);
	}

	FDigetherQuotaState LargeTarget = Initial;
	LargeTarget.TargetCredits = MAX_int32;
	LargeTarget.DepositedCredits = MAX_int32 - 50;
	FDigetherWalletState MaximumWallet = Wallet;
	MaximumWallet.BalanceCredits = MAX_int32;
	const FDigetherDepositResult Saturated = FDigetherEconomyRules::CalculateDeposit(
		MaximumWallet, LargeTarget, MAX_int32, Tuning, false);
	TestEqual(TEXT("큰 요청도 최소 72원만 수납"), Saturated.AcceptedCredits, 72);
	TestEqual(TEXT("큰 목표의 잔여 50만 반영"), Saturated.CreditedCredits, 50);
	TestEqual(TEXT("큰 잔고에서 최소액 차감"), Saturated.WalletState.BalanceCredits, 2147483575);
	TestEqual(TEXT("목표 증가는 포화"), Saturated.QuotaState.TargetCredits, MAX_int32);
	TestTrue(TEXT("큰 목표 달성 성공"), Saturated.bTargetCompleted);

	const FDigetherDepositResult First = FDigetherEconomyRules::CalculateDeposit(Wallet, Initial, 200, Tuning, false);
	TestEqual(TEXT("첫 목표 100 달성 성공"), First.Result, EDigetherEconomyResult::Success);
	TestEqual(TEXT("첫 목표 100에 필요한 1일차 납부액"), First.AcceptedCredits, 143);
	TestEqual(TEXT("첫 목표 반영 100"), First.CreditedCredits, 100);
	TestEqual(TEXT("100 달성 후 목표 200"), First.QuotaState.TargetCredits, 200);
	const FDigetherDepositResult Second = FDigetherEconomyRules::CalculateDeposit(
		First.WalletState, First.QuotaState, 300, Tuning, false);
	TestEqual(TEXT("두 번째 목표 달성 성공"), Second.Result, EDigetherEconomyResult::Success);
	TestEqual(TEXT("두 번째 목표 200에 필요한 납부액"), Second.AcceptedCredits, 286);
	TestEqual(TEXT("두 번째 목표 반영 200"), Second.CreditedCredits, 200);
	TestEqual(TEXT("200 달성 후 목표 300"), Second.QuotaState.TargetCredits, 300);
	TestEqual(TEXT("두 번 달성 후 누적액 0"), Second.QuotaState.DepositedCredits, 0);
	TestEqual(TEXT("두 번 달성 후 세대 2"), Second.QuotaState.PeriodGeneration, 2);
	TestEqual(TEXT("두 번 달성 후 일차 1"), Second.QuotaState.Day, 1);
	TestEqual(TEXT("두 번 낸 금액 전체 차감"), Second.WalletState.BalanceCredits, 571);
	TestEqual(TEXT("목표 증가 함수도 포화"), FDigetherEconomyRules::CalculateNextTargetCredits(MAX_int32 - 1, Tuning), MAX_int32);
	TestEqual(TEXT("입력 지갑은 불변"), Wallet.BalanceCredits, 1000);
	TestEqual(TEXT("입력 목표는 불변"), Initial.TargetCredits, 100);

	// 첫 목표는 런 스냅샷, 달성 후 증가량은 경제 튜닝에서 각각 받는다.
	FDigetherRunTuning SeparateRunTuning = RunTuning;
	SeparateRunTuning.InitialQuotaCredits = 250;
	const FDigetherQuotaState SeparateInitial = FDigetherEconomyRules::MakeInitialQuotaState(SeparateRunTuning);
	TestEqual(TEXT("별도 런 스냅샷의 첫 목표 250"), SeparateInitial.TargetCredits, 250);
	const FDigetherDepositResult SeparateFirst = FDigetherEconomyRules::CalculateDeposit(
		Wallet, SeparateInitial, 400, Tuning, false);
	TestEqual(TEXT("별도 스냅샷 첫 목표 달성 성공"), SeparateFirst.Result, EDigetherEconomyResult::Success);
	TestEqual(TEXT("첫 목표 250에 필요한 1일차 납부액"), SeparateFirst.AcceptedCredits, 358);
	TestEqual(TEXT("별도 스냅샷 첫 목표 반영 250"), SeparateFirst.CreditedCredits, 250);
	TestTrue(TEXT("별도 스냅샷 첫 목표 완료"), SeparateFirst.bTargetCompleted);
	TestEqual(TEXT("첫 목표 250에 경제 증가량 100을 더한 다음 목표 350"), SeparateFirst.QuotaState.TargetCredits, 350);
	TestTrue(TEXT("별도 초기 목표 입력은 불변"), SeparateInitial.Day == 1 && SeparateInitial.TargetCredits == 250 &&
		SeparateInitial.DepositedCredits == 0 && SeparateInitial.PeriodGeneration == 0);
	TestTrue(TEXT("별도 런 스냅샷 입력은 불변"), SeparateRunTuning.InitialQuotaCredits == 250 &&
		SeparateRunTuning.DeathLossPercent == 100 && SeparateRunTuning.CorpseRefundPercent == 50);
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
