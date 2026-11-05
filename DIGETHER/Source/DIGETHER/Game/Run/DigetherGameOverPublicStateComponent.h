#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Run/DigetherGameOverTypes.h"
#include "DigetherGameOverPublicStateComponent.generated.h"

class APlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDigetherGameOverPhaseChanged, EDigetherGameOverPhase, Phase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDigetherGameOverFailureEventStarted, float, FloodSeconds,
	float, FailMotionSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDigetherGameOverPlayerAbsorbed, int32, PlayerId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDigetherGameOverCreditsStarted, const FDigetherGameResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDigetherGameOverCreditPlaybackChanged, const FDigetherCreditPlayback&,
	Playback);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDigetherGameOverNewGameStarted);

// GameState에 부착해 게임 오버 공개 상태를 모두에게 복제한다(COND_None). 값은 서버 진행 컴포넌트가 확정한 것만 받는다.
// 연출(우물 범람·흡입·크레딧 원근 스크롤)은 BP가 아래 델리게이트와 진행 조회로 그린다.
UCLASS(ClassGroup=(Digether), meta=(BlueprintSpawnableComponent))
class DIGETHER_API UDigetherGameOverPublicStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherGameOverPublicStateComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버 전용이다. 이전 값과 비교해 같은 프로세스(호스트)에도 같은 델리게이트를 보낸다.
	bool SetPublicState(const FDigetherGameOverPublicState& NewState);

	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	FDigetherGameOverPublicState GetPublicState() const { return PublicState; }

	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	EDigetherGameOverPhase GetPhase() const { return PublicState.Phase; }

	// 크레딧 단계에서만 유효하다. 그 전에는 빈 결과다.
	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	FDigetherGameResult GetPublishedResult() const { return PublicState.PublishedResult; }

	// 범람·침수 진행률(0~1)이다. 실패 사건이 아니면 0이다. 흡입이 시작되면 1이다.
	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	float GetFloodAlpha() const;

	// 흡입 시작까지 남은 서버 시간(초)이다. 실패 사건이 아니거나 이미 시작했으면 0이다.
	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	float GetSecondsUntilSuction() const;

	// 모두 같은 서버 시각 기준점으로 계산하는 크레딧 진행(재생 초)이다. 크레딧이 아니면 0이다.
	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	float GetCreditProgressSeconds() const;

	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	float GetCreditTotalSeconds() const { return PublicState.Playback.TotalSeconds; }

	// 스크롤이 시작된 뒤 올라간 거리(화면 높이 단위)다. 시작 대기 동안은 0이다.
	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	float GetCreditScrollOffset() const;

	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	bool IsCreditFastForward() const { return PublicState.Playback.bFastForward; }

	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	bool IsPlayerAbsorbed(const APlayerState* Player) const;

	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	int32 GetWaitingPlayerCount() const { return PublicState.WaitingPlayerCount; }

	UPROPERTY(BlueprintAssignable, Category="Digether|Game Over")
	FOnDigetherGameOverPhaseChanged OnGameOverPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category="Digether|Game Over")
	FOnDigetherGameOverFailureEventStarted OnFailureEventStarted;

	UPROPERTY(BlueprintAssignable, Category="Digether|Game Over")
	FOnDigetherGameOverPlayerAbsorbed OnPlayerAbsorbed;

	UPROPERTY(BlueprintAssignable, Category="Digether|Game Over")
	FOnDigetherGameOverCreditsStarted OnCreditsStarted;

	UPROPERTY(BlueprintAssignable, Category="Digether|Game Over")
	FOnDigetherGameOverCreditPlaybackChanged OnCreditPlaybackChanged;

	// 크레딧이 끝나 같은 방의 새 거점(새 게임)이 시작됐다.
	UPROPERTY(BlueprintAssignable, Category="Digether|Game Over")
	FOnDigetherGameOverNewGameStarted OnNewGameStarted;

private:
	UFUNCTION()
	void OnRep_PublicState(const FDigetherGameOverPublicState& PreviousState);

	void BroadcastChanges(const FDigetherGameOverPublicState& PreviousState);
	double GetServerNowSeconds() const;

	UPROPERTY(ReplicatedUsing=OnRep_PublicState)
	FDigetherGameOverPublicState PublicState;
};
