#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DigetherSoilField.h"
#include "DigetherDiggableSoil.generated.h"
class UProceduralMeshComponent;

UENUM(BlueprintType)
enum class EDigetherSoilEditResult : uint8 { Rejected, NoChange, Applied, ApplyFailed };

// Terrain only. The server-side tool caller must validate equipment, reach and cooldown.
UCLASS()
class DIGETHER_API ADigetherDiggableSoil : public AActor
{
	GENERATED_BODY()
public:
	ADigetherDiggableSoil();
	virtual void OnConstruction(const FTransform& Transform) override;
	UFUNCTION(BlueprintCallable, Category="Excavation")
	EDigetherSoilEditResult TryExcavate(FVector WorldPoint, float Radius);
	UFUNCTION(BlueprintCallable, Category="Excavation")
	bool ResetSoil();
	uint32 GetRevision() const { return Revision; }
	uint32 GetDensityHash() const;
	FBox GetLastChangedWorldBounds() const { return LastChangedWorldBounds; }

	UPROPERTY(EditAnywhere, Category="Excavation", meta=(ClampMin="4", ClampMax="64"))
	FIntVector GridCells = FIntVector(8,8,8);
	UPROPERTY(EditAnywhere, Category="Excavation", meta=(ClampMin="2", ClampMax="200", Units="cm"))
	double CellSpacing = 50.0;
	// Local-space protected regions, expanded by one cell by the calculation core.
	UPROPERTY(EditAnywhere, Category="Excavation")
	TArray<FBox> ProtectedRegions;
protected:
	virtual void BeginPlay() override;
private:
	friend class FDigetherSoilActorTestAccess;
	bool Apply(const DigetherSoil::FGrid& Grid, const TArray<uint8>& Candidate);
	UPROPERTY(VisibleAnywhere, Category="Excavation")
	TObjectPtr<UProceduralMeshComponent> SoilMesh;
	UPROPERTY(VisibleAnywhere, Category="Excavation")
	TObjectPtr<UProceduralMeshComponent> SoilBack;
	DigetherSoil::FGrid AppliedGrid;
	TArray<uint8> Density;
	FBox LastChangedWorldBounds = FBox(ForceInit);
	uint32 Revision = 0;
	bool bFrontActive = false;
	bool bReady = false;
};

