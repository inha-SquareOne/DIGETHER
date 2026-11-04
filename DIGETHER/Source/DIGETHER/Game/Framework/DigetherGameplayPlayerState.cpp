#include "Game/Framework/DigetherGameplayPlayerState.h"

#include "Game/Economy/DigetherWalletComponent.h"
#include "Game/Life/DigetherLifeStateComponent.h"

ADigetherGameplayPlayerState::ADigetherGameplayPlayerState()
{
	bReplicates = true;
	WalletComponent = CreateDefaultSubobject<UDigetherWalletComponent>(TEXT("WalletComponent"));
	LifeComponent = CreateDefaultSubobject<UDigetherLifeStateComponent>(TEXT("LifeComponent"));
}

UDigetherWalletComponent* ADigetherGameplayPlayerState::GetWalletComponent() const
{
	return WalletComponent.Get();
}

UDigetherLifeStateComponent* ADigetherGameplayPlayerState::GetLifeComponent() const
{
	return LifeComponent.Get();
}

void ADigetherGameplayPlayerState::InitializeGameplayIdentity()
{
	if (HasAuthority() && !GameplayPlayerId.IsValid())
	{
		GameplayPlayerId = FGuid::NewGuid();
	}
}

FGuid ADigetherGameplayPlayerState::GetGameplayPlayerId() const
{
	return GameplayPlayerId;
}

FString ADigetherGameplayPlayerState::GetGameplayPlayerKey() const
{
	return GameplayPlayerId.IsValid() ? GameplayPlayerId.ToString() : FString();
}

FDigetherWalletState ADigetherGameplayPlayerState::CaptureState() const
{
	return WalletComponent->CaptureState();
}

EDigetherEconomyResult ADigetherGameplayPlayerState::RestoreState(const FDigetherWalletState& State, const bool bNotify)
{
	return WalletComponent->RestoreState(State, bNotify);
}
