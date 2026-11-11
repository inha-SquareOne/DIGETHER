#pragma once

#include "CoreMinimal.h"
#include "Game/Monster/DigetherMonsterSpeciesComponent.h"
#include "Game/Monster/Spider/DigetherGroundSpiderTypes.h"
#include "DigetherGroundSpiderComponent.generated.h"

class ADigetherMonsterCharacter;
class ADigetherSpiderNestActor;
class APawn;
class UBlackboardComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDigetherSpiderStateChanged, const FDigetherSpiderPublicState&, State);

// 땅거미 1단계 규칙(설계 3.5): 둥지 영역·감지 5종·추적·포기·도약·접촉 피해·거미줄·매몰 재등장
// 상태·판정은 서버, BT는 상태(Blackboard)를 보고 이동 순서만 정함, 클라이언트는 공개 상태로 연출
UCLASS(ClassGroup=(Digether), meta=(BlueprintSpawnableComponent))
class DIGETHER_API UDigetherGroundSpiderComponent : public UDigetherMonsterSpeciesComponent
{
	GENERATED_BODY()

public:
	// Blackboard 키 이름, BT·Blackboard 에셋은 이 이름으로 만듦
	static const FName BB_SpiderState;
	static const FName BB_TargetActor;
	static const FName BB_TargetLocation;
	static const FName BB_HomeLocation;
	static const FName BB_InvestigateLocation;

	UDigetherGroundSpiderComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual const UScriptStruct* GetRequiredParamsStruct() const override;
	virtual void HandleStimuli(TConstArrayView<FDigetherMonsterStimulusEvent> Events) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// BT용: 추적 중이고 대상이 도약 거리 안·시야 막힘 없음
	bool CanLeapNow() const;
	bool StartLeap();
	void CancelLeap();
	bool IsLeaping() const;

	// 서버, 붕괴로 매몰됐을 때 붕괴 쪽이 호출(1단계: 시간 뒤 가장 가까운 빈 공간에 나타남)
	UFUNCTION(BlueprintCallable, Category="Digether|Spider")
	void NotifyBuried();

	UFUNCTION(BlueprintPure, Category="Digether|Spider")
	const FDigetherSpiderPublicState& GetPublicState() const { return PublicState; }

	EDigetherSpiderState GetState() const { return PublicState.State; }
	const FString& GetTargetKey() const { return TargetKey; }
	const FVector& GetHomeLocation() const { return HomeLocation; }
	float GetTrackRadius() const;

	// 클라이언트 포함 상태 변경 통지(연출용)
	UPROPERTY(BlueprintAssignable, Category="Digether|Spider")
	FOnDigetherSpiderStateChanged OnStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FLeapRuntime
	{
		int32 LeapId = 0;
		FVector Start = FVector::ZeroVector;
		FVector Destination = FVector::ZeroVector;
		double PhaseEndTime = 0.0;
		TSet<FString> HitKeys;
	};

	const FDigetherGroundSpiderParams* GetParams() const;
	ADigetherMonsterCharacter* GetSpider() const;
	UBlackboardComponent* GetBlackboard() const;
	double Now() const;
	double ServerTimeNow() const;

	void SetState(EDigetherSpiderState NewState, float Duration = 0.f);
	void BeginTracking(const FString& Key, bool bChaseOutside);
	void GiveUp(EDigetherSpiderGiveUpReason Reason);
	APawn* FindTargetPawn(const FString& Key) const;
	bool IsReachable(const FVector& Location) const;
	bool IsReachableCached(const FString& Key, const FVector& Location);

	void TickTracking();
	void TickLeap(float DeltaTime);
	void TickContact();
	void FinishLeapFlight();
	void Emerge();
	void SetupNest();

	UFUNCTION()
	void HandleDied();
	void HandleDamagedByServer(AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION()
	void OnRep_PublicState();

	void DrawDebug() const;

	UPROPERTY(ReplicatedUsing=OnRep_PublicState)
	FDigetherSpiderPublicState PublicState;

	UPROPERTY(Transient)
	TObjectPtr<ADigetherSpiderNestActor> Nest;

	FVector HomeLocation = FVector::ZeroVector;
	float HomeRadius = 0.f;
	FString TargetKey;
	bool bChaseOutsideArea = false;
	TMap<FString, double> LastSeenTime;
	TMap<FString, double> LastContactDamageTime;
	TMap<FString, uint64> LeapDamageFrame;
	FVector InvestigateLocation = FVector::ZeroVector;
	FLeapRuntime Leap;
	int32 NextLeapId = 1;
	TMap<FString, TPair<double, bool>> ReachabilityCache;
	double InvestigateStartTime = 0.0;
	double ReachabilityCheckTime = -1.0;
	bool bLastReachable = true;
	FTimerHandle EmergeTimer;
	TEnumAsByte<ECollisionResponse> SavedPawnResponse = ECR_Block;
	bool bPawnResponseOverridden = false;
	bool bServerInitialized = false;
};
