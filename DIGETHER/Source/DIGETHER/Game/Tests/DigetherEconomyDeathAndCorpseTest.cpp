#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Data/DigetherTuningProfile.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherEconomyDeathAndCorpseTest,
	"DIGETHER.Game.Economy.DeathAndCorpse", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherEconomyDeathAndCorpseTest::RunTest(const FString& Parameters)
{
	// 기본값 없는 런 스냅샷에 기존 시험 입력을 명시한다.
	FDigetherRunTuning RunTuning;
	RunTuning.DeathLossPercent = 100;
	RunTuning.CorpseRefundPercent = 50;
	RunTuning.InitialQuotaCredits = 100;
	struct FDigetherDeathCase
	{
		int32 BalanceCredits;
		int32 Percent;
		int32 PreDeathCredits;
		int32 LossCredits;
		int32 RemainingCredits;
	};
	const FDigetherDeathCase DeathCases[] =
	{
		{ 101, 100, 101, 101, 0 },
		{ 10, 30, 10, 3, 7 },
		{ 101, 50, 101, 50, 51 },
		{ 101, 0, 101, 0, 101 },
		{ 101, -50, 101, 0, 101 },
		{ 10, 150, 10, 10, 0 },
		{ -10, 50, 0, 0, 0 },
		{ MAX_int32, 100, MAX_int32, MAX_int32, 0 },
		{ MAX_int32, 50, MAX_int32, 1073741823, 1073741824 },
		{ 0, 100, 0, 0, 0 }
	};
	for (const FDigetherDeathCase& Case : DeathCases)
	{
		const FDigetherDeathLossResult Loss = FDigetherEconomyRules::CalculateDeathLoss(Case.BalanceCredits, Case.Percent);
		const FString Name = FString::Printf(TEXT("잔고 %d, 손실률 %d"), Case.BalanceCredits, Case.Percent);
		TestEqual(Name + TEXT(": 사망 직전 기준 잔고"), Loss.PreDeathBalanceCredits, Case.PreDeathCredits);
		TestEqual(Name + TEXT(": 내림한 손실액"), Loss.LossCredits, Case.LossCredits);
		TestEqual(Name + TEXT(": 잔여 잔고"), Loss.RemainingBalanceCredits, Case.RemainingCredits);
	}

	TestEqual(TEXT("시체 복구는 사망 직전 잔고에서 내림"), FDigetherEconomyRules::CalculateCorpseRefund(101, RunTuning.CorpseRefundPercent), RunTuning.CorpseRefundPercent);
	TestEqual(TEXT("복구율 0%"), FDigetherEconomyRules::CalculateCorpseRefund(101, 0), 0);
	TestEqual(TEXT("음수 사망 직전 잔고는 복구 없음"), FDigetherEconomyRules::CalculateCorpseRefund(-10, RunTuning.CorpseRefundPercent), 0);
	TestEqual(TEXT("복구율 상한 100%"), FDigetherEconomyRules::CalculateCorpseRefund(101, 150), 101);
	TestEqual(TEXT("음수 복구율은 0%"), FDigetherEconomyRules::CalculateCorpseRefund(101, -50), 0);
	TestEqual(TEXT("최대 잔고의 전액 복구"), FDigetherEconomyRules::CalculateCorpseRefund(MAX_int32, 100), MAX_int32);
	TestEqual(TEXT("최대 잔고의 절반 복구도 내림"), FDigetherEconomyRules::CalculateCorpseRefund(MAX_int32, RunTuning.CorpseRefundPercent), 1073741823);

	FDigetherPlayerSettlementInput Dead;
	Dead.bDied = true;
	Dead.bCorpseRecovered = true;
	Dead.PreDeathBalanceCredits = 101;
	Dead.ReturnedHiddenCredits = 77;
	FDigetherPlayerSettlementInput Living;
	Living.bSurvived = true;
	Living.ReturnedHiddenCredits = 7;
	const FDigetherRunSettlementResult RecoveredRun = FDigetherEconomyRules::CalculateRunSettlement(100, 2, { Dead, Living }, RunTuning.CorpseRefundPercent);
	TestTrue(TEXT("사망자와 생존자가 함께 있는 정산 성공"), RecoveredRun.Result == EDigetherEconomyResult::Success);
	TestEqual(TEXT("회수 시체 정산 결과 인원 수"), RecoveredRun.Players.Num(), 2);
	if (RecoveredRun.Players.Num() == 2)
	{
		TestEqual(TEXT("시체가 회수돼도 사망자 일반 몫 0"), RecoveredRun.Players[0].GeneralCredits, 0);
		TestEqual(TEXT("시체가 회수돼도 사망자 히든 0"), RecoveredRun.Players[0].HiddenCredits, 0);
		TestEqual(TEXT("시체가 회수돼도 사망자 런 지급액 0"), RecoveredRun.Players[0].PayoutCredits, 0);
		TestEqual(TEXT("사망자 본인의 직전 잔고 101에서 복구 50"), RecoveredRun.Players[0].CorpseRefundCredits, RunTuning.CorpseRefundPercent);
		TestEqual(TEXT("생존자 런 지급액은 본인 일반 몫과 히든"), RecoveredRun.Players[1].PayoutCredits, 57);
		TestEqual(TEXT("시체 복구액을 생존자에게 주지 않음"), RecoveredRun.Players[1].CorpseRefundCredits, 0);
	}
	const FDigetherRunSettlementResult WipeoutRun = FDigetherEconomyRules::CalculateRunSettlement(100, 1, { Dead }, RunTuning.CorpseRefundPercent);
	TestTrue(TEXT("생존자 없는 시체 회수 정산 성공"), WipeoutRun.Result == EDigetherEconomyResult::Success);
	TestEqual(TEXT("생존자 없는 시체 회수 결과 인원 수"), WipeoutRun.Players.Num(), 1);
	if (WipeoutRun.Players.Num() == 1)
	{
		TestEqual(TEXT("전멸 시 일반 몫 0"), WipeoutRun.Players[0].GeneralCredits, 0);
		TestEqual(TEXT("전멸 시 히든 0"), WipeoutRun.Players[0].HiddenCredits, 0);
		TestEqual(TEXT("전멸 시 런 지급액 0"), WipeoutRun.Players[0].PayoutCredits, 0);
		// D25 확정: 전멸이어도 유효 회수 시체의 복구금은 생존자가 있을 때와 같은 산식으로 지급한다.
		TestEqual(TEXT("전멸이어도 회수된 시체는 직전 잔고 기준 복구"), WipeoutRun.Players[0].CorpseRefundCredits,
			FDigetherEconomyRules::CalculateCorpseRefund(Dead.PreDeathBalanceCredits, RunTuning.CorpseRefundPercent));
		TestTrue(TEXT("전멸 복구금은 0보다 큼(픽스처 직전 잔고 양수)"), WipeoutRun.Players[0].CorpseRefundCredits > 0);
		TestEqual(TEXT("전멸 일반 화물은 전액 손실"), WipeoutRun.LostGeneralCredits, 100);
	}
	Dead.bCorpseRecovered = false;
	const FDigetherRunSettlementResult UnrecoveredRun = FDigetherEconomyRules::CalculateRunSettlement(100, 2, { Dead, Living }, RunTuning.CorpseRefundPercent);
	TestTrue(TEXT("회수하지 않은 시체 정산 성공"), UnrecoveredRun.Result == EDigetherEconomyResult::Success);
	TestEqual(TEXT("미회수 시체 정산 결과 인원 수"), UnrecoveredRun.Players.Num(), 2);
	if (UnrecoveredRun.Players.Num() == 2)
	{
		TestEqual(TEXT("생존자가 있어도 시체 미회수는 복구 없음"), UnrecoveredRun.Players[0].CorpseRefundCredits, 0);
	}

	const auto CheckWallet = [this](const TCHAR* Name, const FDigetherWalletState& Actual, const FDigetherWalletState& Expected)
	{
		TestTrue(Name, Actual.BalanceCredits == Expected.BalanceCredits &&
			Actual.LastRunPayoutCredits == Expected.LastRunPayoutCredits &&
			Actual.HiddenLifetimeCredits == Expected.HiddenLifetimeCredits &&
			Actual.TotalDeathLossCredits == Expected.TotalDeathLossCredits &&
			Actual.LastPreDeathBalanceCredits == Expected.LastPreDeathBalanceCredits);
	};
	FDigetherWalletState Wallet;
	Wallet.BalanceCredits = 101;
	Wallet.LastRunPayoutCredits = 13;
	Wallet.HiddenLifetimeCredits = 17;
	Wallet.TotalDeathLossCredits = 40;
	Wallet.LastPreDeathBalanceCredits = 42;
	FDigetherWalletState Expected = Wallet;
	Expected.BalanceCredits = 71;
	Expected.TotalDeathLossCredits = 70;
	Expected.LastPreDeathBalanceCredits = 101;
	const FDigetherWalletUpdateResult PartialLoss = FDigetherEconomyRules::ApplyDeathLoss(Wallet, 30);
	TestTrue(TEXT("부분 사망 손실 적용 성공"), PartialLoss.Result == EDigetherEconomyResult::Success);
	CheckWallet(TEXT("손실 누적·직전 잔고 갱신과 다른 기록 보존"), PartialLoss.State, Expected);
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
		&FDigetherWalletState::LastPreDeathBalanceCredits
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
	const FDigetherWalletUpdateResult Survivor = FDigetherEconomyRules::ApplyRunSettlement(Wallet, Settlement);
	TestTrue(TEXT("생존자 정산 적용 성공"), Survivor.Result == EDigetherEconomyResult::Success);
	CheckWallet(TEXT("일반 몫·개인 히든 지급과 히든 평생 누적"), Survivor.State, Expected);

	FDigetherPlayerSettlement Corpse;
	Corpse.CorpseRefundCredits = 50;
	Expected = PartialLoss.State;
	Expected.BalanceCredits = 121;
	Expected.LastRunPayoutCredits = 0;
	const FDigetherWalletUpdateResult Recovered = FDigetherEconomyRules::ApplyRunSettlement(PartialLoss.State, Corpse);
	TestTrue(TEXT("사망자 시체 복구 적용 성공"), Recovered.Result == EDigetherEconomyResult::Success);
	CheckWallet(TEXT("남은 잔고에 직전 잔고 기준 복구액 추가, 런 지급·히든 누적 제외"), Recovered.State, Expected);

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
	const FDigetherWalletUpdateResult SaturatedPayout = FDigetherEconomyRules::ApplyRunSettlement(LargeWallet, Settlement);
	TestTrue(TEXT("상한 지급액 적용 성공"), SaturatedPayout.Result == EDigetherEconomyResult::Success);
	CheckWallet(TEXT("지급 합계·잔고·히든 평생 누적의 포화"), SaturatedPayout.State, Expected);
	Expected = LargeWallet;
	Expected.BalanceCredits = MAX_int32;
	Expected.LastRunPayoutCredits = 0;
	const FDigetherWalletUpdateResult SaturatedRefund = FDigetherEconomyRules::ApplyRunSettlement(LargeWallet, Corpse);
	TestTrue(TEXT("상한 시체 복구 적용 성공"), SaturatedRefund.Result == EDigetherEconomyResult::Success);
	CheckWallet(TEXT("시체 복구로 잔고가 넘으면 포화, 히든 누적 유지"), SaturatedRefund.State, Expected);

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
		&FDigetherPlayerSettlement::PayoutCredits,
		&FDigetherPlayerSettlement::CorpseRefundCredits
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
	Invalid.GeneralCredits = 0;
	Invalid.HiddenCredits = 5;
	Invalid.PayoutCredits = 5;
	Invalid.CorpseRefundCredits = 1;
	CheckRejectedSettlement(TEXT("히든과 시체 복구 동시 지급"), Invalid);
	Invalid.GeneralCredits = 5;
	Invalid.HiddenCredits = 0;
	CheckRejectedSettlement(TEXT("일반 몫과 시체 복구 동시 지급"), Invalid);
	return true;
}

#endif
