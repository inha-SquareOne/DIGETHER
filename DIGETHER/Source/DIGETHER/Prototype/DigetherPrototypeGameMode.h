#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DigetherPrototypeTypes.h"
#include "DigetherPrototypeGameMode.generated.h"

class ADigetherPrototypePlayerState;
class ADigetherPrototypeGameState;
class ADigetherPrototypeElevatorCab;
class ADigetherPrototypePickup;
class ADigetherPrototypeCorpse;

UCLASS()
class DIGETHER_API ADigetherPrototypeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADigetherPrototypeGameMode();
	virtual void Tick(float DeltaSeconds) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
		const FString& Options, const FString& Portal = TEXT("")) override;

	bool CanInteract(APlayerController* Caller, AActor* Target, FString& OutMessage) const;
	bool RequestStartRun(APlayerController* Caller, FString& OutMessage);
	bool RequestStartRunAt(APlayerController* Caller, AActor* Station, FString& OutMessage);
	bool RequestElevator(APlayerController* Caller, AActor* Elevator, FString& OutMessage);
	bool RequestEmergencyExit(APlayerController* Caller, FString& OutMessage);
	bool RequestPurchase(APlayerController* Caller, FString& OutMessage);
	bool RequestPurchaseEmergency(APlayerController* Caller, FString& OutMessage);
	bool RequestBeginEmergencyExit(APlayerController* Caller, AActor* Station, FString& OutMessage);
	bool RequestBeginDeposit(APlayerController* Caller, AActor* Station, FString& OutMessage);
	bool RequestUseWood(APlayerController* Caller, FString& OutMessage);
	bool RequestRestartGame(APlayerController* Caller, FString& OutMessage);
	bool RequestDrop(APlayerController* Caller, int32 ItemIndex, FString& OutMessage);
	bool FindSafeDropLocation(APawn* Pawn, const AActor* IgnoredActor, float Radius, FVector& OutLocation) const;
	bool RequestSelfEscape(APlayerController* Caller, FString& OutMessage);
	bool RequestSave(APlayerController* Caller, FString& OutMessage);
	bool RequestLoad(APlayerController* Caller, FString& OutMessage);
	bool RequestPickup(APlayerController* Caller, AActor* Pickup, int32 Value, float WeightKg,
		bool bHiddenSlot, bool bTwoHanded, FString& OutMessage);
	bool RequestPickupItem(APlayerController* Caller, AActor* Pickup, const FPrototypeItem& Item,
		bool bHiddenSlot, FString& OutMessage);
	bool RequestDeposit(APlayerController* Caller, int32 Amount, FString& OutMessage);
	// Server-only generation changes on a new quota period, restart, or load.
	uint64 GetPeriodGeneration() const { return PeriodGeneration; }
	bool RequestDamage(APlayerController* Caller, APlayerController* Target, float Amount, FString& OutMessage);
	bool RequestWorldDamagePawn(APawn* Target, float Amount);
	bool IsHazardTarget(APawn* Pawn) const;
	bool RequestWorldBurialPawn(APawn* Pawn, int32 Level);
	bool RequestRescue(APlayerController* Caller, APlayerController* Target, FString& OutMessage);
	bool RequestBurial(APlayerController* Target, int32 Level, FString& OutMessage);
	void CancelInteraction(APlayerController* Caller);
	bool HandleControllerDisconnect(APlayerController* Caller);

	// 테스트 픽스처이며 최종 기획 값이 아니다.
	UPROPERTY(EditDefaultsOnly, Category="Prototype|Fixture", meta=(ClampMin="1"))
	int32 InitialQuota = 100;

	UPROPERTY(EditDefaultsOnly, Category="Prototype|Fixture", meta=(ClampMin="1"))
	float RunSeconds = 180.f;

	UPROPERTY(EditDefaultsOnly, Category="Prototype|Fixture", meta=(ClampMin="1"))
	float InteractionDistance = 350.f;

	UPROPERTY(EditDefaultsOnly, Category="Prototype|Fixture", meta=(ClampMin="1"))
	float ElevatorCapacityKg = 40.f;

	UPROPERTY(EditDefaultsOnly, Category="Prototype|Fixture", meta=(ClampMin="1", ClampMax="2"))
	int32 FixtureElevatorTrips = 2;

	UPROPERTY(EditDefaultsOnly, Category="Prototype|Fixture", meta=(ClampMin="0"))
	float FixtureAscentPenaltySeconds = 10.f;

	UPROPERTY(EditDefaultsOnly, Category="Prototype|Fixture", meta=(ClampMin="0.1"))
	float FixtureElevatorTravelSeconds = 4.f;

	UPROPERTY(EditDefaultsOnly, Category="Prototype|Fixture", meta=(ClampMin="0.1"))
	float FixtureDepositInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category="Prototype|Fixture", meta=(ClampMin="0"))
	float FixtureEmergencyWeightKg = 5.f;

