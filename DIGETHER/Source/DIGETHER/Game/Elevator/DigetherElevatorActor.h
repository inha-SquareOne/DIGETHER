#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DigetherElevatorActor.generated.h"

class APawn;
class UBoxComponent;
class UDigetherElevatorComponent;
class USceneComponent;
class UStaticMeshComponent;

// 운행 규칙과 서버 탑승 판정을 배치할 진입점이며, 형상의 이동은 맡지 않는다.
UCLASS()
class DIGETHER_API ADigetherElevatorActor : public AActor
{
	GENERATED_BODY()

public:
	ADigetherElevatorActor();

	UFUNCTION(BlueprintPure, Category="Digether|Elevator")
	UDigetherElevatorComponent* GetElevatorComponent() const;

	UFUNCTION(BlueprintPure, Category="Digether|Elevator")
	UBoxComponent* GetBoardingVolume() const;

	// 탑승자 목록은 서버 조율자만 읽고 공개 복제 상태에는 포함하지 않는다.
	void GetOverlappingPawns(TArray<APawn*>& OutPawns) const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Elevator", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Elevator", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UDigetherElevatorComponent> ElevatorComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Elevator", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> BoardingVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Elevator", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> FloorMesh;
};
