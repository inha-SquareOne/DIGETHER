#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "DigetherButtonWidget.generated.h"

class UCommonTextBlock;

// 라벨 하나짜리 공용 버튼, 모양은 CommonUI 버튼 스타일 에셋·배치는 WBP_Button 담당
UCLASS(Abstract)
class DIGETHER_API UDigetherButtonWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	void SetButtonText(const FText& InText);

protected:
	virtual void NativePreConstruct() override;
	// 버튼 스타일의 상태별(기본·호버·비활성 등) 글자 스타일을 라벨에 반영
	virtual void NativeOnCurrentTextStyleChanged() override;

	// 화면 WBP의 버튼별 지정값, 실행 중 교체는 SetButtonText
	UPROPERTY(EditAnywhere, Category = "Digether|Button")
	FText ButtonText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Label;
};
