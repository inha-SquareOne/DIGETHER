#include "DigetherPrototypeGameState.h"
#include "Net/UnrealNetwork.h"

void ADigetherPrototypeGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADigetherPrototypeGameState, PublicState);
}
