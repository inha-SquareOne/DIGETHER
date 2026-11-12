#include "DigetherDiggableSoil.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Misc/Crc.h"
#include "DigetherSoilProfiling.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#if !UE_BUILD_SHIPPING
DEFINE_LOG_CATEGORY(LogDigetherSoilPerformance);
static TAutoConsoleVariable<int32> GDigetherSoilChunkCells(
	TEXT("digether.SoilChunkCells"), 0, TEXT("Personal comparison: 0 whole, 8 or 16 chunks; applied on BeginPlay"));
#endif

ADigetherDiggableSoil::ADigetherDiggableSoil()
{
	PrimaryActorTick.bCanEverTick = false;
	// 기존 에셋 참조를 위해 SoilMesh 서브오브젝트 이름 유지
	SoilMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SoilMesh"));
	SetRootComponent(SoilMesh);
	SoilBack = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SoilBack"));
	SoilBack->SetupAttachment(SoilMesh);
	for (UProceduralMeshComponent* Mesh : {SoilMesh.Get(), SoilBack.Get()})
	{
		ConfigureMesh(*Mesh);
	}
}
void ADigetherDiggableSoil::SetMeshEnabled(UProceduralMeshComponent& Mesh, bool bEnabled)
{
	Mesh.SetVisibility(bEnabled);
	Mesh.SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void ADigetherDiggableSoil::ConfigureMesh(UProceduralMeshComponent& Mesh)
{
	Mesh.SetCollisionProfileName(TEXT("BlockAll"));
	SetMeshEnabled(Mesh, false);
	Mesh.bUseComplexAsSimpleCollision = true;
	// PrepareMesh 반환 전에 쿠킹 결과를 검사하는 동기 경로 전제
	Mesh.bUseAsyncCooking = false;
}

bool ADigetherDiggableSoil::PrepareMesh(UProceduralMeshComponent& Mesh,
									 const DigetherSoil::FDigetherSoilSurface& Surface)
{
	SetMeshEnabled(Mesh, false);
	DIGETHER_SOIL_TIMER(RemoveTimer, Remove);
	Mesh.ClearAllMeshSections();
	DIGETHER_SOIL_STOP(RemoveTimer);
	if (Surface.Indices.IsEmpty())
	{
		return true;
	}
	// 공개 PMC 호출은 메시 등록과 동기 충돌 생성을 함께 수행
	DIGETHER_SOIL_TIMER(CollisionTimer, MeshCollision);
	Mesh.CreateMeshSection_LinearColor(0, Surface.Vertices, Surface.Indices, Surface.Normals, Surface.UVs,
									  TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
	DIGETHER_SOIL_STOP(CollisionTimer);
	// 쿠킹 산출물 존재 검사, 실제 통행·노멀 방향 검증은 별도 시험 책임
	const UBodySetup* Body = Mesh.GetBodySetup();
	return IsValid(Body) && Body->bCreatedPhysicsMeshes && !Body->bFailedToCreatePhysicsMeshes &&
		   !Body->TriMeshGeometries.IsEmpty();
}

void ADigetherDiggableSoil::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!HasActorBegunPlay())
	{
		ResetSoil();
	}
}
void ADigetherDiggableSoil::BeginPlay()
{
	Super::BeginPlay();
#if !UE_BUILD_SHIPPING
	int32 ComparisonCells = GDigetherSoilChunkCells.GetValueOnGameThread();
	FParse::Value(FCommandLine::Get(), TEXT("SoilChunkCells="), ComparisonCells);
	if (ComparisonCells != 0 && SetComparisonChunkCells(ComparisonCells))
	{
		return;
	}
#endif
	ResetSoil();
}
uint32 ADigetherDiggableSoil::GetDensityHash() const
{
	return FCrc::MemCrc32(Density.GetData(), Density.Num());
}

