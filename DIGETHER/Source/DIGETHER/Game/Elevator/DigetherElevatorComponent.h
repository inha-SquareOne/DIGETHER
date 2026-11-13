#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Elevator/DigetherElevatorTuning.h"
#include "Game/Elevator/DigetherElevatorTypes.h"
#include "DigetherElevatorComponent.generated.h"

struct FDigetherRunTuning;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherElevatorStateChanged, const FDigetherElevatorPublicState&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherElevatorAscentConfirmed, const FDigetherElevatorAscentConfirmed&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherElevatorExplorationFinished, const FDigetherElevatorAscentConfirmed&);
// 인자: 문 닫힘을 시작한 대기 단계(HubWaiting·BottomWaiting)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherElevatorDoorEvent, EDigetherElevatorPhase);

// 소유 Actor 복제 필요, 서버 조율자가 AdvanceTravel 호출
UCLASS(ClassGroup=(Digether), meta=(BlueprintSpawnableComponent))
class DIGETHER_API UDigetherElevatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherElevatorComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버 조율자가 전체 변경을 마칠 때까지 표현 알림만 보류, 확정 신호는 즉시 전달
	void BeginNotificationBatch();
	void EndNotificationBatch();

	EDigetherElevatorResult InitializeForRun(const FDigetherRunTuning& RunTuning, const FDigetherElevatorTuning& Tuning);
	// 거점 대기 또는 거점 문 닫힘에서 첫 하강 시작
	EDigetherElevatorResult DepartHub();
	// 하부 대기 또는 하부 문 닫힘에서 상승 시작
	EDigetherElevatorResult ConfirmAscent(const FDigetherElevatorAscentConfirmed& Ascent);
	// 갱도 상승 문이 닫히는 중 무게 한도 초과 시 문 재개방 후 OnDoorReopenedByOverweight 전송
	// 거점 출발 문은 무게로 재개방하지 않음(게이지 완료 뒤 출발 취소 불가)
	EDigetherElevatorResult UpdateLoad(const TArray<FDigetherElevatorLoadEntry>& LoadEntries);
	// 문 닫힘 시간이 끝난 호출은 DoorClosed 반환, OnDoorClosed 1회 전송
	EDigetherElevatorResult AdvanceTravel(float DeltaSeconds);
	// 거점·하부 대기에서 문 닫힘 시작, 공개 적재 단계가 Over면 OverCapacity로 거절
	EDigetherElevatorResult BeginDoorClose();
	// 조율자가 고정·출발에 실패했을 때 문 재개방, 재개방 알림 없음
	EDigetherElevatorResult ReopenDoor();

	UFUNCTION(BlueprintPure, Category="Digether|Elevator")
	FDigetherElevatorPublicState GetPublicState() const;

	// 정산·이동 조율자가 확정 매니페스트를 읽는 서버 전용 경계, 클라이언트에는 공개 상태만 제공
	bool TryGetLastAscent(FDigetherElevatorAscentConfirmed& OutAscent) const;
	bool TryGetServerState(FDigetherElevatorState& OutState) const;

#if !UE_BUILD_SHIPPING
	// 개발 표시(V-03), 서버 전용 값: 최근 UpdateLoad가 무게 단계 판정에 쓴 중량 합계
	double GetLastLoadWeightKgForDev() const { return LastLoadWeightKg; }
	float GetCapacityKgForDev() const { return CapacityKg; }
	int32 GetTripLimitForDev() const { return TripLimit; }
#endif

	FOnDigetherElevatorStateChanged OnStateChanged;
	FOnDigetherElevatorAscentConfirmed OnAscentConfirmed;
	FOnDigetherElevatorExplorationFinished OnExplorationFinished;
	FOnDigetherElevatorDoorEvent OnDoorClosed;
	FOnDigetherElevatorDoorEvent OnDoorReopenedByOverweight;

private:
	bool RefreshPublicState(EDigetherElevatorWeightStage WeightStage);
	// 서버 상태를 공개 상태로 반영하고 변경 시 상태 알림(묶음 중이면 보류), 공개 상태 변경 여부 반환
	bool CommitState(EDigetherElevatorWeightStage WeightStage);
	void DispatchStateNotification(const FDigetherElevatorPublicState& StateSnapshot);

	UFUNCTION()
	void OnRep_PublicState();

	UPROPERTY(ReplicatedUsing=OnRep_PublicState)
	FDigetherElevatorPublicState PublicState;

	UPROPERTY(Transient)
	FDigetherElevatorState State;

	UPROPERTY(Transient)
	float CapacityKg = 0.f;

	UPROPERTY(Transient)
	int32 TripLimit = 0;

	UPROPERTY(Transient)
	float TravelSeconds = 0.f;

	UPROPERTY(Transient)
	FDigetherElevatorTuning ElevatorTuning;

	UPROPERTY(Transient)
	FDigetherElevatorAscentConfirmed LastAscent;

	UPROPERTY(Transient)
	bool bHasLastAscent = false;

	UPROPERTY(Transient)
	bool bInitialized = false;

	int32 NotificationBatchDepth = 0;
	bool bStateNotificationPending = false;

#if !UE_BUILD_SHIPPING
	// 개발 표시 전용 기록, 판정에는 쓰지 않음
	double LastLoadWeightKg = 0.0;
#endif
};
