#include "Game/UI/Settings/DigetherSettingsPanelWidget.h"

#include "CommonAnimatedSwitcher.h"
#include "CommonTextBlock.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Game/Settings/DigetherGameUserSettings.h"
#include "Game/UI/Common/DigetherButtonWidget.h"
#include "Game/UI/Common/DigetherTabListWidget.h"
#include "Kismet/KismetSystemLibrary.h"

#define LOCTEXT_NAMESPACE "DigetherSettings"

namespace
{
	const TArray<EWindowMode::Type> WindowModes = { EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed };
	const TArray<float> FrameLimits = { 30.f, 60.f, 120.f, 144.f, 0.f };
	constexpr int32 QualityLevelCount = 4;

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

	Slider_Master->OnValueChanged.AddDynamic(this, &ThisClass::HandleSliderChanged);
	Slider_Effects->OnValueChanged.AddDynamic(this, &ThisClass::HandleSliderChanged);
	Slider_Music->OnValueChanged.AddDynamic(this, &ThisClass::HandleSliderChanged);
	Slider_Sensitivity->OnValueChanged.AddDynamic(this, &ThisClass::HandleSliderChanged);
	Slider_Sensitivity->SetMinValue(UDigetherGameUserSettings::MinMouseSensitivity);
	Slider_Sensitivity->SetMaxValue(UDigetherGameUserSettings::MaxMouseSensitivity);

	Btn_Apply->OnClicked().AddUObject(this, &ThisClass::Apply);
	Btn_Defaults->OnClicked().AddUObject(this, &ThisClass::ResetToDefaults);
	Btn_Close->OnClicked().AddUObject(this, &ThisClass::DeactivateWidget);
}

void UDigetherSettingsPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	RegisterTabs();
	LoadFromSettings();
}

UWidget* UDigetherSettingsPanelWidget::NativeGetDesiredFocusTarget() const
{
	return Btn_Apply;
}

void UDigetherSettingsPanelWidget::RegisterTabs()
{
	if (bTabsRegistered || !TabButtonClass) return;
	bTabsRegistered = true;
	Tabs_Category->SetLinkedSwitcher(Switcher_Pages);
	const TPair<FName, FText> Tabs[] = {
		{ TEXT("Graphics"), LOCTEXT("TabGraphics", "그래픽") },
		{ TEXT("Sound"), LOCTEXT("TabSound", "사운드") },
		{ TEXT("Controls"), LOCTEXT("TabControls", "조작") } };
	UWidget* const Pages[] = { Page_Graphics, Page_Sound, Page_Controls };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Tabs); ++Index)
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

	Slider_Master->SetValue(Settings->GetMasterVolume());
	Slider_Effects->SetValue(Settings->GetEffectsVolume());
	Slider_Music->SetValue(Settings->GetMusicVolume());
	Slider_Sensitivity->SetValue(Settings->GetMouseSensitivity());
	Check_InvertY->SetIsChecked(Settings->IsMouseYInverted());
	Text_Notice->SetText(FText::GetEmpty());
	UpdateValueTexts();
}

void UDigetherSettingsPanelWidget::Apply()
{
	UDigetherGameUserSettings* Settings = UDigetherGameUserSettings::Get();
	if (!Settings) return;
	const int32 ResolutionIndex = Combo_Resolution->GetSelectedIndex();
	if (Resolutions.IsValidIndex(ResolutionIndex)) Settings->SetScreenResolution(Resolutions[ResolutionIndex]);
	const int32 ModeIndex = Combo_WindowMode->GetSelectedIndex();
	if (WindowModes.IsValidIndex(ModeIndex)) Settings->SetFullscreenMode(WindowModes[ModeIndex]);
	const int32 QualityIndex = Combo_Quality->GetSelectedIndex();
	if (QualityIndex >= 0 && QualityIndex < QualityLevelCount) Settings->SetOverallScalabilityLevel(QualityIndex);
	const int32 LimitIndex = Combo_FrameLimit->GetSelectedIndex();
	if (FrameLimits.IsValidIndex(LimitIndex)) Settings->SetFrameRateLimit(FrameLimits[LimitIndex]);
	Settings->SetVSyncEnabled(Check_VSync->IsChecked());
	Settings->SetMasterVolume(Slider_Master->GetValue());
	Settings->SetEffectsVolume(Slider_Effects->GetValue());
	Settings->SetMusicVolume(Slider_Music->GetValue());
	Settings->SetMouseSensitivity(Slider_Sensitivity->GetValue());
	Settings->SetMouseYInverted(Check_InvertY->IsChecked());
	// ApplySettings는 해상도·품질을 적용한 뒤 GameUserSettings.ini에 저장한다.
	Settings->ApplySettings(false);
	Text_Notice->SetText(LOCTEXT("Applied", "설정을 적용하고 저장했습니다."));
}

void UDigetherSettingsPanelWidget::ResetToDefaults()
{
	UDigetherGameUserSettings* Settings = UDigetherGameUserSettings::Get();
	if (!Settings) return;
	// 기본값을 화면에만 채우고, 메모리의 설정은 저장된 값으로 되돌린다. '적용'을 눌러야 반영된다.
	Settings->SetToDefaults();
	LoadFromSettings();
	Settings->LoadSettings(true);
	Text_Notice->SetText(LOCTEXT("DefaultsFilled", "기본값을 채웠습니다. 적용을 눌러 저장하세요."));
}

void UDigetherSettingsPanelWidget::UpdateValueTexts()
{
	auto Percent = [](const USlider* Slider) { return FText::AsPercent(Slider->GetValue()); };
	Text_MasterValue->SetText(Percent(Slider_Master));
	Text_EffectsValue->SetText(Percent(Slider_Effects));
	Text_MusicValue->SetText(Percent(Slider_Music));
	FNumberFormattingOptions TwoDecimals;
	TwoDecimals.SetMinimumFractionalDigits(2).SetMaximumFractionalDigits(2);
	Text_SensitivityValue->SetText(FText::AsNumber(Slider_Sensitivity->GetValue(), &TwoDecimals));
}

void UDigetherSettingsPanelWidget::HandleSliderChanged(float Value)
{
	UpdateValueTexts();
}

#undef LOCTEXT_NAMESPACE
