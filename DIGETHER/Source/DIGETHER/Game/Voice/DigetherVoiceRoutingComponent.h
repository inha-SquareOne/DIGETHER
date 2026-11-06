#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Voice/DigetherVoiceTypes.h"
#include "DigetherVoiceRoutingComponent.generated.h"

// 서버 전용: 듣는 사람별 음소거(GameplayMutePlayer)로 음성 수신 제한
// 입력 명단은 소유 GameMode가 공급, 엔진이 음소거된 발화자 패킷을 그 연결에 중계하지 않음
UCLASS(ClassGroup=(Digether))
class DIGETHER_API UDigetherVoiceRoutingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherVoiceRoutingComponent();

	// 명단 공급자, 서버 GameMode가 BeginPlay 전에 지정
	DECLARE_DELEGATE_TwoParams(FCollectParticipants, TArray<FDigetherVoiceRouteEntry>&, EDigetherVoiceStage&);
	FCollectParticipants CollectParticipants;

	// 명단 기준으로 즉시 다시 적용, 시험·상태 변화 직후용
	void Refresh();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	FTimerHandle RefreshTimer;
	// 듣는 PC별 이 컴포넌트가 건 음소거(UniqueId 문자열)
	TMap<TWeakObjectPtr<APlayerController>, TSet<FString>> AppliedMutes;
	// UniqueId 문자열 → 원본, 이 월드에서 본 적 있는 참가자
	TMap<FString, FUniqueNetIdRepl> KnownIds;
};
