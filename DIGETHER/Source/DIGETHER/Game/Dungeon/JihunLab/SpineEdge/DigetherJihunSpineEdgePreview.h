#pragma once

#include "CoreMinimal.h"
#include "DigetherJihunSpineEdgeTypes.h"

class UWorld;

// 생성 결과 디버그 표시. 게임 상태를 바꾸지 않으며 출시 빌드에서는 아무것도 그리지 않음
namespace DigetherJihunSpineEdgePreview
{
	// 배치 하나의 월드 트랜스폼(모듈 피벗 위치·yaw). Origin은 건물 기준 액터 트랜스폼
	DIGETHER_API FTransform GetPlacementTransform(
		const FDigetherJihunSpineEdgePlacement& Placement,
		const FDigetherJihunSpineEdgeRules& Rules,
		const FTransform& Origin);

	// 모듈 칸 상자(역할별 색)와 통로 면(문·열림) 표시
	DIGETHER_API void DrawLayout(
		const UWorld* World,
		const FTransform& Origin,
		const TArray<FDigetherJihunSpineEdgeModuleSpec>& Catalog,
		const FDigetherJihunSpineEdgeRules& Rules,
		const FDigetherJihunSpineEdgeLayout& Layout,
		bool bPersistent,
		float LifeSeconds);
}
