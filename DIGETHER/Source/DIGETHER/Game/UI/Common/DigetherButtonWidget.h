#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "DigetherButtonWidget.generated.h"

class UCommonTextBlock;

// 글자 하나를 가진 공용 버튼. 모양은 CommonUI 버튼 스타일 에셋, 배치는 WBP_Button이 정한다.
UCLASS(Abstract)
class DIGETHER_API UDigetherButtonWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	void SetButtonText(const FText& InText);

protected:
	virtual void NativePreConstruct() override;
	// 버튼 스타일이 상태(기본·호버·비활성 등)별로 정한 글자 스타일을 라벨에 적용한다.
	virtual void NativeOnCurrentTextStyleChanged() override;

	// 화면 WBP에서 버튼마다 지정하는 글자.
	UPROPERTY(EditAnywhere, Category = "Digether|Button")
	FText ButtonText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Label;
};
