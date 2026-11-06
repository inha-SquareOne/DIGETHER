#include "DigetherDiggableSoil.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Misc/Crc.h"

ADigetherDiggableSoil::ADigetherDiggableSoil()
{
	PrimaryActorTick.bCanEverTick = false;
	// Keep the existing SoilMesh subobject name for the personal Blueprint.
	SoilMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SoilMesh"));
	SetRootComponent(SoilMesh);
	SoilBack = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SoilBack"));
	SoilBack->SetupAttachment(SoilMesh);
	for (UProceduralMeshComponent* Mesh : {SoilMesh.Get(), SoilBack.Get()})
	{
		Mesh->SetCollisionProfileName(TEXT("BlockAll"));
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetVisibility(false);
		Mesh->bUseComplexAsSimpleCollision = true;
		Mesh->bUseAsyncCooking = false;
	}
}
void ADigetherDiggableSoil::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!HasActorBegunPlay()) { ResetSoil(); }
}
void ADigetherDiggableSoil::BeginPlay()
{
	Super::BeginPlay();
	ResetSoil();
}
uint32 ADigetherDiggableSoil::GetDensityHash() const { return FCrc::MemCrc32(Density.GetData(), Density.Num()); }

bool ADigetherDiggableSoil::ResetSoil()
{
	FVector Local;
	if (!HasAuthority() || !DigetherSoil::WorldToLocal(GetActorTransform(), GetActorLocation(), Local)) { return false; }
	DigetherSoil::FGrid Grid; Grid.Cells = GridCells; Grid.Spacing = CellSpacing;
	TArray<uint8> Initial;
	if (!DigetherSoil::Initialize(Grid, Initial) || !Apply(Grid, Initial)) { return false; }
	Revision = 0;
	LastChangedWorldBounds = FBox(ForceInit);
	return true;
}

bool ADigetherDiggableSoil::Apply(const DigetherSoil::FGrid& Grid, const TArray<uint8>& Candidate)
{
	DigetherSoil::FSurface Surface;
	if (!DigetherSoil::BuildSurface(Grid, Candidate, Surface)) { return false; }
	UProceduralMeshComponent* Next = bFrontActive ? SoilBack.Get() : SoilMesh.Get();
	UProceduralMeshComponent* Previous = bFrontActive ? SoilMesh.Get() : SoilBack.Get();
	Next->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Next->SetVisibility(false);
	Next->ClearAllMeshSections();
	const bool bEmpty = Surface.Indices.IsEmpty();
	if (!bEmpty)
	{
		Next->CreateMeshSection_LinearColor(0, Surface.Vertices, Surface.Indices, Surface.Normals,
			Surface.UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
		UBodySetup* Body = Next->GetBodySetup();
		if (!Body || !Body->bCreatedPhysicsMeshes || Body->bFailedToCreatePhysicsMeshes || Body->TriMeshGeometries.IsEmpty())
		{
			Next->ClearAllMeshSections();
			return false;
		}
	}
	// Commit only after preparation; empty terrain deliberately disables all old collision.
	Previous->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Previous->SetVisibility(false);
	Next->SetVisibility(!bEmpty);
	Next->SetCollisionEnabled(bEmpty ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	Density = Candidate;
	AppliedGrid = Grid;
	bFrontActive = Next == SoilMesh.Get();
	bReady = true;
	return true;
}
EDigetherSoilEditResult ADigetherDiggableSoil::TryExcavate(FVector WorldPoint, float Radius)
{
	FVector Local;
	if (!HasAuthority() || !bReady || !DigetherSoil::WorldToLocal(GetActorTransform(), WorldPoint, Local)) { return EDigetherSoilEditResult::Rejected; }
	TArray<uint8> Candidate = Density;
	FBox Changed;
	const DigetherSoil::ECarveResult Result = DigetherSoil::Carve(AppliedGrid, Candidate, Local, Radius, ProtectedRegions, Changed);
	if (Result == DigetherSoil::ECarveResult::InvalidInput) { return EDigetherSoilEditResult::Rejected; }
	if (Result == DigetherSoil::ECarveResult::NoChange) { return EDigetherSoilEditResult::NoChange; }
	if (!Apply(AppliedGrid, Candidate)) { return EDigetherSoilEditResult::ApplyFailed; }
	++Revision;
	LastChangedWorldBounds = Changed.TransformBy(GetActorTransform());
	return EDigetherSoilEditResult::Applied;
}

