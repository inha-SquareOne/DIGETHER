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
	// 호스트는 리슨 서버 로컬 플레이어라 RPC 없이 GameMode에 직접 요청
	if (!HasAuthority() || !IsLocalController()) return;
	if (ADigetherLobbyGameMode* LobbyMode = GetWorld()->GetAuthGameMode<ADigetherLobbyGameMode>())
	{
		LobbyMode->TryStartGame(this);
	}
}

// UHT 생성 Exec 진입점이 정의를 요구해 함수는 유지하고 본문만 Shipping에서 제외
void ADigetherLobbyPlayerController::LobbyReady(bool bReady)
{
#if !UE_BUILD_SHIPPING
	RequestSetReady(bReady);
#endif
}

void ADigetherLobbyPlayerController::LobbyStart()
{
#if !UE_BUILD_SHIPPING
	RequestStartGame();
#endif
}

void ADigetherLobbyPlayerController::LobbyLeave()
{
#if !UE_BUILD_SHIPPING
	RequestLeave();
#endif
}

void ADigetherLobbyPlayerController::RequestLeave()
{
	if (UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this))
	{
		Session->LeaveRoom();
	}
}
