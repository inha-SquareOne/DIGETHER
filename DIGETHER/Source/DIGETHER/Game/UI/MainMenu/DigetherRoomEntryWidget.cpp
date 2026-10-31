#include "Game/UI/MainMenu/DigetherRoomEntryWidget.h"

#include "CommonTextBlock.h"
#include "Game/UI/MainMenu/DigetherRoomListItem.h"

#define LOCTEXT_NAMESPACE "DigetherRoomEntry"

UDigetherRoomEntryWidget::UDigetherRoomEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSelectable = true;
}

void UDigetherRoomEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	const UDigetherRoomListItem* Item = Cast<UDigetherRoomListItem>(ListItemObject);
	if (!Item) return;
	const FDigetherRoomInfo& Room = Item->Room;
	Text_Name->SetText(FText::Format(LOCTEXT("NameAndCode", "{0}  [{1}]"), FText::FromString(Room.RoomName), FText::FromString(Room.RoomCode)));
	Text_Host->SetText(FText::FromString(Room.HostName));
	Text_Players->SetText(FText::Format(LOCTEXT("Players", "{0}/{1}"), Room.CurrentPlayers, Room.MaxPlayers));
	Text_State->SetText(Room.bInGame ? LOCTEXT("InGame", "탐사 중") : LOCTEXT("Waiting", "대기 중"));
	Text_Ping->SetText(Room.PingMs > 0 ? FText::Format(LOCTEXT("Ping", "{0} ms"), Room.PingMs) : FText::GetEmpty());
}

void UDigetherRoomEntryWidget::NativeOnItemSelectionChanged(bool bIsSelected)
{
	IUserObjectListEntry::NativeOnItemSelectionChanged(bIsSelected);
	SetIsSelected(bIsSelected, false);
}

#undef LOCTEXT_NAMESPACE
