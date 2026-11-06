#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DigetherDainGameMode.generated.h"
UCLASS()
class ADigetherDainGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ADigetherDainGameMode();
	virtual void BeginPlay() override;
};
