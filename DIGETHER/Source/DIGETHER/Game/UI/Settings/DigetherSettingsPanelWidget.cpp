#include "Game/UI/Settings/DigetherSettingsPanelWidget.h"

#include "CommonAnimatedSwitcher.h"
#include "CommonTextBlock.h"
#include "Components/AudioComponent.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/Slider.h"
#include "Engine/World.h"
#include "Game/Settings/DigetherGameUserSettings.h"
#include "Game/Settings/DigetherKeyBindings.h"
#include "Game/UI/Common/DigetherButtonWidget.h"
#include "Game/UI/Common/DigetherTabListWidget.h"
#include "Game/UI/Settings/DigetherKeyBindingRowWidget.h"
#include "Interfaces/VoiceCapture.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/VoiceConfig.h"
#include "Sound/SoundWaveProcedural.h"
#include "VoiceModule.h"

#define LOCTEXT_NAMESPACE "DigetherSettings"

namespace
{
	const TArray<EWindowMode::Type> WindowModes = { EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed };
	const TArray<float> FrameLimits = { 30.f, 60.f, 120.f, 144.f, 0.f };
	constexpr int32 QualityLevelCount = 4;
	// 증폭 후 크기 이 값 이상이면 찢어짐 경고
	constexpr float MicClipLevel = 0.9f;
	// 내 목소리 듣기 지연 상한, 넘으면 쌓인 소리 버림
	constexpr float MaxMicMonitorDelaySeconds = 0.25f;
	const FLinearColor MicSendingColor(0.2f, 0.8f, 0.3f);
	const FLinearColor MicSilentColor(0.45f, 0.45f, 0.45f);
	const FLinearColor MicClipColor(0.9f, 0.25f, 0.2f);
	// 렌더 해상도 비율 하한 50%, 시험값
	constexpr float MinResolutionScaleNormalized = 0.5f;

	FText WindowModeName(EWindowMode::Type Mode)
	{
		switch (Mode)
		{
		case EWindowMode::Fullscreen: return LOCTEXT("Fullscreen", "전체 화면");
		case EWindowMode::WindowedFullscreen: return LOCTEXT("Borderless", "테두리 없는 창");
		default: return LOCTEXT("Windowed", "창 모드");
		}
	}

	FText QualityName(int32 Level)
	{
		switch (Level)
		{
		case 0: return LOCTEXT("QualityLow", "낮음");
		case 1: return LOCTEXT("QualityMedium", "중간");
		case 2: return LOCTEXT("QualityHigh", "높음");
		default: return LOCTEXT("QualityEpic", "최상");
		}
	}

	FText FrameLimitName(float Limit)
	{
		return Limit <= 0.f ? LOCTEXT("NoLimit", "제한 없음") : FText::Format(LOCTEXT("Fps", "{0} FPS"), FMath::RoundToInt(Limit));
	}

	FString ResolutionName(const FIntPoint& Resolution)
	{
		return FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y);
	}

	FText Decimal(float Value, int32 Digits)
	{
		FNumberFormattingOptions Options;
		Options.SetMinimumFractionalDigits(Digits).SetMaximumFractionalDigits(Digits);
		return FText::AsNumber(Value, &Options);
	}

	void SetupSlider(USlider* Slider, float Min, float Max, const TFunctionRef<void(USlider*)>& Bind)
	{
		if (Slider)
		{
			Slider->SetMinValue(Min);
			Slider->SetMaxValue(Max);
			Bind(Slider);
		}
	}

	void SetText(UCommonTextBlock* Text, const FText& Value)
	{
		if (Text)
		{
			Text->SetText(Value);
		}
	}
}

UDigetherSettingsPanelWidget::UDigetherSettingsPanelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
}

void UDigetherSettingsPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	for (const EWindowMode::Type Mode : WindowModes) Combo_WindowMode->AddOption(WindowModeName(Mode).ToString());
	for (int32 Level = 0; Level < QualityLevelCount; ++Level) Combo_Quality->AddOption(QualityName(Level).ToString());
	for (const float Limit : FrameLimits) Combo_FrameLimit->AddOption(FrameLimitName(Limit).ToString());
	if (Combo_MicrophoneMode)
	{
		Combo_MicrophoneMode->ClearOptions();
		Combo_MicrophoneMode->AddOption(LOCTEXT("MicAlwaysOn", "항상 켜짐").ToString());
		Combo_MicrophoneMode->AddOption(LOCTEXT("MicPushToTalk", "누르고 말하기").ToString());
		Combo_MicrophoneMode->AddOption(LOCTEXT("MicToggle", "토글(눌러서 켜기/끄기)").ToString());
	}

	auto Bind = [this](USlider* Slider) { Slider->OnValueChanged.AddDynamic(this, &ThisClass::HandleSliderChanged); };
	SetupSlider(Slider_Master, 0.f, 1.f, Bind);
	SetupSlider(Slider_Effects, 0.f, 1.f, Bind);
	SetupSlider(Slider_Music, 0.f, 1.f, Bind);
	SetupSlider(Slider_Voice, 0.f, 1.f, Bind);
	SetupSlider(Slider_MicGain, UDigetherGameUserSettings::MinMicInputGain, UDigetherGameUserSettings::MaxMicInputGain, Bind);
	SetupSlider(Slider_MicThreshold, 0.f, UDigetherGameUserSettings::MaxVoiceActivationThreshold, Bind);
	SetupSlider(Slider_Sensitivity, UDigetherGameUserSettings::MinMouseSensitivity, UDigetherGameUserSettings::MaxMouseSensitivity, Bind);
	SetupSlider(Slider_Brightness, UDigetherGameUserSettings::MinDisplayGamma, UDigetherGameUserSettings::MaxDisplayGamma, Bind);
	SetupSlider(Slider_FieldOfView, UDigetherGameUserSettings::MinFieldOfView, UDigetherGameUserSettings::MaxFieldOfView, Bind);
	SetupSlider(Slider_ResolutionScale, MinResolutionScaleNormalized, 1.f, Bind);
	if (Slider_FieldOfView)
	{
		Slider_FieldOfView->SetStepSize(1.f);
	}

	Btn_Apply->OnClicked().AddUObject(this, &ThisClass::Apply);
	Btn_Defaults->OnClicked().AddUObject(this, &ThisClass::ResetToDefaults);
	Btn_Close->OnClicked().AddUObject(this, &ThisClass::DeactivateWidget);
	if (Btn_MicMonitor)
	{
		Btn_MicMonitor->OnClicked().AddUObject(this, &ThisClass::ToggleMicMonitor);
		UpdateMicMonitorButton();
	}
}

void UDigetherSettingsPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	RegisterTabs();
	LoadFromSettings();
	StartMicTest();
}

void UDigetherSettingsPanelWidget::NativeOnDeactivated()
{
	CancelListening();
	StopMicTest();
	// 저장 안 한 증폭·문턱 미리 듣기 값을 저장값으로 복원
	if (const UDigetherGameUserSettings* Settings = UDigetherGameUserSettings::Get())
	{
		UDigetherGameUserSettings::ApplyMicrophoneValues(Settings->GetMicInputGain(), Settings->GetVoiceActivationThreshold());
	}
	Super::NativeOnDeactivated();
}

void UDigetherSettingsPanelWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// 월드 타이머 대신 위젯 Tick, 일시정지 중에도 갱신
	UpdateMicTest();
}

UWidget* UDigetherSettingsPanelWidget::NativeGetDesiredFocusTarget() const
{
	return Btn_Apply;
}

bool UDigetherSettingsPanelWidget::HasKeyPage() const
{
	return Page_Keys != nullptr && Panel_KeyRows != nullptr && KeyRowClass != nullptr;
}

void UDigetherSettingsPanelWidget::RegisterTabs()
{
	// CommonUI는 NativeDestruct에서 탭을 제거하므로 재사용 시 실제 목록을 확인한다.
	if (Tabs_Category->GetTabCount() > 0 || !TabButtonClass)
	{
		return;
	}
	Tabs_Category->SetLinkedSwitcher(Switcher_Pages);
	TArray<TPair<FName, FText>> Tabs = {
		{ TEXT("Graphics"), LOCTEXT("TabGraphics", "그래픽") },
		{ TEXT("Sound"), LOCTEXT("TabSound", "사운드") },
		{ TEXT("Controls"), LOCTEXT("TabControls", "조작") } };
	TArray<UWidget*> Pages = { Page_Graphics, Page_Sound, Page_Controls };
	if (HasKeyPage())
	{
		Tabs.Add({ TEXT("Keys"), LOCTEXT("TabKeys", "키 설정") });
		Pages.Add(Page_Keys);
	}
	for (int32 Index = 0; Index < Tabs.Num(); ++Index)
	{
		Tabs_Category->RegisterTab(Tabs[Index].Key, TabButtonClass, Pages[Index]);
		if (UDigetherButtonWidget* TabButton = Cast<UDigetherButtonWidget>(Tabs_Category->GetTabButtonBaseByID(Tabs[Index].Key)))
		{
			TabButton->SetButtonText(Tabs[Index].Value);
		}
	}
	Tabs_Category->SelectTabByID(Tabs[0].Key);
}

