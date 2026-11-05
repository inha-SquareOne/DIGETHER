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

	// 경고 종류별 안내 문구다. Standard는 팀원에게, Deadline은 레버 조작자에게 보여 주는 것은 BP가 정한다.
	UFUNCTION(BlueprintPure, Category="Digether|Elevator")
	static FText GetDepartureWarningText(EDigetherDepartureWarningKind WarningKind);

	// 서버가 실제 조작자와 레버 사이 거리를 유지 검사할 때만 사용한다.
	FGuid GetActivePullerId() const;

	// 매니페스트는 서버 조율자만 가져간다. Confirmed는 ResetPull/Configure 전까지 유지한다.
	// 게이지 완료 순간의 스냅샷이다. 기획 A3의 편 확정 목록은 문이 완전히 닫힌 순간 조율자가 다시 만든다.
	bool TryGetConfirmedAscent(FDigetherElevatorAscentConfirmed& OutAscent) const;

	FOnDigetherElevatorLeverPublicStateChanged OnPublicStateChanged;
	// 일회성 경고·덜컹임의 원격 클라이언트 전달은 조율자 통합 단계에서 연결한다.
	FOnDigetherElevatorLeverFeedback OnFeedback;
	FOnDigetherElevatorLeverAscentConfirmed OnAscentConfirmed;
	FOnDigetherElevatorLeverDepartureConfirmed OnDepartureConfirmed;

private:
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
