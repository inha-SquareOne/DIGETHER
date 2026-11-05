#include "Game/Frontend/Lobby/DigetherLobbyPlayerState.h"

#include "Engine/World.h"
#include "Game/Frontend/Lobby/DigetherLobbyGameState.h"
#include "Net/UnrealNetwork.h"

void ADigetherLobbyPlayerState::SetReady(bool bInReady)
{
	if (!HasAuthority() || bReady == bInReady) return;
	bReady = bInReady;
	ForceNetUpdate();
	NotifyLobbyChanged();
}

void ADigetherLobbyPlayerState::SetHost(bool bInHost)
{
	if (!HasAuthority() || bHost == bInHost) return;
	bHost = bInHost;
	ForceNetUpdate();
	NotifyLobbyChanged();
}

void ADigetherLobbyPlayerState::OnRep_PlayerName()
{
	Super::OnRep_PlayerName();
	NotifyLobbyChanged();
}

void ADigetherLobbyPlayerState::OnRep_LobbyStatus()
{
	NotifyLobbyChanged();
}

void ADigetherLobbyPlayerState::NotifyLobbyChanged() const
{
	const UWorld* World = GetWorld();
	if (ADigetherLobbyGameState* LobbyState = World ? World->GetGameState<ADigetherLobbyGameState>() : nullptr)
	{
		LobbyState->NotifyLobbyChanged();
	}
}

void ADigetherLobbyPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADigetherLobbyPlayerState, bReady);
	DOREPLIFETIME(ADigetherLobbyPlayerState, bHost);
}
