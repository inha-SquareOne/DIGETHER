#pragma once

#include "CoreMinimal.h"
#include "Game/Hub/DigetherHubTypes.h"
#include "Game/Hub/DigetherHubRules.h"
#include "GameFramework/Actor.h"
#include "DigetherHubStateActor.generated.h"

class ADigetherGameplayPlayerController;
class ADigetherGameplayPlayerState;
class ADigetherHubFacilityActor;
class UDigetherHubCatalog;
class ALight;
class APawn;

// 거점 거래의 서버 처리와 팀 공용 상태 복제
UCLASS()
class DIGETHER_API ADigetherHubStateActor : public AActor
{
	GENERATED_BODY()

public:
	ADigetherHubStateActor();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	static ADigetherHubStateActor* Find(const UWorld* World);

	FDigetherHubView Execute(ADigetherGameplayPlayerController* Requester, ADigetherHubFacilityActor* Facility,
		EDigetherHubAction Action, FName OptionId, FGuid RequestId);
	FDigetherHubView BuildView(const ADigetherGameplayPlayerController* Requester,
		const ADigetherHubFacilityActor* Facility) const;
	FDigetherHubProgress CaptureProgress() const;
	bool ValidateProgress(const FDigetherHubProgress& Candidate) const;
	bool RestoreProgress(const FDigetherHubProgress& Candidate);
	void LockDestination(bool bLocked);
	FName GetSelectedStageId() const { return SelectedStageId; }
	bool GetSelectedStageTransform(FTransform& OutTransform) const;
	bool MoveElevatorToSelectedStage();
	void ReturnElevatorToHub();
	void EndRest(ADigetherGameplayPlayerController* Requester);
	void EndAllRest();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Digether|Hub")
	TObjectPtr<UDigetherHubCatalog> Catalog;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Digether|Hub")
	TArray<FName> UnlockedStageIds;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Digether|Hub")
	FName SelectedStageId;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Digether|Hub")
	bool bDestinationLocked = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Digether|Hub")
	int32 TotalOfferingCredits = 0;

	UPROPERTY(ReplicatedUsing=OnRep_OfferingPulseCounter, BlueprintReadOnly, Category="Digether|Hub")
	int32 OfferingPulseCounter = 0;

	UPROPERTY(ReplicatedUsing=OnRep_OfferingPulseCounter, BlueprintReadOnly, Category="Digether|Hub")
	double OfferingPulseEndsAtServerTime = 0.0;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Digether|Hub")
	TObjectPtr<ALight> OfferingLight;

private:
	bool IsRequesterAllowed(const ADigetherGameplayPlayerController* Requester,
		const ADigetherHubFacilityActor* Facility) const;
	void InitializeCatalogDefaults();
	void CleanupRestingPlayers();
	bool StartRest(ADigetherGameplayPlayerController* Requester, ADigetherHubFacilityActor* Facility);
	bool IsResting(const ADigetherGameplayPlayerController* Requester) const;
	void CacheElevatorTransforms();
	UFUNCTION()
	void OnRep_OfferingPulseCounter();
	void RestoreOfferingLight();

	// 휴식 종료 시 기존 Pawn과 원래 위치 복원
	struct FRestRecord
	{
		TWeakObjectPtr<ADigetherGameplayPlayerController> Controller;
		TWeakObjectPtr<ADigetherHubFacilityActor> Bed;
		TWeakObjectPtr<APawn> Pawn;
		FTransform ReturnTransform;
	};
	TArray<FRestRecord> RestingPlayers;
	TArray<FDigetherHubRequestKey> AppliedRequests;
	// 개인 소지품·배송 상태의 서버 전용 원본
	TArray<FDigetherHubPlayerProgress> PrivatePlayers;
	TArray<TPair<TWeakObjectPtr<AActor>, FTransform>> ElevatorTransforms;
	FTimerHandle RestCleanupTimer;
	FTimerHandle OfferingPulseTimer;
	float OfferingLightBaseIntensity = 0.f;
	bool bOfferingLightIntensityCached = false;
	bool bProgressInitialized = false;
};
