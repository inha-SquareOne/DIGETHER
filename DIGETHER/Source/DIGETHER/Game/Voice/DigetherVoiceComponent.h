#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DigetherVoiceComponent.generated.h"

class APlayerController;
class APlayerState;
class UDigetherVoiceTalker;
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

	// 누르고 말하기 키 입력: 누르고 말하기는 누르는 동안, 토글은 누를 때마다 전환, 항상 켜짐은 무시
	void SetPushToTalkHeld(bool bHeld);

	// 지금 마이크 송신 중인지, HUD 마이크 표시용
	UFUNCTION(BlueprintPure, Category="Digether|Voice")
	bool IsMicrophoneOpen() const { return bHasAppliedSpeaking && bAppliedSpeaking; }

	// 설정 변경 직후 즉시 반영, 호출하지 않아도 갱신 주기에 반영
	void ApplyMicrophoneMode();

	// 나 말고 음성 대상인 참가자, 플레이어별 음량 화면 목록용
	UFUNCTION(BlueprintCallable, Category="Digether|Voice")
	TArray<APlayerState*> GetRemoteVoicePlayers() const;

	// 이 PC에서만 적용, 0~MaxPlayerVolume, 접속 중에만 유지(저장 안 함)
	UFUNCTION(BlueprintCallable, Category="Digether|Voice")
	void SetPlayerVoiceVolume(APlayerState* Player, float Volume);

	UFUNCTION(BlueprintPure, Category="Digether|Voice")
	float GetPlayerVoiceVolume(const APlayerState* Player) const;

	// 나만 안 들림, 상대·서버 수신 제한과 별개
	UFUNCTION(BlueprintCallable, Category="Digether|Voice")
	void SetPlayerVoiceMuted(APlayerState* Player, bool bMuted);

	UFUNCTION(BlueprintPure, Category="Digether|Voice")
	bool IsPlayerVoiceMuted(const APlayerState* Player) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 누르고 말하기는 누르는 동안·토글은 누를 때마다 송신 전환, 키 재설정은 이 액션 에셋에 Player Mappable Key Settings 지정 후 가능
	// 비어 있으면 DIGETHER Options의 고정 키(임시 V)로 실행 중 생성
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
	void ApplyPlaybackVolume(const APlayerState* Player) const;
	static FString PlayerKey(const APlayerState* Player);
#if !UE_BUILD_SHIPPING
	// 호스트에서 다른 참가자 발화 감지 여부 기록(PQ-13), 콘솔 Digether.VoiceProbe 1
	void UpdateProbe();
	TMap<FString, bool> ProbeTalking;
	bool bProbeStarted = false;
#endif

	FTimerHandle RefreshTimer;
	bool bServerVoiceEnabled = true;
	bool bPushToTalkHeld = false;
	// 토글 모드 켜짐 상태, 접속마다 꺼짐으로 시작
	bool bToggleOpen = false;
	bool bHasAppliedSpeaking = false;
	bool bAppliedSpeaking = false;
	bool bInputBound = false;

	// 생존자 목소리 거리 감쇠, 시험값으로 실행 중 생성
	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> SurvivorAttenuation;

	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> BuriedAttenuation;

	// 비공간 음성에도 감쇠 설정 객체를 유지해 발화 중 공간/비공간 전환 지원
	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> UnspatializedAttenuation;

	// 액션 에셋 미지정 시 실행 중 생성한 누르고 말하기 입력
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FallbackPushToTalkAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> FallbackMappingContext;

	// 실제로 붙인 컨텍스트(에셋 또는 대체), EndPlay 제거용
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> AppliedMappingContext;

	// 발화자 PlayerState별 재생 설정, 컴포넌트는 그 PlayerState가 소유·수명 관리
	TMap<TWeakObjectPtr<APlayerState>, TWeakObjectPtr<UDigetherVoiceTalker>> Talkers;

	// 고유 ID 문자열 기준, 재접속해도 같은 사람이면 유지
	TMap<FString, float> PlayerVolumes;
	TSet<FString> LocallyMutedPlayers;
};
