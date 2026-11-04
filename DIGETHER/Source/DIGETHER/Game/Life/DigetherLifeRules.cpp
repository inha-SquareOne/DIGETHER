#include "Game/Life/DigetherLifeRules.h"

namespace
{
	bool IsValidDeathCause(const EDigetherDeathCause Cause)
	{
		return Cause == EDigetherDeathCause::None || Cause == EDigetherDeathCause::Killed
			|| FDigetherLifeRules::IsMissingCause(Cause);
	}

	bool IsValidEscapeKind(const EDigetherEscapeKind Kind)
	{
		return Kind == EDigetherEscapeKind::None || Kind == EDigetherEscapeKind::Elevator
			|| Kind == EDigetherEscapeKind::Emergency;
	}

	bool IsFiniteLocation(const FVector& Location)
	{
		return FMath::IsFinite(Location.X) && FMath::IsFinite(Location.Y) && FMath::IsFinite(Location.Z);
	}
}

bool FDigetherLifeRules::IsValidLifeState(const EDigetherLifeState Life)
{
	return Life == EDigetherLifeState::Active || Life == EDigetherLifeState::Dead || Life == EDigetherLifeState::Escaped;
}

bool FDigetherLifeRules::IsValidPlayerKey(const FString& PlayerKey)
{
	// 인증 방식의 문자열 형식을 이 도메인에서 재정의하지 않고 빈 식별자만 거른다.
	return !PlayerKey.TrimStartAndEnd().IsEmpty();
}

bool FDigetherLifeRules::IsMissingCause(const EDigetherDeathCause Cause)
{
	return Cause == EDigetherDeathCause::CollapseTimeExpired
		|| Cause == EDigetherDeathCause::CollapseRemainedAfterFinalAscent
		|| Cause == EDigetherDeathCause::CollapseEjected;
}

bool FDigetherLifeRules::IsValidState(const FDigetherLifeComponentState& State)
{
	if (!IsValidLifeState(State.PublicState.Life) || !IsValidDeathCause(State.PrivateState.DeathCause)
		|| !IsValidEscapeKind(State.PrivateState.EscapeKind)
		|| (!State.PrivateState.SpectateTargetPlayerKey.IsEmpty()
			&& !IsValidPlayerKey(State.PrivateState.SpectateTargetPlayerKey)))
	{
		return false;
	}

	switch (State.PublicState.Life)
	{
	case EDigetherLifeState::Active:
		return !State.PublicState.bSpectating && State.PrivateState.DeathCause == EDigetherDeathCause::None
			&& State.PrivateState.EscapeKind == EDigetherEscapeKind::None && State.PrivateState.SpectateTargetPlayerKey.IsEmpty();
	case EDigetherLifeState::Dead:
		return State.PublicState.bSpectating && State.PrivateState.DeathCause != EDigetherDeathCause::None
			&& State.PrivateState.EscapeKind == EDigetherEscapeKind::None;
	case EDigetherLifeState::Escaped:
		return State.PublicState.bSpectating && State.PrivateState.DeathCause == EDigetherDeathCause::None
			&& State.PrivateState.EscapeKind != EDigetherEscapeKind::None;
	default:
		return false;
	}
}

FDigetherLifeUpdateResult FDigetherLifeRules::ApplyDeath(const FDigetherLifeComponentState& State,
	const EDigetherDeathCause Cause)
{
	FDigetherLifeUpdateResult Update;
	Update.State = State;
	if (!IsValidState(State) || !IsValidDeathCause(Cause) || Cause == EDigetherDeathCause::None)
	{
		return Update;
	}
	if (State.PublicState.Life != EDigetherLifeState::Active)
	{
		Update.Result = EDigetherLifeResult::NotActive;
		return Update;
	}

	Update.State.PublicState.Life = EDigetherLifeState::Dead;
	Update.State.PublicState.bSpectating = true;
	Update.State.PrivateState.DeathCause = Cause;
	Update.State.PrivateState.EscapeKind = EDigetherEscapeKind::None;
	Update.State.PrivateState.SpectateTargetPlayerKey.Reset();
	Update.Result = EDigetherLifeResult::Success;
	return Update;
}

FDigetherLifeUpdateResult FDigetherLifeRules::ApplyEscape(const FDigetherLifeComponentState& State,
	const EDigetherEscapeKind Kind)
{
	FDigetherLifeUpdateResult Update;
	Update.State = State;
	if (!IsValidState(State) || !IsValidEscapeKind(Kind) || Kind == EDigetherEscapeKind::None)
	{
		return Update;
	}
	if (State.PublicState.Life != EDigetherLifeState::Active)
	{
		Update.Result = EDigetherLifeResult::NotActive;
		return Update;
	}

	// 탈출 확정과 관전 전환을 한 값에서 처리해 중간 상태가 저장되지 않게 한다.
	Update.State.PublicState.Life = EDigetherLifeState::Escaped;
	Update.State.PublicState.bSpectating = true;
	Update.State.PrivateState.DeathCause = EDigetherDeathCause::None;
	Update.State.PrivateState.EscapeKind = Kind;
	Update.State.PrivateState.SpectateTargetPlayerKey.Reset();
	Update.Result = EDigetherLifeResult::Success;
	return Update;
}

