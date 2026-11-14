#include "Game/Framework/DigetherGameplayGameState.h"

#include "Game/Economy/DigetherQuotaStateComponent.h"
#include "Game/Framework/DigetherFrameworkRules.h"
#include "Game/Run/DigetherGameOverPublicStateComponent.h"
#include "Game/Run/DigetherRunPublicStateComponent.h"

ADigetherGameplayGameState::ADigetherGameplayGameState()
{
	bReplicates = true;
	QuotaComponent = CreateDefaultSubobject<UDigetherQuotaStateComponent>(TEXT("QuotaComponent"));
	RunPublicStateComponent = CreateDefaultSubobject<UDigetherRunPublicStateComponent>(TEXT("RunPublicStateComponent"));
	GameOverPublicStateComponent = CreateDefaultSubobject<UDigetherGameOverPublicStateComponent>(
		TEXT("GameOverPublicStateComponent"));
}

UDigetherGameOverPublicStateComponent* ADigetherGameplayGameState::GetGameOverPublicStateComponent() const
{
	return GameOverPublicStateComponent.Get();
}

UDigetherRunPublicStateComponent* ADigetherGameplayGameState::GetRunPublicStateComponent() const
{
	return RunPublicStateComponent.Get();
}

UDigetherQuotaStateComponent* ADigetherGameplayGameState::GetQuotaComponent() const
{
	return QuotaComponent.Get();
}

EDigetherDepartureWarningKind ADigetherGameplayGameState::GetDepartureWarningKind(
	const EDigetherDepartureWarningKind LeverWarning) const
{
	return IsValid(RunPublicStateComponent)
		? DigetherFrameworkRules::ResolveDepartureWarning(LeverWarning, RunPublicStateComponent->CaptureState())
		: LeverWarning;
}
