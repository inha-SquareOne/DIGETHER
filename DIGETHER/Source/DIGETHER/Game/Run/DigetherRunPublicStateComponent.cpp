#include "Game/Run/DigetherRunPublicStateComponent.h"

#include "Game/Run/DigetherRunRules.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UDigetherRunPublicStateComponent::UDigetherRunPublicStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDigetherRunPublicStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UDigetherRunPublicStateComponent, PublicState, COND_None);
}

FDigetherRunPublicState UDigetherRunPublicStateComponent::CaptureState() const
{
	return PublicState;
}

EDigetherRunResult UDigetherRunPublicStateComponent::RestoreState(
	const FDigetherRunPublicState& State, const bool bNotify)
{
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return EDigetherRunResult::NotAuthority;
	}
	if (!FDigetherRunRules::IsValidPublicState(State))
	{
		return EDigetherRunResult::InvalidInput;
	}
	PublicState = State;
	if (bNotify)
	{
		NotifyStateChanged();
	}
	return EDigetherRunResult::Success;
}

EDigetherRunResult UDigetherRunPublicStateComponent::NotifyStateChanged()
{
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return EDigetherRunResult::NotAuthority;
	}
	const FDigetherRunPublicState State = PublicState;
	OnRunPublicStateChanged.Broadcast(State);
	return EDigetherRunResult::Success;
}

void UDigetherRunPublicStateComponent::OnRep_PublicState()
{
	const FDigetherRunPublicState State = PublicState;
	OnRunPublicStateChanged.Broadcast(State);
}
