#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DigetherExcavationLabGameMode.generated.h"
// 개인 시험 Pawn·HUD 지정, 공용 GameMode와 분리
UCLASS()
class ADigetherExcavationLabGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ADigetherExcavationLabGameMode();
	virtual void BeginPlay() override;
};
