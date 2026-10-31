#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "DigetherCreateRoomPanelWidget.generated.h"

class UCheckBox;
class UEditableTextBox;
class UDigetherButtonWidget;

// '방 만들기' 패널: 방 이름·공개 여부를 받아 세션 서브시스템에 방 생성을 요청한다. Esc·닫기로 닫힌다.
UCLASS(Abstract)
class DIGETHER_API UDigetherCreateRoomPanelWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UDigetherCreateRoomPanelWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UEditableTextBox> Input_RoomName;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCheckBox> Check_Public;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_Create;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_Close;

private:
	void HandleSessionStatusChanged();
	void CreateRoom();
	void RefreshState();

	FDelegateHandle StatusChangedHandle;
};
