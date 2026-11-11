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

#if !UE_BUILD_SHIPPING
	// 개발 B-02: 해금·비용 무시 목적지 선택, 출발 판정이 해금 목록을 요구해 미해금이면 비용 없이 해금도 함께
	// 거점 대기·출발 준비 전(목적지 고정 아님)·정차 위치 있는 목적지만
	EDigetherHubResult DevSelectStage(FName StageId);
	// 개발 B-03: 카탈로그 목적지 전체를 비용 없이 해금, 해금 상한은 기존 규칙 유지
	EDigetherHubResult DevUnlockAllStages(int32& OutNewlyUnlocked);
#endif

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
#if !UE_BUILD_SHIPPING
	// 개발 목적지 변경 공통: 서버·초기화·거점 대기(런 진행·게임 오버 아님, 엘리베이터 HubWaiting)
	bool IsDevStageChangeAllowed() const;
#endif

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
