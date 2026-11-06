#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "DigetherFrontendLayoutWidget.generated.h"

class UCommonActivatableWidget;
class UCommonActivatableWidgetStack;

// 프런트엔드 최상위 레이아웃, 화면(메인 메뉴·대기실)은 CommonUI 스택에 적재, 배치는 WBP_FrontendLayout
UCLASS(Abstract)
class DIGETHER_API UDigetherFrontendLayoutWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UCommonActivatableWidget* PushScreen(TSubclassOf<UCommonActivatableWidget> ScreenClass);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> Stack_Screens;
};
