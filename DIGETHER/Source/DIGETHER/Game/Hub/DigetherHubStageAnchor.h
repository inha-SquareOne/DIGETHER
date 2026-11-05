#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DigetherHubStageAnchor.generated.h"

class USceneComponent;

// 시험 월드의 목적지 도착 위치·회전 기준점, 거래 RPC 제외
UCLASS()
class DIGETHER_API ADigetherHubStageAnchor : public AActor
{
	GENERATED_BODY()

public:
	ADigetherHubStageAnchor();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Digether|Hub")
	FName StageId;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;
};
