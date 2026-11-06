#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "DigetherLobbyPlayerSlotWidget.generated.h"

class UCommonTextBlock;
class UCommonTextStyle;
class ADigetherLobbyPlayerState;

// 대기실 참가자 한 칸(번호·이름·나 표시·호스트/준비 상태 또는 빈 자리), 배치는 WBP_LobbyPlayerSlot
UCLASS(Abstract)
class DIGETHER_API UDigetherLobbyPlayerSlotWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// Player null이면 빈 자리
	void ShowPlayer(int32 SlotNumber, const ADigetherLobbyPlayerState* Player, bool bIsLocalPlayer);

protected:
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_Number;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_Name;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_Tag;
	// 내 칸 강조 테두리, 모양은 WBP 담당
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> Highlight_Local;

	UPROPERTY(EditDefaultsOnly, Category = "Digether|Style") TSubclassOf<UCommonTextStyle> NameStyle;
	UPROPERTY(EditDefaultsOnly, Category = "Digether|Style") TSubclassOf<UCommonTextStyle> EmptyNameStyle;
	UPROPERTY(EditDefaultsOnly, Category = "Digether|Style") TSubclassOf<UCommonTextStyle> HostTagStyle;
	UPROPERTY(EditDefaultsOnly, Category = "Digether|Style") TSubclassOf<UCommonTextStyle> ReadyTagStyle;
	UPROPERTY(EditDefaultsOnly, Category = "Digether|Style") TSubclassOf<UCommonTextStyle> WaitingTagStyle;
};
