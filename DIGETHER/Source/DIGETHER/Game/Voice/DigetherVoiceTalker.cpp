#include "Game/Voice/DigetherVoiceTalker.h"

#include "Components/AudioComponent.h"
#include "Components/SynthComponent.h"
#include "Sound/SoundAttenuation.h"

void UDigetherVoiceTalker::SetPlaybackVolume(const float Volume)
{
	PlaybackVolume = FMath::Max(0.f, Volume);
	if (UAudioComponent* Audio = ActiveAudio.Get())
	{
		Audio->SetVolumeMultiplier(PlaybackVolume);
	}
}

void UDigetherVoiceTalker::SetPlaybackEnvironment(USceneComponent* Source, USoundAttenuation* Attenuation,
	const bool bBuried, const float LowPassHz)
{
	const float EffectiveHz = FMath::IsFinite(LowPassHz) ? FMath::Clamp(LowPassHz, 20.f, 20000.f) : 1500.f;
	const bool bChanged = Settings.ComponentToAttachTo.Get() != Source || Settings.AttenuationSettings != Attenuation
		|| bPlaybackBuried != bBuried || BuriedLowPassHz != EffectiveHz;
	Settings.ComponentToAttachTo = Source;
	Settings.AttenuationSettings = Attenuation;
	bPlaybackBuried = bBuried;
	BuriedLowPassHz = EffectiveHz;
	if (bChanged)
	{
		ApplyPlaybackEnvironment();
	}
}

void UDigetherVoiceTalker::ApplyPlaybackEnvironment()
{
	UAudioComponent* Audio = ActiveAudio.Get();
	if (Audio == nullptr || Settings.AttenuationSettings == nullptr)
	{
		return;
	}
	// 매 프로필에 감쇠 객체가 있어 ActiveSound의 감쇠 비트를 발화 시작부터 유지
	const FSoundAttenuationSettings& Attenuation = Settings.AttenuationSettings->Attenuation;
	Audio->AdjustAttenuation(Attenuation);
	Audio->bAllowSpatialization = true;
	if (USynthComponent* Synth = Cast<USynthComponent>(Audio->GetOuter()))
	{
		// VOIP는 OnTalkingBegin 없이도 Synth를 재시작하므로 Synth 원본까지 동기화
		Synth->AttenuationSettings = Settings.AttenuationSettings;
		Synth->bOverrideAttenuation = true;
		Synth->AttenuationOverrides = Attenuation;
		Synth->bAllowSpatialization = true;
	}
	if (USceneComponent* Source = Settings.ComponentToAttachTo.Get())
	{
		Audio->AttachToComponent(Source, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	else
	{
		Audio->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}
	// LPF setter는 ActiveSound만 바꾸므로 다음 재생용 프로퍼티도 함께 보관
	Audio->bEnableLowPassFilter = bPlaybackBuried;
	Audio->LowPassFilterFrequency = BuriedLowPassHz;
	Audio->SetLowPassFilterEnabled(bPlaybackBuried);
	Audio->SetLowPassFilterFrequency(BuriedLowPassHz);
}

void UDigetherVoiceTalker::OnTalkingBegin(UAudioComponent* AudioComponent)
{
	ActiveAudio = AudioComponent;
	if (AudioComponent != nullptr)
	{
		ApplyPlaybackEnvironment();
		AudioComponent->SetVolumeMultiplier(PlaybackVolume);
	}
	Super::OnTalkingBegin(AudioComponent);
}

void UDigetherVoiceTalker::OnTalkingEnd()
{
	ActiveAudio.Reset();
	Super::OnTalkingEnd();
}
