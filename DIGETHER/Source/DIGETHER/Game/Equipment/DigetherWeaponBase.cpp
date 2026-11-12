#include "Game/Equipment/DigetherWeaponBase.h"

#include "Animation/AnimMontage.h"
#include "Components/StaticMeshComponent.h"

ADigetherWeaponBase::ADigetherWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false;
	// 장비 자체는 위치가 캐릭터를 따라가므로 이동 복제가 필요 없고, 어느 장비를 들었는지는 캐릭터가 복제한다
	bReplicates = true;
	SetReplicateMovement(false);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(Root);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);
	WeaponMesh->SetCastShadow(true);
}

void ADigetherWeaponBase::AttachToCharacter(USceneComponent* Parent)
{
	if (Parent == nullptr)
	{
		return;
	}
	WeaponMesh->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, GripSocketName);
}

void ADigetherWeaponBase::SetWeaponVisible(bool bVisible)
{
	WeaponMesh->SetVisibility(bVisible);
}
