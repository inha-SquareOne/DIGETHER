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
	EDigetherElevatorResult DepartHub();
	EDigetherElevatorResult ConfirmAscent(const FDigetherElevatorAscentConfirmed& Ascent);
	EDigetherElevatorResult UpdateLoad(const TArray<FDigetherElevatorLoadEntry>& LoadEntries);
	EDigetherElevatorResult AdvanceTravel(float DeltaSeconds);

	UFUNCTION(BlueprintPure, Category="Digether|Elevator")
	FDigetherElevatorPublicState GetPublicState() const;

	// 정산·이동 조율자가 확정 매니페스트를 읽는 서버 전용 경계다. 클라이언트에는 공개 상태만 제공한다.
	bool TryGetLastAscent(FDigetherElevatorAscentConfirmed& OutAscent) const;
	bool TryGetServerState(FDigetherElevatorState& OutState) const;

	FOnDigetherElevatorStateChanged OnStateChanged;
	FOnDigetherElevatorAscentConfirmed OnAscentConfirmed;
	FOnDigetherElevatorExplorationFinished OnExplorationFinished;

private:
	bool HasServerAuthority() const;
	bool RefreshPublicState(EDigetherElevatorWeightStage WeightStage);
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
