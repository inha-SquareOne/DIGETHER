#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DigetherDainSoilWall.generated.h"
class UProceduralMeshComponent;

// Excavation core: no input, inventory, session or player-class dependency.
UCLASS()
class ADigetherDainSoilWall : public AActor
{
	GENERATED_BODY()

public:
	ADigetherDainSoilWall();
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	// The calling gameplay authority must validate the tool hit/range/cooldown.
	bool TryExcavate(const FVector& WorldHit, float BrushRadius);
	uint32 GetDensityHash() const;
	int32 GetRevision() const { return Revision; }

private:
	bool Apply(const TArray<uint8>& Candidate);
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> MeshA;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> MeshB;
	TArray<uint8> Density;
	bool bMeshAActive = false;
	bool bReady = false;
	int32 Revision = 0;
};
