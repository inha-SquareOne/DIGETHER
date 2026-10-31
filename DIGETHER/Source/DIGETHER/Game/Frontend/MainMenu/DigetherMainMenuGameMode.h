#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DigetherMainMenuGameMode.generated.h"

// 메인 메뉴 맵: 캐릭터 없이 메뉴 화면만 띄운다.
UCLASS()
class DIGETHER_API ADigetherMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADigetherMainMenuGameMode();

protected:
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
};
