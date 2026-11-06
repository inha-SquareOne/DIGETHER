#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DigetherVoiceComponent.generated.h"

class APlayerController;
class APlayerState;
class UInputAction;
class UInputMappingContext;
class USoundAttenuation;
class UVOIPTalker;
struct FInputActionValue;

// 로컬 PC 전용 음성 처리: 마이크 모드·누르고 말하기, 다른 참가자 목소리 재생 설정, 관전 청취 위치
// 누가 누구를 듣는지는 서버 UDigetherVoiceRoutingComponent가 결정
UCLASS(ClassGroup=(Digether), meta=(BlueprintSpawnableComponent))
class DIGETHER_API UDigetherVoiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherVoiceComponent();

	// 소유 PC의 ClientEnableNetworkVoice 대체 진입점, 서버 기본 음성 허용 값 기록 후 개인 마이크 모드 적용
	void HandleServerVoiceEnabled(bool bEnable);

	// 누르고 말하기 입력, 항상 켜짐 모드에서는 무시
	void SetPushToTalkHeld(bool bHeld);

	// 설정 변경 직후 즉시 반영, 호출하지 않아도 갱신 주기에 반영
	void ApplyMicrophoneMode();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 단축키 미배정(2026-11-06), 지정 시 누르는 동안 송신
	UPROPERTY(EditDefaultsOnly, Category="Digether|Voice|Input")
	TObjectPtr<UInputAction> PushToTalkAction;

	UPROPERTY(EditDefaultsOnly, Category="Digether|Voice|Input")
	TObjectPtr<UInputMappingContext> VoiceMappingContext;

private:
	APlayerController* GetOwningController() const;
	void Refresh();
	void TryBindInput();
	void UpdateTalkers();
	void UpdateListener();
	void HandlePushToTalkStarted(const FInputActionValue& Value);
	void HandlePushToTalkReleased(const FInputActionValue& Value);
#if !UE_BUILD_SHIPPING
	// 호스트에서 다른 참가자 발화 감지 여부 기록(PQ-13), 콘솔 Digether.VoiceProbe 1
	void UpdateProbe();
	TMap<FString, bool> ProbeTalking;
	bool bProbeStarted = false;
#endif

	FTimerHandle RefreshTimer;
	bool bServerVoiceEnabled = true;
	bool bPushToTalkHeld = false;
	bool bHasAppliedSpeaking = false;
	bool bAppliedSpeaking = false;
	bool bInputBound = false;

	// 생존자 목소리 거리 감쇠, 시험값으로 실행 중 생성
	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> SurvivorAttenuation;

	// 발화자 PlayerState별 재생 설정, 컴포넌트는 그 PlayerState가 소유·수명 관리
	TMap<TWeakObjectPtr<APlayerState>, TWeakObjectPtr<UVOIPTalker>> Talkers;
};
