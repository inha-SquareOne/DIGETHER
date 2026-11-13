#pragma once

#include "CoreMinimal.h"

// 피해 적용 결과, 무시 사유는 로그·테스트 확인용
enum class EDigetherPlayerDamageResult : uint8
{
	Damaged,
	Killed,
	IgnoredDead,
	IgnoredInvalidAmount,
	IgnoredInvalidState
};

// 플레이어 한 명분의 체력 상태, 엔진 월드·시간에 의존하지 않음
struct DIGETHER_API FDigetherPlayerHealthState
{
	float Current = 0.f;
	float Max = 0.f;
	bool bDead = false;
};

// 플레이어 체력 순수 규칙(S03), 최대 체력은 DT_PlayerStats의 MaxHealth(D1 기준 시험값 100)
// 회복 아이템·자연 회복은 기획에 없으므로 두지 않음, 귀환·부활 때만 MakeFull로 초기화함
class DIGETHER_API FDigetherPlayerHealthRules
{
public:
	// 최대 체력으로 가득 찬 상태, 최대치가 유한한 양수가 아니면 false(상태는 기본값)
	static bool TryMakeFull(float MaxHealth, FDigetherPlayerHealthState& OutState);

	// 피해 적용, 무시되는 경우 상태를 바꾸지 않음, 0에 닿으면 사망
	static EDigetherPlayerDamageResult ApplyDamage(FDigetherPlayerHealthState& State, float Amount);

	static bool IsValidState(const FDigetherPlayerHealthState& State);
};
