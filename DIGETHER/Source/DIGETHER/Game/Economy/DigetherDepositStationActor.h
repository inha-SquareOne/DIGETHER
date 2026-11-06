#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/Economy/DigetherEconomyTypes.h"
#include "DigetherDepositStationActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UDigetherQuotaStateComponent;

// 납부 금액·붓기 세션·바닥 금화 생성은 GameMode 소유
// 공개 신호기 단계·달성 알림만 BP로 전달
UCLASS()
class DIGETHER_API ADigetherDepositStationActor : public AActor
{
	GENERATED_BODY()

public:
	ADigetherDepositStationActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category="Digether|Deposit")
	FName GetStationId() const { return StationId; }

	// +X가 입구 바깥쪽인 붓기 기준 변환
	FTransform GetPourInletTransform() const;
	// 초과 금화 생성용 바닥 기준 변환
	FTransform GetCoinDropTransform() const;

	// 신호기 단계 변경·첫 연결·이어하기 상태 전달
	UFUNCTION(BlueprintImplementableEvent, Category="Digether|Deposit", meta=(DisplayName="On Payment Signal Changed"))
	void ReceivePaymentSignalChanged(EDigetherPaymentSignal Signal);

	// 이어하기·늦은 참가 재생을 제외한 목표 달성 연출
	// 새 사이클 색과 금괴 낙하 순서는 BP에서 조정
	UFUNCTION(BlueprintImplementableEvent, Category="Digether|Deposit", meta=(DisplayName="On Quota Completed"))
	void ReceiveQuotaCompleted();

private:
	void TryBindQuota();
	void HandleQuotaStateChanged(const FDigetherQuotaPublicState& State);
	void HandleQuotaCompleted();
	void PublishSignal(EDigetherPaymentSignal Signal);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Deposit", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Deposit", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> MachineMesh;

	// 서버 거리·앞면·시선·가림 판정용 BP 입구 기준점
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Deposit", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> PourInlet;

	// 초과 금화 더미의 BP 배치 기준점
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Deposit", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> CoinDropPoint;

	// 레벨 내 납부대별 고유 ID
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Digether|Deposit", meta=(AllowPrivateAccess="true"))
	FName StationId;

	TWeakObjectPtr<UDigetherQuotaStateComponent> BoundQuota;
	FDelegateHandle QuotaChangedHandle;
	FDelegateHandle QuotaCompletedHandle;
	FTimerHandle BindRetryTimer;
	EDigetherPaymentSignal LastSignal = EDigetherPaymentSignal::None;
	bool bSignalPublished = false;
};
