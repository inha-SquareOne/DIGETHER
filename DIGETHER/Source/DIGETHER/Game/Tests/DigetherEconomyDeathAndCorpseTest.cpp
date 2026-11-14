#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Data/DigetherTuningProfile.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherEconomyDeathAndCorpseTest,
	"DIGETHER.Game.Economy.DeathAndCorpse", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherEconomyDeathAndCorpseTest::RunTest(const FString& Parameters)
{
	// 기본값 없는 런 스냅샷이라 시험 입력 명시, 100%·50%는 런 플로우 v1.1.0 9.4 시험값
	FDigetherRunTuning RunTuning;
	RunTuning.DeathLossPercent = 100;
	RunTuning.RecoveredDeathLossPercent = 50;
	RunTuning.InitialQuotaCredits = 100;
	struct FDigetherDeathCase
	{
		int32 BalanceCredits;
		int32 Percent;
		int32 PreDeathCredits;
		int32 LossCredits;
		int32 RemainingCredits;
	};
	// D25 개정(2026-11-14): 손실액 올림, 이전 시험의 내림 기대값(101의 50% → 50)을 올림(51)으로 변경
	const FDigetherDeathCase DeathCases[] =
	{
		{ 101, 100, 101, 101, 0 },
		{ 10, 30, 10, 3, 7 },
		{ 101, 50, 101, 51, 50 },
		{ 3, 30, 3, 1, 2 },
		{ 1, 50, 1, 1, 0 },
		{ 101, 0, 101, 0, 101 },
		{ 101, -50, 101, 0, 101 },
		{ 10, 150, 10, 10, 0 },
		{ -10, 50, 0, 0, 0 },
		{ MAX_int32, 100, MAX_int32, MAX_int32, 0 },
		{ MAX_int32, 50, MAX_int32, 1073741824, 1073741823 },
		{ 0, 100, 0, 0, 0 }
	};
	for (const FDigetherDeathCase& Case : DeathCases)
	{
		const FDigetherDeathLossResult Loss = FDigetherEconomyRules::CalculateDeathLoss(Case.BalanceCredits, Case.Percent);
		const FString Name = FString::Printf(TEXT("잔고 %d, 손실률 %d"), Case.BalanceCredits, Case.Percent);
		TestEqual(Name + TEXT(": 사망 직전 기준 잔고"), Loss.PreDeathBalanceCredits, Case.PreDeathCredits);
		TestEqual(Name + TEXT(": 올림한 손실액"), Loss.LossCredits, Case.LossCredits);
		TestEqual(Name + TEXT(": 잔여 잔고"), Loss.RemainingBalanceCredits, Case.RemainingCredits);
	}

	// 시체 회수 여부로 탐사 사망 손실률 선택, 미스터리 박스 포식(시체 없음)은 미회수와 같음
	TestEqual(TEXT("시체 유효 회수 손실률"), FDigetherEconomyRules::GetExplorationDeathLossPercent(RunTuning, true), 50);
	TestEqual(TEXT("시체 미회수 손실률"), FDigetherEconomyRules::GetExplorationDeathLossPercent(RunTuning, false), 100);

	const auto CheckWallet = [this](const TCHAR* Name, const FDigetherWalletState& Actual, const FDigetherWalletState& Expected)
	{
		TestTrue(Name, Actual.BalanceCredits == Expected.BalanceCredits &&
			Actual.LastRunPayoutCredits == Expected.LastRunPayoutCredits &&
			Actual.HiddenLifetimeCredits == Expected.HiddenLifetimeCredits &&
			Actual.TotalDeathLossCredits == Expected.TotalDeathLossCredits &&
			Actual.LastPreDeathBalanceCredits == Expected.LastPreDeathBalanceCredits &&
			Actual.LifetimeEarnedCredits == Expected.LifetimeEarnedCredits &&
			Actual.LifetimeCorpseRefundCredits == Expected.LifetimeCorpseRefundCredits);
	};
	FDigetherWalletState Wallet;
	Wallet.BalanceCredits = 101;
	Wallet.LastRunPayoutCredits = 13;
	Wallet.HiddenLifetimeCredits = 17;
	Wallet.TotalDeathLossCredits = 40;
	Wallet.LastPreDeathBalanceCredits = 42;

	// 기획 예시: 잔고 101, 유효 회수 50% → 51 차감·50 잔존, 미회수 100% → 101 차감
	FDigetherWalletState Expected = Wallet;
	Expected.BalanceCredits = 50;
	Expected.TotalDeathLossCredits = 91;
	Expected.LastPreDeathBalanceCredits = 101;
	const FDigetherWalletUpdateResult Recovered = FDigetherEconomyRules::ApplyDeathLoss(Wallet,
		FDigetherEconomyRules::GetExplorationDeathLossPercent(RunTuning, true));
	TestTrue(TEXT("회수 시체 손실 적용 성공"), Recovered.Result == EDigetherEconomyResult::Success);
	CheckWallet(TEXT("회수 시체는 51 차감·50 잔존, 누적 손실은 실제 차감액"), Recovered.State, Expected);
	Expected = Wallet;
	Expected.BalanceCredits = 0;
	Expected.TotalDeathLossCredits = 141;
	Expected.LastPreDeathBalanceCredits = 101;
	const FDigetherWalletUpdateResult Unrecovered = FDigetherEconomyRules::ApplyDeathLoss(Wallet,
		FDigetherEconomyRules::GetExplorationDeathLossPercent(RunTuning, false));
	TestTrue(TEXT("미회수 시체 손실 적용 성공"), Unrecovered.Result == EDigetherEconomyResult::Success);
	CheckWallet(TEXT("미회수 시체는 101 전액 차감"), Unrecovered.State, Expected);

	// 정산은 사망자에게 지급·복구 없음, 회수 여부와 무관하게 지갑은 그대로(복구금 폐지)
	FDigetherPlayerSettlementInput Dead;
	Dead.bDied = true;
	Dead.bCorpseRecovered = true;
	Dead.ReturnedHiddenCredits = 77;
	FDigetherPlayerSettlementInput Living;
	Living.bSurvived = true;
	Living.ReturnedHiddenCredits = 7;
	const FDigetherRunSettlementResult RecoveredRun = FDigetherEconomyRules::CalculateRunSettlement(100, 2, { Dead, Living });
	TestTrue(TEXT("사망자와 생존자가 함께 있는 정산 성공"), RecoveredRun.Result == EDigetherEconomyResult::Success);
	if (TestEqual(TEXT("회수 시체 정산 결과 인원 수"), RecoveredRun.Players.Num(), 2))
	{
		TestEqual(TEXT("시체가 회수돼도 사망자 일반 몫 0"), RecoveredRun.Players[0].GeneralCredits, 0);
		TestEqual(TEXT("시체가 회수돼도 사망자 히든 0"), RecoveredRun.Players[0].HiddenCredits, 0);
		TestEqual(TEXT("시체가 회수돼도 사망자 런 지급액 0"), RecoveredRun.Players[0].PayoutCredits, 0);
		TestEqual(TEXT("생존자 런 지급액은 본인 일반 몫과 히든"), RecoveredRun.Players[1].PayoutCredits, 57);
		const FDigetherWalletUpdateResult DeadSettled = FDigetherEconomyRules::ApplyRunSettlement(Recovered.State,
			RecoveredRun.Players[0]);
		TestTrue(TEXT("사망자 정산 적용 성공"), DeadSettled.Result == EDigetherEconomyResult::Success);
		Expected = Recovered.State;
		Expected.LastRunPayoutCredits = 0;
		CheckWallet(TEXT("사망자 정산은 복구 없이 손실 뒤 잔고 유지"), DeadSettled.State, Expected);
	}
	const FDigetherRunSettlementResult WipeoutRun = FDigetherEconomyRules::CalculateRunSettlement(100, 1, { Dead });
	TestTrue(TEXT("생존자 없는 시체 회수 정산 성공"), WipeoutRun.Result == EDigetherEconomyResult::Success);
	if (TestEqual(TEXT("생존자 없는 시체 회수 결과 인원 수"), WipeoutRun.Players.Num(), 1))
	{
		TestEqual(TEXT("전멸 시 런 지급액 0"), WipeoutRun.Players[0].PayoutCredits, 0);
		TestEqual(TEXT("전멸 일반 화물은 전액 제외"), WipeoutRun.LostGeneralCredits, 100);
	}

	Expected = Wallet;
	Expected.BalanceCredits = 70;
	Expected.TotalDeathLossCredits = 71;
	Expected.LastPreDeathBalanceCredits = 101;
	const FDigetherWalletUpdateResult PartialLoss = FDigetherEconomyRules::ApplyDeathLoss(Wallet, 30);
	TestTrue(TEXT("부분 사망 손실 적용 성공"), PartialLoss.Result == EDigetherEconomyResult::Success);
	CheckWallet(TEXT("30.3을 올린 31 차감, 손실 누적·직전 잔고 갱신과 다른 기록 보존"), PartialLoss.State, Expected);
	Expected = Wallet;
	Expected.LastPreDeathBalanceCredits = 101;
	const FDigetherWalletUpdateResult NoLoss = FDigetherEconomyRules::ApplyDeathLoss(Wallet, -10);
	TestTrue(TEXT("음수 손실률은 유효한 0% 처리"), NoLoss.Result == EDigetherEconomyResult::Success);
	CheckWallet(TEXT("손실 0이어도 사망 직전 잔고는 갱신"), NoLoss.State, Expected);
	FDigetherWalletState LargeLossWallet = Wallet;
	LargeLossWallet.BalanceCredits = 10;
	LargeLossWallet.TotalDeathLossCredits = MAX_int32 - 2;
	Expected = LargeLossWallet;
	Expected.BalanceCredits = 0;
	Expected.TotalDeathLossCredits = MAX_int32;
	Expected.LastPreDeathBalanceCredits = 10;
	const FDigetherWalletUpdateResult LargeLoss = FDigetherEconomyRules::ApplyDeathLoss(LargeLossWallet, RunTuning.DeathLossPercent);
	TestTrue(TEXT("누적 손실 포화 적용 성공"), LargeLoss.Result == EDigetherEconomyResult::Success);
	CheckWallet(TEXT("누적 사망 손실은 상한에서 포화"), LargeLoss.State, Expected);

	using FDigetherWalletCreditMember = int32 FDigetherWalletState::*;
	const FDigetherWalletCreditMember WalletMembers[] =
	{
		&FDigetherWalletState::BalanceCredits,
		&FDigetherWalletState::LastRunPayoutCredits,
		&FDigetherWalletState::HiddenLifetimeCredits,
		&FDigetherWalletState::TotalDeathLossCredits,
		&FDigetherWalletState::LastPreDeathBalanceCredits,
		&FDigetherWalletState::LifetimeEarnedCredits,
		&FDigetherWalletState::LifetimeCorpseRefundCredits
	};
	for (const FDigetherWalletCreditMember Member : WalletMembers)
	{
		FDigetherWalletState InvalidWallet = Wallet;
		InvalidWallet.*Member = -1;
		const FDigetherWalletUpdateResult InvalidDeath = FDigetherEconomyRules::ApplyDeathLoss(InvalidWallet, RunTuning.DeathLossPercent);
		TestTrue(TEXT("음수 저장 지갑은 사망 적용 거절"), InvalidDeath.Result == EDigetherEconomyResult::InvalidInput);
		CheckWallet(TEXT("사망 적용 실패는 지갑 전체 보존"), InvalidDeath.State, InvalidWallet);
		const FDigetherWalletUpdateResult InvalidSettlement = FDigetherEconomyRules::ApplyRunSettlement(InvalidWallet, FDigetherPlayerSettlement());
		TestTrue(TEXT("음수 저장 지갑은 정산 적용 거절"), InvalidSettlement.Result == EDigetherEconomyResult::InvalidInput);
		CheckWallet(TEXT("정산 적용 실패는 지갑 전체 보존"), InvalidSettlement.State, InvalidWallet);
	}

	FDigetherPlayerSettlement Settlement;
	Settlement.GeneralCredits = 225;
	Settlement.HiddenCredits = 7;
	Settlement.PayoutCredits = 232;
	Expected = Wallet;
	Expected.BalanceCredits = 333;
	Expected.LastRunPayoutCredits = 232;
	Expected.HiddenLifetimeCredits = 24;
	Expected.LifetimeEarnedCredits = 232;
	const FDigetherWalletUpdateResult Survivor = FDigetherEconomyRules::ApplyRunSettlement(Wallet, Settlement);
	TestTrue(TEXT("생존자 정산 적용 성공"), Survivor.Result == EDigetherEconomyResult::Success);
	CheckWallet(TEXT("일반 몫·개인 히든 지급과 히든·획득 누적"), Survivor.State, Expected);

	// 구 저장의 복구 누적은 정산으로 바뀌지 않음(통계 순액 계산 전용)
	FDigetherWalletState LegacyWallet = Wallet;
	LegacyWallet.LifetimeCorpseRefundCredits = 25;
	const FDigetherWalletUpdateResult LegacySettled = FDigetherEconomyRules::ApplyRunSettlement(LegacyWallet, Settlement);
	TestEqual(TEXT("구 복구 누적 유지"), LegacySettled.State.LifetimeCorpseRefundCredits, 25);

	FDigetherWalletState LargeWallet = Wallet;
	LargeWallet.BalanceCredits = MAX_int32 - 2;
	LargeWallet.HiddenLifetimeCredits = MAX_int32 - 1;
	Settlement.GeneralCredits = MAX_int32;
	Settlement.HiddenCredits = MAX_int32;
	Settlement.PayoutCredits = MAX_int32;
	Expected = LargeWallet;
	Expected.BalanceCredits = MAX_int32;
	Expected.LastRunPayoutCredits = MAX_int32;
	Expected.HiddenLifetimeCredits = MAX_int32;
	Expected.LifetimeEarnedCredits = MAX_int32;
	const FDigetherWalletUpdateResult SaturatedPayout = FDigetherEconomyRules::ApplyRunSettlement(LargeWallet, Settlement);
	TestTrue(TEXT("상한 지급액 적용 성공"), SaturatedPayout.Result == EDigetherEconomyResult::Success);
	CheckWallet(TEXT("지급 합계·잔고·히든 평생 누적의 포화"), SaturatedPayout.State, Expected);

	const auto CheckRejectedSettlement = [this, &Wallet, &CheckWallet](const TCHAR* Name, const FDigetherPlayerSettlement& Invalid)
	{
		const FDigetherWalletUpdateResult Result = FDigetherEconomyRules::ApplyRunSettlement(Wallet, Invalid);
		TestTrue(FString::Printf(TEXT("%s: 정산 거절"), Name), Result.Result == EDigetherEconomyResult::InvalidInput);
		CheckWallet(TEXT("정산 거절 시 모든 지갑 기록 보존"), Result.State, Wallet);
	};
	using FDigetherSettlementCreditMember = int32 FDigetherPlayerSettlement::*;
	const FDigetherSettlementCreditMember SettlementMembers[] =
	{
		&FDigetherPlayerSettlement::GeneralCredits,
		&FDigetherPlayerSettlement::HiddenCredits,
		&FDigetherPlayerSettlement::PayoutCredits
	};
	for (const FDigetherSettlementCreditMember Member : SettlementMembers)
	{
		FDigetherPlayerSettlement Invalid;
		Invalid.*Member = -1;
		CheckRejectedSettlement(TEXT("음수 정산 금액"), Invalid);
	}
	FDigetherPlayerSettlement Invalid;
	Invalid.GeneralCredits = 5;
	Invalid.HiddenCredits = 2;
	Invalid.PayoutCredits = 6;
	CheckRejectedSettlement(TEXT("일반·히든 합계와 다른 지급액"), Invalid);
	return true;
}

#endif