void UDigetherSettingsPanelWidget::LoadFromSettings()
{
	UDigetherGameUserSettings* Settings = UDigetherGameUserSettings::Get();
	if (!Settings) return;

	Resolutions.Reset();
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
	const FIntPoint Current = Settings->GetScreenResolution();
	Resolutions.AddUnique(Current);
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X > B.X : A.Y > B.Y; });
	Combo_Resolution->ClearOptions();
	for (const FIntPoint& Resolution : Resolutions) Combo_Resolution->AddOption(ResolutionName(Resolution));
	Combo_Resolution->SetSelectedOption(ResolutionName(Current));

	Combo_WindowMode->SetSelectedIndex(FMath::Max(0, WindowModes.IndexOfByKey(Settings->GetFullscreenMode())));
	const int32 Quality = Settings->GetOverallScalabilityLevel();
	if (Quality >= 0 && Quality < QualityLevelCount) Combo_Quality->SetSelectedIndex(Quality);
	else Combo_Quality->ClearSelection();
	const float CurrentLimit = Settings->GetFrameRateLimit();
	const int32 LimitIndex = FrameLimits.IndexOfByPredicate([CurrentLimit](float Option) { return FMath::IsNearlyEqual(Option, CurrentLimit, 0.5f); });
	if (LimitIndex != INDEX_NONE) Combo_FrameLimit->SetSelectedIndex(LimitIndex);
	else Combo_FrameLimit->ClearSelection();
	Check_VSync->SetIsChecked(Settings->IsVSyncEnabled());
	if (Slider_Brightness) Slider_Brightness->SetValue(Settings->GetDisplayGamma());
	if (Slider_FieldOfView) Slider_FieldOfView->SetValue(Settings->GetFieldOfView());
	if (Slider_ResolutionScale)
	{
		// 0은 엔진에서 프로젝트 기본(100%) 의미
		const float Scale = Settings->GetResolutionScaleNormalized();
		Slider_ResolutionScale->SetValue(Scale > 0.f ? FMath::Max(Scale, MinResolutionScaleNormalized) : 1.f);
	}
	if (Check_MotionBlur) Check_MotionBlur->SetIsChecked(Settings->IsMotionBlurEnabled());

	Slider_Master->SetValue(Settings->GetMasterVolume());
	Slider_Effects->SetValue(Settings->GetEffectsVolume());
	Slider_Music->SetValue(Settings->GetMusicVolume());
	if (Slider_Voice) Slider_Voice->SetValue(Settings->GetVoiceVolume());
	if (Slider_MicGain) Slider_MicGain->SetValue(Settings->GetMicInputGain());
	if (Slider_MicThreshold) Slider_MicThreshold->SetValue(Settings->GetVoiceActivationThreshold());
	Slider_Sensitivity->SetValue(Settings->GetMouseSensitivity());
	Check_InvertY->SetIsChecked(Settings->IsMouseYInverted());
	if (Combo_MicrophoneMode)
	{
		Combo_MicrophoneMode->SetSelectedIndex(static_cast<int32>(Settings->GetMicrophoneMode()));
	}
	if (Check_ReduceShake)
	{
		Check_ReduceShake->SetIsChecked(Settings->IsCameraShakeReduced());
	}
	LoadKeyRows(false);
	Text_Notice->SetText(FText::GetEmpty());
	UpdateValueTexts();
	PreviewMicrophoneValues();
}

