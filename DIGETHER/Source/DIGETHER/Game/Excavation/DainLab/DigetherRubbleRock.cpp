#include "DigetherRubbleRock.h"
#include "DigetherRockStrikeEffect.h"
#include "DigetherLabOreDrop.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ADigetherRubbleRock::ADigetherRubbleRock()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	RockMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RockMesh"));
	RockMesh->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Game/Developers/Dain/Excavation/Meshes/SM_Rock_Rubble.SM_Rock_Rubble"));
	RockMesh->SetStaticMesh(Mesh.Object);
	RockMesh->SetCollisionProfileName(TEXT("BlockAll"));
	RockMesh->CanCharacterStepUpOn = ECB_No;
}

bool ADigetherRubbleRock::ApplyToolHit(EDigetherLabTool Tool, const FHitResult& Hit)
{
	if (Tool == EDigetherLabTool::Hands || !HasAuthority() || IsActorBeingDestroyed() || Stage == EDigetherRubbleStage::Destroyed)
	{
		return false;
	}

	const bool bCommonMiningTool = Tool == EDigetherLabTool::Pickaxe || Tool == EDigetherLabTool::SteelPickaxe || Tool == EDigetherLabTool::Drill;
	if (Tool != EDigetherLabTool::UpperPickaxe && (RockKind == EDigetherRockKind::Hard || !bCommonMiningTool))
	{
		if (GetNetMode() != NM_DedicatedServer)
		{
			GetWorld()->SpawnActor<ADigetherRockStrikeEffect>(Hit.ImpactPoint, Hit.ImpactNormal.Rotation());
		}
		return false;
	}

	const int32 RequiredHits = Stage == EDigetherRubbleStage::Intact ? HitsToChip :
		(Stage == EDigetherRubbleStage::Chipped ? HitsToFracture : HitsToDestroy);
	if (++HitsInStage < FMath::Max(1, RequiredHits))
	{
		return true;
	}

	HitsInStage = 0;
	// 바닥 기준 피벗에서 메시와 그 메시의 충돌을 함께 축소한다.
	switch (Stage)
	{
	case EDigetherRubbleStage::Intact:
		Stage = EDigetherRubbleStage::Chipped;
		RockMesh->SetRelativeScale3D(FVector(.70, .70, .78));
		break;
	case EDigetherRubbleStage::Chipped:
		Stage = EDigetherRubbleStage::Fractured;
		RockMesh->SetRelativeScale3D(FVector(.42, .42, .53));
		break;
	case EDigetherRubbleStage::Fractured:
		Stage = EDigetherRubbleStage::Destroyed;
		if (RockKind == EDigetherRockKind::Ore)
		{
			// 최종 타격 한 번만 내용물을 방출한다. 생성 실패 시 같은 타격을 다시 시도할 수 있다.
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FVector Location = RockMesh->Bounds.Origin + FVector::UpVector * (RockMesh->Bounds.BoxExtent.Z + 10.);
			if (!GetWorld()->SpawnActor<ADigetherLabOreDrop>(Location, FRotator::ZeroRotator, Params))
			{
				Stage = EDigetherRubbleStage::Fractured;
				HitsInStage = FMath::Max(1, RequiredHits) - 1;
				return false;
			}
		}
		SetActorEnableCollision(false);
		SetActorHiddenInGame(true);
		Destroy();
		break;
	default:
		return false;
	}
	return true;
}
