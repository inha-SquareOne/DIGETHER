#pragma once

#include "CoreMinimal.h"
#include "Game/Elevator/DigetherElevatorTypes.h"
#include "GameFramework/Actor.h"
#include "DigetherElevatorLeverActor.generated.h"

class ADigetherElevatorActor;
class UDigetherElevatorLeverComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class DIGETHER_API ADigetherElevatorLeverActor : public AActor
{
	GENERATED_BODY()

public:
	ADigetherElevatorLeverActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Digether|Elevator")
	UDigetherElevatorLeverComponent* GetLeverComponent() const;

	UFUNCTION(BlueprintPure, Category="Digether|Elevator")
	EDigetherElevatorLeverKind GetLeverKind() const;

	// 갱도 상승 레버의 안·밖 위치다. 출발 레버는 기본값 Inside를 그대로 둔다.
	UFUNCTION(BlueprintPure, Category="Digether|Elevator")
	EDigetherElevatorLeverPlacement GetPlacement() const;

	UFUNCTION(BlueprintPure, Category="Digether|Elevator")
	ADigetherElevatorActor* GetElevatorActor() const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Elevator", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Elevator", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UDigetherElevatorLeverComponent> LeverComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Elevator", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> LeverMesh;

	UPROPERTY(EditInstanceOnly, Replicated, BlueprintReadOnly, Category="Digether|Elevator", meta=(AllowPrivateAccess="true"))
	EDigetherElevatorLeverKind LeverKind = EDigetherElevatorLeverKind::Ascent;

	// 맵 배치 인스턴스마다 정한다. 안쪽은 탑승 영역 안, 밖은 탑승 영역 밖에 둔다(시작 시 경고로만 검사).
	UPROPERTY(EditInstanceOnly, Replicated, BlueprintReadOnly, Category="Digether|Elevator", meta=(AllowPrivateAccess="true"))
	EDigetherElevatorLeverPlacement Placement = EDigetherElevatorLeverPlacement::Inside;

	UPROPERTY(EditInstanceOnly, Replicated, BlueprintReadOnly, Category="Digether|Elevator", meta=(AllowPrivateAccess="true"))
	TObjectPtr<ADigetherElevatorActor> ElevatorActor;
};
