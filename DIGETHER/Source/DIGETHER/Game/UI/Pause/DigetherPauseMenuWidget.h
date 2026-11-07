#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "DigetherPauseMenuWidget.generated.h"

class UCommonActivatableWidgetStack;
class UDigetherButtonWidget;

// 게임 중 ESC 메뉴(계속하기·설정·메인 메뉴·게임 종료), 멀티플레이라 월드는 멈추지 않음
// 설정 등 하위 패널은 Stack_Panels에 적재, 배치·모양은 WBP_PauseMenu 담당
UCLASS(Abstract)
class DIGETHER_API UDigetherPauseMenuWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_Resume;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_Settings;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_MainMenu;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_Quit;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonActivatableWidgetStack> Stack_Panels;
	// 하위 패널이 열린 동안 숨길 버튼 묶음, 없으면 버튼을 그대로 둠
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Panel_Menu;

	UPROPERTY(EditDefaultsOnly, Category = "Digether|Panels") TSubclassOf<UCommonActivatableWidget> SettingsPanelClass;

private:
	void OpenSettings();
	void ReturnToMainMenu();
	void QuitGame();
	void HandleDisplayedPanelChanged(UCommonActivatableWidget* Widget);
	void SetMenuVisible(bool bVisible);
};
