#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "DigetherMeleeRules.generated.h"

// 근접 공격 한 종류의 판정 값, 장비 BP(BP_Weapon_*)가 갖고 판정 컴포넌트에 넘긴다
// 히트박스는 시선(눈 위치·조준 방향) 앞에 놓는 상자이며, 몽타주의 판정 구간(노티파이) 동안만 존재한다
// 수치는 장비마다 BP에서 정하고 코드 기본값은 0이다(0이면 판정하지 않는다)
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherMeleeHitParams
{
	GENERATED_BODY()

	FDigetherMeleeHitParams();

	// 한 번 맞혔을 때 피해량, 판정 구간의 DamageScale을 곱한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Melee", meta=(ClampMin="0"))
	float Damage = 0.f;

	// 눈 위치에서 상자 앞면까지 거리(cm), 0이면 눈 바로 앞에서 시작한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Melee", meta=(ClampMin="0"))
	float ForwardOffsetCm = 0.f;

	// 상자 앞뒤 길이(cm), 사거리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Melee", meta=(ClampMin="0"))
	float RangeCm = 0.f;

	// 상자 좌우 반폭(cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Melee", meta=(ClampMin="0"))
	float HalfWidthCm = 0.f;

	// 상자 상하 반높이(cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Melee", meta=(ClampMin="0"))
	float HalfHeightCm = 0.f;

	// 켜면 눈에서 대상까지 벽 등이 가로막을 때 맞히지 않는다(Visibility 채널)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Melee")
	bool bRequireLineOfSight = true;

	// 판정할 대상의 오브젝트 종류, 기본은 Pawn(플레이어·몬스터)과 PhysicsBody
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Melee")
	TArray<TEnumAsByte<ECollisionChannel>> TargetObjectTypes;
};

// 계산된 히트박스(월드 좌표)
struct DIGETHER_API FDigetherMeleeHitBox
{
	FVector Center = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	FVector HalfExtent = FVector::ZeroVector;
};

// 근접 판정 순수 규칙, 월드·액터에 의존하지 않는다
namespace DigetherMeleeRules
{
	// 판정에 쓸 수 있는 값인지(피해·크기가 양수, 대상 종류가 있음), 문제를 OutProblems에 모은다
	DIGETHER_API bool IsUsable(const FDigetherMeleeHitParams& Params, TArray<FString>* OutProblems = nullptr);

	// 시선 위치·방향 앞의 상자, 상자 X축이 시선 방향이다
	DIGETHER_API FDigetherMeleeHitBox ComputeHitBox(const FVector& ViewLocation, const FRotator& ViewRotation, const FDigetherMeleeHitParams& Params);

	// 판정 구간 배율을 곱한 피해량, 음수·NaN 배율은 0으로 본다
	DIGETHER_API float ComputeDamage(const FDigetherMeleeHitParams& Params, float DamageScale);
}
