#include "DigetherJihunSpineEdgeBuilder.h"

#include "DigetherJihunSpineEdgeGenerator.h"
#include "DigetherJihunSpineEdgePreview.h"
#include "DigetherJihunSpineEdgeRuleSet.h"
#include "DigetherJihunSpineEdgeSampleCatalog.h"
#include "DrawDebugHelpers.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogDigetherJihunSpineEdge, Log, All);

ADigetherJihunSpineEdgeBuilder::ADigetherJihunSpineEdgeBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// 건물 전체를 정하는 값이라 거리와 무관하게 모든 클라이언트에 복제
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
}

void ADigetherJihunSpineEdgeBuilder::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADigetherJihunSpineEdgeBuilder, BuildRequest);
}

void ADigetherJihunSpineEdgeBuilder::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		if (bBuildOnBeginPlay)
		{
			RequestBuild(Seed);
		}
		return;
	}
	// BeginPlay 전에 복제값이 먼저 도착한 경우 여기서 조립
	BuildFromRequest();
}

void ADigetherJihunSpineEdgeBuilder::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearLayout();
	Super::EndPlay(EndPlayReason);
}

void ADigetherJihunSpineEdgeBuilder::RequestBuild(const int32 InSeed)
{
	if (!HasAuthority())
	{
		return;
	}
	BuildRequest.Seed = bRandomSeed ? FMath::RandRange(0, MAX_int32 - 1) : InSeed;
	++BuildRequest.Revision;
	ForceNetUpdate();
	BuildFromRequest();
}

void ADigetherJihunSpineEdgeBuilder::OnRep_BuildRequest()
{
	if (!HasActorBegunPlay())
	{
		return;
	}
	BuildFromRequest();
}

void ADigetherJihunSpineEdgeBuilder::GatherInputs(TArray<FDigetherJihunSpineEdgeModuleSpec>& OutCatalog, FDigetherJihunSpineEdgeRules& OutRules) const
{
	if (IsValid(RuleSet))
	{
		OutCatalog = RuleSet->BuildCatalog();
		OutRules = RuleSet->Rules;
		return;
	}
	OutCatalog = DigetherJihunSpineEdge::MakeGleyBoxCatalog();
	OutRules = FDigetherJihunSpineEdgeRules();
}

bool ADigetherJihunSpineEdgeBuilder::BuildFromRequest()
{
	if (BuildRequest.Revision == 0 || BuildRequest.Revision == BuiltRevision)
	{
		return false;
	}
	BuiltRevision = BuildRequest.Revision;
	ClearLayout();
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	TArray<FDigetherJihunSpineEdgeModuleSpec> Catalog;
	FDigetherJihunSpineEdgeRules Rules;
	GatherInputs(Catalog, Rules);

	const double GenerateStart = FPlatformTime::Seconds();
	Layout = FDigetherJihunSpineEdgeGenerator::Generate(Catalog, Rules, BuildRequest.Seed);
	const double GenerateMs = (FPlatformTime::Seconds() - GenerateStart) * 1000.0;
	const TCHAR* NetSide = HasAuthority() ? TEXT("server") : TEXT("client");
	if (!Layout.bSuccess)
	{
		UE_LOG(LogDigetherJihunSpineEdge, Warning, TEXT("[%s] Generate failed seed %d rev %d after %d attempts: %s"),
			NetSide, BuildRequest.Seed, BuildRequest.Revision, Layout.Attempts, *Layout.FailureReason);
		return false;
	}
	// 서버·클라이언트 로그의 배치 해시가 같으면 같은 건물
	uint32 LayoutHash = 0;
	for (const FDigetherJihunSpineEdgePlacement& Placement : Layout.Placements)
	{
		LayoutHash = HashCombineFast(LayoutHash, GetTypeHash(Placement.ModuleIndex));
		LayoutHash = HashCombineFast(LayoutHash, GetTypeHash(Placement.PivotUnits));
		LayoutHash = HashCombineFast(LayoutHash, GetTypeHash(Placement.Floor * 4 + Placement.QuarterTurns));
	}
	UE_LOG(LogDigetherJihunSpineEdge, Log, TEXT("[%s] Generated seed %d rev %d (used %d): %d modules, %d floors, %d classrooms, hash %08x in %.2f ms"),
		NetSide, BuildRequest.Seed, BuildRequest.Revision, Layout.SeedUsed, Layout.Placements.Num(), Layout.FloorCount, Layout.ClassroomCount, LayoutHash, GenerateMs);

	const FTransform Origin(FRotator(0.0, GetActorRotation().Yaw, 0.0), GetActorLocation());
	BuildStartSeconds = FPlatformTime::Seconds();
	ShownModules = 0;
	for (int32 Index = 0; Index < Layout.Placements.Num(); ++Index)
	{
		const FDigetherJihunSpineEdgePlacement& Placement = Layout.Placements[Index];
		const FDigetherJihunSpineEdgeModuleSpec& Spec = Catalog[Placement.ModuleIndex];
		if (Spec.Level.IsNull())
		{
			UE_LOG(LogDigetherJihunSpineEdge, Warning, TEXT("Module %s has no level"), *Spec.Id.ToString());
			continue;
		}
		bool bSuccess = false;
		// 레벨 이름을 Revision·배치 순서로 고정: 서버·클라이언트가 같은 이름, 재생성 시 이전 레벨과 충돌 없음
		const FString LevelName = FString::Printf(TEXT("%s_JSE_r%d_%03d"), *GetName(), BuildRequest.Revision, Index);
		ULevelStreamingDynamic* Streaming = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
			this, Spec.Level, DigetherJihunSpineEdgePreview::GetPlacementTransform(Placement, Rules, Origin), bSuccess, LevelName);
		if (!bSuccess || !IsValid(Streaming))
		{
			UE_LOG(LogDigetherJihunSpineEdge, Warning, TEXT("Failed to load module %s"), *Spec.Id.ToString());
			continue;
		}
		Streaming->OnLevelShown.AddDynamic(this, &ADigetherJihunSpineEdgeBuilder::HandleModuleShown);
		LoadedModules.Add(Streaming);
	}

	if (bDrawLayoutOnBuild)
	{
		DigetherJihunSpineEdgePreview::DrawLayout(World, Origin, Catalog, Rules, Layout, false, DrawLayoutSeconds);
	}
	return true;
}

