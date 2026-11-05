#include "Game/UI/Common/DigetherFrontendLayoutWidget.h"

#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

UCommonActivatableWidget* UDigetherFrontendLayoutWidget::PushScreen(TSubclassOf<UCommonActivatableWidget> ScreenClass)
{
	return Stack_Screens && ScreenClass ? Stack_Screens->AddWidget(ScreenClass) : nullptr;
}
