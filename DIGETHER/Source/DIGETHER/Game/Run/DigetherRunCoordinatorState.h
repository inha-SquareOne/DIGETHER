#pragma once

#include "CoreMinimal.h"
#include "Game/Data/DigetherTuningProfile.h"
#include "Game/Run/DigetherRunRuleTuning.h"
#include "Game/Run/DigetherRunRules.h"

// 서버 값만 보관, 월드 없는 보관 레코드에도 같은 시간·종료 규칙 적용
struct DIGETHER_API FDigetherRunCoordinatorState
{
	FDigetherRunTuning Tuning;
	FDigetherRunRuleTuning RuleTuning;
	EDigetherRunPhase Phase = EDigetherRunPhase::NotStarted;
	double StartedAtSeconds = 0.0;
	double LastUpdatedAtSeconds = 0.0;
	double RemainingSeconds = 0.0;
	int32 ConfirmedAscentCount = 0;
	int32 AftershockStage = 0;
	bool bTimeExpiredNotified = false;
	// 제한 시간 시작 여부, 기획: 최초 하강을 마치고 갱도에 도착한 순간부터 흐름
	// 시작 전에는 경과 0으로 계산해 전체 제한 시간 유지
	bool bClockStarted = false;
#if !UE_BUILD_SHIPPING
	// 개발 시계 정지, UpdateTime마다 시작·마지막 갱신 시각을 같은 양만큼 밀어 경과 시간 유지
	// StartRun이 새 상태를 만들므로 다음 런에는 이어지지 않음
	bool bDevClockPaused = false;
#endif
};

struct DIGETHER_API FDigetherRunAdvanceResult
{
	EDigetherRunResult Result = EDigetherRunResult::InvalidInput;
	bool bTimeExpiredNow = false;
};

class DIGETHER_API FDigetherRunCoordinatorRules
{
public:
	// bStartClock이 false면 런만 시작, 제한 시간은 StartClock 전까지 정지
	static FDigetherRunAdvanceResult StartRun(FDigetherRunCoordinatorState& State,
		const FDigetherRunTuning& Tuning, const FDigetherRunRuleTuning& RuleTuning, double ServerSeconds,
		bool bStartClock = true);
	// 진행 중이고 시계가 멈춘 런의 제한 시간을 ServerSeconds부터 시작, 이미 흐르면 AlreadyInProgress
	static FDigetherRunAdvanceResult StartClock(FDigetherRunCoordinatorState& State, double ServerSeconds);
	static FDigetherRunAdvanceResult RecordConfirmedAscent(FDigetherRunCoordinatorState& State);
	static FDigetherRunAdvanceResult UpdateTime(FDigetherRunCoordinatorState& State, double ServerSeconds);
	static FDigetherRunTerminationResult CompleteRun(FDigetherRunCoordinatorState& State,
		const FDigetherRunTerminationInput& Input);
	static FDigetherRunPublicState MakePublicState(const FDigetherRunCoordinatorState& State,
		bool bGameOver, bool bPaymentDeadline);

#if !UE_BUILD_SHIPPING
	// 개발: 시계 시작 뒤 설정 가능한 최대 남은 시간(탐사 시간 - 확정 상승 페널티 누적), 음수면 0
	static double DevGetMaxRemainingSeconds(const FDigetherRunCoordinatorState& State);
	// 개발: 남은 시간이 RemainingSeconds가 되도록 시작 시각을 옮기고 같은 ServerSeconds로 갱신
	// 0이면 실제 만료 경로(bTimeExpiredNow)를 타며, 시계 시작 전·이미 만료·범위 밖은 거절
	static FDigetherRunAdvanceResult DevSetRemainingSeconds(FDigetherRunCoordinatorState& State,
		double RemainingSeconds, double ServerSeconds);
	// 개발: 정지 중 ServerSeconds까지 흐른 만큼 시작·마지막 갱신 시각을 함께 밀기
	// 둘의 간격(마지막 갱신 기준 경과)이 그대로라 상승 확정의 재계산도 거절되지 않음
	static void DevHoldClock(FDigetherRunCoordinatorState& State, double ServerSeconds);
#endif
};
