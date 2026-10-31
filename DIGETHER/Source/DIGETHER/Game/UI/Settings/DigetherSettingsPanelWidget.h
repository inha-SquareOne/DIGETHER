#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "DigetherSettingsPanelWidget.generated.h"

class UCheckBox;
class UComboBoxString;
class UCommonAnimatedSwitcher;
class UCommonTextBlock;
class USlider;
class UDigetherButtonWidget;
class UDigetherTabListWidget;

// 그래픽·사운드·조작 설정 패널. 탭은 CommonUI 탭 목록, 값은 '적용'을 눌러야 GameUserSettings에 저장된다.
UCLASS(Abstract)
class DIGETHER_API UDigetherSettingsPanelWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UDigetherSettingsPanelWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherTabListWidget> Tabs_Category;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonAnimatedSwitcher> Switcher_Pages;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> Page_Graphics;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> Page_Sound;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> Page_Controls;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> Combo_Resolution;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> Combo_WindowMode;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> Combo_Quality;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> Combo_FrameLimit;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCheckBox> Check_VSync;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider> Slider_Master;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider> Slider_Effects;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider> Slider_Music;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_MasterValue;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_EffectsValue;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_MusicValue;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider> Slider_Sensitivity;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_SensitivityValue;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCheckBox> Check_InvertY;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_Notice;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_Apply;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_Defaults;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_Close;

	// 탭 버튼으로 만들 WBP(WBP_TabButton).
	UPROPERTY(EditDefaultsOnly, Category = "Digether|Tabs") TSubclassOf<UDigetherButtonWidget> TabButtonClass;

private:
	UFUNCTION() void HandleSliderChanged(float Value);

	void RegisterTabs();
	void LoadFromSettings();
	void Apply();
	void ResetToDefaults();
	void UpdateValueTexts();

	TArray<FIntPoint> Resolutions;
	bool bTabsRegistered = false;
};
