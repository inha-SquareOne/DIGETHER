#include "Game/Monster/DigetherMonsterHealthRules.h"

bool DigetherMonsterHealthRules::TryCreateState(const bool bKillable, const float MaxHealth, FDigetherMonsterHealthState& OutState)
{
	OutState = FDigetherMonsterHealthState();
	if (!FMath::IsFinite(MaxHealth) || MaxHealth < 0.f || (bKillable && MaxHealth <= 0.f))
	{
		return false;
	}
	if (!bKillable)
	{
		// 불사는 체력을 복제·피해 판정에 쓰지 않음
		return true;
	}
	OutState.bKillable = true;
	OutState.CurrentHealth = MaxHealth;
	OutState.MaxHealth = MaxHealth;
	return true;
}

EDigetherMonsterDamageResult DigetherMonsterHealthRules::ApplyDamage(FDigetherMonsterHealthState& State, const float Amount)
{
	if (!State.bKillable)
	{
		return EDigetherMonsterDamageResult::IgnoredNotKillable;
	}
	if (State.Life == EDigetherMonsterLife::Dead)
	{
		return EDigetherMonsterDamageResult::IgnoredAlreadyDead;
	}
	if (!FMath::IsFinite(Amount) || Amount <= 0.f)
	{
		return EDigetherMonsterDamageResult::IgnoredInvalidAmount;
	}
	if (State.Life != EDigetherMonsterLife::Alive
		|| !FMath::IsFinite(State.MaxHealth) || State.MaxHealth <= 0.f
		|| !FMath::IsFinite(State.CurrentHealth) || State.CurrentHealth <= 0.f
		|| State.CurrentHealth > State.MaxHealth)
	{
		return EDigetherMonsterDamageResult::IgnoredInvalidState;
	}
	State.CurrentHealth = FMath::Max(0.f, State.CurrentHealth - Amount);
	if (State.CurrentHealth == 0.f)
	{
		State.Life = EDigetherMonsterLife::Dead;
		return EDigetherMonsterDamageResult::Killed;
	}
	return EDigetherMonsterDamageResult::Damaged;
}
