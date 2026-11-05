#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "DigetherGameUserSettings.generated.h"

UENUM(BlueprintType)
enum class EDigetherMicrophoneMode : uint8
{
	AlwaysOn,
	PushToTalk
};

// 엔진 GameUserSettings에 사운드·조작 값을 더한다. 그래픽 값은 엔진 기본 항목을 그대로 쓴다.
// 저장 위치는 엔진 기본 GameUserSettings.ini다.
UCLASS(Config = GameUserSettings, ConfigDoNotCheckDefaults)
class DIGETHER_API UDigetherGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Digether|Settings", meta=(DisplayName="Get Digether Game User Settings"))
	static UDigetherGameUserSettings* Get();

	virtual void SetToDefaults() override;
	virtual void ApplyNonResolutionSettings() override;

	float GetMasterVolume() const { return MasterVolume; }
	float GetEffectsVolume() const { return EffectsVolume; }
	float GetMusicVolume() const { return MusicVolume; }
	float GetMouseSensitivity() const { return MouseSensitivity; }
	bool IsMouseYInverted() const { return bInvertMouseY; }
	UFUNCTION(BlueprintPure, Category="Digether|Settings")
	EDigetherMicrophoneMode GetMicrophoneMode() const { return MicrophoneMode; }

	UFUNCTION(BlueprintPure, Category="Digether|Settings")
	bool IsCameraShakeReduced() const { return bReduceCameraShake; }

	void SetMasterVolume(float Value) { MasterVolume = FMath::Clamp(Value, 0.f, 1.f); }
	void SetEffectsVolume(float Value) { EffectsVolume = FMath::Clamp(Value, 0.f, 1.f); }
	void SetMusicVolume(float Value) { MusicVolume = FMath::Clamp(Value, 0.f, 1.f); }
	void SetMouseSensitivity(float Value) { MouseSensitivity = FMath::Clamp(Value, MinMouseSensitivity, MaxMouseSensitivity); }
	void SetMouseYInverted(bool bValue) { bInvertMouseY = bValue; }
	void SetMicrophoneMode(EDigetherMicrophoneMode Value) { MicrophoneMode = Value; }
	void SetCameraShakeReduced(bool bValue) { bReduceCameraShake = bValue; }

	static constexpr float MinMouseSensitivity = 0.1f;
	static constexpr float MaxMouseSensitivity = 3.f;

private:
	UPROPERTY(Config) float MasterVolume = 1.f;
	// 효과음·음악 분류(SoundClass)가 생기면 그 볼륨에 연결한다. 지금은 저장만 한다.
	UPROPERTY(Config) float EffectsVolume = 1.f;
	UPROPERTY(Config) float MusicVolume = 1.f;
	UPROPERTY(Config) float MouseSensitivity = 1.f;
	UPROPERTY(Config) bool bInvertMouseY = false;
	UPROPERTY(Config) EDigetherMicrophoneMode MicrophoneMode = EDigetherMicrophoneMode::AlwaysOn;
	UPROPERTY(Config) bool bReduceCameraShake = false;
};
