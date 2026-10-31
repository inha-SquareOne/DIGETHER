#include "Game/Frontend/Lobby/DigetherLobbyPlayerController.h"

#include "CommonActivatableWidget.h"
#include "Engine/World.h"
#include "Game/Frontend/DigetherFrontendSettings.h"
#include "Game/Frontend/Lobby/DigetherLobbyGameMode.h"
#include "Game/Frontend/Lobby/DigetherLobbyGameState.h"
#include "Game/Frontend/Lobby/DigetherLobbyPlayerState.h"
#include "Game/Online/DigetherSessionSubsystem.h"

TSoftClassPtr<UCommonActivatableWidget> ADigetherLobbyPlayerController::GetScreenClass() const
{
	return UDigetherFrontendSettings::Get()->LobbyScreenClass;
}

void ADigetherLobbyPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	if (ADigetherLobbyGameState* LobbyState = GetWorld() ? GetWorld()->GetGameState<ADigetherLobbyGameState>() : nullptr)
	{
		LobbyState->NotifyLobbyChanged();
	}
}

void ADigetherLobbyPlayerController::RequestSetReady(bool bReady)
{
	Server_SetReady(bReady);
}

void ADigetherLobbyPlayerController::Server_SetReady_Implementation(bool bReady)
{
	if (ADigetherLobbyPlayerState* LobbyState = GetPlayerState<ADigetherLobbyPlayerState>())
	{
		LobbyState->SetReady(bReady);
	}
}

void ADigetherLobbyPlayerController::RequestStartGame()
{
	// 호스트는 리슨 서버의 로컬 플레이어이므로 RPC 없이 GameMode에 바로 요청한다.
	if (!HasAuthority() || !IsLocalController()) return;
	if (ADigetherLobbyGameMode* LobbyMode = GetWorld()->GetAuthGameMode<ADigetherLobbyGameMode>())
	{
		LobbyMode->TryStartGame(this);
	}
}

void ADigetherLobbyPlayerController::LobbyReady(bool bReady) { RequestSetReady(bReady); }
void ADigetherLobbyPlayerController::LobbyStart() { RequestStartGame(); }
void ADigetherLobbyPlayerController::LobbyLeave() { RequestLeave(); }

void ADigetherLobbyPlayerController::RequestLeave()
{
	if (UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this))
	{
		Session->LeaveRoom();
	}
}
