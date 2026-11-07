#include "Game/UI/Settings/DigetherKeyBindingRowWidget.h"

#include "CommonTextBlock.h"
#include "Game/UI/Common/DigetherButtonWidget.h"

#define LOCTEXT_NAMESPACE "DigetherSettings"

void UDigetherKeyBindingRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Btn_Key->OnClicked().AddUObject(this, &ThisClass::HandleClicked);
}

void UDigetherKeyBindingRowWidget::Setup(const FName InMappingName, const FText& InDisplayName, const FKey& InKey)
{
	MappingName = InMappingName;
	Text_Action->SetText(InDisplayName);
	SetKey(InKey);
}

void UDigetherKeyBindingRowWidget::SetKey(const FKey& InKey)
{
	Key = InKey;
	Btn_Key->SetButtonText(InKey.IsValid() ? InKey.GetDisplayName(false) : LOCTEXT("KeyNone", "없음"));
}

void UDigetherKeyBindingRowWidget::SetListening(const bool bListening)
{
	if (bListening)
	{
		Btn_Key->SetButtonText(LOCTEXT("KeyListening", "키 입력..."));
	}
	else
	{
		SetKey(Key);
	}
}

void UDigetherKeyBindingRowWidget::HandleClicked()
{
	OnClicked.ExecuteIfBound(MappingName);
}

#undef LOCTEXT_NAMESPACE
