#include "Game/Framework/DigetherGameplayGameState.h"

#include "Game/Economy/DigetherQuotaStateComponent.h"
#include "Game/Run/DigetherRunPublicStateComponent.h"

ADigetherGameplayGameState::ADigetherGameplayGameState()
{
	bReplicates = true;
	QuotaComponent = CreateDefaultSubobject<UDigetherQuotaStateComponent>(TEXT("QuotaComponent"));
	RunPublicStateComponent = CreateDefaultSubobject<UDigetherRunPublicStateComponent>(TEXT("RunPublicStateComponent"));
}

UDigetherRunPublicStateComponent* ADigetherGameplayGameState::GetRunPublicStateComponent() const
{
	return RunPublicStateComponent.Get();
}

UDigetherQuotaStateComponent* ADigetherGameplayGameState::GetQuotaComponent() const
{
	return QuotaComponent.Get();
}
