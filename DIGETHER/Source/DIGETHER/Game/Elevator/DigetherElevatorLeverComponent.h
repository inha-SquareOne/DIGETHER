#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Elevator/DigetherElevatorTuning.h"
#include "Game/Elevator/DigetherElevatorTypes.h"
#include "DigetherElevatorLeverComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherElevatorLeverPublicStateChanged, const FDigetherElevatorLeverPublicState&);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDigetherElevatorLeverVisualStateChanged,
	const FDigetherElevatorLeverPublicState&, State);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherElevatorLeverFeedback, const FDigetherElevatorLeverFeedback&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherElevatorLeverAscentConfirmed, const FDigetherElevatorAscentConfirmed&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherElevatorLeverDepartureConfirmed, const FDigetherElevatorDepartureConfirmed&);

// 입력·시간·현재 조건은 서버 조율자가 전달, 엘리베이터·일차 시스템 직접 호출 안 함
UCLASS(ClassGroup=(Digether), meta=(BlueprintSpawnableComponent))
class DIGETHER_API UDigetherElevatorLeverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherElevatorLeverComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버 조율자가 전체 변경을 마칠 때 표현 상태·피드백 전달, 확정 신호는 즉시 전달
	void BeginNotificationBatch();
	void EndNotificationBatch();

	// 성공 시 Released 반환, Idle로 초기화
	EDigetherElevatorLeverResult Configure(EDigetherElevatorLeverKind Kind, const FDigetherElevatorTuning& Tuning);
	EDigetherElevatorLeverResult TryBeginPull(const FGuid& PullerId, const FDigetherElevatorPullContext& Context);
	EDigetherElevatorLeverResult ReleasePull(const FGuid& PullerId);
	EDigetherElevatorLeverResult AdvancePull(float DeltaSeconds, const FDigetherElevatorPullContext& Context);
	EDigetherElevatorLeverResult ResetPull();

	UFUNCTION(BlueprintPure, Category="Digether|Elevator")
	FDigetherElevatorLeverPublicState GetPublicState() const;

	// 경고 종류별 안내 문구, 표시 대상(Standard는 팀원, Deadline은 레버 조작자)은 BP 결정
	UFUNCTION(BlueprintPure, Category="Digether|Elevator")
	static FText GetDepartureWarningText(EDigetherDepartureWarningKind WarningKind);

	// 서버가 실제 조작자와 레버 사이 거리를 유지 검사할 때만 사용
	FGuid GetActivePullerId() const;

	// 매니페스트는 서버 조율자 전용, Confirmed는 ResetPull/Configure 전까지 유지
	// 게이지 완료 순간 스냅샷, 기획 A3의 편 확정 목록은 문이 완전히 닫힌 순간 조율자가 재생성
	bool TryGetConfirmedAscent(FDigetherElevatorAscentConfirmed& OutAscent) const;

	FOnDigetherElevatorLeverPublicStateChanged OnPublicStateChanged;
	// 확정 공개 스냅샷의 BP 연출 전용 알림, 게임 상태 변경 금지·초기 표시는 GetPublicState 조회
	UPROPERTY(BlueprintAssignable, Category="Digether|Elevator")
	FOnDigetherElevatorLeverVisualStateChanged OnLeverVisualStateChanged;

	// 일회성 경고·덜컹임의 원격 클라이언트 전달은 조율자 통합 단계에서 연결 예정
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
