#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DigetherLabTool.h"
#include "DigetherRubbleRock.generated.h"

class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EDigetherRockKind : uint8
{
	Rubble,
	Ore,
	Hard
};

UENUM(BlueprintType)
enum class EDigetherRubbleStage : uint8
{
	Intact,
	Chipped,
	Fractured,
	Destroyed
};

// 개인 갱도 시험용 바위. 단계별 타수와 외형·충돌 갱신은 세 종류가 공유한다.
UCLASS()
class ADigetherRubbleRock : public AActor
{
	GENERATED_BODY()

public:
	ADigetherRubbleRock();
	bool ApplyToolHit(EDigetherLabTool Tool, const FHitResult& Hit);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rock")
	EDigetherRockKind RockKind = EDigetherRockKind::Rubble;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rock Rubble|Test", meta = (ClampMin = "1"))
	int32 HitsToChip = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rock Rubble|Test", meta = (ClampMin = "1"))
	int32 HitsToFracture = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rock Rubble|Test", meta = (ClampMin = "1"))
	int32 HitsToDestroy = 1;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Rock Rubble")
	EDigetherRubbleStage Stage = EDigetherRubbleStage::Intact;

private:
	UPROPERTY(VisibleAnywhere, Category = "Rock Rubble")
	TObjectPtr<UStaticMeshComponent> RockMesh;
	int32 HitsInStage = 0;
};
