#include "Game/UI/MainMenu/DigetherCreateRoomPanelWidget.h"

#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Game/Online/DigetherSessionSubsystem.h"
#include "Game/UI/Common/DigetherButtonWidget.h"

UDigetherCreateRoomPanelWidget::UDigetherCreateRoomPanelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
}

void UDigetherCreateRoomPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Btn_Create->OnClicked().AddUObject(this, &ThisClass::CreateRoom);
	Btn_Close->OnClicked().AddUObject(this, &ThisClass::DeactivateWidget);
}

// CommonUI 스택은 닫힌 패널 인스턴스를 다시 쓰므로 NativeOnInitialized가 아니라 Construct/Destruct 짝에서 구독한다.
void UDigetherCreateRoomPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this))
	{
		StatusChangedHandle = Session->OnStatusChanged.AddUObject(this, &ThisClass::HandleSessionStatusChanged);
	}
}

void UDigetherCreateRoomPanelWidget::NativeDestruct()
{
	if (UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this))
	{
		Session->OnStatusChanged.Remove(StatusChangedHandle);
	}
	StatusChangedHandle.Reset();
	Super::NativeDestruct();
}

void UDigetherCreateRoomPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	const UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this);
	if (Session && Input_RoomName->GetText().IsEmpty())
	{
		Input_RoomName->SetText(FText::FromString(Session->MakeDefaultRoomName()));
	}
	RefreshState();
}

UWidget* UDigetherCreateRoomPanelWidget::NativeGetDesiredFocusTarget() const
{
	return Input_RoomName;
}

void UDigetherCreateRoomPanelWidget::HandleSessionStatusChanged()
{
	RefreshState();
}

void UDigetherCreateRoomPanelWidget::CreateRoom()
{
	if (UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this))
	{
		Session->HostRoom(Input_RoomName->GetText().ToString(), Check_Public->IsChecked());
	}
}

void UDigetherCreateRoomPanelWidget::RefreshState()
{
	const UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this);
	Btn_Create->SetIsEnabled(Session && !Session->IsBusy() && Session->IsOnlineAvailable());
}
