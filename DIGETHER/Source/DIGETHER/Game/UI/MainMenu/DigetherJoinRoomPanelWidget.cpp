#include "Game/UI/MainMenu/DigetherJoinRoomPanelWidget.h"

#include "CommonListView.h"
#include "CommonTextBlock.h"
#include "Components/EditableTextBox.h"
#include "Game/Online/DigetherSessionSubsystem.h"
#include "Game/UI/Common/DigetherButtonWidget.h"
#include "Game/UI/MainMenu/DigetherRoomListItem.h"

#define LOCTEXT_NAMESPACE "DigetherJoinRoom"

UDigetherJoinRoomPanelWidget::UDigetherJoinRoomPanelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
}

void UDigetherJoinRoomPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Btn_JoinCode->OnClicked().AddUObject(this, &ThisClass::JoinByCode);
	Btn_Refresh->OnClicked().AddUObject(this, &ThisClass::RefreshRooms);
	Btn_JoinSelected->OnClicked().AddUObject(this, &ThisClass::JoinSelected);
	Btn_Close->OnClicked().AddUObject(this, &ThisClass::DeactivateWidget);
	Input_Code->OnTextChanged.AddDynamic(this, &ThisClass::HandleCodeChanged);
	List_Rooms->OnItemSelectionChanged().AddUObject(this, &ThisClass::HandleSelectionChanged);
}

// CommonUI 스택은 닫힌 패널 인스턴스를 다시 쓰므로 NativeOnInitialized가 아니라 Construct/Destruct 짝에서 구독한다.
void UDigetherJoinRoomPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this))
	{
		StatusChangedHandle = Session->OnStatusChanged.AddUObject(this, &ThisClass::HandleSessionStatusChanged);
		RoomsUpdatedHandle = Session->OnRoomsUpdated.AddUObject(this, &ThisClass::HandleRoomsUpdated);
	}
}

void UDigetherJoinRoomPanelWidget::NativeDestruct()
{
	if (UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this))
	{
		Session->OnStatusChanged.Remove(StatusChangedHandle);
		Session->OnRoomsUpdated.Remove(RoomsUpdatedHandle);
	}
	StatusChangedHandle.Reset();
	RoomsUpdatedHandle.Reset();
	Super::NativeDestruct();
}

void UDigetherJoinRoomPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	RefreshRooms();
	RefreshState();
}

UWidget* UDigetherJoinRoomPanelWidget::NativeGetDesiredFocusTarget() const
{
	return Input_Code;
}

void UDigetherJoinRoomPanelWidget::HandleCodeChanged(const FText& Text)
{
	RefreshState();
}

void UDigetherJoinRoomPanelWidget::HandleSelectionChanged(UObject* Item)
{
	const UDigetherRoomListItem* RoomItem = Cast<UDigetherRoomListItem>(Item);
	SelectedRoomIndex = RoomItem ? RoomItem->Room.Index : INDEX_NONE;
	RefreshState();
}

void UDigetherJoinRoomPanelWidget::HandleSessionStatusChanged()
{
	RefreshState();
}

void UDigetherJoinRoomPanelWidget::HandleRoomsUpdated()
{
	const UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this);
	if (!Session)
	{
		return;
	}
	RoomItems.Reset();
	for (const FDigetherRoomInfo& Room : Session->GetRooms())
	{
		UDigetherRoomListItem* Item = NewObject<UDigetherRoomListItem>(this);
		Item->Room = Room;
		RoomItems.Add(Item);
	}
	SelectedRoomIndex = INDEX_NONE;
	List_Rooms->SetListItems(RoomItems);
	Text_Empty->SetText(LOCTEXT("NoRooms", "열린 공개 방이 없습니다. 새로고침하거나 방 코드로 참가하세요."));
	Text_Empty->SetVisibility(RoomItems.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!RoomItems.IsEmpty())
	{
		List_Rooms->SetSelectedIndex(0);
	}
	RefreshState();
}

void UDigetherJoinRoomPanelWidget::RefreshRooms()
{
	UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this);
	if (Session && !Session->IsBusy())
	{
		Session->FindRooms();
	}
}

void UDigetherJoinRoomPanelWidget::JoinByCode()
{
	if (UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this))
	{
		// 자동 생성 코드는 대문자이므로 입력을 대문자로 맞춘다.
		Session->JoinByCode(Input_Code->GetText().ToString().TrimStartAndEnd().ToUpper());
	}
}

void UDigetherJoinRoomPanelWidget::JoinSelected()
{
	if (UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this))
	{
		Session->JoinRoom(SelectedRoomIndex);
	}
}

void UDigetherJoinRoomPanelWidget::RefreshState()
{
	const UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this);
	const bool bReady = Session && !Session->IsBusy() && Session->IsOnlineAvailable();
	Btn_JoinCode->SetIsEnabled(bReady && !Input_Code->GetText().IsEmpty());
	Btn_Refresh->SetIsEnabled(bReady);
	Btn_JoinSelected->SetIsEnabled(bReady && SelectedRoomIndex != INDEX_NONE);
}

#undef LOCTEXT_NAMESPACE
