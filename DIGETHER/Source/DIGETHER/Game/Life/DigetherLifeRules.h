#pragma once

#include "Game/Life/DigetherLifeTypes.h"

// 입출력은 값뿐이다. Pawn·Controller·시각·월드에 의존하지 않는다.
class DIGETHER_API FDigetherLifeRules
{
public:
	static bool IsValidLifeState(EDigetherLifeState Life);
	static bool IsValidPlayerKey(const FString& PlayerKey);
	static bool IsMissingCause(EDigetherDeathCause Cause);
	static bool IsValidState(const FDigetherLifeComponentState& State);

	// 실패 결과도 입력 상태를 그대로 보존하여 보관 값과 접속 중 값에 같은 규칙을 쓴다.
	static FDigetherLifeUpdateResult ApplyDeath(const FDigetherLifeComponentState& State, EDigetherDeathCause Cause);
	static FDigetherLifeUpdateResult ApplyEscape(const FDigetherLifeComponentState& State, EDigetherEscapeKind Kind);
	static FDigetherLifeUpdateResult Revive(const FDigetherLifeComponentState& State);

	// 모든 ID와 실제 사용할 위치를 먼저 검사한다. 실패 시 부분 드롭 계획을 돌려주지 않는다.
	static FDigetherDeathDropPlan PlanDeathDrop(const TArray<FDigetherLifeDropItem>& Items,
		const FVector& PlayerLocation, const TOptional<FVector>& DropLocation = TOptional<FVector>());

	static FDigetherSpectateSelectionResult SelectSpectateTarget(const TArray<FDigetherSpectateCandidate>& Candidates,
		const FString& SelfPlayerKey, const FString& CurrentTargetPlayerKey, EDigetherSpectateDirection Direction);

	// 한도는 런 시작에 고정한 튜닝 스냅샷에서 받는다. 신체 무게는 0이며 항목에 넣지 않는다.
	static EDigetherEmergencyEscapeResult CheckEmergencyEscape(EDigetherLifeState Life,
		const TArray<FDigetherEmergencyCarriedItem>& Items, float EmergencyWeightLimitKg);
};
