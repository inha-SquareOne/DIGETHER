#include "Game/Framework/DigetherPlayerCameraManager.h"

#include "Game/Settings/DigetherGameUserSettings.h"

void ADigetherPlayerCameraManager::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	OnAudioFadeChangeEvent.AddDynamic(this, &ThisClass::HandleAudioFadeChange);
}

void ADigetherPlayerCameraManager::UpdateViewTargetInternal(FTViewTarget& OutVT, const float DeltaTime)
{
	Super::UpdateViewTargetInternal(OutVT, DeltaTime);
	const UDigetherGameUserSettings* Settings = UDigetherGameUserSettings::Get();
	if (Settings != nullptr && OutVT.POV.ProjectionMode == ECameraProjectionMode::Perspective)
	{
		OutVT.POV.FOV = Settings->GetFieldOfView();
	}
}

void ADigetherPlayerCameraManager::HandleAudioFadeChange(bool bAudioFadeOut, float AudioFadeTime)
{
	if (UDigetherGameUserSettings* Settings = UDigetherGameUserSettings::Get())
	{
		Settings->ApplyRuntimeSettings();
	}
}
