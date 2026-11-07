#include "Game/UI/Pause/DigetherPauseMenuWidget.h"

#include "Game/Online/DigetherSessionSubsystem.h"
#include "Game/UI/Common/DigetherButtonWidget.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

void UDigetherPauseMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// ESC(뒤로)로 메뉴 닫기, 하위 패널이 열려 있으면 패널이 먼저 받음
	bIsBackHandler = true;
	Btn_Resume->OnClicked().AddUObject(this, &ThisClass::DeactivateWidget);
	Btn_Settings->OnClicked().AddUObject(this, &ThisClass::OpenSettings);
	Btn_MainMenu->OnClicked().AddUObject(this, &ThisClass::ReturnToMainMenu);
	Btn_Quit->OnClicked().AddUObject(this, &ThisClass::QuitGame);
	Stack_Panels->OnDisplayedWidgetChanged().AddUObject(this, &ThisClass::HandleDisplayedPanelChanged);
}

void UDigetherPauseMenuWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	// 다시 열 때는 버튼 목록부터
	Stack_Panels->ClearWidgets();
	SetMenuVisible(true);
	if (const UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this))
	{
		Btn_MainMenu->SetIsEnabled(!Session->IsBusy());
	}
}

UWidget* UDigetherPauseMenuWidget::NativeGetDesiredFocusTarget() const
{
	return Btn_Resume;
}

void UDigetherPauseMenuWidget::OpenSettings()
{
	if (!SettingsPanelClass) return;
	Stack_Panels->ClearWidgets();
	Stack_Panels->AddWidget(SettingsPanelClass);
}

void UDigetherPauseMenuWidget::ReturnToMainMenu()
{
	// 호스트면 세션 종료 후 메뉴, 세션 밖(PIE 직접 실행)이면 메뉴 맵만 열기
	if (UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this))
	{
		Btn_MainMenu->SetIsEnabled(false);
		Session->LeaveRoom();
	}
}

void UDigetherPauseMenuWidget::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UDigetherPauseMenuWidget::HandleDisplayedPanelChanged(UCommonActivatableWidget* Widget)
{
	const bool bPanelOpen = Widget != nullptr;
	SetMenuVisible(!bPanelOpen);
	if (!bPanelOpen && IsActivated())
	{
		// 설정을 닫으면 버튼 목록으로 초점 복귀
		Btn_Settings->SetFocus();
	}
}

void UDigetherPauseMenuWidget::SetMenuVisible(const bool bVisible)
{
	if (Panel_Menu)
	{
		Panel_Menu->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}
