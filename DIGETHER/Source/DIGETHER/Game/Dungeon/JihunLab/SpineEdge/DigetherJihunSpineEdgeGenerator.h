#pragma once

#include "CoreMinimal.h"
#include "DigetherJihunSpineEdgeTypes.h"

// 배치된 모듈 면 하나를 월드 cm 좌표로 옮긴 것. 연결 검사·디버그 표시용
struct FDigetherJihunSpineEdgeWorldEdge
{
	int32 Floor = 0;
	// 면이 바라보는 월드 방향
	EDigetherJihunSpineEdgeDir Dir = EDigetherJihunSpineEdgeDir::North;
	// N·S면은 월드 Y, W·E면은 월드 X
	int32 LineCm = 0;
	int32 StartCm = 0;
	int32 EndCm = 0;
	EDigetherJihunSpineEdgeType Type = EDigetherJihunSpineEdgeType::Open;
};

// 뼈대(복도 갈래) → 교실 → 모듈 변형 선택 → 검증 순서로 건물을 계산
// UWorld를 쓰지 않으며, 같은 Catalog·Rules·Seed면 항상 같은 결과
class DIGETHER_API FDigetherJihunSpineEdgeGenerator
{
public:
	static FDigetherJihunSpineEdgeLayout Generate(
		const TArray<FDigetherJihunSpineEdgeModuleSpec>& Catalog,
		const FDigetherJihunSpineEdgeRules& Rules,
		int32 Seed);
};

namespace DigetherJihunSpineEdge
{
	// 배치가 차지하는 칸 사각형(층 범위는 Placement.Floor부터 Spec.FloorSpan개)
	DIGETHER_API void GetPlacementRect(
		const FDigetherJihunSpineEdgeModuleSpec& Spec,
		const FDigetherJihunSpineEdgePlacement& Placement,
		FIntPoint& OutMin,
		FIntPoint& OutSize);

	DIGETHER_API void GetWorldEdges(
		const FDigetherJihunSpineEdgeModuleSpec& Spec,
		const FDigetherJihunSpineEdgePlacement& Placement,
		int32 UnitSizeCm,
		TArray<FDigetherJihunSpineEdgeWorldEdge>& OutEdges);
}
