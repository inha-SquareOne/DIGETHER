#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DigetherDainHud.generated.h"
UCLASS()
class ADigetherDainHud : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
};
