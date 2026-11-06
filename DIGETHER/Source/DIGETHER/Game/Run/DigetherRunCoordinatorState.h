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
};