void UDigetherSettingsPanelWidget::LoadKeyRows(const bool bUseDefaults)
{
	if (!HasKeyPage())
	{
		return;
	}
	CancelListening();
	Panel_KeyRows->ClearChildren();
	KeyRows.Reset();
	PendingKeys.Reset();
	for (const FDigetherKeyBindingRow& Row : UDigetherKeyBindingLibrary::GetKeyBindingRows(GetOwningLocalPlayer()))
	{
		const FKey Key = bUseDefaults ? Row.DefaultKey : Row.CurrentKey;
		PendingKeys.Add(Row.MappingName, Key);
		UDigetherKeyBindingRowWidget* RowWidget = CreateWidget<UDigetherKeyBindingRowWidget>(this, KeyRowClass);
		RowWidget->Setup(Row.MappingName, Row.DisplayName, Key);
		RowWidget->OnClicked.BindUObject(this, &ThisClass::BeginListening);
		Panel_KeyRows->AddChild(RowWidget);
		KeyRows.Add(RowWidget);
	}
}

void UDigetherSettingsPanelWidget::Apply()
{
	UDigetherGameUserSettings* Settings = UDigetherGameUserSettings::Get();
	if (!Settings) return;
	CancelListening();
	const int32 ResolutionIndex = Combo_Resolution->GetSelectedIndex();
	if (Resolutions.IsValidIndex(ResolutionIndex)) Settings->SetScreenResolution(Resolutions[ResolutionIndex]);
	const int32 ModeIndex = Combo_WindowMode->GetSelectedIndex();
	if (WindowModes.IsValidIndex(ModeIndex)) Settings->SetFullscreenMode(WindowModes[ModeIndex]);
	const int32 QualityIndex = Combo_Quality->GetSelectedIndex();
	if (QualityIndex >= 0 && QualityIndex < QualityLevelCount) Settings->SetOverallScalabilityLevel(QualityIndex);
	const int32 LimitIndex = Combo_FrameLimit->GetSelectedIndex();
	if (FrameLimits.IsValidIndex(LimitIndex)) Settings->SetFrameRateLimit(FrameLimits[LimitIndex]);
	Settings->SetVSyncEnabled(Check_VSync->IsChecked());
	// 렌더 비율은 품질 단계 뒤에 지정해야 품질 단계 기본값에 덮이지 않음
	if (Slider_ResolutionScale) Settings->SetResolutionScaleNormalized(Slider_ResolutionScale->GetValue());
	if (Slider_Brightness) Settings->SetDisplayGamma(Slider_Brightness->GetValue());
	if (Slider_FieldOfView) Settings->SetFieldOfView(Slider_FieldOfView->GetValue());
	if (Check_MotionBlur) Settings->SetMotionBlurEnabled(Check_MotionBlur->IsChecked());
	Settings->SetMasterVolume(Slider_Master->GetValue());
	Settings->SetEffectsVolume(Slider_Effects->GetValue());
	Settings->SetMusicVolume(Slider_Music->GetValue());
	if (Slider_Voice) Settings->SetVoiceVolume(Slider_Voice->GetValue());
	if (Slider_MicGain) Settings->SetMicInputGain(Slider_MicGain->GetValue());
	if (Slider_MicThreshold) Settings->SetVoiceActivationThreshold(Slider_MicThreshold->GetValue());
	Settings->SetMouseSensitivity(Slider_Sensitivity->GetValue());
	Settings->SetMouseYInverted(Check_InvertY->IsChecked());
	if (Combo_MicrophoneMode && Combo_MicrophoneMode->GetSelectedIndex() >= 0
		&& Combo_MicrophoneMode->GetSelectedIndex() <= static_cast<int32>(EDigetherMicrophoneMode::Toggle))
	{
		Settings->SetMicrophoneMode(static_cast<EDigetherMicrophoneMode>(Combo_MicrophoneMode->GetSelectedIndex()));
	}
	if (Check_ReduceShake)
	{
		Settings->SetCameraShakeReduced(Check_ReduceShake->IsChecked());
	}
	// ApplySettings가 해상도·품질 적용 후 GameUserSettings.ini 저장
	Settings->ApplySettings(false);
	const bool bKeysApplied = !HasKeyPage() || UDigetherKeyBindingLibrary::ApplyKeyBindings(GetOwningLocalPlayer(), PendingKeys);
	Text_Notice->SetText(bKeysApplied ? LOCTEXT("Applied", "설정을 저장했습니다.")
		: LOCTEXT("KeysFailed", "설정을 저장했지만 일부 키는 바꾸지 못했습니다."));
}

