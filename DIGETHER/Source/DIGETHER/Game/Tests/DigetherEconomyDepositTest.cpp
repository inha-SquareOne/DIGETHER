#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Economy/DigetherEconomyTuning.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherEconomyDepositTest,
	"DIGETHER.Game.Economy.Deposit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherEconomyDepositTest::RunTest(const FString& Parameters)
{
	const FDigetherEconomyTuning Tuning;
	FDigetherEconomyTuning InvalidTuning = Tuning;
	InvalidTuning.TargetIncreaseCredits = -1;
	FDigetherEconomyTuning ZeroIncreaseTuning = Tuning;
	ZeroIncreaseTuning.TargetIncreaseCredits = 0;
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
			Actual.DepositedCredits == Expected.DepositedCredits && Actual.PeriodGeneration == Expected.PeriodGeneration);
	};
	const auto CheckRejected = [this, &CheckWallet, &CheckQuota](const TCHAR* Name, const FDigetherDepositResult& Actual,
		EDigetherEconomyResult ExpectedResult, const FDigetherWalletState& Wallet, const FDigetherQuotaState& Quota)
	{
		TestTrue(FString::Printf(TEXT("%s: 실패 사유"), Name), Actual.Result == ExpectedResult);
		TestEqual(FString::Printf(TEXT("%s: 부분 납부 없음"), Name), Actual.AcceptedCredits, 0);
		TestEqual(FString::Printf(TEXT("%s: 목표 반영 없음"), Name), Actual.CreditedCredits, 0);
		TestFalse(FString::Printf(TEXT("%s: 목표 완료 없음"), Name), Actual.bTargetCompleted);
		CheckWallet(TEXT("실패 시 모든 개인 지갑 값 보존"), Actual.WalletState, Wallet);
		CheckQuota(TEXT("실패 시 모든 목표 값 보존"), Actual.QuotaState, Quota);
	};
	const auto CheckAccepted = [this, &CheckWallet, &CheckQuota](const TCHAR* Name, const FDigetherDepositResult& Actual,
		int32 AcceptedCredits, bool bCompleted, const FDigetherWalletState& Wallet, const FDigetherQuotaState& Quota)
	{
		TestTrue(FString::Printf(TEXT("%s: 납부 성공"), Name), Actual.Result == EDigetherEconomyResult::Success);
		TestEqual(FString::Printf(TEXT("%s: 실제 납부액"), Name), Actual.AcceptedCredits, AcceptedCredits);
		TestEqual(FString::Printf(TEXT("%s: 4일차 반영액"), Name), Actual.CreditedCredits, AcceptedCredits);
		TestEqual(FString::Printf(TEXT("%s: 목표 완료 여부"), Name), Actual.bTargetCompleted, bCompleted);
		CheckWallet(TEXT("납부 후 지갑 값"), Actual.WalletState, Wallet);
		CheckQuota(TEXT("납부 후 목표 값"), Actual.QuotaState, Quota);
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

	FDigetherWalletState ExpectedWallet = Wallet;
	ExpectedWallet.BalanceCredits = 40;
	FDigetherQuotaState ExpectedQuota = Quota;
	ExpectedQuota.DepositedCredits = 30;
	CheckAccepted(TEXT("목표 미완료 납부"), FDigetherEconomyRules::CalculateDeposit(Wallet, Quota, 10, Tuning, false),
		10, false, ExpectedWallet, ExpectedQuota);

	FDigetherQuotaState NearTarget = Quota;
	NearTarget.DepositedCredits = 90;
	ExpectedQuota = NearTarget;
	ExpectedQuota.Day = 1;
	ExpectedQuota.TargetCredits = 200;
	ExpectedQuota.DepositedCredits = 0;
	ExpectedQuota.PeriodGeneration = 8;
	CheckAccepted(TEXT("잔여 목표만 납부하고 새 기간 시작"), FDigetherEconomyRules::CalculateDeposit(Wallet, NearTarget, 40, Tuning, false),
		10, true, ExpectedWallet, ExpectedQuota);
	CheckRejected(TEXT("잔여 목표가 적어도 요청액 자체의 잔고 초과 거절"),
		FDigetherEconomyRules::CalculateDeposit(Wallet, NearTarget, 60, Tuning, false),
		EDigetherEconomyResult::InsufficientFunds, Wallet, NearTarget);

	CheckRejected(TEXT("납부 마감"), FDigetherEconomyRules::CalculateDeposit(Wallet, Quota, 10, Tuning, true),
		EDigetherEconomyResult::DepositClosed, Wallet, Quota);
	CheckRejected(TEXT("마감이 잘못된 요청 금액보다 우선"), FDigetherEconomyRules::CalculateDeposit(Wallet, Quota, 0, Tuning, true),
		EDigetherEconomyResult::DepositClosed, Wallet, Quota);
	CheckRejected(TEXT("0원 요청"), FDigetherEconomyRules::CalculateDeposit(Wallet, Quota, 0, Tuning, false),
		EDigetherEconomyResult::InvalidAmount, Wallet, Quota);
	CheckRejected(TEXT("음수 요청"), FDigetherEconomyRules::CalculateDeposit(Wallet, Quota, -1, Tuning, false),
		EDigetherEconomyResult::InvalidAmount, Wallet, Quota);
	CheckRejected(TEXT("음수 목표 증가량"), FDigetherEconomyRules::CalculateDeposit(Wallet, Quota, 10, InvalidTuning, false),
		EDigetherEconomyResult::InvalidInput, Wallet, Quota);

	FDigetherQuotaState Full = Quota;
	Full.DepositedCredits = Full.TargetCredits;
	CheckRejected(TEXT("이미 완료된 목표"), FDigetherEconomyRules::CalculateDeposit(Wallet, Full, 1, Tuning, false),
		EDigetherEconomyResult::QuotaAlreadySatisfied, Wallet, Full);
	CheckRejected(TEXT("잔고 초과 요청은 완료 목표보다 우선 거절"), FDigetherEconomyRules::CalculateDeposit(Wallet, Full, 51, Tuning, false),
		EDigetherEconomyResult::InsufficientFunds, Wallet, Full);
	FDigetherQuotaState ZeroTarget = Quota;
	ZeroTarget.TargetCredits = 0;
	ZeroTarget.DepositedCredits = 0;
	CheckRejected(TEXT("목표 0은 이미 완료된 목표"), FDigetherEconomyRules::CalculateDeposit(Wallet, ZeroTarget, 1, Tuning, false),
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
		CheckRejected(TEXT("음수 저장 지갑"), FDigetherEconomyRules::CalculateDeposit(InvalidWallet, Quota, 1, Tuning, false),
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
		CheckRejected(TEXT("음수 저장 목표"), FDigetherEconomyRules::CalculateDeposit(Wallet, InvalidQuota, 1, Tuning, false),
			EDigetherEconomyResult::InvalidInput, Wallet, InvalidQuota);
		const FDigetherDayAdvanceResult Advance = FDigetherEconomyRules::AdvanceDay(InvalidQuota);
		TestTrue(TEXT("음수 저장 목표의 일차 전환 거절"), Advance.Result == EDigetherEconomyResult::InvalidInput);
		CheckQuota(TEXT("일차 전환 실패는 모든 목표 값 보존"), Advance.State, InvalidQuota);
	}
	FDigetherQuotaState Uninitialized = Quota;
	Uninitialized.Day = 0;
	TestTrue(TEXT("초기화 전 일차 0은 저장 가능한 값"), FDigetherEconomyRules::IsValidQuotaState(Uninitialized));
	CheckRejected(TEXT("일차 0에서는 납부 불가"), FDigetherEconomyRules::CalculateDeposit(Wallet, Uninitialized, 1, Tuning, false),
		EDigetherEconomyResult::InvalidInput, Wallet, Uninitialized);
	const FDigetherDayAdvanceResult UninitializedAdvance = FDigetherEconomyRules::AdvanceDay(Uninitialized);
	TestTrue(TEXT("일차 0에서는 일차 전환 불가"), UninitializedAdvance.Result == EDigetherEconomyResult::InvalidInput);
	CheckQuota(TEXT("초기화 전 일차 전환 실패는 상태 보존"), UninitializedAdvance.State, Uninitialized);
	FDigetherQuotaState OverTarget = Quota;
	OverTarget.DepositedCredits = OverTarget.TargetCredits + 1;
	CheckRejected(TEXT("목표액을 넘은 저장 진행량"), FDigetherEconomyRules::CalculateDeposit(Wallet, OverTarget, 1, Tuning, false),
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
	ExpectedWallet.BalanceCredits = 49;
	CheckAccepted(TEXT("목표 증가량 합계 포화"), FDigetherEconomyRules::CalculateDeposit(Wallet, LargeTarget, 1, Tuning, false),
		1, true, ExpectedWallet, ExpectedQuota);
	FDigetherQuotaState LastGeneration = Quota;
	LastGeneration.PeriodGeneration = MAX_int32;
	ExpectedQuota = LastGeneration;
	ExpectedQuota.DepositedCredits = 21;
	CheckAccepted(TEXT("최대 세대에서 목표 미완료 납부 허용"), FDigetherEconomyRules::CalculateDeposit(Wallet, LastGeneration, 1, Tuning, false),
		1, false, ExpectedWallet, ExpectedQuota);
	LastGeneration.DepositedCredits = 99;
	CheckRejected(TEXT("세대 재사용을 막기 위해 최대 세대의 목표 완료 거절"),
		FDigetherEconomyRules::CalculateDeposit(Wallet, LastGeneration, 1, Tuning, false),
		EDigetherEconomyResult::InvalidInput, Wallet, LastGeneration);
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
	CheckAccepted(TEXT("최대 잔고 전액 납부와 목표 증가 0 허용"),
		FDigetherEconomyRules::CalculateDeposit(MaximumWallet, LargeTarget, MAX_int32, ZeroIncreaseTuning, false),
		MAX_int32, true, ExpectedWallet, ExpectedQuota);

	FDigetherQuotaState ThirdDay = Quota;
	ThirdDay.Day = 3;
	const FDigetherDayAdvanceResult NextDay = FDigetherEconomyRules::AdvanceDay(ThirdDay);
	ExpectedQuota = ThirdDay;
	ExpectedQuota.Day = 4;
	TestTrue(TEXT("일차 전환 성공"), NextDay.Result == EDigetherEconomyResult::Success);
	CheckQuota(TEXT("일차만 증가하고 목표·진행량·세대 유지"), NextDay.State, ExpectedQuota);
	const FDigetherDayAdvanceResult BeyondFourthDay = FDigetherEconomyRules::AdvanceDay(NextDay.State);
	ExpectedQuota.Day = 5;
	TestTrue(TEXT("4일차 뒤 게임 오버를 순수 경제 함수가 확정하지 않음"), BeyondFourthDay.Result == EDigetherEconomyResult::Success);
	CheckQuota(TEXT("4일차 뒤도 상한 없이 일차만 증가"), BeyondFourthDay.State, ExpectedQuota);
	FDigetherQuotaState MaximumDay = Quota;
	MaximumDay.Day = MAX_int32;
	const FDigetherDayAdvanceResult SaturatedDay = FDigetherEconomyRules::AdvanceDay(MaximumDay);
	TestTrue(TEXT("자료형 최대 일차 전환 성공"), SaturatedDay.Result == EDigetherEconomyResult::Success);
	CheckQuota(TEXT("최대 일차는 자료형 상한에서 포화"), SaturatedDay.State, MaximumDay);
	return true;
}

#endif
