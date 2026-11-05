#pragma once

#include "CoreMinimal.h"
#include "Game/Hub/DigetherHubTypes.h"
#include "GameFramework/Actor.h"
#include "DigetherHubFacilityActor.generated.h"

class ADigetherGameplayPlayerController;
class USceneComponent;

// 외형과 독립된 서버 거리·시야 판정 기준점
UCLASS()
class DIGETHER_API ADigetherHubFacilityActor : public AActor
{
	GENERATED_BODY()

public:
	ADigetherHubFacilityActor();
	bool CanInteract(const ADigetherGameplayPlayerController* Requester) const;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Digether|Hub")
	FName FacilityId;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Digether|Hub")
	EDigetherHubFacilityKind Kind = EDigetherHubFacilityKind::Shop;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Digether|Hub", meta=(ClampMin="1"))
	float InteractionDistanceCm = 250.f;

	// 시설 로컬 좌표 기준 휴식 위치, 외형 교체 시 유지
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Digether|Hub")
	FVector RestLocationOffset = FVector(100.f, 0.f, 80.f);

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;
};
