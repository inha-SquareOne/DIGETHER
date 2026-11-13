#include "Game/Player/DigetherStaminaRules.h"

#include "Game/Data/DigetherPlayerStatRow.h"

FDigetherStaminaParams FDigetherStaminaParams::FromRow(const FDigetherPlayerStatRow& Row)
{
	FDigetherStaminaParams Params;
	Params.MaxStamina = Row.MaxStamina;
	Params.SprintDrainPerSec = Row.SprintDrainPerSec;
	Params.JumpStaminaCost = Row.JumpStaminaCost;
	Params.StaminaRegenPerSec = Row.StaminaRegenPerSec;
	Params.StaminaRegenDelaySec = Row.StaminaRegenDelaySec;
	Params.SprintResumeThreshold = Row.SprintResumeThreshold;
	return Params;
}

FDigetherStaminaState FDigetherStaminaRules::MakeFull(const FDigetherStaminaParams& Params)
{
	FDigetherStaminaState State;
	State.Current = Params.MaxStamina;
	return State;
}

bool FDigetherStaminaRules::CanSprint(const FDigetherStaminaState& State)
{
	return !State.bSprintLocked && State.Current > 0.f;
}

bool FDigetherStaminaRules::CanJump(const FDigetherStaminaState& State, const FDigetherStaminaParams& Params)
{
	return State.Current >= Params.JumpStaminaCost;
}

bool FDigetherStaminaRules::TryConsumeJump(FDigetherStaminaState& State, const FDigetherStaminaParams& Params)
{
	if (!CanJump(State, Params))
	{
		return false;
	}
	State.Current = FMath::Max(0.f, State.Current - Params.JumpStaminaCost);
	State.SecondsSinceUse = 0.f;
	return true;
}

bool FDigetherStaminaRules::Advance(FDigetherStaminaState& State, const FDigetherStaminaParams& Params, float DeltaSeconds, bool bSprinting)
{
	if (!(DeltaSeconds > 0.f))
	{
		return true;
	}

	if (bSprinting && CanSprint(State))
	{
		State.SecondsSinceUse = 0.f;
		State.Current = FMath::Max(0.f, State.Current - Params.SprintDrainPerSec * DeltaSeconds);
		if (State.Current <= 0.f)
		{
			State.bSprintLocked = true;
			return false;
		}
		return true;
	}

	// 회복: 대기 시간이 지난 뒤의 시간분만 회복한다(DeltaSeconds가 커도 대기 시간을 건너뛰지 않는다)
	const float PreviousWait = State.SecondsSinceUse;
	State.SecondsSinceUse += DeltaSeconds;
	const float RegenSeconds = FMath::Clamp(State.SecondsSinceUse - FMath::Max(PreviousWait, Params.StaminaRegenDelaySec), 0.f, DeltaSeconds);
	State.Current = FMath::Min(Params.MaxStamina, State.Current + Params.StaminaRegenPerSec * RegenSeconds);

	if (State.bSprintLocked && State.Current > 0.f && State.Current >= Params.SprintResumeThreshold)
	{
		State.bSprintLocked = false;
	}

	// 잠금 중이거나 달리기를 원하지 않은 경우, 달리기 요청이 거절되었는지는 호출자가 CanSprint로 판단한다
	return !(bSprinting && !CanSprint(State));
}
