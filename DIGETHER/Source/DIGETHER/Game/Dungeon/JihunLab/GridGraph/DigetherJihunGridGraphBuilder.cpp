#include "DigetherJihunGridGraphBuilder.h"

#include "DigetherJihunGridGraphCatalog.h"
#include "DigetherJihunGridGraphGenerator.h"
#include "DigetherJihunGridGraphPreview.h"
#include "DigetherJihunGridGraphRuleSet.h"
#include "DrawDebugHelpers.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogDigetherJihunGridGraph, Log, All);

ADigetherJihunGridGraphBuilder::ADigetherJihunGridGraphBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// 던전 전체를 정하는 값이라 거리와 무관하게 모든 클라이언트에 복제
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
}

void ADigetherJihunGridGraphBuilder::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADigetherJihunGridGraphBuilder, BuildRequest);
}

void ADigetherJihunGridGraphBuilder::BeginPlay()
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

void ADigetherJihunGridGraphBuilder::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearLayout();
	Super::EndPlay(EndPlayReason);
}

void ADigetherJihunGridGraphBuilder::RequestBuild(const int32 InSeed)
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

void ADigetherJihunGridGraphBuilder::OnRep_BuildRequest()
{
	if (!HasActorBegunPlay())
	{
		return;
	}
	BuildFromRequest();
}

void ADigetherJihunGridGraphBuilder::GatherInputs(TArray<FDigetherJihunGridGraphModuleSpec>& OutCatalog, FDigetherJihunGridGraphRules& OutRules) const
{
	if (IsValid(RuleSet))
	{
		OutRules = RuleSet->Rules;
		OutCatalog = RuleSet->BuildCatalog();
		// 모듈 Data Asset을 아직 만들지 않은 동안은 규칙만 에셋에서, 모듈은 실측 카탈로그에서
		if (!OutCatalog.IsEmpty())
		{
			return;
		}
	}
	else
	{
		OutRules = FDigetherJihunGridGraphRules();
	}
	OutCatalog = DigetherJihunGridGraph::MakeGleyBoxCatalog();
}

