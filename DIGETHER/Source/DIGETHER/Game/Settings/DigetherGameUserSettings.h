#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "DigetherGameUserSettings.generated.h"

UENUM(BlueprintType)
enum class EDigetherMicrophoneMode : uint8
{
	AlwaysOn,
	PushToTalk,
	// 누르고 말하기 키를 한 번 누르면 켜짐, 다시 누르면 꺼짐(11-06 답변 14 마이크 온오프, 사용자 선택)
	Toggle
};

namespace DigetherMicrophone
{
	// 마이크 송신 여부, 서버가 음성을 끈 상태면 항상 끔
	inline bool ShouldTransmit(const bool bServerVoiceEnabled, const EDigetherMicrophoneMode Mode, const bool bKeyHeld, const bool bToggleOpen)
	{
		switch (Mode)
		{
		case EDigetherMicrophoneMode::PushToTalk: return bServerVoiceEnabled && bKeyHeld;
		case EDigetherMicrophoneMode::Toggle: return bServerVoiceEnabled && bToggleOpen;
		default: return bServerVoiceEnabled;
		}
	}
}

// 엔진 GameUserSettings에 사운드·음성·화면·조작 값 추가, 해상도·품질·렌더 비율은 엔진 기본 항목 사용
// 저장 위치는 엔진 기본 GameUserSettings.ini, 키 변경은 Enhanced Input 사용자 설정(SaveGame) 담당
UCLASS(Config = GameUserSettings, ConfigDoNotCheckDefaults)
class DIGETHER_API UDigetherGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Digether|Settings", meta=(DisplayName="Get Digether Game User Settings"))
	static UDigetherGameUserSettings* Get();

	virtual void SetToDefaults() override;
	virtual void ApplyNonResolutionSettings() override;

	// 볼륨 SoundMix·밝기·모션 블러·마이크 값만 다시 적용, 맵 이동 후 재적용용
	void ApplyRuntimeSettings();

	// 마이크 증폭·감지 문턱만 엔진에 반영, 설정창 시험 중 저장 전 미리 듣기용
	static void ApplyMicrophoneValues(float Gain, float Threshold);

	float GetMasterVolume() const { return MasterVolume; }
	float GetEffectsVolume() const { return EffectsVolume; }
	float GetMusicVolume() const { return MusicVolume; }
	UFUNCTION(BlueprintPure, Category="Digether|Settings")
	float GetVoiceVolume() const { return VoiceVolume; }
	float GetMicInputGain() const { return MicInputGain; }
	float GetVoiceActivationThreshold() const { return VoiceActivationThreshold; }
	float GetMouseSensitivity() const { return MouseSensitivity; }
	bool IsMouseYInverted() const { return bInvertMouseY; }
	UFUNCTION(BlueprintPure, Category="Digether|Settings")
	EDigetherMicrophoneMode GetMicrophoneMode() const { return MicrophoneMode; }

	UFUNCTION(BlueprintPure, Category="Digether|Settings")
	bool IsCameraShakeReduced() const { return bReduceCameraShake; }

	float GetDisplayGamma() const { return DisplayGamma; }
	UFUNCTION(BlueprintPure, Category="Digether|Settings")
	float GetFieldOfView() const { return FieldOfView; }
	bool IsMotionBlurEnabled() const { return bMotionBlur; }

	void SetMasterVolume(float Value) { MasterVolume = FMath::Clamp(Value, 0.f, 1.f); }
	void SetEffectsVolume(float Value) { EffectsVolume = FMath::Clamp(Value, 0.f, 1.f); }
	void SetMusicVolume(float Value) { MusicVolume = FMath::Clamp(Value, 0.f, 1.f); }
	void SetVoiceVolume(float Value) { VoiceVolume = FMath::Clamp(Value, 0.f, 1.f); }
	void SetMicInputGain(float Value) { MicInputGain = FMath::Clamp(Value, MinMicInputGain, MaxMicInputGain); }
	void SetVoiceActivationThreshold(float Value) { VoiceActivationThreshold = FMath::Clamp(Value, 0.f, MaxVoiceActivationThreshold); }
	void SetMouseSensitivity(float Value) { MouseSensitivity = FMath::Clamp(Value, MinMouseSensitivity, MaxMouseSensitivity); }
	void SetMouseYInverted(bool bValue) { bInvertMouseY = bValue; }
	void SetMicrophoneMode(EDigetherMicrophoneMode Value) { MicrophoneMode = Value; }
	void SetCameraShakeReduced(bool bValue) { bReduceCameraShake = bValue; }
	void SetDisplayGamma(float Value) { DisplayGamma = FMath::Clamp(Value, MinDisplayGamma, MaxDisplayGamma); }
	void SetFieldOfView(float Value) { FieldOfView = FMath::Clamp(Value, MinFieldOfView, MaxFieldOfView); }
	void SetMotionBlurEnabled(bool bValue) { bMotionBlur = bValue; }

	static constexpr float MinMouseSensitivity = 0.1f;
	static constexpr float MaxMouseSensitivity = 3.f;
	// 엔진 voice.MicInputGain 기본 1
	static constexpr float MinMicInputGain = 0.5f;
	static constexpr float MaxMicInputGain = 3.f;
	// 엔진 voice.SilenceDetectionThreshold 기본 0.08, 0이면 모든 소리 송신
	static constexpr float DefaultVoiceActivationThreshold = 0.08f;
	static constexpr float MaxVoiceActivationThreshold = 0.3f;
	// 엔진 DisplayGamma 기본 2.2, 클수록 밝음
	static constexpr float DefaultDisplayGamma = 2.2f;
	static constexpr float MinDisplayGamma = 1.6f;
	static constexpr float MaxDisplayGamma = 3.f;
	// 엔진 PlayerCameraManager 기본 90, 범위는 시험값
	static constexpr float DefaultFieldOfView = 90.f;
	static constexpr float MinFieldOfView = 60.f;
	static constexpr float MaxFieldOfView = 110.f;

private:
	UPROPERTY(Config) float MasterVolume = 1.f;
	// 효과음·음악은 DIGETHER Options의 SoundMix·SoundClass 지정 시 적용, 미지정이면 저장만
	UPROPERTY(Config) float EffectsVolume = 1.f;
	UPROPERTY(Config) float MusicVolume = 1.f;
	// 동료 목소리 전체 크기, 플레이어별 크기와 곱해 적용
	UPROPERTY(Config) float VoiceVolume = 1.f;
	UPROPERTY(Config) float MicInputGain = 1.f;
	UPROPERTY(Config) float VoiceActivationThreshold = DefaultVoiceActivationThreshold;
	UPROPERTY(Config) float MouseSensitivity = 1.f;
	UPROPERTY(Config) bool bInvertMouseY = false;
	UPROPERTY(Config) EDigetherMicrophoneMode MicrophoneMode = EDigetherMicrophoneMode::AlwaysOn;
	UPROPERTY(Config) bool bReduceCameraShake = false;
	UPROPERTY(Config) float DisplayGamma = DefaultDisplayGamma;
	UPROPERTY(Config) float FieldOfView = DefaultFieldOfView;
	UPROPERTY(Config) bool bMotionBlur = true;

	// 적용 중인 SoundMix·SoundClass, 오디오 스레드가 쓰는 동안 GC 방지
	UPROPERTY(Transient) TArray<TObjectPtr<UObject>> AppliedAudioAssets;
};