bool ADigetherDiggableSoil::ResetSoil()
{
	// 현재 모드의 초기화도 같은 성공 확정 경로 사용
	return SetComparisonChunkCells(AppliedChunkCells);
}

EDigetherSoilEditResult ADigetherDiggableSoil::TryExcavate(FVector WorldPoint, float Radius)
{
#if !UE_BUILD_SHIPPING
	DigetherSoilProfiling::FDigetherSoilRequestTimer RequestTimer;
#endif
	const auto Finish = [&](EDigetherSoilEditResult EditResult)
	{
#if !UE_BUILD_SHIPPING
		RequestTimer.Values.Result = static_cast<int32>(EditResult);
#endif
		return EditResult;
	};
	LastUpdatedChunks = 0;
	LastGeneratedTriangles = 0;
	FVector Local;
	// 로컬 권한 검사만 수행, 클라이언트 요청 전달·밀도 복제는 미구현
	if (!HasAuthority() || !bReady || !DigetherSoil::WorldToLocal(GetActorTransform(), WorldPoint, Local))
	{
		return Finish(EDigetherSoilEditResult::Rejected);
	}
	DIGETHER_SOIL_TIMER(CopyTimer, Copy);
	// 확정 밀도 보존용 사본, 충돌 준비 성공 전 원본 교체 금지
	TArray<uint8> Candidate = Density;
	DIGETHER_SOIL_STOP(CopyTimer);
	// 로컬 표본 변경 범위와 이웃 한 셀, 실제 빈 공간 부피와 구분
	FBox Changed;
	DIGETHER_SOIL_TIMER(CarveTimer, Carve);
	const DigetherSoil::EDigetherSoilCarveResult Result =
		DigetherSoil::Carve(AppliedGrid, Candidate, Local, Radius, ProtectedRegions, Changed);
	DIGETHER_SOIL_STOP(CarveTimer);
	if (Result == DigetherSoil::EDigetherSoilCarveResult::InvalidInput)
	{
		return Finish(EDigetherSoilEditResult::Rejected);
	}
	if (Result == DigetherSoil::EDigetherSoilCarveResult::NoChange)
	{
		return Finish(EDigetherSoilEditResult::NoChange);
	}
	if (!(AppliedChunkCells == 0 ? Apply(AppliedGrid, Candidate)
								 : ApplyChunks(AppliedGrid, Candidate, AppliedChunkCells, false)))
	{
		return Finish(EDigetherSoilEditResult::ApplyFailed);
	}
	DIGETHER_SOIL_TIMER(RevisionTimer, Commit);
	++Revision;
	LastChangedWorldBounds = Changed.TransformBy(GetActorTransform());
	return Finish(EDigetherSoilEditResult::Applied);
}

bool ADigetherDiggableSoil::Apply(const DigetherSoil::FDigetherSoilGrid& Grid, const TArray<uint8>& Candidate)
{
	DigetherSoil::FDigetherSoilSurface Surface;
	if (!DigetherSoil::BuildSurface(Grid, Candidate, Surface))
	{
		return false;
	}
	UProceduralMeshComponent* Next = bFrontActive ? SoilBack.Get() : SoilMesh.Get();
	UProceduralMeshComponent* Previous = bFrontActive ? SoilMesh.Get() : SoilBack.Get();
	if (!PrepareMesh(*Next, Surface))
	{
		Next->ClearAllMeshSections();
		return false;
	}
	const bool bEmpty = Surface.Indices.IsEmpty();
	// 준비 성공 후에만 교체, 빈 지형이면 기존 충돌도 해제
	DIGETHER_SOIL_TIMER(CommitTimer, Commit);
	SetMeshEnabled(*Previous, false);
	SetMeshEnabled(*Next, !bEmpty);
	LastUpdatedChunks = 1;
	LastGeneratedTriangles = Surface.Indices.Num() / 3;
	Density = Candidate;
	AppliedGrid = Grid;
	bFrontActive = Next == SoilMesh.Get();
	bReady = true;
	return true;
}
