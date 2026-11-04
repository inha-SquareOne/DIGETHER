#pragma once

#include "CoreMinimal.h"
#include "Game/Data/DigetherTuningProfile.h"
#include "Game/Run/DigetherRunRuleTuning.h"
#include "Game/Run/DigetherRunRules.h"

// 서버 값만 보관해 월드가 없는 보관 레코드에도 같은 시간·종료 규칙을 적용한다.
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
};

struct DIGETHER_API FDigetherRunAdvanceResult
{
	EDigetherRunResult Result = EDigetherRunResult::InvalidInput;
	bool bTimeExpiredNow = false;
};

class DIGETHER_API FDigetherRunCoordinatorRules
{
public:
	static FDigetherRunAdvanceResult StartRun(FDigetherRunCoordinatorState& State,
		const FDigetherRunTuning& Tuning, const FDigetherRunRuleTuning& RuleTuning, double ServerSeconds);
	static FDigetherRunAdvanceResult RecordConfirmedAscent(FDigetherRunCoordinatorState& State);
	static FDigetherRunAdvanceResult UpdateTime(FDigetherRunCoordinatorState& State, double ServerSeconds);
	static FDigetherRunTerminationResult CompleteRun(FDigetherRunCoordinatorState& State,
		const FDigetherRunTerminationInput& Input);
	static FDigetherRunPublicState MakePublicState(const FDigetherRunCoordinatorState& State,
		bool bGameOver, bool bPaymentDeadline);
};
