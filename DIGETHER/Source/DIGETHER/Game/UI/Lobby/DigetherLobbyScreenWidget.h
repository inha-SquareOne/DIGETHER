#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "DigetherLobbyScreenWidget.generated.h"

class UCommonTextBlock;
class UDynamicEntryBox;
class UDigetherButtonWidget;
class ADigetherLobbyGameState;
class ADigetherLobbyPlayerController;
class ADigetherLobbyPlayerState;

// 대기실 화면: 방 정보, 참가자 칸, 준비/시작·초대·코드 복사·나가기. 대기실 GameState의 변경 알림으로 갱신한다.
// 배치·모양은 WBP_LobbyScreen이 정한다.
UCLASS(Abstract)
class DIGETHER_API UDigetherLobbyScreenWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_RoomName;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_RoomCode;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_PlayerCount;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_Hint;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_Status;
	// 참가자 칸 목록. 항목 위젯은 WBP_LobbyPlayerSlot.
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDynamicEntryBox> Box_Players;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_Primary;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_Invite;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_CopyCode;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_Leave;

	// '코드 복사' 같은 화면 안내를 보여 주는 시간.
	UPROPERTY(EditDefaultsOnly, Category = "Digether|Lobby") float LocalNoticeSeconds = 2.5f;

private:
	void HandleSessionStatusChanged();
	void HandleLobbyChanged();
	void HandlePrimaryAction();
	void HandleInvite();
	void HandleCopyCode();
	void HandleLeave();
	void ClearLocalNotice();
	void BindGameState();
	void Refresh();
	ADigetherLobbyGameState* GetLobbyGameState() const;
	ADigetherLobbyPlayerController* GetLobbyController() const;
	ADigetherLobbyPlayerState* GetLocalLobbyState() const;

	TWeakObjectPtr<ADigetherLobbyGameState> BoundGameState;
	FDelegateHandle LobbyChangedHandle;
	FDelegateHandle StatusChangedHandle;
	FTimerHandle NoticeTimer;
	FTimerHandle GameStateWaitTimer;
	FText LocalNotice;
};
