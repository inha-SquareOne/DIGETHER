#pragma once

#include "CoreMinimal.h"
#include "Game/Run/DigetherGameOverTypes.h"
#include "GameFramework/Actor.h"
#include "DigetherHubWellActor.generated.h"

class USceneComponent;
class UDigetherGameOverPublicStateComponent;

// 기획 S33 실패 사건의 우물이다. 거점 맵에 하나 둔다. 서버는 흡입 중심·반경·대기 지점만 읽고,
// 범람·침수·흡입 연출은 BP 이벤트로 각 클라이언트가 그린다(공통 컷신·조작 잠금 없음).
UCLASS()
class DIGETHER_API ADigetherHubWellActor : public AActor
{
	GENERATED_BODY()

public:
	ADigetherHubWellActor();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 맵에 정확히 하나일 때만 돌려준다. 없거나 여러 개이면 nullptr이고 호출자가 대체 기준을 쓴다.
	static ADigetherHubWellActor* Find(const UWorld* World);

	// 흡입 중심(우물 입구)이다.
	FVector GetSuctionCenter() const;
	// 흡입된 참가자가 기다리는 우물 아래 자리다. Index번째 사람을 대기 지점 둘레에 간격을 두고 놓는다.
	FTransform GetWaitingTransform(int32 Index) const;
	float GetSuctionRadiusCm() const { return SuctionRadiusCm; }

	// 실패 사건 시작(범람 시작)이다. FloodSeconds 뒤 흡입이 시작되고 FailMotionSeconds가 흡입 마감이다.
	UFUNCTION(BlueprintImplementableEvent, Category="Digether|Game Over", meta=(DisplayName="On Flood Start"))
	void BP_OnFloodStart(float FloodSeconds, float FailMotionSeconds);

	// 흡입 시작이다. 이 뒤로 우물에 가까운 사람부터 빨려 들어간다.
	UFUNCTION(BlueprintImplementableEvent, Category="Digether|Game Over", meta=(DisplayName="On Suction Start"))
	void BP_OnSuctionStart();

	// 실패 사건이 끝났다(크레딧 시작 또는 새 게임). 남은 물·흡입 연출을 정리한다.
	UFUNCTION(BlueprintImplementableEvent, Category="Digether|Game Over", meta=(DisplayName="On Failure Event End"))
	void BP_OnFailureEventEnd();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Game Over")
	TObjectPtr<USceneComponent> SceneRoot;

	// 우물 아래 대기 지점이다. 흡입된 Pawn은 충돌·이동을 끄고 이 주변에 놓인다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Game Over")
	TObjectPtr<USceneComponent> WaitingPoint;

	// 임시값: 기획 미정(흡입 반경, cm), 2026-11-05. 이보다 먼 사람은 이 거리로 보고 흡입 시각을 정한다(마감 전 흡입 보장).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Digether|Game Over", meta=(ClampMin="1.0"))
	float SuctionRadiusCm = 3000.f;

	// 임시값: 기획 미정(대기 지점 간격, cm), 2026-11-05.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Digether|Game Over", meta=(ClampMin="0.0"))
	float WaitingSpacingCm = 120.f;

private:
	void TryBindPublicState();
	UFUNCTION()
	void HandlePhaseChanged(EDigetherGameOverPhase Phase);
	UFUNCTION()
	void HandleFailureEventStarted(float FloodSeconds, float FailMotionSeconds);
	void HandleSuctionStart();

	TWeakObjectPtr<UDigetherGameOverPublicStateComponent> BoundPublicState;
	FTimerHandle BindRetryTimer;
	FTimerHandle SuctionTimer;
	bool bFailureEventActive = false;
};