void UDigetherSettingsPanelWidget::ResetToDefaults()
{
	UDigetherGameUserSettings* Settings = UDigetherGameUserSettings::Get();
	if (!Settings) return;
	// 기본값은 화면에만 채우고 메모리 설정은 저장값으로 복원, 반영은 적용 시점
	Settings->SetToDefaults();
	LoadFromSettings();
	Settings->LoadSettings(true);
	LoadKeyRows(true);
	Text_Notice->SetText(LOCTEXT("DefaultsFilled", "기본값을 채웠습니다. 적용을 눌러 저장하세요."));
}

void UDigetherSettingsPanelWidget::UpdateValueTexts()
{
	auto Percent = [](const USlider* Slider) { return FText::AsPercent(Slider->GetValue()); };
	Text_MasterValue->SetText(Percent(Slider_Master));
	Text_EffectsValue->SetText(Percent(Slider_Effects));
	Text_MusicValue->SetText(Percent(Slider_Music));
	if (Slider_Voice) SetText(Text_VoiceValue, Percent(Slider_Voice));
	if (Slider_MicGain) SetText(Text_MicGainValue, FText::Format(LOCTEXT("Gain", "x{0}"), Decimal(Slider_MicGain->GetValue(), 1)));
	if (Slider_MicThreshold) SetText(Text_MicThresholdValue, Decimal(Slider_MicThreshold->GetValue(), 2));
	Text_SensitivityValue->SetText(Decimal(Slider_Sensitivity->GetValue(), 2));
	if (Slider_Brightness) SetText(Text_BrightnessValue, Decimal(Slider_Brightness->GetValue(), 1));
	if (Slider_FieldOfView) SetText(Text_FieldOfViewValue, FText::AsNumber(FMath::RoundToInt(Slider_FieldOfView->GetValue())));
	if (Slider_ResolutionScale) SetText(Text_ResolutionScaleValue, FText::AsPercent(Slider_ResolutionScale->GetValue()));
}

void UDigetherSettingsPanelWidget::HandleSliderChanged(float Value)
{
	UpdateValueTexts();
	PreviewMicrophoneValues();
}

void UDigetherSettingsPanelWidget::PreviewMicrophoneValues() const
{
	// 슬라이더 값을 바로 엔진 마이크 처리에 넣어 막대·듣기로 확인, 저장은 적용 시점
	const UDigetherGameUserSettings* Settings = UDigetherGameUserSettings::Get();
	if (Settings == nullptr || (!Slider_MicGain && !Slider_MicThreshold))
	{
		return;
	}
	UDigetherGameUserSettings::ApplyMicrophoneValues(
		Slider_MicGain ? Slider_MicGain->GetValue() : Settings->GetMicInputGain(),
		Slider_MicThreshold ? Slider_MicThreshold->GetValue() : Settings->GetVoiceActivationThreshold());
}

UDigetherKeyBindingRowWidget* UDigetherSettingsPanelWidget::FindKeyRow(const FName MappingName) const
{
	const TObjectPtr<UDigetherKeyBindingRowWidget>* Row = KeyRows.FindByPredicate(
		[MappingName](const UDigetherKeyBindingRowWidget* Entry) { return Entry != nullptr && Entry->GetMappingName() == MappingName; });
	return Row != nullptr ? Row->Get() : nullptr;
}

void UDigetherSettingsPanelWidget::BeginListening(const FName MappingName)
{
	CancelListening();
	UDigetherKeyBindingRowWidget* Row = FindKeyRow(MappingName);
	if (Row == nullptr)
	{
		return;
	}
	ListeningMapping = MappingName;
	Row->SetListening(true);
	// 마우스로 눌렀어도 키 입력이 이 패널을 거치도록 초점 이동
	Row->GetKeyButton()->SetFocus();
	Text_Notice->SetText(LOCTEXT("PressKey", "바꿀 키를 누르세요. Esc는 취소합니다."));
}

void UDigetherSettingsPanelWidget::FinishListening(const FKey& Key)
{
	const FName Mapping = ListeningMapping;
	ListeningMapping = NAME_None;
	DigetherKeyBindings::AssignWithSwap(PendingKeys, Mapping, Key);
	// 교환된 다른 줄까지 화면 갱신
	for (UDigetherKeyBindingRowWidget* Row : KeyRows)
	{
		if (const FKey* Pending = Row != nullptr ? PendingKeys.Find(Row->GetMappingName()) : nullptr)
		{
			Row->SetKey(*Pending);
		}
	}
	Text_Notice->SetText(LOCTEXT("KeyChanged", "키를 바꿨습니다. 적용을 눌러 저장하세요."));
}

