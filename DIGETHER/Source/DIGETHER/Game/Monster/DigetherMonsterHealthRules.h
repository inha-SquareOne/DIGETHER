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

// 정의 로드와 무관하게 체력·생명 상태를 함께 관찰하도록 한 단위로 전달
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
	// 실패 시에도 OutState는 안전한 불사 상태, 이전 처치 가능 상태 잔존 방지
	DIGETHER_API bool TryCreateState(bool bKillable, float MaxHealth, FDigetherMonsterHealthState& OutState);

	// 월드·Actor·시간 비의존, 회복·부활 전이 없음
	DIGETHER_API EDigetherMonsterDamageResult ApplyDamage(FDigetherMonsterHealthState& State, float Amount);
}
