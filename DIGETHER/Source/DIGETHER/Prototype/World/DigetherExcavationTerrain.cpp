#include "DigetherExcavationTerrain.h"
#include "DigetherExcavationField.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Prototype/DigetherPrototypeGameMode.h"
#include "Prototype/DigetherPrototypeGameState.h"
#include "Prototype/DigetherPrototypePlayerState.h"
#include "Prototype/DigetherPrototypeCorpse.h"
#include "Prototype/World/DigetherPrototypeWorld.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogDigetherExcavation, Log, All);

ADigetherExcavationTerrain::ADigetherExcavationTerrain()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(10.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	MeshA = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainA"));
	MeshB = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainB"));
	for (UProceduralMeshComponent* Mesh : { MeshA.Get(), MeshB.Get() })
	{
		Mesh->SetupAttachment(RootComponent);
		Mesh->bUseAsyncCooking = false;
		Mesh->bUseComplexAsSimpleCollision = true;
		Mesh->SetCollisionProfileName(TEXT("BlockAll"));
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetHiddenInGame(true);
	}
	SupportMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Support"));
	SupportMesh->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) { SupportMesh->SetStaticMesh(Cube.Object); }
	SupportMesh->SetRelativeScale3D(FVector(.18f, .18f, 1.8f));
	SupportMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SupportMesh->SetHiddenInGame(true);
	Snapshot.Density = DigetherExcavation::InitialDensity();
}

void ADigetherExcavationTerrain::BeginPlay()
{
	Super::BeginPlay();
	if (!ApplySnapshot(Snapshot)) { UE_LOG(LogDigetherExcavation, Error, TEXT("Initial terrain mesh or collision failed")); }
	OnRep_Support();
	if (HasAuthority()) { GetWorldTimerManager().SetTimer(TremorTimer, this, &ADigetherExcavationTerrain::CheckTremor, 1.f, true); }
}

void ADigetherExcavationTerrain::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(TremorTimer);
	Super::EndPlay(Reason);
}

void ADigetherExcavationTerrain::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADigetherExcavationTerrain, Snapshot);
	DOREPLIFETIME(ADigetherExcavationTerrain, Support);
}

