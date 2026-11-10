#include "Game/Voice/DigetherVoiceTalker.h"

#include "Components/AudioComponent.h"

void UDigetherVoiceTalker::SetPlaybackVolume(const float Volume)
{
	PlaybackVolume = FMath::Max(0.f, Volume);
	if (UAudioComponent* Audio = ActiveAudio.Get())
	{
		Audio->SetVolumeMultiplier(PlaybackVolume);
	}
}

void UDigetherVoiceTalker::OnTalkingBegin(UAudioComponent* AudioComponent)
{
	ActiveAudio = AudioComponent;
	if (AudioComponent != nullptr)
	{
		AudioComponent->SetVolumeMultiplier(PlaybackVolume);
	}
	Super::OnTalkingBegin(AudioComponent);
}

void UDigetherVoiceTalker::OnTalkingEnd()
{
	ActiveAudio.Reset();
	Super::OnTalkingEnd();
}
