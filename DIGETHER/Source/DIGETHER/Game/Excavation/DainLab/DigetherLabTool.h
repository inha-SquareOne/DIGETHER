#pragma once

#include "CoreMinimal.h"
#include "DigetherLabTool.generated.h"

// 개인 시험 입력에 사용하는 식별자. 기존 장비의 직렬화 값을 유지한다.
UENUM()
enum class EDigetherLabTool : uint8
{
	Pickaxe,
	Shovel,
	Hands,
	SteelPickaxe UMETA(DisplayName = "Steel Pickaxe"),
	UpperPickaxe UMETA(DisplayName = "Upper Pickaxe"),
	Drill
};
