#include "Game/UI/Common/DigetherButtonWidget.h"

#include "CommonTextBlock.h"

void UDigetherButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (Text_Label) Text_Label->SetText(ButtonText);
	NativeOnCurrentTextStyleChanged();
}

void UDigetherButtonWidget::NativeOnCurrentTextStyleChanged()
{
	Super::NativeOnCurrentTextStyleChanged();
	if (Text_Label)
	{
		if (const TSubclassOf<UCommonTextStyle> TextStyle = GetCurrentTextStyleClass()) Text_Label->SetStyle(TextStyle);
	}
}

void UDigetherButtonWidget::SetButtonText(const FText& InText)
{
	ButtonText = InText;
	if (Text_Label) Text_Label->SetText(ButtonText);
}
