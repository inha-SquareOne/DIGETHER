#pragma once

#include "Game/Economy/DigetherEconomyTypes.h"

struct FDigetherEconomyTuning;
struct FDigetherRunTuning;

// 입출력은 값뿐이다. 월드·시각·UObject 인스턴스를 참조하지 않는다.
class DIGETHER_API FDigetherEconomyRules
{
public:
	static int32 AddCredits(int32 LeftCredits, int32 RightCredits);
	static bool IsValidWalletState(const FDigetherWalletState& State);
	static bool IsValidQuotaState(const FDigetherQuotaState& State);
	static bool IsValidPublicState(const FDigetherQuotaPublicState& State);
	// 1~4일차와 선택한 일차의 1~100%만 허용한다. 실패하면 OutPercent는 0이다.
	static EDigetherEconomyResult GetDepositEfficiencyPercent(int32 Day, const FDigetherEconomyTuning& Tuning,
		int32& OutPercent);
	// 런 시작 때 고정한 스냅샷의 첫 목표를 받는다. 음수는 0으로 보정하고 누적액·세대 0, 일차 1로 시작한다.
	static FDigetherQuotaState MakeInitialQuotaState(const FDigetherRunTuning& RunTuning);
	// 현재 목표에 고정 증가량을 더해 포화한다. 비율 증가는 적용하지 않는다.
	static int32 CalculateNextTargetCredits(int32 CurrentTargetCredits, const FDigetherEconomyTuning& Tuning);

	// 비율 인자는 런 시작 때 고정한 FDigetherRunTuning 스냅샷의 해당 필드에서 전달한다.
	static FDigetherDeathLossResult CalculateDeathLoss(int32 BalanceCredits, int32 DeathLossPercent);
	static int32 CalculateCorpseRefund(int32 PreDeathBalanceCredits, int32 CorpseRefundPercent);
	static FDigetherRunSettlementResult CalculateRunSettlement(int32 GeneralCargoCredits, int32 DepartureCount,
		const TArray<FDigetherPlayerSettlementInput>& Players, int32 CorpseRefundPercent);

	// 사망·정산의 중복 적용 방지와 플레이어별 결과 대응은 조율자의 책임이다.
	static FDigetherWalletUpdateResult ApplyDeathLoss(const FDigetherWalletState& State, int32 DeathLossPercent);
	static FDigetherWalletUpdateResult ApplyRunSettlement(const FDigetherWalletState& State,
		const FDigetherPlayerSettlement& Settlement);
	static FDigetherDepositResult CalculateDeposit(const FDigetherWalletState& WalletState,
		const FDigetherQuotaState& QuotaState, int32 RequestedCredits, const FDigetherEconomyTuning& Tuning,
		bool bDepositClosed);
	static FDigetherDayAdvanceResult AdvanceDay(const FDigetherQuotaState& State);
};
