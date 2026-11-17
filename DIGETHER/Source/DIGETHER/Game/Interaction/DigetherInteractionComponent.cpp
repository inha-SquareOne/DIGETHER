#include "Game/Interaction/DigetherInteractionComponent.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Game/Interaction/DigetherInteractable.h"
#include "Game/Inventory/DigetherInventoryLog.h"
#include "Game/Player/DigetherPlayerCharacter.h"
#include "GameFramework/Pawn.h"

UDigetherInteractionComponent::UDigetherInteractionComponent()
{
	// 조준 대상 갱신은 소유 클라이언트의 화면용이라 틱에서 함
	PrimaryComponentTick.bCanEverTick = true;
}

bool UDigetherInteractionComponent::IsOwnerLocallyControlled() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return IsValid(Pawn) && Pawn->IsLocallyControlled();
}

void UDigetherInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsOwnerLocallyControlled())
	{
		return;
	}
	FocusedTarget = FindBestTarget();
}

FText UDigetherInteractionComponent::GetFocusedPrompt() const
{
	AActor* Target = FocusedTarget.Get();
	const IDigetherInteractable* Interactable = IsValid(Target) ? Cast<IDigetherInteractable>(Target) : nullptr;
	return Interactable != nullptr ? Interactable->GetInteractionPrompt() : FText::GetEmpty();
}

AActor* UDigetherInteractionComponent::FindBestTarget() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	UWorld* World = GetWorld();
	if (!IsValid(Pawn) || !IsValid(World))
	{
		return nullptr;
	}
	const FVector Start = Pawn->GetPawnViewLocation();
	const FVector End = Start + Pawn->GetBaseAimRotation().Vector() * InteractionRangeCm;

	// 상호작용 대상은 Visibility에 Overlap으로 응답해 한 번에 여러 개를 모으고, 벽 같은 Block이 나오면 거기서 끊김
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DigetherInteractAim), false, Pawn);
	TArray<FHitResult> Hits;
	World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(AimRadiusCm), Params);

	AActor* BestTarget = nullptr;
	EDigetherInteractionKind BestKind = EDigetherInteractionKind::Reinforce;
	float BestDistance = TNumericLimits<float>::Max();
	for (const FHitResult& Hit : Hits)
	{
		AActor* Candidate = Hit.GetActor();
		const IDigetherInteractable* Interactable = IsValid(Candidate) ? Cast<IDigetherInteractable>(Candidate) : nullptr;
		if (Interactable != nullptr && Interactable->CanInteract(Pawn))
		{
			const EDigetherInteractionKind Kind = Interactable->GetInteractionKind();
			const bool bBetter = BestTarget == nullptr
				|| static_cast<uint8>(Kind) < static_cast<uint8>(BestKind)
				|| (Kind == BestKind && Hit.Distance < BestDistance);
			if (bBetter)
			{
				BestTarget = Candidate;
				BestKind = Kind;
				BestDistance = Hit.Distance;
			}
		}
		if (Hit.bBlockingHit)
		{
			break;
		}
	}
	return BestTarget;
}

void UDigetherInteractionComponent::RequestInteract()
{
	const ADigetherPlayerCharacter* Character = Cast<ADigetherPlayerCharacter>(GetOwner());
	if (!IsValid(Character) || Character->IsDead())
	{
		return;
	}
	AActor* Target = FindBestTarget();
	if (!IsValid(Target))
	{
		return;
	}
	Server_Interact(Target);
}

void UDigetherInteractionComponent::Server_Interact_Implementation(AActor* Target)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	UWorld* World = GetWorld();
	const ADigetherPlayerCharacter* Character = Cast<ADigetherPlayerCharacter>(GetOwner());
	if (!IsValid(Pawn) || !IsValid(World) || !IsValid(Target) || (IsValid(Character) && Character->IsDead()))
	{
		return;
	}
	IDigetherInteractable* Interactable = Cast<IDigetherInteractable>(Target);
	if (Interactable == nullptr)
	{
		return;
	}

	// 서버가 거리와 시선 가림을 다시 검사함(클라이언트가 보낸 대상을 믿지 않음)
	const FVector ViewLocation = Pawn->GetPawnViewLocation();
	const FBox TargetBox = Target->GetComponentsBoundingBox();
	const float MaxDistance = InteractionRangeCm + ServerRangeToleranceCm;
	if (!TargetBox.IsValid || TargetBox.ComputeSquaredDistanceToPoint(ViewLocation) > FMath::Square(MaxDistance))
	{
		UE_LOG(LogDigetherInventory, Verbose, TEXT("%s 상호작용 거절: %s가 너무 멀다"), *GetNameSafe(Pawn), *GetNameSafe(Target));
		return;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DigetherInteractSight), false, Pawn);
	Params.AddIgnoredActor(Target);
	FHitResult Blocked;
	if (World->LineTraceSingleByChannel(Blocked, ViewLocation, TargetBox.GetCenter(), ECC_Visibility, Params))
	{
		UE_LOG(LogDigetherInventory, Verbose, TEXT("%s 상호작용 거절: %s가 %s에 가려졌다"), *GetNameSafe(Pawn), *GetNameSafe(Target), *GetNameSafe(Blocked.GetActor()));
		return;
	}
	if (!Interactable->CanInteract(Pawn))
	{
		return;
	}
	Interactable->Interact(Pawn);
}
