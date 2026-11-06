#include "DigetherDainSoilWall.h"
#include "DigetherDainField.h"
#include "Components/SceneComponent.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogDigetherSoil, Log, All);

ADigetherDainSoilWall::ADigetherDainSoilWall()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	MeshA = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SoilA"));
	MeshB = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SoilB"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
	for (UProceduralMeshComponent* Mesh : { MeshA.Get(), MeshB.Get() })
	{
		Mesh->SetupAttachment(RootComponent);
		Mesh->bUseAsyncCooking = false;
		Mesh->bUseComplexAsSimpleCollision = true;
		Mesh->SetCollisionProfileName(TEXT("BlockAll"));
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetHiddenInGame(true);
		if (Material.Succeeded()) { Mesh->SetMaterial(0, Material.Object); }
	}
}

void ADigetherDainSoilWall::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Apply(DigetherDainField::InitialDensity());
}

void ADigetherDainSoilWall::BeginPlay()
{
	Super::BeginPlay();
	for (UProceduralMeshComponent* Mesh : { MeshA.Get(), MeshB.Get() })
	{
		if (UMaterialInstanceDynamic* Material = Mesh->CreateDynamicMaterialInstance(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(.32f, .13f, .045f));
		}
	}
	if (!Apply(DigetherDainField::InitialDensity()))
	{
		UE_LOG(LogDigetherSoil, Error, TEXT("Initial wall collision failed"));
	}
}

// Adapted from prototype ApplySnapshot: prepare/cook the inactive mesh first.
// Failed collision preparation leaves the previous visible/collidable state intact.
bool ADigetherDainSoilWall::Apply(const TArray<uint8>& Candidate)
{
	DigetherDainField::FSurface Surface;
	if (!DigetherDainField::BuildSurface(Candidate, Surface)) { return false; }
	UProceduralMeshComponent* Next = bMeshAActive ? MeshB.Get() : MeshA.Get();
	UProceduralMeshComponent* Previous = bMeshAActive ? MeshA.Get() : MeshB.Get();
	Next->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Next->SetHiddenInGame(true);
	Next->CreateMeshSection_LinearColor(0, Surface.Vertices, Surface.Indices, Surface.Normals,
		Surface.UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
	UBodySetup* Body = Next->GetBodySetup();
	if (!Body || !Body->bCreatedPhysicsMeshes || Body->bFailedToCreatePhysicsMeshes || Body->TriMeshGeometries.IsEmpty())
	{
		Next->ClearAllMeshSections();
		return false;
	}
	Previous->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Previous->SetHiddenInGame(true);
	Next->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Next->SetHiddenInGame(false);
	bMeshAActive = Next == MeshA.Get();
	bReady = true;
	Density = Candidate;
	return true;
}

bool ADigetherDainSoilWall::TryExcavate(const FVector& WorldHit, float BrushRadius)
{
	if (!HasAuthority() || !bReady || WorldHit.ContainsNaN()) { return false; }
	TArray<uint8> Candidate = Density;
	if (!DigetherDainField::Carve(Candidate, GetActorTransform().InverseTransformPosition(WorldHit), BrushRadius)) { return false; }
	if (!Apply(Candidate)) { return false; }
	++Revision;
	UE_LOG(LogDigetherSoil, Display, TEXT("Dig revision=%d hash=%u"), Revision, GetDensityHash());
	return true;
}

uint32 ADigetherDainSoilWall::GetDensityHash() const { return DigetherDainField::Hash(Density); }
