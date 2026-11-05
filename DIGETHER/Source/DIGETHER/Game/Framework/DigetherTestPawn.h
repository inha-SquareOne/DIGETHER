#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DefaultPawn.h"
#include "DigetherTestPawn.generated.h"

// 임시 시험 Pawn, 정식 캐릭터(플레이어 담당)로 교체 예정
UCLASS()
class DIGETHER_API ADigetherTestPawn : public ADefaultPawn
{
	GENERATED_BODY()

public:
	ADigetherTestPawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
