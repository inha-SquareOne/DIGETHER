#include "Game/Combat/DigetherAnimNotifyState_MeleeHitWindow.h"

#include "Components/SkeletalMeshComponent.h"
#include "Game/Combat/DigetherMeleeHitComponent.h"

namespace
{
	UDigetherMeleeHitComponent* FindHitComponent(const USkeletalMeshComponent* MeshComp)
	{
		const AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
		return IsValid(Owner) ? Owner->FindComponentByClass<UDigetherMeleeHitComponent>() : nullptr;
	}
}

void UDigetherAnimNotifyState_MeleeHitWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (UDigetherMeleeHitComponent* HitComponent = FindHitComponent(MeshComp))
	{
		HitComponent->BeginHitWindow(DamageScale);
	}
}

void UDigetherAnimNotifyState_MeleeHitWindow::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	if (UDigetherMeleeHitComponent* HitComponent = FindHitComponent(MeshComp))
	{
		HitComponent->TickHitWindow();
	}
}

void UDigetherAnimNotifyState_MeleeHitWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (UDigetherMeleeHitComponent* HitComponent = FindHitComponent(MeshComp))
	{
		HitComponent->EndHitWindow();
	}
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UDigetherAnimNotifyState_MeleeHitWindow::GetNotifyName_Implementation() const
{
	return TEXT("MeleeHitWindow");
}
