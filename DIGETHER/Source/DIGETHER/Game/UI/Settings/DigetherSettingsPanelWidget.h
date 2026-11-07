#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "InputCoreTypes.h"
#include "DigetherSettingsPanelWidget.generated.h"

class IVoiceCapture;
class UAudioComponent;
class UCheckBox;
class USoundWaveProcedural;
class UComboBoxString;
class UCommonAnimatedSwitcher;
class UCommonTextBlock;
class UPanelWidget;
class UProgressBar;
class USlider;
class UDigetherButtonWidget;
class UDigetherKeyBindingRowWidget;
class UDigetherTabListWidget;

// 그래픽·사운드·조작·키 설정 패널, 탭은 CommonUI 탭 목록, 저장은 적용 버튼 시점만
// BindWidgetOptional 항목은 WBP에 같은 이름 위젯을 두면 표시·저장, 없으면 생략
UCLASS(Abstract)
class DIGETHER_API UDigetherSettingsPanelWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UDigetherSettingsPanelWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherTabListWidget> Tabs_Category;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonAnimatedSwitcher> Switcher_Pages;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> Page_Graphics;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> Page_Sound;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> Page_Controls;
	// 키 탭, Switcher_Pages 자식으로 두고 줄 목록은 Panel_KeyRows
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Page_Keys;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> Panel_KeyRows;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> Combo_Resolution;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> Combo_WindowMode;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> Combo_Quality;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> Combo_FrameLimit;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCheckBox> Check_VSync;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USlider> Slider_Brightness;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_BrightnessValue;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USlider> Slider_FieldOfView;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_FieldOfViewValue;
	// 0~1 정규화 값, 엔진 렌더 해상도 비율
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USlider> Slider_ResolutionScale;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_ResolutionScaleValue;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCheckBox> Check_MotionBlur;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider> Slider_Master;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider> Slider_Effects;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider> Slider_Music;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_MasterValue;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_EffectsValue;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_MusicValue;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USlider> Slider_Voice;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_VoiceValue;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USlider> Slider_MicGain;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_MicGainValue;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USlider> Slider_MicThreshold;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_MicThresholdValue;
	// 마이크 시험: 패널이 열려 있는 동안 증폭 반영한 말하는 크기 표시, 송신 초록·대기 회색·너무 큼 빨강
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> ProgressBar_MicLevel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_MicStatus;
	// 누르면 송신될 내 목소리(증폭·문턱 적용)를 바로 들려줌, 다시 누르면 멈춤
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UDigetherButtonWidget> Btn_MicMonitor;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider> Slider_Sensitivity;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_SensitivityValue;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCheckBox> Check_InvertY;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_MicrophoneMode;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCheckBox> Check_ReduceShake;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_Notice;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_Apply;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_Defaults;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_Close;

	// 탭 버튼 생성용 WBP(WBP_TabButton)
	UPROPERTY(EditDefaultsOnly, Category = "Digether|Tabs") TSubclassOf<UDigetherButtonWidget> TabButtonClass;

	// 키 탭 한 줄 WBP
	UPROPERTY(EditDefaultsOnly, Category = "Digether|Keys") TSubclassOf<UDigetherKeyBindingRowWidget> KeyRowClass;

private:
	UFUNCTION() void HandleSliderChanged(float Value);

	void RegisterTabs();
	void LoadFromSettings();
	void LoadKeyRows(bool bUseDefaults);
	void Apply();
	void ResetToDefaults();
	void UpdateValueTexts();
	bool HasKeyPage() const;

	void BeginListening(FName MappingName);
	void FinishListening(const FKey& Key);
	void CancelListening();
	UDigetherKeyBindingRowWidget* FindKeyRow(FName MappingName) const;

	void StartMicTest();
	void StopMicTest();
	void UpdateMicTest();
	void PreviewMicrophoneValues() const;
	void ToggleMicMonitor();
	void StartMicMonitor();
	void StopMicMonitor();
	void UpdateMicMonitorButton();

	TArray<FIntPoint> Resolutions;

	UPROPERTY(Transient) TArray<TObjectPtr<UDigetherKeyBindingRowWidget>> KeyRows;
	// 적용 전 화면 값, 적용 시 바뀐 키만 저장
	TMap<FName, FKey> PendingKeys;
	FName ListeningMapping;

	TSharedPtr<IVoiceCapture> MicTestCapture;
	TArray<uint8> MicTestBuffer;

	// 내 목소리 듣기 재생, 시험 캡처 데이터를 그대로 넣음
	UPROPERTY(Transient) TObjectPtr<USoundWaveProcedural> MicMonitorWave;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> MicMonitorAudio;
};
