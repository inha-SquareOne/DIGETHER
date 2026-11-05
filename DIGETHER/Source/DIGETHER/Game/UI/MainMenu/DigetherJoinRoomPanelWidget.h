#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "DigetherJoinRoomPanelWidget.generated.h"

class UCommonListView;
class UCommonTextBlock;
class UEditableTextBox;
class UDigetherButtonWidget;
class UDigetherRoomListItem;

// '방 참가' 패널: 방 코드 입력과 공개 방 목록. Steam 초대 수락은 세션 서브시스템이 직접 처리한다.
UCLASS(Abstract)
class DIGETHER_API UDigetherJoinRoomPanelWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UDigetherJoinRoomPanelWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UEditableTextBox> Input_Code;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_JoinCode;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_Refresh;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_JoinSelected;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_Close;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonListView> List_Rooms;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_Empty;

private:
	UFUNCTION() void HandleCodeChanged(const FText& Text);
	void HandleSelectionChanged(UObject* Item);
	void HandleSessionStatusChanged();
	void HandleRoomsUpdated();
	void RefreshRooms();
	void JoinByCode();
	void JoinSelected();
	void RefreshState();

	UPROPERTY(Transient) TArray<TObjectPtr<UDigetherRoomListItem>> RoomItems;
	int32 SelectedRoomIndex = INDEX_NONE;
	FDelegateHandle StatusChangedHandle;
	FDelegateHandle RoomsUpdatedHandle;
};
