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
// 인자는 문 닫힘을 시작한 대기 단계(HubWaiting·BottomWaiting)다.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherElevatorDoorEvent, EDigetherElevatorPhase);

// 소유 Actor의 복제를 켜고 서버 조율자가 AdvanceTravel을 호출한다.
UCLASS(ClassGroup=(Digether), meta=(BlueprintSpawnableComponent))
class DIGETHER_API UDigetherElevatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherElevatorComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버 조율자가 전체 변경을 마칠 때까지 표현 알림만 보류한다. 확정 신호는 즉시 전달한다.
	void BeginNotificationBatch();
	void EndNotificationBatch();

	EDigetherElevatorResult InitializeForRun(const FDigetherRunTuning& RunTuning, const FDigetherElevatorTuning& Tuning);
	// 거점 대기 또는 거점 문 닫힘에서 첫 하강을 시작한다.
	EDigetherElevatorResult DepartHub();
	// 하부 대기 또는 하부 문 닫힘에서 상승을 시작한다.
	EDigetherElevatorResult ConfirmAscent(const FDigetherElevatorAscentConfirmed& Ascent);
	// 갱도 상승 문이 닫히는 중 무게가 한도를 넘으면 문을 다시 열고 OnDoorReopenedByOverweight를 보낸다.
	// 거점 출발 문은 무게로 다시 열지 않는다(게이지 완료 뒤 출발 취소 불가).
	EDigetherElevatorResult UpdateLoad(const TArray<FDigetherElevatorLoadEntry>& LoadEntries);
	// 문 닫힘 시간이 끝난 호출은 DoorClosed를 반환하고 OnDoorClosed를 한 번 보낸다.
	EDigetherElevatorResult AdvanceTravel(float DeltaSeconds);
	// 거점·하부 대기에서 문 닫힘을 시작한다. 공개 적재 단계가 Over면 OverCapacity로 거절한다.
	EDigetherElevatorResult BeginDoorClose();
	// 조율자가 고정·출발에 실패했을 때 문을 다시 연다. 재개방 알림은 보내지 않는다.
	EDigetherElevatorResult ReopenDoor();

	UFUNCTION(BlueprintPure, Category="Digether|Elevator")
	FDigetherElevatorPublicState GetPublicState() const;

	// 정산·이동 조율자가 확정 매니페스트를 읽는 서버 전용 경계다. 클라이언트에는 공개 상태만 제공한다.
	bool TryGetLastAscent(FDigetherElevatorAscentConfirmed& OutAscent) const;
	bool TryGetServerState(FDigetherElevatorState& OutState) const;

	FOnDigetherElevatorStateChanged OnStateChanged;
	FOnDigetherElevatorAscentConfirmed OnAscentConfirmed;
	FOnDigetherElevatorExplorationFinished OnExplorationFinished;
	FOnDigetherElevatorDoorEvent OnDoorClosed;
	FOnDigetherElevatorDoorEvent OnDoorReopenedByOverweight;

private:
	bool RefreshPublicState(EDigetherElevatorWeightStage WeightStage);
	// 서버 상태를 공개 상태로 반영하고 바뀌었으면 상태 알림을 보낸다(묶음 중이면 보류). 공개 상태가 바뀌었는지 돌려준다.
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
};
