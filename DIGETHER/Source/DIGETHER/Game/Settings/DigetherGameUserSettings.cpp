#include "Game/Settings/DigetherGameUserSettings.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"

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
	MouseSensitivity = 1.f;
	bInvertMouseY = false;
	MicrophoneMode = EDigetherMicrophoneMode::AlwaysOn;
	bReduceCameraShake = false;
}

void UDigetherGameUserSettings::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings();
	if (GEngine)
	{
		if (FAudioDeviceHandle AudioDevice = GEngine->GetMainAudioDevice())
		{
			AudioDevice->SetTransientPrimaryVolume(MasterVolume);
		}
	}
}
