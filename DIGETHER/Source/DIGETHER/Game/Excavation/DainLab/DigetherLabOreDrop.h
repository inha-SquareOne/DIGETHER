#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DigetherLabOreDrop.generated.h"

// 광맥 바위의 최종 파괴를 확인하기 위한 물리 광석. 정식 아이템·인벤토리와 독립이다.
UCLASS()
class ADigetherLabOreDrop : public AActor
{
	GENERATED_BODY()
public:
	ADigetherLabOreDrop();
};
