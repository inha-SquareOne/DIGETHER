#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Elevator/DigetherElevatorTuning.h"
#include "Game/Elevator/DigetherElevatorTypes.h"
#include "DigetherElevatorLeverComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherElevatorLeverPublicStateChanged, const FDigetherElevatorLeverPublicState&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherElevatorLeverFeedback, const FDigetherElevatorLeverFeedback&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherElevatorLeverAscentConfirmed, const FDigetherElevatorAscentConfirmed&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherElevatorLeverDepartureConfirmed, const FDigetherElevatorDepartureConfirmed&);

// 입력·시간·현재 조건은 서버 조율자가 전달한다. 엘리베이터나 일차 시스템을 직접 호출하지 않는다.
UCLASS(ClassGroup=(Digether), meta=(BlueprintSpawnableComponent))
class DIGETHER_API UDigetherElevatorLeverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherElevatorLeverComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버 조율자가 전체 변경을 마칠 때 표현 상태·피드백을 전달한다. 확정 신호는 즉시 전달한다.
	void BeginNotificationBatch();
	void EndNotificationBatch();

	// 성공하면 Released를 반환하고 Idle로 초기화한다.
	EDigetherElevatorLeverResult Configure(EDigetherElevatorLeverKind Kind, const FDigetherElevatorTuning& Tuning);
	EDigetherElevatorLeverResult TryBeginPull(const FGuid& PullerId, const FDigetherElevatorPullContext& Context);
	EDigetherElevatorLeverResult ReleasePull(const FGuid& PullerId);
	EDigetherElevatorLeverResult AdvancePull(float DeltaSeconds, const FDigetherElevatorPullContext& Context);
	EDigetherElevatorLeverResult ResetPull();

	UFUNCTION(BlueprintPure, Category="Digether|Elevator")
	FDigetherElevatorLeverPublicState GetPublicState() const;

	// 매니페스트는 서버 조율자만 가져간다. Confirmed는 ResetPull/Configure 전까지 유지한다.
	bool TryGetConfirmedAscent(FDigetherElevatorAscentConfirmed& OutAscent) const;

	FOnDigetherElevatorLeverPublicStateChanged OnPublicStateChanged;
	// 일회성 경고·덜컹임의 원격 클라이언트 전달은 조율자 통합 단계에서 연결한다.
	FOnDigetherElevatorLeverFeedback OnFeedback;
	FOnDigetherElevatorLeverAscentConfirmed OnAscentConfirmed;
	FOnDigetherElevatorLeverDepartureConfirmed OnDepartureConfirmed;

private:
	bool HasServerAuthority() const;
	bool RefreshPublicState();
	void DispatchPublicStateNotification(const FDigetherElevatorLeverPublicState& StateSnapshot);
	void DispatchFeedbackNotification(const FDigetherElevatorLeverFeedback& FeedbackSnapshot);
	EDigetherElevatorLeverResult ApplyGaugeUpdate(const FDigetherElevatorGaugeUpdate& Update);

	UFUNCTION()
	void OnRep_PublicState();

	UPROPERTY(ReplicatedUsing=OnRep_PublicState)
	FDigetherElevatorLeverPublicState PublicState;

	UPROPERTY(Transient)
	FDigetherElevatorGaugeState GaugeState;

	UPROPERTY(Transient)
	EDigetherElevatorLeverKind LeverKind = EDigetherElevatorLeverKind::Ascent;

	UPROPERTY(Transient)
	FDigetherElevatorTuning LeverTuning;

	UPROPERTY(Transient)
	bool bInitialized = false;

	int32 NotificationBatchDepth = 0;
	bool bPublicStateNotificationPending = false;
	TArray<FDigetherElevatorLeverFeedback> PendingFeedback;
};
