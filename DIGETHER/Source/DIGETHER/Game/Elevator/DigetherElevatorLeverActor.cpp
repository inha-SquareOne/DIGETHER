#include "Game/Elevator/DigetherElevatorLeverActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Game/Elevator/DigetherElevatorActor.h"
#include "Game/Elevator/DigetherElevatorLeverComponent.h"
#include "Game/Framework/DigetherFrameworkDefaults.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ADigetherElevatorLeverActor::ADigetherElevatorLeverActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	LeverComponent = CreateDefaultSubobject<UDigetherElevatorLeverComponent>(TEXT("LeverComponent"));

	LeverMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeverMesh"));
	LeverMesh->SetupAttachment(SceneRoot);
	LeverMesh->SetRelativeScale3D(DigetherFrameworkDefaults::ElevatorLeverMeshScale);
	LeverMesh->SetRelativeLocation(DigetherFrameworkDefaults::ElevatorLeverMeshOffsetCm);
	LeverMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeverMesh->SetGenerateOverlapEvents(false);
	LeverMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		LeverMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void ADigetherElevatorLeverActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADigetherElevatorLeverActor, LeverKind);
	DOREPLIFETIME(ADigetherElevatorLeverActor, Placement);
	DOREPLIFETIME(ADigetherElevatorLeverActor, ElevatorActor);
}

UDigetherElevatorLeverComponent* ADigetherElevatorLeverActor::GetLeverComponent() const
{
	return LeverComponent;
}

EDigetherElevatorLeverKind ADigetherElevatorLeverActor::GetLeverKind() const
{
	return LeverKind;
}

EDigetherElevatorLeverPlacement ADigetherElevatorLeverActor::GetPlacement() const
{
	return Placement;
}

ADigetherElevatorActor* ADigetherElevatorLeverActor::GetElevatorActor() const
{
	return ElevatorActor;
}
