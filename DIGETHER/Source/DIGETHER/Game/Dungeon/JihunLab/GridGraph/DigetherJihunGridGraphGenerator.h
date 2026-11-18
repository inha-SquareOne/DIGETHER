#pragma once

#include "CoreMinimal.h"
#include "DigetherJihunGridGraphTypes.h"

// 큰 격자 그래프(Skeleton) → 복도·계단·로비·다리 → 교실 → 모듈 변형 선택 → 검증 순서로 학교 던전을 계산
// UWorld를 쓰지 않으며, 같은 Catalog·Rules·Seed면 항상 같은 결과
class DIGETHER_API FDigetherJihunGridGraphGenerator
{
public:
	static FDigetherJihunGridGraphLayout Generate(
		const TArray<FDigetherJihunGridGraphModuleSpec>& Catalog,
		const FDigetherJihunGridGraphRules& Rules,
		int32 Seed);
};
