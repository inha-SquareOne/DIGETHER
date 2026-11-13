#include "Game/Player/DigetherPlayerHealthRules.h"

bool FDigetherPlayerHealthRules::TryMakeFull(const float MaxHealth, FDigetherPlayerHealthState& OutState)
{
	OutState = FDigetherPlayerHealthState();
	if (!FMath::IsFinite(MaxHealth) || MaxHealth <= 0.f)
	{
		return false;
	}
	OutState.Current = MaxHealth;
	OutState.Max = MaxHealth;
	return true;
}

bool FDigetherPlayerHealthRules::IsValidState(const FDigetherPlayerHealthState& State)
{
	if (!FMath::IsFinite(State.Max) || State.Max <= 0.f || !FMath::IsFinite(State.Current)
		|| State.Current < 0.f || State.Current > State.Max)
	{
		return false;
	}
	// 사망이면 0, 생존이면 0보다 커야 한다
	return State.bDead ? State.Current == 0.f : State.Current > 0.f;
}

EDigetherPlayerDamageResult FDigetherPlayerHealthRules::ApplyDamage(FDigetherPlayerHealthState& State, const float Amount)
{
	if (State.bDead)
	{
		return EDigetherPlayerDamageResult::IgnoredDead;
	}
	if (!FMath::IsFinite(Amount) || Amount <= 0.f)
	{
		return EDigetherPlayerDamageResult::IgnoredInvalidAmount;
	}
	if (!IsValidState(State))
	{
		return EDigetherPlayerDamageResult::IgnoredInvalidState;
	}
	State.Current = FMath::Max(0.f, State.Current - Amount);
	if (State.Current == 0.f)
	{
		State.bDead = true;
		return EDigetherPlayerDamageResult::Killed;
	}
	return EDigetherPlayerDamageResult::Damaged;
}
