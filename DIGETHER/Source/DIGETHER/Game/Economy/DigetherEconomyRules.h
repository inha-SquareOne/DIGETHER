#pragma once

#include "Game/Economy/DigetherEconomyTypes.h"

struct FDigetherEconomyTuning;
struct FDigetherRunTuning;

// 값 입출력 전용, 월드·시각·UObject 인스턴스 비참조
class DIGETHER_API FDigetherEconomyRules
{
public:
	static int32 AddCredits(int32 LeftCredits, int32 RightCredits);
	static bool IsValidWalletState(const FDigetherWalletState& State);
	// 각 값이 음수가 아니고 누적 인정액이 목표 이하인지만 검사
	// 전체 일차 >= 기간 일차 관계는 개발·시험 경로가 기간 일차만 바꾸는 경우가 있어 비강제
	// 런타임 규칙(AdvanceDay·붓기 달성)이 관계 유지
	static bool IsValidQuotaState(const FDigetherQuotaState& State);
	static bool IsValidPublicState(const FDigetherQuotaPublicState& State);
	// 허용 조건: 1~4일차 효율 1~100%, 목표 증가량 0 이상, 유한한 양수 입금 세션 수명(미사용·저장 호환),
	// 주기당 붓기 기준액 1 이상, 0 < 주황 경계 < 초록 경계 <= 1인 신호기 경계
	static bool IsValidTuning(const FDigetherEconomyTuning& Tuning);
	// 목표액 대비 공동 인정액 비율로 신호기 단계 결정, 목표 없음·잘못된 입력은 None
	static EDigetherPaymentSignal CalculatePaymentSignal(const FDigetherQuotaState& State,
		const FDigetherEconomyTuning& Tuning);
	// 서버 공개 상태에서 누적 인정액을 빼고 신호기 단계를 넣은 복제 값 생성
	static FDigetherQuotaReplicatedState MakeReplicatedQuotaState(const FDigetherQuotaPublicState& State,
		const FDigetherEconomyTuning& Tuning);
	// 클라이언트가 받은 복제 값을 기존 조회 형태로 변환, 누적 인정액은 0
	static FDigetherQuotaPublicState MakeClientQuotaPublicState(const FDigetherQuotaReplicatedState& State);
	// 1~4일차와 선택 일차의 1~100%만 허용, 실패 시 OutPercent는 0
	static EDigetherEconomyResult GetDepositEfficiencyPercent(int32 Day, const FDigetherEconomyTuning& Tuning,
		int32& OutPercent);
	// 런 시작 때 고정한 스냅샷의 첫 목표 사용(음수는 0으로 보정)
	// 누적액·세대 0, 기간·전체 일차 1로 시작
	static FDigetherQuotaState MakeInitialQuotaState(const FDigetherRunTuning& RunTuning);
	// 현재 목표에 고정 증가량을 더해 포화, 비율 증가 미적용
	static int32 CalculateNextTargetCredits(int32 CurrentTargetCredits, const FDigetherEconomyTuning& Tuning);

	// 비율 인자는 런 시작 때 고정한 FDigetherRunTuning 스냅샷의 해당 필드 값
	static FDigetherDeathLossResult CalculateDeathLoss(int32 BalanceCredits, int32 DeathLossPercent);
	static int32 CalculateCorpseRefund(int32 PreDeathBalanceCredits, int32 CorpseRefundPercent);
	static FDigetherRunSettlementResult CalculateRunSettlement(int32 GeneralCargoCredits, int32 DepartureCount,
		const TArray<FDigetherPlayerSettlementInput>& Players, int32 CorpseRefundPercent);

	// 사망·정산 중복 적용 방지와 플레이어별 결과 대응은 조율자 책임
	static FDigetherWalletUpdateResult ApplyDeathLoss(const FDigetherWalletState& State, int32 DeathLossPercent);
	static FDigetherWalletUpdateResult ApplyRunSettlement(const FDigetherWalletState& State,
		const FDigetherPlayerSettlement& Settlement);
	// 붓기 한 주기 계산: S = min(PourUnitCredits, 잔고), 인정 C = floor(S × 그 순간 일차 효율 / 100)
	// C가 0이면 차감 없이 DepositCreditTooSmall, C가 남은 목표 R 이상이면 목표 달성
	// C > R이면 N = ceil(100R / 효율)만 목표에 쓰고 지갑에서는 S 차감, F = S - N은 바닥 금화로 반환(PQ-34 안 A, 임시)
	// 달성 시 다음 목표·누적 0·기간 1일차·세대+1(전체 일차 유지), 세대가 최댓값이면 재사용 방지로 거절
	static FDigetherPourTickResult CalculatePourTick(const FDigetherWalletState& WalletState,
		const FDigetherQuotaState& QuotaState, const FDigetherEconomyTuning& Tuning, bool bDepositClosed);
	// 목표 달성 직후 새 기간 상태: 다음 목표·누적 0·기간 1일차·세대+1, 전체 일차 유지
	// 붓기 달성과 개발 즉시 달성 공용, 잘못된 입력·세대 최댓값이면 false·OutState 보존
	static bool TryMakeCompletedQuotaState(const FDigetherQuotaState& QuotaState, const FDigetherEconomyTuning& Tuning,
		FDigetherQuotaState& OutState);
	// 바닥 금화 생성 실패 시 사용, F를 지갑에 되돌려 차감을 N으로 줄이고 F는 0(돈 소멸 방지)
	static FDigetherPourTickResult ReclaimFloorCredits(const FDigetherPourTickResult& Result);
	// 탐사 종료 확정 1회당 호출, 기간 일차·전체 일차 각각 +1(정상 종료·전멸 공통)
	// 1차 탈출 상승처럼 다른 인원이 아직 탐사 중이면 조율자가 호출하지 않음
	static FDigetherDayAdvanceResult AdvanceDay(const FDigetherQuotaState& State);
};
