#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DigetherRockStrikeEffect.generated.h"

class UInstancedStaticMeshComponent;

// 개인 시험용 한 번의 타격 연출. 로컬 +X 축이 맞은 면의 바깥쪽이다.
UCLASS(NotPlaceable, Transient)
class ADigetherRockStrikeEffect : public AActor
{
	GENERATED_BODY()

public:
	ADigetherRockStrikeEffect();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Sparks;
	TArray<FVector> Velocities;
};
