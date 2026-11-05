#include "Game/UI/Common/DigetherTabListWidget.h"

#include "CommonButtonBase.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

void UDigetherTabListWidget::HandleTabCreation_Implementation(FName TabNameID, UCommonButtonBase* TabButton)
{
	Super::HandleTabCreation_Implementation(TabNameID, TabButton);
	if (!Box_Tabs || !TabButton) return;
	if (UHorizontalBoxSlot* TabSlot = Box_Tabs->AddChildToHorizontalBox(TabButton))
	{
		TabSlot->SetPadding(TabPadding);
	}
}

void UDigetherTabListWidget::HandleTabRemoval_Implementation(FName TabNameID, UCommonButtonBase* TabButton)
{
	Super::HandleTabRemoval_Implementation(TabNameID, TabButton);
	if (TabButton) TabButton->RemoveFromParent();
}
