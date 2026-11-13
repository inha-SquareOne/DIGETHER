#pragma once

#include "CoreMinimal.h"

struct FDigetherPlayerStatRow;

// 스태미나 규칙에 쓰는 수치, DT_PlayerStats 행에서 복사한다(코드 기본값 없음)
struct DIGETHER_API FDigetherStaminaParams
{
	float MaxStamina = 0.f;
	float SprintDrainPerSec = 0.f;
	float JumpStaminaCost = 0.f;
	float StaminaRegenPerSec = 0.f;
	float StaminaRegenDelaySec = 0.f;
	float SprintResumeThreshold = 0.f;

	static FDigetherStaminaParams FromRow(const FDigetherPlayerStatRow& Row);
};

// 스태미나 한 명분의 상태, 엔진 월드·시간에 의존하지 않는다(서버와 소유 클라이언트가 같은 함수로 갱신)
struct DIGETHER_API FDigetherStaminaState
{
	float Current = 0.f;

	// 마지막으로 소모한 뒤 지난 시간(초), 회복 대기 판정에 쓴다
	float SecondsSinceUse = 0.f;

	// 바닥나서 달리기가 잠긴 상태, Current가 SprintResumeThreshold 이상이 되면 풀린다
	bool bSprintLocked = false;
};

// 스태미나 순수 규칙, 모든 함수는 상태를 인자로 받아 새 상태를 돌려준다
class DIGETHER_API FDigetherStaminaRules
{
public:
	// 최대치로 가득 찬 시작 상태
	static FDigetherStaminaState MakeFull(const FDigetherStaminaParams& Params);

	// 달리기를 시작·유지할 수 있는지(잠기지 않았고 스태미나가 남음)
	static bool CanSprint(const FDigetherStaminaState& State);

	// 점프할 수 있는지(스태미나가 점프 소모 이상)
	static bool CanJump(const FDigetherStaminaState& State, const FDigetherStaminaParams& Params);

	// 점프 1회 소모, 부족하면 상태를 바꾸지 않고 false
	static bool TryConsumeJump(FDigetherStaminaState& State, const FDigetherStaminaParams& Params);

	// DeltaSeconds만큼 진행한다. bSprinting이면 소모, 아니면 대기 후 회복
	// 달리기를 계속할 수 없으면 false(소모 중 0에 닿아 잠겼거나, 잠긴 채로 달리기를 요청함). 호출자가 달리기를 종료해야 한다
	static bool Advance(FDigetherStaminaState& State, const FDigetherStaminaParams& Params, float DeltaSeconds, bool bSprinting);
};