void UDigetherSettingsPanelWidget::CancelListening()
{
	if (ListeningMapping.IsNone())
	{
		return;
	}
	if (UDigetherKeyBindingRowWidget* Row = FindKeyRow(ListeningMapping))
	{
		Row->SetListening(false);
	}
	ListeningMapping = NAME_None;
	Text_Notice->SetText(FText::GetEmpty());
}

bool UDigetherSettingsPanelWidget::NativeOnHandleBackAction()
{
	if (!ListeningMapping.IsNone())
	{
		CancelListening();
		return true;
	}
	return Super::NativeOnHandleBackAction();
}

FReply UDigetherSettingsPanelWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (ListeningMapping.IsNone())
	{
		return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
	}
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape)
	{
		CancelListening();
	}
	else if (DigetherKeyBindings::IsBindableKey(Key))
	{
		FinishListening(Key);
	}
	return FReply::Handled();
}

FReply UDigetherSettingsPanelWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (ListeningMapping.IsNone())
	{
		return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
	}
	const FKey Key = InMouseEvent.GetEffectingButton();
	if (DigetherKeyBindings::IsBindableKey(Key))
	{
		FinishListening(Key);
	}
	return FReply::Handled();
}

void UDigetherSettingsPanelWidget::StartMicTest()
{
	if ((!ProgressBar_MicLevel && !Btn_MicMonitor) || MicTestCapture.IsValid() || !FVoiceModule::IsAvailable())
	{
		return;
	}
	// 게임 VoIP 송신과 별개인 시험용 캡처, 윈도우 기본 입력 장치
	// 엔진이 같은 voice cvar로 증폭·노이즈 게이트·감지 문턱을 적용하므로 결과가 실제 송신과 같음
	MicTestCapture = FVoiceModule::Get().CreateVoiceCapture(FString());
	if (!MicTestCapture.IsValid() || !MicTestCapture->Start())
	{
		MicTestCapture.Reset();
		SetText(Text_MicStatus, LOCTEXT("MicUnavailable", "마이크를 찾지 못했습니다"));
		return;
	}
	MicTestBuffer.SetNumUninitialized(FMath::Max(MicTestCapture->GetBufferSize(), 1));
}

void UDigetherSettingsPanelWidget::StopMicTest()
{
	StopMicMonitor();
	if (MicTestCapture.IsValid())
	{
		MicTestCapture->Stop();
		MicTestCapture->Shutdown();
		MicTestCapture.Reset();
	}
	if (ProgressBar_MicLevel)
	{
		ProgressBar_MicLevel->SetPercent(0.f);
	}
}