void ADigetherJihunSpineEdgeBuilder::HandleModuleShown()
{
	++ShownModules;
	if (ShownModules == LoadedModules.Num())
	{
		UE_LOG(LogDigetherJihunSpineEdge, Log, TEXT("[%s] All %d modules shown in %.2f s"),
			HasAuthority() ? TEXT("server") : TEXT("client"), ShownModules, FPlatformTime::Seconds() - BuildStartSeconds);
	}
}

void ADigetherJihunSpineEdgeBuilder::ClearLayout()
{
	for (ULevelStreamingDynamic* Streaming : LoadedModules)
	{
		if (IsValid(Streaming))
		{
			Streaming->OnLevelShown.RemoveAll(this);
			Streaming->SetIsRequestingUnloadAndRemoval(true);
		}
	}
	LoadedModules.Reset();
	ShownModules = 0;
}

#if WITH_EDITOR
void ADigetherJihunSpineEdgeBuilder::PreviewInEditor()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}
	TArray<FDigetherJihunSpineEdgeModuleSpec> Catalog;
	FDigetherJihunSpineEdgeRules Rules;
	GatherInputs(Catalog, Rules);
	Layout = FDigetherJihunSpineEdgeGenerator::Generate(Catalog, Rules, Seed);
	FlushPersistentDebugLines(World);
	if (!Layout.bSuccess)
	{
		UE_LOG(LogDigetherJihunSpineEdge, Warning, TEXT("Preview failed seed %d: %s"), Seed, *Layout.FailureReason);
		return;
	}
	const FTransform Origin(FRotator(0.0, GetActorRotation().Yaw, 0.0), GetActorLocation());
	DigetherJihunSpineEdgePreview::DrawLayout(World, Origin, Catalog, Rules, Layout, true, -1.0f);
	UE_LOG(LogDigetherJihunSpineEdge, Log, TEXT("Preview seed %d: %d modules, %d floors, %d classrooms"),
		Layout.SeedUsed, Layout.Placements.Num(), Layout.FloorCount, Layout.ClassroomCount);
}

void ADigetherJihunSpineEdgeBuilder::NextSeedPreview()
{
	Modify();
	++Seed;
	PreviewInEditor();
}

void ADigetherJihunSpineEdgeBuilder::ClearPreview()
{
	if (UWorld* World = GetWorld())
	{
		FlushPersistentDebugLines(World);
	}
}
#endif
