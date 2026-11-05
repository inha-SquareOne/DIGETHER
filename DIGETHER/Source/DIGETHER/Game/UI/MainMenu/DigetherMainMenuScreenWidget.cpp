#include "Game/UI/MainMenu/DigetherMainMenuScreenWidget.h"

#include "CommonTextBlock.h"
#include "Game/Online/DigetherSessionSubsystem.h"
#include "Game/UI/Common/DigetherButtonWidget.h"
#include "Input/CommonUIInputTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#define LOCTEXT_NAMESPACE "DigetherMainMenu"

void UDigetherMainMenuScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Btn_CreateRoom->OnClicked().AddUObject(this, &ThisClass::OpenPanel, CreateRoomPanelClass);
	Btn_JoinRoom->OnClicked().AddUObject(this, &ThisClass::OpenPanel, JoinRoomPanelClass);
	Btn_Settings->OnClicked().AddUObject(this, &ThisClass::OpenPanel, SettingsPanelClass);
	Btn_Quit->OnClicked().AddUObject(this, &ThisClass::QuitGame);
}

// CommonUI 스택의 화면 인스턴스 재사용 대비, NativeOnInitialized 대신 Construct/Destruct 짝에서 구독
void UDigetherMainMenuScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this))
	{
		// 메뉴 복귀 이유는 다음 패널을 열 때까지 표시
		NoticeMessage = Session->ConsumeNotice();
		StatusChangedHandle = Session->OnStatusChanged.AddUObject(this, &ThisClass::HandleSessionStatusChanged);
	}
}

void UDigetherMainMenuScreenWidget::NativeDestruct()
{
	if (UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this))
	{
		Session->OnStatusChanged.Remove(StatusChangedHandle);
	}
	StatusChangedHandle.Reset();
	Super::NativeDestruct();
}

void UDigetherMainMenuScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	RefreshState();
}

UWidget* UDigetherMainMenuScreenWidget::NativeGetDesiredFocusTarget() const
{
	return Btn_CreateRoom;
}

TOptional<FUIInputConfig> UDigetherMainMenuScreenWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void UDigetherMainMenuScreenWidget::HandleSessionStatusChanged()
{
	RefreshState();
}

void UDigetherMainMenuScreenWidget::OpenPanel(TSubclassOf<UCommonActivatableWidget> PanelClass)
{
	if (!PanelClass || !Stack_Panels) return;
	NoticeMessage = FText::GetEmpty();
	// 패널은 한 번에 하나, 닫기·Esc(뒤로)는 패널 자체 비활성화로 스택에서 제거
	Stack_Panels->ClearWidgets();
	Stack_Panels->AddWidget(PanelClass);
	RefreshState();
}

void UDigetherMainMenuScreenWidget::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UDigetherMainMenuScreenWidget::RefreshState()
{
	const UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this);
	if (!Session)
	{
		return;
	}
	const bool bCanRequestRoom = Session->CanStartRoomRequest();

	const bool bShowNotice = !NoticeMessage.IsEmpty();
	Text_Status->SetText(bShowNotice ? NoticeMessage : Session->GetStatus());
	if (const TSubclassOf<UCommonTextStyle> Style = bShowNotice ? NoticeTextStyle : StatusTextStyle)
	{
		Text_Status->SetStyle(Style);
	}
	Text_Account->SetText(Session->IsSteamMode()
		? FText::Format(LOCTEXT("SteamAccount", "Steam 계정: {0}"), FText::FromString(Session->GetLocalPlayerName()))
		: LOCTEXT("LanMode", "Steam 미연결: 같은 네트워크(LAN) 시험 모드"));

	Btn_CreateRoom->SetIsEnabled(bCanRequestRoom);
	Btn_JoinRoom->SetIsEnabled(bCanRequestRoom);
	Btn_Settings->SetIsEnabled(!Session->IsBusy());
}

#undef LOCTEXT_NAMESPACE
