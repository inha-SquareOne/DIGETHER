#pragma once

#include "CoreMinimal.h"
#include "DigetherJihunGridGraphTypes.h"

class UWorld;

// 생성 결과 디버그 표시. 게임 상태를 바꾸지 않으며 출시 빌드에서는 아무것도 그리지 않음
namespace DigetherJihunGridGraphPreview
{
	// 배치 하나의 월드 트랜스폼(모듈 피벗 위치·yaw). Origin은 던전 기준 액터 트랜스폼
	DIGETHER_API FTransform GetPlacementTransform(
		const FDigetherJihunGridGraphPlacement& Placement,
		const FDigetherJihunGridGraphRules& Rules,
		const FTransform& Origin);

	// 모듈 칸 상자(역할별 색)와 통로 면(문·열림) 표시. OnlyFloor가 0 이상이면 그 층에 걸친 모듈만
	DIGETHER_API void DrawLayout(
		const UWorld* World,
		const FTransform& Origin,
		const TArray<FDigetherJihunGridGraphModuleSpec>& Catalog,
		const FDigetherJihunGridGraphRules& Rules,
		const FDigetherJihunGridGraphLayout& Layout,
		int32 OnlyFloor,
		bool bPersistent,
		float LifeSeconds);
}
