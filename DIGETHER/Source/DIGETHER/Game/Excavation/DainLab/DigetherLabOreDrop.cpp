#include "DigetherLabOreDrop.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ADigetherLabOreDrop::ADigetherLabOreDrop()
{
	USphereComponent* Body = CreateDefaultSubobject<USphereComponent>(TEXT("OreBody"));
	SetRootComponent(Body);
	Body->InitSphereRadius(9.f);
	Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Body->SetSimulatePhysics(true);
	Body->BodyInstance.bUseCCD = true;

	UStaticMeshComponent* Ore = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ore"));
	Ore->SetupAttachment(Body);
	Ore->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ore->SetRelativeScale3D(FVector(.26, .30, .27));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Game/Developers/Dain/Excavation/Meshes/SM_CollapseClod.SM_CollapseClod"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Metal(TEXT("/Game/Prototype/Materials/MI_PrototypeMetal.MI_PrototypeMetal"));
	Ore->SetStaticMesh(Mesh.Object);
	Ore->SetMaterial(0, Metal.Object);
}