FDigetherLifeUpdateResult FDigetherLifeRules::Revive(const FDigetherLifeComponentState& State)
{
	FDigetherLifeUpdateResult Update;
	Update.State = State;
	if (!IsValidState(State))
	{
		return Update;
	}
	if (State.PublicState.Life == EDigetherLifeState::Active)
	{
		Update.Result = EDigetherLifeResult::AlreadyActive;
		return Update;
	}

	Update.State = FDigetherLifeComponentState();
	Update.Result = EDigetherLifeResult::Success;
	return Update;
}

FDigetherDeathDropPlan FDigetherLifeRules::PlanDeathDrop(const TArray<FDigetherLifeDropItem>& Items,
	const FVector& PlayerLocation, const TOptional<FVector>& DropLocation)
{
	FDigetherDeathDropPlan Plan;
	const FVector& SelectedLocation = DropLocation.IsSet() ? DropLocation.GetValue() : PlayerLocation;
	if (!IsFiniteLocation(SelectedLocation))
	{
		return Plan;
	}

	TSet<FGuid> InstanceIds;
	for (const FDigetherLifeDropItem& Item : Items)
	{
		if (!Item.InstanceId.IsValid() || InstanceIds.Contains(Item.InstanceId))
		{
			return Plan;
		}
		InstanceIds.Add(Item.InstanceId);
	}

	Plan.InstanceIds.Reserve(Items.Num());
	for (const FDigetherLifeDropItem& Item : Items)
	{
		// 히든도 같은 소지품 실체이므로 포함한다. 잔고 손실은 경제 도메인의 별도 명령이 담당한다.
		Plan.InstanceIds.Add(Item.InstanceId);
	}
	Plan.Location = SelectedLocation;
	Plan.Result = EDigetherLifeResult::Success;
	return Plan;
}

FDigetherSpectateSelectionResult FDigetherLifeRules::SelectSpectateTarget(
	const TArray<FDigetherSpectateCandidate>& Candidates, const FString& SelfPlayerKey,
	const FString& CurrentTargetPlayerKey, const EDigetherSpectateDirection Direction)
{
	FDigetherSpectateSelectionResult Selection;
	if (!IsValidPlayerKey(SelfPlayerKey)
		|| (!CurrentTargetPlayerKey.IsEmpty() && !IsValidPlayerKey(CurrentTargetPlayerKey))
		|| (Direction != EDigetherSpectateDirection::Next && Direction != EDigetherSpectateDirection::Previous))
	{
		return Selection;
	}

	TSet<FString> PlayerKeys;
	int32 CurrentIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		const FDigetherSpectateCandidate& Candidate = Candidates[Index];
		if (!IsValidPlayerKey(Candidate.PlayerKey) || !IsValidLifeState(Candidate.Life)
			|| PlayerKeys.Contains(Candidate.PlayerKey))
		{
			return Selection;
		}
		PlayerKeys.Add(Candidate.PlayerKey);
		if (Candidate.PlayerKey == CurrentTargetPlayerKey)
		{
			CurrentIndex = Index;
		}
	}

	Selection.Result = EDigetherLifeResult::Success;
	const int32 CandidateCount = Candidates.Num();
	const int32 Step = Direction == EDigetherSpectateDirection::Next ? 1 : -1;
	// 현 대상이 사망해도 원래 순서를 기준으로 한다. 후보를 먼저 거르면 이 위치를 잃는다.
	const int32 StartIndex = CurrentIndex != INDEX_NONE ? CurrentIndex
		: (Direction == EDigetherSpectateDirection::Next ? CandidateCount - 1 : 0);
	for (int32 Offset = 1; Offset <= CandidateCount; ++Offset)
	{
		const int32 Index = static_cast<int32>((static_cast<int64>(StartIndex)
			+ static_cast<int64>(Step) * Offset + CandidateCount) % CandidateCount);
		const FDigetherSpectateCandidate& Candidate = Candidates[Index];
		if (Candidate.Life == EDigetherLifeState::Active && Candidate.PlayerKey != SelfPlayerKey)
		{
			Selection.TargetPlayerKey = Candidate.PlayerKey;
			break;
		}
	}
	return Selection;
}

EDigetherEmergencyEscapeResult FDigetherLifeRules::CheckEmergencyEscape(const EDigetherLifeState Life,
	const TArray<FDigetherEmergencyCarriedItem>& Items, const float EmergencyWeightLimitKg)
{
	if (!IsValidLifeState(Life) || !FMath::IsFinite(EmergencyWeightLimitKg) || EmergencyWeightLimitKg < 0.f)
	{
		return EDigetherEmergencyEscapeResult::InvalidInput;
	}

	double EquipmentWeightKg = 0.0;
	bool bCarryingRecoverable = false;
	for (const FDigetherEmergencyCarriedItem& Item : Items)
	{
		if (!FMath::IsFinite(Item.WeightKg) || Item.WeightKg < 0.f)
		{
			return EDigetherEmergencyEscapeResult::InvalidInput;
		}
		EquipmentWeightKg += Item.WeightKg;
		bCarryingRecoverable |= Item.bRecoverable;
	}
	if (!FMath::IsFinite(EquipmentWeightKg))
	{
		return EDigetherEmergencyEscapeResult::InvalidInput;
	}
	if (Life != EDigetherLifeState::Active)
	{
		return EDigetherEmergencyEscapeResult::NotActive;
	}
	if (bCarryingRecoverable)
	{
		return EDigetherEmergencyEscapeResult::CarryingRecoverable;
	}
	return EquipmentWeightKg <= EmergencyWeightLimitKg
		? EDigetherEmergencyEscapeResult::Allowed : EDigetherEmergencyEscapeResult::Overweight;
}