void UDigetherSettingsPanelWidget::UpdateMicTest()
{
	if (!MicTestCapture.IsValid())
	{
		return;
	}
	// 버퍼를 비워야 입력 크기가 갱신됨, 감지 문턱 이상 구간만 데이터가 나옴
	uint32 Available = 0;
	while (MicTestCapture->GetCaptureState(Available) == EVoiceCaptureState::Ok && Available > 0)
	{
		if (static_cast<uint32>(MicTestBuffer.Num()) < Available)
		{
			MicTestBuffer.SetNumUninitialized(Available);
		}
		uint32 Written = 0;
		if (MicTestCapture->GetVoiceData(MicTestBuffer.GetData(), MicTestBuffer.Num(), Written) != EVoiceCaptureState::Ok || Written == 0)
		{
			break;
		}
		if (MicMonitorWave)
		{
			const int32 BytesPerSecond = UVOIPStatics::GetVoiceSampleRate() * UVOIPStatics::GetVoiceNumChannels() * static_cast<int32>(sizeof(int16));
			if (MicMonitorWave->GetAvailableAudioByteCount() > BytesPerSecond * MaxMicMonitorDelaySeconds)
			{
				MicMonitorWave->ResetAudio();
			}
			MicMonitorWave->QueueAudio(MicTestBuffer.GetData(), static_cast<int32>(Written));
		}
	}
	if (!ProgressBar_MicLevel && !Text_MicStatus)
	{
		return;
	}

	const UDigetherGameUserSettings* Settings = UDigetherGameUserSettings::Get();
	const float Gain = Slider_MicGain ? Slider_MicGain->GetValue() : (Settings != nullptr ? Settings->GetMicInputGain() : 1.f);
	const float Threshold = Slider_MicThreshold ? Slider_MicThreshold->GetValue()
		: (Settings != nullptr ? Settings->GetVoiceActivationThreshold() : UDigetherGameUserSettings::DefaultVoiceActivationThreshold);
	// 감지는 증폭 전 크기로 판정(엔진 Windows 캡처 기준), 막대는 증폭 후 크기 = 상대가 듣는 크기
	const float Input = FMath::Max(0.f, MicTestCapture->GetCurrentAmplitude());
	const float Spoken = Input * Gain;
	const bool bSending = Input > Threshold;
	const bool bClipping = Spoken >= MicClipLevel;
	if (ProgressBar_MicLevel)
	{
		// 작은 소리도 보이도록 제곱근 눈금
		ProgressBar_MicLevel->SetPercent(FMath::Sqrt(FMath::Clamp(Spoken, 0.f, 1.f)));
		ProgressBar_MicLevel->SetFillColorAndOpacity(bClipping ? MicClipColor : (bSending ? MicSendingColor : MicSilentColor));
	}
	SetText(Text_MicStatus, bClipping ? LOCTEXT("MicClipping", "너무 큼, 증폭을 낮추세요")
		: (bSending ? LOCTEXT("MicSending", "송신") : LOCTEXT("MicSilent", "대기(감지 문턱보다 작음)")));
}

void UDigetherSettingsPanelWidget::ToggleMicMonitor()
{
	if (MicMonitorAudio)
	{
		StopMicMonitor();
	}
	else
	{
		StartMicMonitor();
	}
}

void UDigetherSettingsPanelWidget::StartMicMonitor()
{
	StartMicTest();
	if (!MicTestCapture.IsValid() || MicMonitorAudio)
	{
		UpdateMicMonitorButton();
		return;
	}
	// 시험 캡처와 같은 형식(엔진 음성 기본 샘플 레이트·채널, 16비트)
	MicMonitorWave = NewObject<USoundWaveProcedural>(this);
	MicMonitorWave->SetSampleRate(UVOIPStatics::GetVoiceSampleRate());
	MicMonitorWave->NumChannels = UVOIPStatics::GetVoiceNumChannels();
	MicMonitorWave->Duration = INDEFINITELY_LOOPING_DURATION;
	MicMonitorWave->SoundGroup = SOUNDGROUP_Voice;
	MicMonitorWave->bLooping = false;
	MicMonitorAudio = UGameplayStatics::CreateSound2D(this, MicMonitorWave, 1.f, 1.f, 0.f, nullptr, false, true);
	if (MicMonitorAudio == nullptr)
	{
		// 오디오 장치 없음(-nosound 등)
		MicMonitorWave = nullptr;
		Text_Notice->SetText(LOCTEXT("MonitorUnavailable", "소리를 재생할 수 없어 내 목소리를 들을 수 없습니다."));
		UpdateMicMonitorButton();
		return;
	}
	// 일시정지 중에도 재생
	MicMonitorAudio->bIsUISound = true;
	MicMonitorAudio->Play();
	Text_Notice->SetText(LOCTEXT("MonitorStarted", "내 목소리를 들려줍니다. 스피커는 소리가 울릴 수 있어 헤드폰을 권장합니다."));
	UpdateMicMonitorButton();
}

void UDigetherSettingsPanelWidget::StopMicMonitor()
{
	if (MicMonitorAudio)
	{
		// 자동 제거 설정이라 멈추면 컴포넌트도 정리됨
		MicMonitorAudio->Stop();
		MicMonitorAudio = nullptr;
	}
	MicMonitorWave = nullptr;
	UpdateMicMonitorButton();
}

void UDigetherSettingsPanelWidget::UpdateMicMonitorButton()
{
	if (Btn_MicMonitor)
	{
		Btn_MicMonitor->SetButtonText(MicMonitorAudio ? LOCTEXT("MonitorStop", "듣기 멈추기") : LOCTEXT("MonitorStart", "내 목소리 듣기"));
	}
}

#undef LOCTEXT_NAMESPACE
