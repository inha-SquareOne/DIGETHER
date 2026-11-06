#pragma once

#include "CoreMinimal.h"
#include "Game/Run/DigetherGameOverTypes.h"
#include "GameFramework/Actor.h"
#include "DigetherHubWellActor.generated.h"

class USceneComponent;
class UDigetherGameOverPublicStateComponent;

// 기획 S33 실패 사건 우물, 거점 맵에 하나만 배치
// 서버는 흡입 중심·반경·대기 지점만 사용, 범람·침수·흡입 연출은 각 클라이언트 BP 이벤트(공통 컷신·조작 잠금 없음)
UCLASS()
class DIGETHER_API ADigetherHubWellActor : public AActor
{
	GENERATED_BODY()

public:
	ADigetherHubWellActor();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 맵에 정확히 하나일 때만 반환, 없거나 여럿이면 nullptr·호출자 대체 기준 사용
	static ADigetherHubWellActor* Find(const UWorld* World);

	// 흡입 중심(우물 입구)
	FVector GetSuctionCenter() const;
	// 흡입된 참가자의 우물 아래 대기 자리, Index번째를 대기 지점 둘레에 간격 배치
	FTransform GetWaitingTransform(int32 Index) const;
	float GetSuctionRadiusCm() const { return SuctionRadiusCm; }

	// 실패 사건(범람) 시작, FloodSeconds 뒤 흡입 시작·FailMotionSeconds가 흡입 마감
	UFUNCTION(BlueprintImplementableEvent, Category="Digether|Game Over", meta=(DisplayName="On Flood Start"))
	void BP_OnFloodStart(float FloodSeconds, float FailMotionSeconds);

	// 흡입 시작, 이후 우물에 가까운 사람부터 흡입
	UFUNCTION(BlueprintImplementableEvent, Category="Digether|Game Over", meta=(DisplayName="On Suction Start"))
	void BP_OnSuctionStart();

	// 실패 사건 종료(크레딧 시작 또는 새 게임), 남은 물·흡입 연출 정리
	UFUNCTION(BlueprintImplementableEvent, Category="Digether|Game Over", meta=(DisplayName="On Failure Event End"))
	void BP_OnFailureEventEnd();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Game Over")
	TObjectPtr<USceneComponent> SceneRoot;

	// 우물 아래 대기 지점, 흡입된 Pawn은 충돌·이동 해제 후 주변 배치
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Game Over")
	TObjectPtr<USceneComponent> WaitingPoint;

	// 임시값: 기획 미정(흡입 반경, cm), 2026-11-05, 더 먼 사람도 이 거리로 흡입 시각 계산(마감 전 흡입 보장)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Digether|Game Over", meta=(ClampMin="1.0"))
	float SuctionRadiusCm = 3000.f;

	// 임시값: 기획 미정(대기 지점 간격, cm), 2026-11-05
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
