#include "Game/UI/Lobby/DigetherLobbyPlayerSlotWidget.h"

#include "CommonTextBlock.h"
#include "Game/Frontend/Lobby/DigetherLobbyPlayerState.h"

#define LOCTEXT_NAMESPACE "DigetherLobbySlot"

void UDigetherLobbyPlayerSlotWidget::ShowPlayer(int32 SlotNumber, const ADigetherLobbyPlayerState* Player, bool bIsLocalPlayer)
{
	Text_Number->SetText(FText::AsNumber(SlotNumber));
	Highlight_Local->SetVisibility(bIsLocalPlayer ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	if (!Player)
	{
		Text_Name->SetText(LOCTEXT("EmptySlot", "빈 자리"));
		if (EmptyNameStyle) Text_Name->SetStyle(EmptyNameStyle);
		Text_Tag->SetText(FText::GetEmpty());
		return;
	}

	const FText Name = FText::FromString(Player->GetPlayerName());
	Text_Name->SetText(bIsLocalPlayer ? FText::Format(LOCTEXT("Me", "{0} (나)"), Name) : Name);
	if (NameStyle) Text_Name->SetStyle(NameStyle);
	if (Player->IsHost())
	{
		Text_Tag->SetText(LOCTEXT("Host", "호스트"));
		if (HostTagStyle) Text_Tag->SetStyle(HostTagStyle);
	}
	else if (Player->IsReady())
	{
		Text_Tag->SetText(LOCTEXT("Ready", "준비 완료"));
		if (ReadyTagStyle) Text_Tag->SetStyle(ReadyTagStyle);
	}
	else
	{
		Text_Tag->SetText(LOCTEXT("Waiting", "준비 중"));
		if (WaitingTagStyle) Text_Tag->SetStyle(WaitingTagStyle);
	}
}

#undef LOCTEXT_NAMESPACE