bool ADigetherExcavationTerrain::ApplySnapshot(const FDigetherExcavationSnapshot& Candidate)
{
	if (Candidate.Schema != 1 || Candidate.Density.Num() != DigetherExcavation::SampleCount) { return false; }
	DigetherExcavation::FSurface Surface;
	if (!DigetherExcavation::BuildSurface(Candidate.Density, Surface)) { return false; }
	UProceduralMeshComponent* Next = !bHasActiveMesh || !bMeshAActive ? MeshA.Get() : MeshB.Get();
	UProceduralMeshComponent* Previous = bHasActiveMesh ? (bMeshAActive ? MeshA.Get() : MeshB.Get()) : nullptr;
	Next->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Next->SetHiddenInGame(true);
	Next->CreateMeshSection_LinearColor(0, Surface.Vertices, Surface.Indices, Surface.Normals, Surface.UVs,
		TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
	UBodySetup* Body = Next->GetBodySetup();
	// A synchronous cook must produce triangle meshes; an empty result is a cook failure.
	if (!Body || !Body->bCreatedPhysicsMeshes || Body->bFailedToCreatePhysicsMeshes || Body->TriMeshGeometries.IsEmpty())
	{
		Next->ClearAllMeshSections();
		return false;
	}
	if (TerrainMaterial) { Next->SetMaterial(0, TerrainMaterial); }
	if (Previous)
	{
		Previous->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Previous->SetHiddenInGame(true);
	}
	Next->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Next->SetHiddenInGame(false);
	bMeshAActive = Next == MeshA.Get();
	bHasActiveMesh = true;
	AppliedDensity = Candidate.Density;
	AppliedRevision = Candidate.Revision;
	return true;
}

void ADigetherExcavationTerrain::OnRep_Snapshot()
{
	if (!ApplySnapshot(Snapshot)) { UE_LOG(LogDigetherExcavation, Error, TEXT("Rejected terrain snapshot or collision cook failed; retaining previous terrain")); }
}

bool ADigetherExcavationTerrain::ServerExcavateAt(const FVector& WorldPoint, FString& OutReason)
{
	if (!HasAuthority() || !bHasActiveMesh || WorldPoint.ContainsNaN()) { OutReason = TEXT("지형을 변경할 수 없습니다."); return false; }
	FDigetherExcavationSnapshot Candidate = Snapshot;
	if (!DigetherExcavation::Carve(Candidate.Density, GetActorTransform().InverseTransformPosition(WorldPoint)))
	{
		OutReason = TEXT("굴착 가능한 흙이 없습니다."); return false;
	}
	++Candidate.Revision;
	if (!ApplySnapshot(Candidate)) { OutReason = TEXT("지형 충돌 생성에 실패했습니다."); return false; }
	Snapshot = MoveTemp(Candidate);
	bHasExcavation = true;
	LastExcavationLocal = GetActorTransform().InverseTransformPosition(WorldPoint);
	ForceNetUpdate();
	OutReason = TEXT("굴착했습니다.");
	return true;
}


bool ADigetherExcavationTerrain::ServerRestoreAt(const FVector& WorldPoint, float Radius,
	const TArray<FBox>& ProtectedWorldBoxes, FString& OutReason)
{
	if (!HasAuthority() || !bHasActiveMesh) { OutReason = TEXT("지형을 복원할 수 없습니다."); return false; }
	FDigetherExcavationSnapshot Candidate = Snapshot;
	if (!DigetherExcavation::Restore(Candidate.Density, GetActorTransform(), WorldPoint, Radius, ProtectedWorldBoxes))
	{
		OutReason = TEXT("복원할 수 있는 흙이 없거나 입력이 유효하지 않습니다."); return false;
	}
	++Candidate.Revision;
	if (!ApplySnapshot(Candidate)) { OutReason = TEXT("지형 충돌 생성에 실패했습니다."); return false; }
	Snapshot = MoveTemp(Candidate);
	ForceNetUpdate();
	OutReason = TEXT("굴착된 흙이 복원됐습니다.");
	return true;
}


void ADigetherExcavationTerrain::OnRep_Support()
{
	SupportMesh->SetHiddenInGame(!Support.bInstalled);
	if (Support.bInstalled)
	{
		const FVector WorldAnchor = GetActorTransform().TransformPosition(Support.LocalAnchor);
		SupportMesh->SetWorldLocation(WorldAnchor + FVector(0.f, 0.f, 70.f));
		SupportMesh->SetWorldRotation(FRotator::ZeroRotator);
	}
}

bool ADigetherExcavationTerrain::FindExcavationNear(const FVector& WorldPoint, float Radius) const
{
	using namespace DigetherExcavation;
	if (AppliedDensity.Num() != SampleCount) { return false; }
	const TArray<uint8> Original = InitialDensity();
	for (int32 Z = 1; Z < SamplesZ; ++Z)
		for (int32 Y = 1; Y < CellsXY; ++Y)
			for (int32 X = 1; X < CellsXY; ++X)
			{
				const int32 I = Index(X, Y, Z);
				if (Original[I] > 128 && AppliedDensity[I] <= 128 &&
					FVector::DistSquared(GetActorTransform().TransformPosition(Position(X, Y, Z)), WorldPoint) <= FMath::Square(Radius)) { return true; }
			}
	return false;
}

bool ADigetherExcavationTerrain::ServerSupportAt(const FVector& WorldPoint, APlayerController* Caller, FString& OutReason)
{
	ADigetherPrototypeGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ADigetherPrototypeGameMode>() : nullptr;
	const ADigetherPrototypeGameState* State = GetWorld() ? GetWorld()->GetGameState<ADigetherPrototypeGameState>() : nullptr;
	const ADigetherPrototypePlayerState* Player = Caller ? Caller->GetPlayerState<ADigetherPrototypePlayerState>() : nullptr;
	if (!HasAuthority() || !Mode || !State || State->PublicState.Phase != EPrototypePhase::Expedition ||
		!Player || Player->PrivateState.Life != EPrototypeLife::Active || !Caller->GetPawn() || WorldPoint.ContainsNaN())
	{
		OutReason = TEXT("탐사 중인 플레이어만 보강할 수 있습니다."); return false;
	}
	for (const FPrototypeItem& Item : Player->PrivateState.Items)
		if (Item.bTwoHanded) { OutReason = TEXT("양손 화물을 내려놓아야 보강할 수 있습니다."); return false; }
	for (TActorIterator<ADigetherPrototypeCorpse> It(GetWorld()); It; ++It)
		if (It->Carrier == Caller->GetPawn()) { OutReason = TEXT("시체를 내려놓아야 보강할 수 있습니다."); return false; }
	if (!FindExcavationNear(WorldPoint, 150.f)) { OutReason = TEXT("굴착한 구간에 보강을 설치하세요."); return false; }
	if (Support.bInstalled)
	{
		const FVector Anchor = GetActorTransform().TransformPosition(Support.LocalAnchor);
		if (FVector::DistSquared(Anchor, WorldPoint) > FMath::Square(250.f)) { OutReason = TEXT("이 시험 지형의 지지대가 다른 곳에 설치되어 있습니다."); return false; }
		if (Support.Strength >= 2) { OutReason = TEXT("보강이 이미 튼튼합니다."); return false; }
		OutReason = TEXT("보강을 무료로 보수했습니다.");
	}
	else
	{
		if (!Mode->RequestUseWood(Caller, OutReason)) { return false; }
		Support.bInstalled = true;
		Support.LocalAnchor = GetActorTransform().InverseTransformPosition(WorldPoint);
		OutReason = TEXT("시험용 목재 1개로 보강했습니다.");
	}
	Support.Strength = 2;
	OnRep_Support();
	ForceNetUpdate();
	return true;
}

void ADigetherExcavationTerrain::CheckTremor()
{
	if (!HasAuthority() || !bHasExcavation || !GetWorld()) { return; }
	ADigetherPrototypeGameState* State = GetWorld()->GetGameState<ADigetherPrototypeGameState>();
	ADigetherPrototypeGameMode* Mode = GetWorld()->GetAuthGameMode<ADigetherPrototypeGameMode>();
	if (!State || !Mode || State->PublicState.Phase != EPrototypePhase::Expedition) { return; }
	const float RemainingFraction = FMath::Clamp(State->PublicState.RemainingSeconds / FMath::Max(1.f, Mode->GetTuning().RunSeconds), 0.f, 1.f);
	const float Interval = FMath::Max(1.f, 25.f * FMath::Lerp(.3f, 1.f, RemainingFraction) * (State->PublicState.ElevatorTrips > 0 ? .7f : 1.f));
	SecondsSinceTremor += 1.f;
	if (SecondsSinceTremor < Interval) { return; }
	SecondsSinceTremor = 0.f;
	const FVector Center = GetActorTransform().TransformPosition(LastExcavationLocal);
	const bool bSupportCovers = Support.bInstalled && FVector::DistSquared(GetActorTransform().TransformPosition(Support.LocalAnchor), Center) <= FMath::Square(250.f);
	if (bSupportCovers && Support.Strength > 0)
	{
		--Support.Strength;
		State->PublicState.Message = TEXT("여진이 발생했습니다. 굴착 구간과 보강 상태를 확인하세요.");
		State->ForceNetUpdate(); ForceNetUpdate(); return;
	}
	TArray<FBox> Protected;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const FBox Box = It->GetComponentsBoundingBox(true);
		if (Box.IsValid) { Protected.Add(Box); }
	}
	for (TActorIterator<ADigetherPrototypePickup> It(GetWorld()); It; ++It)
	{
		const FBox Box = It->GetComponentsBoundingBox(true);
		if (Box.IsValid) { Protected.Add(Box); }
	}
	for (TActorIterator<ADigetherPrototypeCorpse> It(GetWorld()); It; ++It)
	{
		const FBox Box = It->GetComponentsBoundingBox(true);
		if (Box.IsValid) { Protected.Add(Box); }
	}
	FString Message;
	if (!ServerRestoreAt(Center, 175.f, Protected, Message))
	{
		// A protected or unbuildable volume is retried without burying anyone.
		SecondsSinceTremor = FMath::Max(0.f, Interval - 1.f); return;
	}
	if (bSupportCovers) { Support = FDigetherExcavationSupport(); OnRep_Support(); }
	int32 PlayersWithPawns = 0;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It) { if (Mode->IsHazardTarget(*It)) { ++PlayersWithPawns; } }
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
		if (Mode->IsHazardTarget(*It) && FVector::DistSquared(It->GetActorLocation(), Center) < FMath::Square(275.f))
			Mode->RequestWorldBurialPawn(*It, PlayersWithPawns == 1 ? 3 : 1);
	State->PublicState.Message = TEXT("여진으로 굴착 구간이 붕괴했습니다.");
	State->ForceNetUpdate(); ForceNetUpdate();
	// Continue checking remaining excavations rather than tracking an operation history.
	bHasExcavation = false;
	const TArray<uint8> Original = DigetherExcavation::InitialDensity();
	for (int32 Z = 1; Z < DigetherExcavation::SamplesZ && !bHasExcavation; ++Z)
		for (int32 Y = 1; Y < DigetherExcavation::CellsXY && !bHasExcavation; ++Y)
			for (int32 X = 1; X < DigetherExcavation::CellsXY; ++X)
			{
				const int32 I = DigetherExcavation::Index(X, Y, Z);
				if (Original[I] > 128 && AppliedDensity[I] <= 128)
				{
					LastExcavationLocal = DigetherExcavation::Position(X, Y, Z);
					bHasExcavation = true; break;
				}
			}
}

bool ADigetherExcavationTerrain::ResetTerrain()
{
	if (!HasAuthority() || !bHasActiveMesh) { return false; }
	FDigetherExcavationSnapshot Candidate;
	Candidate.Revision = Snapshot.Revision + 1;
	Candidate.Density = DigetherExcavation::InitialDensity();
	if (!ApplySnapshot(Candidate)) { return false; }
	Snapshot = MoveTemp(Candidate);
	bHasExcavation = false;
	SecondsSinceTremor = 0.f;
	Support = FDigetherExcavationSupport();
	OnRep_Support();
	ForceNetUpdate();
	return true;
}

uint32 ADigetherExcavationTerrain::GetTerrainHash() const { return DigetherExcavation::Hash(AppliedDensity); }
bool ADigetherExcavationTerrain::IsSolidAtWorldPoint(const FVector& WorldPoint) const
{
	return DigetherExcavation::IsSolid(AppliedDensity, GetActorTransform().InverseTransformPosition(WorldPoint));
}
void ADigetherExcavationTerrain::SetTerrainMaterial(UMaterialInterface* Material)
{
	TerrainMaterial = Material;
	MeshA->SetMaterial(0, Material);
	MeshB->SetMaterial(0, Material);
}
