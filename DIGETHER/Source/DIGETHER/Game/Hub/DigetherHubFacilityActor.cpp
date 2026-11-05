#include "Game/Hub/DigetherHubFacilityActor.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "GameFramework/Pawn.h"

ADigetherHubFacilityActor::ADigetherHubFacilityActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

bool ADigetherHubFacilityActor::CanInteract(const ADigetherGameplayPlayerController* Requester) const
{
	if (!IsValid(Requester) || Requester->GetWorld() != GetWorld() || FacilityId.IsNone()
		|| !FMath::IsFinite(InteractionDistanceCm) || InteractionDistanceCm <= 0.f || GetActorTransform().ContainsNaN())
	{
		return false;
	}
	const APawn* Pawn = Requester->GetPawn();
	const ADigetherGameplayPlayerState* Player = Requester->GetPlayerState<ADigetherGameplayPlayerState>();
	if (!IsValid(Pawn) || Pawn->GetWorld() != GetWorld() || Pawn->GetController() != Requester
		|| Pawn->GetActorTransform().ContainsNaN()
		|| !IsValid(Player) || Player->GetOwner() != Requester || !IsValid(Player->GetLifeComponent())
		|| Player->GetLifeComponent()->CaptureState().PublicState.Life != EDigetherLifeState::Active
		|| FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) > FMath::Square(InteractionDistanceCm))
	{
		return false;
	}
	// 외형 중심 대신 접근 가능한 기능 원점까지 시야 검사
	FVector Eye;
	FRotator Rotation;
	Requester->GetPlayerViewPoint(Eye, Rotation);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(DigetherHubInteraction), false, Pawn);
	Query.AddIgnoredActor(Requester);
	Query.AddIgnoredActor(this);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, GetActorLocation(), ECC_Visibility, Query);
}
