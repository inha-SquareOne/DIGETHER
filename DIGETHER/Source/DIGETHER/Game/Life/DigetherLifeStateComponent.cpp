#include "Game/Life/DigetherLifeStateComponent.h"

#include "Game/Life/DigetherLifeLog.h"
#include "Game/Life/DigetherLifeRules.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UDigetherLifeStateComponent::UDigetherLifeStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDigetherLifeStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UDigetherLifeStateComponent, PublicState, COND_None);
	DOREPLIFETIME_CONDITION(UDigetherLifeStateComponent, PrivateState, COND_OwnerOnly);
}

FDigetherLifeComponentState UDigetherLifeStateComponent::CaptureState() const
{
	FDigetherLifeComponentState State;
	State.PublicState = PublicState;
	State.PrivateState = PrivateState;
	return State;
}

bool UDigetherLifeStateComponent::HasAuthorityOwner() const
{
	const AActor* OwnerActor = GetOwner();
	return IsValid(OwnerActor) && OwnerActor->HasAuthority();
}

EDigetherLifeResult UDigetherLifeStateComponent::RestoreState(const FDigetherLifeComponentState& State,
	const bool bNotify)
{
	if (!HasAuthorityOwner())
	{
		return EDigetherLifeResult::NotAuthority;
	}
	if (!FDigetherLifeRules::IsValidState(State))
	{
		UE_LOG(LogDigetherLife, Verbose, TEXT("생존 상태의 형태가 잘못되어 복원을 거절했다."));
		return EDigetherLifeResult::InvalidInput;
	}

	PublicState = State.PublicState;
	PrivateState = State.PrivateState;
	if (bNotify)
	{
		NotifyStateChanged();
	}
	return EDigetherLifeResult::Success;
}

EDigetherLifeResult UDigetherLifeStateComponent::ApplyUpdate(const FDigetherLifeUpdateResult& Update,
	const bool bNotify)
{
	if (!HasAuthorityOwner())
	{
		return EDigetherLifeResult::NotAuthority;
	}
	if (Update.Result != EDigetherLifeResult::Success)
	{
		switch (Update.Result)
		{
		case EDigetherLifeResult::InvalidInput:
		case EDigetherLifeResult::NotActive:
		case EDigetherLifeResult::AlreadyActive:
		case EDigetherLifeResult::NotSpectating:
		case EDigetherLifeResult::NotAuthority:
			return Update.Result;
		default:
			return EDigetherLifeResult::InvalidInput;
		}
	}
	return RestoreState(Update.State, bNotify);
}

EDigetherLifeResult UDigetherLifeStateComponent::ApplyDeath(const EDigetherDeathCause Cause, const bool bNotify)
{
	return ApplyUpdate(FDigetherLifeRules::ApplyDeath(CaptureState(), Cause), bNotify);
}

EDigetherLifeResult UDigetherLifeStateComponent::ApplyEscape(const EDigetherEscapeKind Kind, const bool bNotify)
{
	return ApplyUpdate(FDigetherLifeRules::ApplyEscape(CaptureState(), Kind), bNotify);
}

EDigetherLifeResult UDigetherLifeStateComponent::Revive(const bool bNotify)
{
	return ApplyUpdate(FDigetherLifeRules::Revive(CaptureState()), bNotify);
}

EDigetherLifeResult UDigetherLifeStateComponent::SelectSpectateTarget(
	const TArray<FDigetherSpectateCandidate>& Candidates, const FString& SelfPlayerKey,
	const EDigetherSpectateDirection Direction, const bool bNotify)
{
	if (!HasAuthorityOwner())
	{
		return EDigetherLifeResult::NotAuthority;
	}
	FDigetherLifeComponentState State = CaptureState();
	if (!FDigetherLifeRules::IsValidState(State))
	{
		return EDigetherLifeResult::InvalidInput;
	}
	if (!State.PublicState.bSpectating)
	{
		return EDigetherLifeResult::NotSpectating;
	}

	const FDigetherSpectateSelectionResult Selection = FDigetherLifeRules::SelectSpectateTarget(Candidates,
		SelfPlayerKey, State.PrivateState.SpectateTargetPlayerKey, Direction);
	if (Selection.Result != EDigetherLifeResult::Success)
	{
		return Selection.Result;
	}
	State.PrivateState.SpectateTargetPlayerKey = Selection.TargetPlayerKey;
	return RestoreState(State, bNotify);
}

EDigetherLifeResult UDigetherLifeStateComponent::NotifyStateChanged()
{
	if (!HasAuthorityOwner())
	{
		return EDigetherLifeResult::NotAuthority;
	}
	const FDigetherLifeComponentState State = CaptureState();
	OnPublicStateChanged.Broadcast(State.PublicState);
	OnPrivateStateChanged.Broadcast(State.PrivateState);
	return EDigetherLifeResult::Success;
}

void UDigetherLifeStateComponent::OnRep_PublicState()
{
	const FDigetherLifePublicState State = PublicState;
	OnPublicStateChanged.Broadcast(State);
}

void UDigetherLifeStateComponent::OnRep_PrivateState()
{
	const FDigetherLifePrivateState State = PrivateState;
	OnPrivateStateChanged.Broadcast(State);
}
