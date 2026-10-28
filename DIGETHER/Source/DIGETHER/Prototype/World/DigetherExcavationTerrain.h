#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DigetherExcavationTerrain.generated.h"

class UMaterialInterface;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class APlayerController;

USTRUCT()
struct FDigetherExcavationSnapshot
{
	GENERATED_BODY()

	UPROPERTY() int32 Schema = 1;
	UPROPERTY() uint32 Revision = 0;
	UPROPERTY() TArray<uint8> Density;
};

USTRUCT()
struct FDigetherExcavationSupport
{
	GENERATED_BODY()
	UPROPERTY() bool bInstalled = false;
	UPROPERTY() int32 Strength = 0;
	UPROPERTY() FVector LocalAnchor = FVector::ZeroVector;
};

// Server-owned, bounded full-state replication: reconnecting clients receive the
// current volume without replaying a growing history of excavation RPCs.
UCLASS()
class DIGETHER_API ADigetherExcavationTerrain : public AActor
{
	GENERATED_BODY()
public:
	ADigetherExcavationTerrain();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Caller must validate the player's reach, phase, line of sight and hit on this terrain.
	bool ServerExcavateAt(const FVector& WorldPoint, FString& OutReason);
	// Protected boxes are expanded by at least one cell before any restore.
	bool ServerRestoreAt(const FVector& WorldPoint, float Radius, const TArray<FBox>& ProtectedWorldBoxes, FString& OutReason);
	bool ServerSupportAt(const FVector& WorldPoint, APlayerController* Caller, FString& OutReason);
	bool ResetTerrain();
	uint32 GetTerrainRevision() const { return AppliedRevision; }
	uint32 GetTerrainHash() const;
	bool HasSupport() const { return Support.bInstalled; }
	int32 GetSupportStrength() const { return Support.Strength; }
	bool IsSolidAtWorldPoint(const FVector& WorldPoint) const;
	void SetTerrainMaterial(UMaterialInterface* Material);

private:
	UFUNCTION() void OnRep_Snapshot();
	UFUNCTION() void OnRep_Support();
	void CheckTremor();
	bool FindExcavationNear(const FVector& WorldPoint, float Radius) const;
	bool ApplySnapshot(const FDigetherExcavationSnapshot& Candidate);

	UPROPERTY() TObjectPtr<UProceduralMeshComponent> MeshA;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> MeshB;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> SupportMesh;
	UPROPERTY(ReplicatedUsing=OnRep_Support) FDigetherExcavationSupport Support;
	UPROPERTY(ReplicatedUsing=OnRep_Snapshot) FDigetherExcavationSnapshot Snapshot;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Excavation", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UMaterialInterface> TerrainMaterial;
	TArray<uint8> AppliedDensity;
	uint32 AppliedRevision = 0;
	bool bMeshAActive = false;
	bool bHasActiveMesh = false;
	bool bHasExcavation = false;
	FVector LastExcavationLocal = FVector::ZeroVector;
	float SecondsSinceTremor = 0.f;
	FTimerHandle TremorTimer;
};
