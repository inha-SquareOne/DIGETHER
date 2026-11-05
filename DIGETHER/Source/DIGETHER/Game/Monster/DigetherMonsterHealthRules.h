#pragma once

#include "CoreMinimal.h"
#include "DigetherMonsterHealthRules.generated.h"

UENUM(BlueprintType)
enum class EDigetherMonsterLife : uint8
{
	Alive,
	Dead
};

UENUM(BlueprintType)
enum class EDigetherMonsterDamageResult : uint8
{
	Damaged,
	Killed,
	IgnoredNotKillable,
	IgnoredAlreadyDead,
	IgnoredInvalidAmount,
	IgnoredInvalidState
};

// 정의 로드와 무관하게 같은 체력·생명 상태를 관찰할 수 있도록 한 단위로 전달한다.
USTRUCT()
struct DIGETHER_API FDigetherMonsterHealthState
{
	GENERATED_BODY()

	UPROPERTY()
	bool bKillable = false;

	UPROPERTY()
	float CurrentHealth = 0.f;

	UPROPERTY()
	float MaxHealth = 0.f;

	UPROPERTY()
	EDigetherMonsterLife Life = EDigetherMonsterLife::Alive;
};

namespace DigetherMonsterHealthRules
{
	// 실패해도 호출자가 이전의 처치 가능 상태를 쓰지 않도록 안전한 불사 상태를 돌려준다.
	DIGETHER_API bool TryCreateState(bool bKillable, float MaxHealth, FDigetherMonsterHealthState& OutState);

	// 월드·Actor·시간에 의존하지 않으며 회복이나 부활 전이는 제공하지 않는다.
	DIGETHER_API EDigetherMonsterDamageResult ApplyDamage(FDigetherMonsterHealthState& State, float Amount);
}
