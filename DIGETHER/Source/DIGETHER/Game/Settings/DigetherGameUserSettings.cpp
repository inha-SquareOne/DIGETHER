#include "Game/Settings/DigetherGameUserSettings.h"

#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Engine/Engine.h"
#include "Game/Settings/DigetherOptionsSettings.h"
#include "HAL/IConsoleManager.h"
#include "Net/VoiceConfig.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace
{
	void SetFloatCVar(const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			CVar->Set(Value, ECVF_SetByGameSetting);
		}
	}
}

UDigetherGameUserSettings* UDigetherGameUserSettings::Get()
{
	return GEngine ? Cast<UDigetherGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UDigetherGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	MasterVolume = 1.f;
	EffectsVolume = 1.f;
	MusicVolume = 1.f;
	VoiceVolume = 1.f;
	MicInputGain = 1.f;
	VoiceActivationThreshold = DefaultVoiceActivationThreshold;
	MouseSensitivity = 1.f;
	bInvertMouseY = false;
	MicrophoneMode = EDigetherMicrophoneMode::AlwaysOn;
	bReduceCameraShake = false;
	DisplayGamma = DefaultDisplayGamma;
	FieldOfView = DefaultFieldOfView;
	bMotionBlur = true;
}

void UDigetherGameUserSettings::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings();
	ApplyRuntimeSettings();
}

void UDigetherGameUserSettings::ApplyMicrophoneValues(const float Gain, const float Threshold)
{
	SetFloatCVar(TEXT("voice.MicInputGain"), FMath::Clamp(Gain, MinMicInputGain, MaxMicInputGain));
	// 문턱값보다 작은 소리는 송신 안 함, 노이즈 게이트도 같은 값이어야 낮은 문턱이 효과 있음
	const float ClampedThreshold = FMath::Clamp(Threshold, 0.f, MaxVoiceActivationThreshold);
	UVOIPStatics::SetMicThreshold(ClampedThreshold);
	SetFloatCVar(TEXT("voice.MicNoiseGateThreshold"), ClampedThreshold);
}

void UDigetherGameUserSettings::ApplyRuntimeSettings()
{
	if (GEngine == nullptr)
	{
		return;
	}
	GEngine->DisplayGamma = DisplayGamma;
	// -1은 포스트 프로세스 값 그대로, 0은 끔
	SetFloatCVar(TEXT("r.MotionBlur.Amount"), bMotionBlur ? -1.f : 0.f);
	ApplyMicrophoneValues(MicInputGain, VoiceActivationThreshold);

	const UDigetherOptionsSettings* Options = UDigetherOptionsSettings::Get();
	USoundMix* Mix = Options->VolumeSoundMix.LoadSynchronous();
	USoundClass* Effects = Options->EffectsSoundClass.LoadSynchronous();
	USoundClass* Music = Options->MusicSoundClass.LoadSynchronous();
	AppliedAudioAssets.Reset();
	for (UObject* Asset : { static_cast<UObject*>(Mix), static_cast<UObject*>(Effects), static_cast<UObject*>(Music) })
	{
		if (Asset != nullptr)
		{
			AppliedAudioAssets.Add(Asset);
		}
	}
	FAudioDeviceManager* Devices = GEngine->GetAudioDeviceManager();
	if (Devices == nullptr)
	{
		return;
	}
	const float Master = MasterVolume;
	const float EffectsValue = EffectsVolume;
	const float MusicValue = MusicVolume;
	Devices->IterateOverAllDevices([=](Audio::FDeviceId, FAudioDevice* Device)
	{
		Device->SetTransientPrimaryVolume(Master);
		if (Mix == nullptr)
		{
			return;
		}
		if (Effects != nullptr)
		{
			Device->SetSoundMixClassOverride(Mix, Effects, EffectsValue, 1.f, 0.f, true);
		}
		if (Music != nullptr)
		{
			Device->SetSoundMixClassOverride(Mix, Music, MusicValue, 1.f, 0.f, true);
		}
		Device->PushSoundMixModifier(Mix);
	});
}
