#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DigetherDainHud.generated.h"
// 조준선·조작 안내만 담당, 굴착 상태 소유 없음
UCLASS()
class ADigetherDainHud : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
};