private:
	ADigetherPrototypeGameState* PrototypeState() const;
	ADigetherPrototypePlayerState* PrototypePlayer(APlayerController* Controller) const;
	float GetBoardingWeight(AActor* Elevator) const;
	bool CanUseEmergency(APlayerController* Caller, FString& OutMessage) const;
	void UpdateElevator(float DeltaSeconds);
	void CompleteElevatorAscent();
	void CancelElevatorTransit();
	void FinishRun();
	bool TryReturnPawn(APawn* Pawn, int32 Index) const;
	bool TryMoveBuriedPawnToSafety(APawn* Pawn) const;
	void CompleteReturn();
	void KillPlayer(ADigetherPrototypePlayerState* Player);
	void KillDisconnectedPlayer(const FString& FixtureId);
	void UpdatePlayer(ADigetherPrototypePlayerState* Player);
	bool SaveFixture(FString& OutMessage);
	FString FixtureSlotName() const;
	bool bRunFinalized = true;
	bool bQuotaFailedAtDeparture = false;
	int32 DepartureCount = 0;
	int32 CollectedValue = 0;
	float TimeAccumulator = 0.f;
	TWeakObjectPtr<APlayerController> HoldingPlayer;
	TWeakObjectPtr<AActor> HoldingElevator;
	TWeakObjectPtr<AActor> HoldingStation;
	FVector HoldStartLocation = FVector::ZeroVector;
	float HoldElapsed = 0.f;
	float HoldDuration = 0.f;
	FGuid HoldRunId;
	bool bCompletingHold = false;
	bool bHoldElevator = false;
	bool bHoldEmergency = false;
	bool bTimeoutPending = false;
	bool bEmptyReturn = false;
	bool bTransitRollbackPending = false;
	float ElevatorElapsed = 0.f;
	float TransitElapsedSeconds = 0.f;
	float TransitRemainingBeforePenalty = 0.f;
	bool bTransitPenaltyApplied = false;
	TWeakObjectPtr<ADigetherPrototypeElevatorCab> TransitCab;
	TArray<FString> TransitPlayers;
	TMap<FString, EPrototypeLife> TransitPreviousLives;
	TArray<TWeakObjectPtr<ADigetherPrototypePickup>> TransitPickups;
	TArray<TWeakObjectPtr<ADigetherPrototypePickup>> ReturnedThisRun;
	TArray<FPrototypeItem> TransitPickupItems;
	TArray<FTransform> TransitPickupTransforms;
	TArray<TWeakObjectPtr<ADigetherPrototypeCorpse>> TransitCorpses;
	TArray<FString> TransitCorpseIds;
	TArray<FTransform> TransitCorpseTransforms;
	TMap<TWeakObjectPtr<AActor>, FTransform> TransitOriginalTransforms;
	TMap<TWeakObjectPtr<APawn>, FTransform> NonBoardingPositions;
	TWeakObjectPtr<APlayerController> DepositHolder;
	TWeakObjectPtr<AActor> DepositStation;
	float DepositElapsed = 0.f;
	uint64 PeriodGeneration = 0;
	uint64 DepositPeriodGeneration = 0;
	// 새 시험 게임은 이전 게임 슬롯을 덮지 않는 별도 슬롯을 사용한다.
	FString ActiveGameSaveSlot;
	struct FRescueJob
	{
		TWeakObjectPtr<APlayerController> Rescuer;
		TWeakObjectPtr<APlayerController> Target;
		float Elapsed = 0.f;
	};
	TMap<TWeakObjectPtr<ADigetherPrototypePlayerState>, FRescueJob> RescueJobs;
	struct FDisconnectedPlayer
	{
		FPrototypePrivateState State;
		TWeakObjectPtr<APawn> Pawn;
		int32 Hidden = 0;
		int32 Death = 0;
		bool bBodyRecovered = false;
	};
	TMap<FString, FDisconnectedPlayer> DisconnectedPlayers;
	TMap<FString, TWeakObjectPtr<APlayerController>> ConnectedPlayers;
	TSet<FString> RunParticipants;
	TMap<TWeakObjectPtr<APawn>, int32> PendingReturns;
	TMap<TWeakObjectPtr<ADigetherPrototypePlayerState>, int32> HiddenPayout;
	TMap<TWeakObjectPtr<ADigetherPrototypePlayerState>, int32> DeathLoss;
	TSet<TWeakObjectPtr<ADigetherPrototypePlayerState>> RecoveredBodies;
};
