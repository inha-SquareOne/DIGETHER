#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Hub/DigetherHubVisualActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

namespace
{
	class FDigetherHubVisualTestWorld
	{
	public:
		FDigetherHubVisualTestWorld()
		{
			UWorld::InitializationValues Values;
			Values.InitializeScenes(false).AllowAudioPlayback(false).CreatePhysicsScene(false)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false)
				.CreateFXSystem(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &Values);
		}

		~FDigetherHubVisualTestWorld()
		{
			if (World != nullptr)
			{
				World->DestroyWorld(false);
			}
		}

		FDigetherHubVisualTestWorld(const FDigetherHubVisualTestWorld&) = delete;
		FDigetherHubVisualTestWorld& operator=(const FDigetherHubVisualTestWorld&) = delete;

		UWorld* World = nullptr;
	};

	UBoxComponent* AddBox(ADigetherHubVisualActor& Visual, USceneComponent& Parent, const TCHAR* Name)
	{
		UBoxComponent* Box = NewObject<UBoxComponent>(&Visual, Name);
		Visual.AddInstanceComponent(Box);
		Box->SetupAttachment(&Parent);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetGenerateOverlapEvents(true);
		Box->RegisterComponent();
		return Box;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherHubVisualSwapTest,
	"DIGETHER.Game.Hub.Visual.SwapPreservesGameplayGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherHubVisualSwapTest::RunTest(const FString& Parameters)
{
	FDigetherHubVisualTestWorld Fixture;
	if (!TestNotNull(TEXT("독립 검사 월드"), Fixture.World))
	{
		return false;
	}
	ADigetherHubVisualActor* Visual = Fixture.World->SpawnActor<ADigetherHubVisualActor>();
	if (!TestNotNull(TEXT("시설 비주얼"), Visual))
	{
		return false;
	}
	Visual->SetActorTransform(FTransform(FRotator(0.f, 40.f, 0.f), FVector(700.f, -190.f, 0.f)));
	Visual->InteractionAnchor->SetRelativeLocation(FVector(-140.f, 0.f, 85.f));
	UBoxComponent* Placeholder = AddBox(*Visual, *Visual->PlaceholderRoot, TEXT("Placeholder"));
	UBoxComponent* Collision = AddBox(*Visual, *Visual->CollisionRoot, TEXT("GameplayCollision"));
	Collision->SetBoxExtent(FVector(60.f, 90.f, 40.f));
	Collision->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	USceneComponent* NestedArtRoot = NewObject<USceneComponent>(Visual, TEXT("NestedArtRoot"));
	Visual->AddInstanceComponent(NestedArtRoot);
	NestedArtRoot->SetupAttachment(Visual->ArtRoot);
	NestedArtRoot->RegisterComponent();
	UBoxComponent* NestedArt = AddBox(*Visual, *NestedArtRoot, TEXT("NestedArt"));
	AActor* Gameplay = Fixture.World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("기능 Actor"), Gameplay))
	{
		return false;
	}
	Gameplay->SetRootComponent(NewObject<USceneComponent>(Gameplay, TEXT("GameplayRoot")));
	Gameplay->GetRootComponent()->RegisterComponent();
	Gameplay->AttachToComponent(Visual->InteractionAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Visual->GameplayActor = Gameplay;
	const FTransform ActorBefore = Visual->GetActorTransform();
	const FTransform AnchorBefore = Visual->InteractionAnchor->GetComponentTransform();
	const FTransform CollisionBefore = Collision->GetComponentTransform();
	const FTransform GameplayBefore = Gameplay->GetActorTransform();
	Visual->RefreshVisuals();
	TestTrue(TEXT("메시 미지정 시 임시 외형 표시"), Placeholder->GetVisibleFlag() && !Placeholder->bHiddenInGame);
	TestTrue(TEXT("기본 아트 계층 숨김"), !NestedArt->GetVisibleFlag() && NestedArt->bHiddenInGame);
	Visual->ReplacementMesh->SetStaticMesh(NewObject<UStaticMesh>(Visual));
	Visual->ArtRoot->SetRelativeLocation(FVector(10.f, 20.f, 5.f));
	Visual->ArtRoot->SetRelativeScale3D(FVector(1.2f, 0.8f, 1.1f));
	Visual->RefreshVisuals();
	TestTrue(TEXT("메시 지정 시 임시 외형 숨김"), !Placeholder->GetVisibleFlag() && Placeholder->bHiddenInGame);
	TestTrue(TEXT("교체 메시와 중첩 아트 표시"), Visual->ReplacementMesh->GetVisibleFlag()
		&& !Visual->ReplacementMesh->bHiddenInGame && NestedArt->GetVisibleFlag() && !NestedArt->bHiddenInGame);
	TestTrue(TEXT("임시 외형 충돌 제거"), Placeholder->GetCollisionEnabled() == ECollisionEnabled::NoCollision
		&& !Placeholder->GetGenerateOverlapEvents());
	TestTrue(TEXT("교체 메시 충돌 제거"), Visual->ReplacementMesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision
		&& !Visual->ReplacementMesh->GetGenerateOverlapEvents());
	TestTrue(TEXT("중첩 아트 충돌 제거"), NestedArt->GetCollisionEnabled() == ECollisionEnabled::NoCollision
		&& !NestedArt->GetGenerateOverlapEvents());
	TestTrue(TEXT("별도 충돌 설정 보존"), Collision->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics
		&& Collision->GetGenerateOverlapEvents() && Collision->GetUnscaledBoxExtent().Equals(FVector(60.f, 90.f, 40.f)));
	TestTrue(TEXT("Actor 배치 보존"), Visual->GetActorTransform().Equals(ActorBefore));
	TestTrue(TEXT("상호작용 기준점 보존"), Visual->InteractionAnchor->GetComponentTransform().Equals(AnchorBefore));
	TestTrue(TEXT("통행 충돌 배치 보존"), Collision->GetComponentTransform().Equals(CollisionBefore));
	TestTrue(TEXT("연결된 기능 Actor와 위치 보존"), Visual->GameplayActor.Get() == Gameplay
		&& Gameplay->GetActorTransform().Equals(GameplayBefore));
	TestFalse(TEXT("시각 Actor 복제 추가 없음"), Visual->GetIsReplicated());
	Visual->ReplacementMesh->SetStaticMesh(nullptr);
	Visual->RefreshVisuals();
	TestTrue(TEXT("메시 제거 시 임시 외형 복원"), Placeholder->GetVisibleFlag() && !Placeholder->bHiddenInGame);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherHubCustomArtTest,
	"DIGETHER.Game.Hub.Visual.CustomArtWithoutSingleMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherHubCustomArtTest::RunTest(const FString& Parameters)
{
	FDigetherHubVisualTestWorld Fixture;
	if (!TestNotNull(TEXT("독립 검사 월드"), Fixture.World))
	{
		return false;
	}
	ADigetherHubVisualActor* Visual = Fixture.World->SpawnActor<ADigetherHubVisualActor>();
	if (!TestNotNull(TEXT("시설 비주얼"), Visual))
	{
		return false;
	}
	UBoxComponent* Placeholder = AddBox(*Visual, *Visual->PlaceholderRoot, TEXT("Placeholder"));
	UBoxComponent* Art = AddBox(*Visual, *Visual->ArtRoot, TEXT("CustomArtPart"));
	Visual->bUseCustomArt = true;
	Visual->RefreshVisuals();
	TestTrue(TEXT("단일 메시 없이 조립 아트 표시"), Art->GetVisibleFlag() && !Art->bHiddenInGame);
	TestTrue(TEXT("조립 아트 사용 시 임시 외형 숨김"), !Placeholder->GetVisibleFlag() && Placeholder->bHiddenInGame);
	TestTrue(TEXT("조립 아트 충돌 제거"), Art->GetCollisionEnabled() == ECollisionEnabled::NoCollision
		&& !Art->GetGenerateOverlapEvents());
	Visual->bUseCustomArt = false;
	Visual->RefreshVisuals();
	TestTrue(TEXT("조립 아트 해제 시 임시 외형 복원"), Placeholder->GetVisibleFlag() && !Placeholder->bHiddenInGame);
	TestTrue(TEXT("조립 아트 해제 시 아트 숨김"), !Art->GetVisibleFlag() && Art->bHiddenInGame);
	return true;
}

#endif
