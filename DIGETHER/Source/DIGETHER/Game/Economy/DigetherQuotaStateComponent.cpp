#include "Game/Economy/DigetherQuotaStateComponent.h"

#include "Game/Economy/DigetherEconomyRules.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UDigetherQuotaStateComponent::UDigetherQuotaStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDigetherQuotaStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UDigetherQuotaStateComponent, PublicState, COND_None);
}

FDigetherQuotaPublicState UDigetherQuotaStateComponent::CaptureState() const
{
	return PublicState;
}

EDigetherEconomyResult UDigetherQuotaStateComponent::RestoreState(const FDigetherQuotaPublicState& State, const bool bNotify)
{
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return EDigetherEconomyResult::NotAuthority;
	}

	if (!FDigetherEconomyRules::IsValidPublicState(State))
	{
		return EDigetherEconomyResult::InvalidInput;
	}

	PublicState = State;
	if (bNotify)
	{
		NotifyStateChanged();
	}
	return EDigetherEconomyResult::Success;
}

EDigetherEconomyResult UDigetherQuotaStateComponent::SetQuotaState(const FDigetherQuotaState& State, const bool bNotify)
{
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return EDigetherEconomyResult::NotAuthority;
	}

	if (!FDigetherEconomyRules::IsValidQuotaState(State))
	{
		return EDigetherEconomyResult::InvalidInput;
	}

	FDigetherQuotaPublicState UpdatedState = PublicState;
	UpdatedState.Quota = State;
	return RestoreState(UpdatedState, bNotify);
}

EDigetherEconomyResult UDigetherQuotaStateComponent::SetHiddenReveals(
	const TArray<FDigetherHiddenRevealEntry>& HiddenReveals, const bool bNotify)
{
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return EDigetherEconomyResult::NotAuthority;
	}

	FDigetherQuotaPublicState UpdatedState = PublicState;
	UpdatedState.HiddenReveals = HiddenReveals;
	return RestoreState(UpdatedState, bNotify);
}

EDigetherEconomyResult UDigetherQuotaStateComponent::NotifyStateChanged()
{
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return EDigetherEconomyResult::NotAuthority;
	}

	const FDigetherQuotaPublicState State = PublicState;
	OnQuotaStateChanged.Broadcast(State);
	return EDigetherEconomyResult::Success;
}

void UDigetherQuotaStateComponent::OnRep_PublicState()
{
	const FDigetherQuotaPublicState State = PublicState;
	OnQuotaStateChanged.Broadcast(State);
}
