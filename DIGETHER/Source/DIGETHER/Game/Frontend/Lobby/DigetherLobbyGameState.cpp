#include "Game/Frontend/Lobby/DigetherLobbyGameState.h"

#include "Game/Frontend/Lobby/DigetherLobbyPlayerState.h"
#include "Net/UnrealNetwork.h"

void ADigetherLobbyGameState::SetRoomInfo(const FString& InRoomName, const FString& InRoomCode)
{
	if (!HasAuthority()) return;
	RoomName = InRoomName;
	RoomCode = InRoomCode;
	ForceNetUpdate();
	// 서버는 OnRep 미호출이라 직접 통지
	NotifyLobbyChanged();
}

bool ADigetherLobbyGameState::AreGuestsReady() const
{
	for (const APlayerState* Player : PlayerArray)
	{
		const ADigetherLobbyPlayerState* LobbyPlayer = Cast<ADigetherLobbyPlayerState>(Player);
		if (LobbyPlayer && !LobbyPlayer->IsHost() && !LobbyPlayer->IsReady())
		{
			return false;
		}
	}
	return true;
}

void ADigetherLobbyGameState::NotifyLobbyChanged()
{
	OnLobbyChanged.Broadcast();
}

void ADigetherLobbyGameState::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);
	NotifyLobbyChanged();
}

void ADigetherLobbyGameState::RemovePlayerState(APlayerState* PlayerState)
{
	Super::RemovePlayerState(PlayerState);
	NotifyLobbyChanged();
}

void ADigetherLobbyGameState::OnRep_RoomInfo()
{
	NotifyLobbyChanged();
}

void ADigetherLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADigetherLobbyGameState, RoomName);
	DOREPLIFETIME(ADigetherLobbyGameState, RoomCode);
}
