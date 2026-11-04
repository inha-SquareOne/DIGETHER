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

	UPROPERTY(EditInstanceOnly, Replicated, BlueprintReadOnly, Category="Digether|Elevator", meta=(AllowPrivateAccess="true"))
	TObjectPtr<ADigetherElevatorActor> ElevatorActor;
};
