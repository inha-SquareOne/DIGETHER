#include "Game/Equipment/DigetherWeaponBase.h"

#include "Animation/AnimMontage.h"
#include "Components/StaticMeshComponent.h"

ADigetherWeaponBase::ADigetherWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false;
	// 장비 자체는 위치가 캐릭터를 따라가므로 이동 복제가 필요 없고, 어느 장비를 들었는지는 캐릭터가 복제함
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
	if (!IsValid(Parent))
	{
		return;
	}
	// 소켓에 맞춘 뒤 장비 BP가 메시에 지정한 위치·회전(잡는 위치 보정)을 다시 적용함
	// 소켓 맞춤(SnapToTarget)이 상대 위치·회전을 0으로 덮어써서, 따로 적용하지 않으면 BP 보정이 사라짐
	// 크기는 월드 배율 유지(KeepWorld)라 BP 상대 배율이 그대로 남음
	const ADigetherWeaponBase* Defaults = GetClass()->GetDefaultObject<ADigetherWeaponBase>();
	const FTransform GripOffset = IsValid(Defaults) && IsValid(Defaults->WeaponMesh) ? Defaults->WeaponMesh->GetRelativeTransform() : FTransform::Identity;
	WeaponMesh->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, GripSocketName);
	WeaponMesh->SetRelativeLocationAndRotation(GripOffset.GetLocation(), GripOffset.GetRotation());
}

void ADigetherWeaponBase::SetWeaponVisible(bool bVisible)
{
	WeaponMesh->SetVisibility(bVisible);
}
