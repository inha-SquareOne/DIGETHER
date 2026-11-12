#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DigetherDainGameMode.generated.h"
// 개인 시험 Pawn·HUD 지정, 공용 GameMode와 분리
UCLASS()
class ADigetherDainGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ADigetherDainGameMode();
	virtual void BeginPlay() override;
};
