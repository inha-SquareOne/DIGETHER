#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DigetherWeaponBase.generated.h"

class UAnimMontage;
class UStaticMeshComponent;

// 손에 드는 장비(삽·곡괭이 등)의 공통 부모. 캐릭터는 이 클래스만 알고 삽 같은 구체 장비는 모른다
// 장비 종류는 이 클래스를 상속한 BP(BP_Weapon_Shovel 등)로 만들고 메시·잡는 위치·공격 몽타주를 거기서 지정한다
// 메시는 하나이고 1인칭·3인칭 화면이 공용으로 쓴다
// 피해·채굴 판정은 이 클래스가 하지 않는다(별도 시스템)
UCLASS(Abstract, Blueprintable)
class DIGETHER_API ADigetherWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	ADigetherWeaponBase();

	// 소유 캐릭터의 몸에 붙인다. 화면이 없는 전용 서버는 붙이지 않아도 된다
	void AttachToCharacter(USceneComponent* Parent);

	// 모든 머신에서 이 장비 모습을 숨기거나 보인다(장비 교체 중 등)
	void SetWeaponVisible(bool bVisible);

	// 공격 몽타주(서버 쿨다운 길이와 재생에 같은 에셋을 쓴다), 없으면 공격할 수 없다
	UAnimMontage* GetAttackMontage() const { return AttackMontage; }

protected:
	// 장비 모습, 본인과 다른 플레이어 모두 이 메시 하나를 본다
	UPROPERTY(VisibleAnywhere, Category="Digether|Weapon")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	// 붙일 몸 소켓 이름, 기본은 드워프 스켈레톤(SK_Ch_Dwarf_Miner_Skeleton)의 오른손 소켓이다
	// 소켓 위치·방향은 스켈레톤 소켓에서 조정한다
	UPROPERTY(EditDefaultsOnly, Category="Digether|Weapon")
	FName GripSocketName = TEXT("hand_r_socket");

	UPROPERTY(EditDefaultsOnly, Category="Digether|Weapon")
	TObjectPtr<UAnimMontage> AttackMontage;
};
