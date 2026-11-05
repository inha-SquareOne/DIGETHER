#include "Game/Hub/DigetherHubVisualActor.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"

namespace
{
	void RefreshVisualBranch(USceneComponent* Branch, bool bVisible)
	{
		Branch->SetVisibility(bVisible, true);
		Branch->SetHiddenInGame(!bVisible, true);
		// 아트 메쉬 교체와 무관한 통행 판정 유지
		// 실제 충돌은 CollisionRoot 하위 컴포넌트 담당
		TArray<USceneComponent*> Descendants;
		Branch->GetChildrenComponents(true, Descendants);
		for (USceneComponent* Descendant : Descendants)
		{
			if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Descendant))
			{
				Primitive->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
				Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Primitive->SetGenerateOverlapEvents(false);
			}
		}
	}
}

ADigetherHubVisualActor::ADigetherHubVisualActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	ArtRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ArtRoot"));
	ArtRoot->SetupAttachment(SceneRoot);
	ReplacementMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReplacementMesh"));
	ReplacementMesh->SetupAttachment(ArtRoot);
	ReplacementMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	ReplacementMesh->SetGenerateOverlapEvents(false);
	PlaceholderRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PlaceholderRoot"));
	PlaceholderRoot->SetupAttachment(SceneRoot);
	CollisionRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CollisionRoot"));
	CollisionRoot->SetupAttachment(SceneRoot);
	InteractionAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("InteractionAnchor"));
	InteractionAnchor->SetupAttachment(SceneRoot);
}

void ADigetherHubVisualActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshVisuals();
}

void ADigetherHubVisualActor::BeginPlay()
{
	Super::BeginPlay();
	RefreshVisuals();
}

void ADigetherHubVisualActor::RefreshVisuals()
{
	const bool bShowArt = bUseCustomArt || IsValid(ReplacementMesh->GetStaticMesh());
	RefreshVisualBranch(ArtRoot, bShowArt);
	RefreshVisualBranch(PlaceholderRoot, !bShowArt);
}
