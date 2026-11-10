#pragma once

#include "CoreMinimal.h"
#include "Net/VoiceConfig.h"
#include "DigetherVoiceTalker.generated.h"

class UAudioComponent;

// 발화자 1명의 재생 설정, 엔진이 발화 시작 때 넘기는 오디오 컴포넌트에 음량 적용
UCLASS(ClassGroup=(Digether))
class DIGETHER_API UDigetherVoiceTalker : public UVOIPTalker
{
	GENERATED_BODY()

public:
	// 말하는 중이면 즉시, 아니면 다음 발화부터 적용
	void SetPlaybackVolume(float Volume);
	float GetPlaybackVolume() const { return PlaybackVolume; }

	virtual void OnTalkingBegin(UAudioComponent* AudioComponent) override;
	virtual void OnTalkingEnd() override;

private:
	TWeakObjectPtr<UAudioComponent> ActiveAudio;
	float PlaybackVolume = 1.f;
};
