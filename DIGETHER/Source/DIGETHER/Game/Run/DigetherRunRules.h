#pragma once

#include "Game/Run/DigetherRunTypes.h"

struct FDigetherQuotaState;
struct FDigetherRunRuleTuning;

namespace DigetherRunRules
{
	// 보드 확정값(DEV-023). Economy는 일차를 증가시킬 뿐 마감 정책을 소유하지 않는다.
	inline constexpr int32 PaymentDeadlineDay = 4;
}

// 월드·현재 시각·UObject 인스턴스 없이 입력 값만으로 종료 계획을 계산한다.
class DIGETHER_API FDigetherRunRules
{
public:
	static bool IsValidPublicState(const FDigetherRunPublicState& State);
	static bool IsValidRuleTuning(const FDigetherRunRuleTuning& Tuning);
	static FDigetherRunTimeResult CalculateRemainingTime(double RunSeconds, double ElapsedSeconds,
		int32 ConfirmedAscentCount, double AscentPenaltySeconds);

	// Stage는 0~AftershockStageCount다. 남은 비율이 낮아질 때 단계는 커지고 간격은 짧아진다.
	static FDigetherRunAftershockResult CalculateAftershock(double RunSeconds, double RemainingSeconds,
		const FDigetherRunRuleTuning& Tuning);

	// 실제 사망·엘리베이터 조작은 하지 않는다. 호출자는 전체 계획 검증 후 같은 실행 안에서 적용한다.
	static FDigetherRunTerminationResult CalculateTermination(const FDigetherRunTerminationInput& Input);

	// 레버 완료 때는 귀환 전 current quota로 마감 여부만 소비한다.
	// 귀환 때는 Economy가 AdvanceDay 또는 목표 달성의 day1 초기화를 마친 스냅샷으로 게임 오버만 소비한다.
	static FDigetherRunQuotaResult EvaluateQuota(const FDigetherQuotaState& QuotaState);
};
