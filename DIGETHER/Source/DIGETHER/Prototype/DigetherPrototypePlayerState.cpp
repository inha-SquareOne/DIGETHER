#include "DigetherPrototypePlayerState.h"
#include "Net/UnrealNetwork.h"

void ADigetherPrototypePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADigetherPrototypePlayerState, PrivateState, COND_OwnerOnly);
	DOREPLIFETIME(ADigetherPrototypePlayerState, PublicLife);
	DOREPLIFETIME(ADigetherPrototypePlayerState, PublicBurialLevel);
	DOREPLIFETIME_CONDITION(ADigetherPrototypePlayerState, FixtureId, COND_OwnerOnly);
}
