#include "Game/Economy/DigetherWalletComponent.h"

#include "Game/Core/DigetherAuthority.h"
#include "Game/Economy/DigetherEconomyRules.h"
#include "Net/UnrealNetwork.h"

UDigetherWalletComponent::UDigetherWalletComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDigetherWalletComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UDigetherWalletComponent, PrivateState, COND_OwnerOnly);
}

FDigetherWalletState UDigetherWalletComponent::CaptureState() const
{
	return PrivateState;
}

EDigetherEconomyResult UDigetherWalletComponent::RestoreState(const FDigetherWalletState& State, const bool bNotify)
{
	if (!DigetherAuthority::HasOwnerAuthority(*this))
	{
		return EDigetherEconomyResult::NotAuthority;
	}

	if (!FDigetherEconomyRules::IsValidWalletState(State))
	{
		return EDigetherEconomyResult::InvalidInput;
	}

	PrivateState = State;
	if (bNotify)
	{
		NotifyStateChanged();
	}
	return EDigetherEconomyResult::Success;
}

EDigetherEconomyResult UDigetherWalletComponent::ApplyUpdate(const FDigetherWalletUpdateResult& Update, const bool bNotify)
{
	if (!DigetherAuthority::HasOwnerAuthority(*this))
	{
		return EDigetherEconomyResult::NotAuthority;
	}

	if (Update.Result != EDigetherEconomyResult::Success)
	{
		return Update.Result;
	}

	return RestoreState(Update.State, bNotify);
}

EDigetherEconomyResult UDigetherWalletComponent::NotifyStateChanged()
{
	if (!DigetherAuthority::HasOwnerAuthority(*this))
	{
		return EDigetherEconomyResult::NotAuthority;
	}

	const FDigetherWalletState State = PrivateState;
	OnWalletChanged.Broadcast(State);
	return EDigetherEconomyResult::Success;
}

void UDigetherWalletComponent::OnRep_PrivateState()
{
	const FDigetherWalletState State = PrivateState;
	OnWalletChanged.Broadcast(State);
}
