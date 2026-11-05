#include "Game/Elevator/DigetherElevatorActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Game/Elevator/DigetherElevatorComponent.h"
#include "Game/Framework/DigetherFrameworkDefaults.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

ADigetherElevatorActor::ADigetherElevatorActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ElevatorComponent = CreateDefaultSubobject<UDigetherElevatorComponent>(TEXT("ElevatorComponent"));

	BoardingVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("BoardingVolume"));
	BoardingVolume->SetupAttachment(SceneRoot);
	BoardingVolume->SetBoxExtent(DigetherFrameworkDefaults::ElevatorBoardingExtentCm);
	BoardingVolume->SetRelativeLocation(DigetherFrameworkDefaults::ElevatorBoardingOffsetCm);
	BoardingVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoardingVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoardingVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	BoardingVolume->SetGenerateOverlapEvents(true);
	BoardingVolume->SetCanEverAffectNavigation(false);

	FloorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FloorMesh"));
	FloorMesh->SetupAttachment(SceneRoot);
	FloorMesh->SetRelativeScale3D(DigetherFrameworkDefaults::ElevatorFloorMeshScale);
	FloorMesh->SetRelativeLocation(DigetherFrameworkDefaults::ElevatorFloorMeshOffsetCm);
	FloorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FloorMesh->SetGenerateOverlapEvents(false);
	FloorMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		FloorMesh->SetStaticMesh(CubeMesh.Object);
	}
}

UDigetherElevatorComponent* ADigetherElevatorActor::GetElevatorComponent() const
{
	return ElevatorComponent;
}

UBoxComponent* ADigetherElevatorActor::GetBoardingVolume() const
{
	return BoardingVolume;
}

void ADigetherElevatorActor::GetOverlappingPawns(TArray<APawn*>& OutPawns) const
{
	OutPawns.Reset();
	if (!HasAuthority() || !IsValid(BoardingVolume))
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	BoardingVolume->GetOverlappingActors(OverlappingActors, APawn::StaticClass());
	for (AActor* OverlappingActor : OverlappingActors)
	{
		APawn* OverlappingPawn = Cast<APawn>(OverlappingActor);
		if (IsValid(OverlappingPawn))
		{
			OutPawns.Add(OverlappingPawn);
		}
	}
}
