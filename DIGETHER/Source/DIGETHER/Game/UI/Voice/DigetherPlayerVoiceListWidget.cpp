#include "Game/UI/Voice/DigetherPlayerVoiceListWidget.h"

#include "Components/PanelWidget.h"
#include "Engine/World.h"
#include "Game/UI/Voice/DigetherPlayerVoiceRowWidget.h"
#include "Game/Voice/DigetherVoiceComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

void UDigetherPlayerVoiceListWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshPlayers();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimer, this, &ThisClass::RefreshPlayers, 1.f, true);
	}
}

void UDigetherPlayerVoiceListWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	Super::NativeDestruct();
}

void UDigetherPlayerVoiceListWidget::RefreshPlayers()
{
	const APlayerController* Controller = GetOwningPlayer();
	UDigetherVoiceComponent* Voice = IsValid(Controller) ? Controller->FindComponentByClass<UDigetherVoiceComponent>() : nullptr;
	const TArray<APlayerState*> Players = Voice != nullptr ? Voice->GetRemoteVoicePlayers() : TArray<APlayerState*>();

	// 같은 사람·같은 순서면 다시 만들지 않음, 슬라이더 조작 중 끊김 방지
	const int32 RowCount = Panel_Players->GetChildrenCount();
	bool bSame = RowCount == Players.Num();
	for (int32 Index = 0; bSame && Index < RowCount; ++Index)
	{
		const UDigetherPlayerVoiceRowWidget* Row = Cast<UDigetherPlayerVoiceRowWidget>(Panel_Players->GetChildAt(Index));
		bSame = Row != nullptr && Row->GetPlayer() == Players[Index];
	}
	if (bSame || !RowClass)
	{
		return;
	}
	Panel_Players->ClearChildren();
	for (APlayerState* Player : Players)
	{
		UDigetherPlayerVoiceRowWidget* Row = CreateWidget<UDigetherPlayerVoiceRowWidget>(this, RowClass);
		Row->SetPlayer(Player, Voice);
		Panel_Players->AddChild(Row);
	}
}
