#pragma once

#include "CoreMinimal.h"
#include "Net/VoiceConfig.h"
#include "DigetherVoiceTalker.generated.h"

class UAudioComponent;
class USceneComponent;
class USoundAttenuation;

// 발화자별 재생 설정, 현재 발화와 엔진의 Synth 재시작에 같은 설정 적용
UCLASS(ClassGroup=(Digether))
class DIGETHER_API UDigetherVoiceTalker : public UVOIPTalker
{
	GENERATED_BODY()

public:
	// 말하는 중이면 즉시, 아니면 다음 발화부터 적용
	void SetPlaybackVolume(float Volume);
	float GetPlaybackVolume() const { return PlaybackVolume; }

	// 감쇠 객체는 비공간 프로필에도 지정, 다른 발화자의 공용 설정은 수정하지 않음
	void SetPlaybackEnvironment(USceneComponent* Source, USoundAttenuation* Attenuation, bool bBuried, float LowPassHz);

	virtual void OnTalkingBegin(UAudioComponent* AudioComponent) override;
	virtual void OnTalkingEnd() override;

private:
	void ApplyPlaybackEnvironment();

	TWeakObjectPtr<UAudioComponent> ActiveAudio;
	float PlaybackVolume = 1.f;
	float BuriedLowPassHz = 1500.f;
	bool bPlaybackBuried = false;
};