bool ADigetherJihunGridGraphBuilder::BuildFromRequest()
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

	TArray<FDigetherJihunGridGraphModuleSpec> Catalog;
	FDigetherJihunGridGraphRules Rules;
	GatherInputs(Catalog, Rules);

	const double GenerateStart = FPlatformTime::Seconds();
	Layout = FDigetherJihunGridGraphGenerator::Generate(Catalog, Rules, BuildRequest.Seed);
	const double GenerateMs = (FPlatformTime::Seconds() - GenerateStart) * 1000.0;
	const TCHAR* NetSide = HasAuthority() ? TEXT("server") : TEXT("client");
	if (!Layout.bSuccess)
	{
		UE_LOG(LogDigetherJihunGridGraph, Warning, TEXT("[%s] Generate failed seed %d rev %d after %d attempts: %s"),
			NetSide, BuildRequest.Seed, BuildRequest.Revision, Layout.Attempts, *Layout.FailureReason);
		return false;
	}
	// 서버·클라이언트 로그의 배치 해시가 같으면 같은 던전
	uint32 LayoutHash = 0;
	for (const FDigetherJihunGridGraphPlacement& Placement : Layout.Placements)
	{
		LayoutHash = HashCombineFast(LayoutHash, GetTypeHash(Placement.ModuleIndex));
		LayoutHash = HashCombineFast(LayoutHash, GetTypeHash(Placement.PivotUnits));
		LayoutHash = HashCombineFast(LayoutHash, GetTypeHash(Placement.Floor * 4 + Placement.QuarterTurns));
	}
	UE_LOG(LogDigetherJihunGridGraph, Log, TEXT("[%s] Generated seed %d rev %d (used %d): %d placements, %d modules, %d floors, %d classrooms, farthest %d, hash %08x in %.2f ms"),
		NetSide, BuildRequest.Seed, BuildRequest.Revision, Layout.SeedUsed, Layout.Placements.Num(), Layout.TotalModules, Layout.FloorCount,
		Layout.ClassroomCount, Layout.FarthestModules, LayoutHash, GenerateMs);

	const FTransform Origin(FRotator(0.0, GetActorRotation().Yaw, 0.0), GetActorLocation());
	BuildStartSeconds = FPlatformTime::Seconds();
	ShownModules = 0;
	for (int32 Index = 0; Index < Layout.Placements.Num(); ++Index)
	{
		const FDigetherJihunGridGraphPlacement& Placement = Layout.Placements[Index];
		const FDigetherJihunGridGraphModuleSpec& Spec = Catalog[Placement.ModuleIndex];
		if (Spec.Level.IsNull())
		{
			UE_LOG(LogDigetherJihunGridGraph, Warning, TEXT("Module %s has no level"), *Spec.Id.ToString());
			continue;
		}
		bool bSuccess = false;
		// 레벨 이름을 Revision·배치 순서로 고정: 서버·클라이언트가 같은 이름, 재생성 시 이전 레벨과 충돌 없음
		const FString LevelName = FString::Printf(TEXT("%s_JGG_r%d_%04d"), *GetName(), BuildRequest.Revision, Index);
		ULevelStreamingDynamic* Streaming = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
			this, Spec.Level, DigetherJihunGridGraphPreview::GetPlacementTransform(Placement, Rules, Origin), bSuccess, LevelName);
		if (!bSuccess || !IsValid(Streaming))
		{
			UE_LOG(LogDigetherJihunGridGraph, Warning, TEXT("Failed to load module %s"), *Spec.Id.ToString());
			continue;
		}
		Streaming->OnLevelShown.AddDynamic(this, &ADigetherJihunGridGraphBuilder::HandleModuleShown);
		LoadedModules.Add(Streaming);
	}
	UE_LOG(LogDigetherJihunGridGraph, Log, TEXT("[%s] Requested %d module levels in %.2f ms"),
		NetSide, LoadedModules.Num(), (FPlatformTime::Seconds() - BuildStartSeconds) * 1000.0);

	if (bDrawLayoutOnBuild)
	{
		DigetherJihunGridGraphPreview::DrawLayout(World, Origin, Catalog, Rules, Layout, PreviewFloor, false, DrawLayoutSeconds);
	}
	return true;
}

void ADigetherJihunGridGraphBuilder::HandleModuleShown()
{
	++ShownModules;
	if (ShownModules == LoadedModules.Num())
	{
		UE_LOG(LogDigetherJihunGridGraph, Log, TEXT("[%s] All %d modules shown in %.2f s"),
			HasAuthority() ? TEXT("server") : TEXT("client"), ShownModules, FPlatformTime::Seconds() - BuildStartSeconds);
	}
}

void ADigetherJihunGridGraphBuilder::ClearLayout()
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
void ADigetherJihunGridGraphBuilder::PreviewInEditor()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}
	TArray<FDigetherJihunGridGraphModuleSpec> Catalog;
	FDigetherJihunGridGraphRules Rules;
	GatherInputs(Catalog, Rules);
	Layout = FDigetherJihunGridGraphGenerator::Generate(Catalog, Rules, Seed);
	FlushPersistentDebugLines(World);
	if (!Layout.bSuccess)
	{
		UE_LOG(LogDigetherJihunGridGraph, Warning, TEXT("Preview failed seed %d: %s"), Seed, *Layout.FailureReason);
		return;
	}
	const FTransform Origin(FRotator(0.0, GetActorRotation().Yaw, 0.0), GetActorLocation());
	DigetherJihunGridGraphPreview::DrawLayout(World, Origin, Catalog, Rules, Layout, PreviewFloor, true, -1.0f);
	UE_LOG(LogDigetherJihunGridGraph, Log, TEXT("Preview seed %d: %d modules, %d floors, %d classrooms, farthest %d"),
		Layout.SeedUsed, Layout.TotalModules, Layout.FloorCount, Layout.ClassroomCount, Layout.FarthestModules);
}

void ADigetherJihunGridGraphBuilder::NextSeedPreview()
{
	Modify();
	++Seed;
	PreviewInEditor();
}

void ADigetherJihunGridGraphBuilder::ClearPreview()
{
	if (UWorld* World = GetWorld())
	{
		FlushPersistentDebugLines(World);
	}
}
#endif
