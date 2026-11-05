#include "Game/Hub/DigetherHubStageAnchor.h"

#include "Components/SceneComponent.h"

ADigetherHubStageAnchor::ADigetherHubStageAnchor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}
