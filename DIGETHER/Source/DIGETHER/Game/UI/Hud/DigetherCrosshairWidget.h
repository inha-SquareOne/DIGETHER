#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "DigetherCrosshairWidget.generated.h"

class SImage;

// 화면 중앙 조준점(에임), 조종 중인 로컬 플레이어 화면에만 있음
// 평소 흰 점, E로 상호작용할 대상을 조준하면 커지고 노란색이 됨. 사망하면 숨김
// 모양은 코드(Slate)로 만들어 에셋이 없음, 정식 HUD(P2-7)가 생기면 그쪽으로 옮김
UCLASS()
class DIGETHER_API UDigetherCrosshairWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	TSharedPtr<SImage> DotImage;

	// 평소·상호작용 대상 조준 중 모양
	FSlateBrush NormalBrush;
	FSlateBrush FocusedBrush;
	bool bShowingFocused = false;
};
