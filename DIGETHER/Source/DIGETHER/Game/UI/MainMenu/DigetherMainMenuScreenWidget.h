#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "DigetherMainMenuScreenWidget.generated.h"

class UCommonActivatableWidgetStack;
class UCommonTextBlock;
class UCommonTextStyle;
class UDigetherButtonWidget;

// 메인 메뉴 화면(방 만들기·방 참가·설정·종료, 상태 안내), 기능 패널은 Stack_Panels에 적재
// 배치·모양은 WBP_MainMenuScreen 담당
UCLASS(Abstract)
class DIGETHER_API UDigetherMainMenuScreenWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_CreateRoom;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_JoinRoom;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_Settings;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_Quit;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_Status;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_Account;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonActivatableWidgetStack> Stack_Panels;

	UPROPERTY(EditDefaultsOnly, Category = "Digether|Panels") TSubclassOf<UCommonActivatableWidget> CreateRoomPanelClass;
	UPROPERTY(EditDefaultsOnly, Category = "Digether|Panels") TSubclassOf<UCommonActivatableWidget> JoinRoomPanelClass;
	UPROPERTY(EditDefaultsOnly, Category = "Digether|Panels") TSubclassOf<UCommonActivatableWidget> SettingsPanelClass;

	// 평소 상태 문구·메뉴 복귀 이유(연결 끊김 등) 안내 문구 스타일
	UPROPERTY(EditDefaultsOnly, Category = "Digether|Style") TSubclassOf<UCommonTextStyle> StatusTextStyle;
	UPROPERTY(EditDefaultsOnly, Category = "Digether|Style") TSubclassOf<UCommonTextStyle> NoticeTextStyle;

private:
	void HandleSessionStatusChanged();
	void OpenPanel(TSubclassOf<UCommonActivatableWidget> PanelClass);
	void QuitGame();
	void RefreshState();

	// 메뉴 복귀 이유, 비어 있지 않으면 상태 문구보다 우선
	FText NoticeMessage;
	FDelegateHandle StatusChangedHandle;
};
