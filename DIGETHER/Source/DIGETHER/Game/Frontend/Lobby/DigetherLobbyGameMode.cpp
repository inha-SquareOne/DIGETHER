#include "Game/Frontend/Lobby/DigetherLobbyGameMode.h"

#include "Game/Frontend/Lobby/DigetherLobbyGameState.h"
#include "Game/Frontend/Lobby/DigetherLobbyPlayerController.h"
#include "Game/Frontend/Lobby/DigetherLobbyPlayerState.h"
#include "Game/Online/DigetherJoinRules.h"
#include "Game/Online/DigetherRoomProtocol.h"
#include "Game/Online/DigetherSessionSubsystem.h"
#include "Game/Online/DigetherSessionText.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "OnlineSubsystem.h"

ADigetherLobbyGameMode::ADigetherLobbyGameMode()
{
	GameStateClass = ADigetherLobbyGameState::StaticClass();
	PlayerStateClass = ADigetherLobbyPlayerState::StaticClass();
	PlayerControllerClass = ADigetherLobbyPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	// 대기실과 게임의 컨트롤러·상태 클래스가 달라 다시 접속시키는 이동을 쓴다.
	bUseSeamlessTravel = false;
}

void ADigetherLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();
	const UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this);
	if (ADigetherLobbyGameState* LobbyState = GetGameState<ADigetherLobbyGameState>())
	{
		LobbyState->SetRoomInfo(Session ? Session->GetActiveRoomName() : FString(), Session ? Session->GetActiveRoomCode() : FString());
	}
}

void ADigetherLobbyGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}
	const IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	DigetherJoinRules::FDigetherJoinCheck Check;
	Check.bIdValid = UniqueId.IsValid();
	Check.bRequireSteamId = Subsystem && Subsystem->GetSubsystemName() == STEAM_SUBSYSTEM;
	Check.bIdIsSteam = Check.bIdValid && UniqueId.GetType() == STEAM_SUBSYSTEM;
	Check.ConnectedPlayers = GetNumPlayers();
	Check.MaxPlayers = DigetherRoomProtocol::MaxPlayers;
	Check.bNewJoinLocked = false;
	Check.bIdInRoster = false;
	if (Check.bIdValid && IsValid(GameState))
	{
		// PlayerArray에 아직 없는 동시 접속 대기 연결은 이 비교로 검출하지 못한다.
		for (const APlayerState* Player : GameState->PlayerArray)
		{
			if (!IsValid(Player))
			{
				continue;
			}
			const FUniqueNetIdRepl& ConnectedId = Player->GetUniqueId();
			if (ConnectedId.IsValid() && ConnectedId == UniqueId)
			{
				Check.bIdAlreadyConnected = true;
				break;
			}
		}
	}
	const DigetherJoinRules::EDigetherJoinDecision Decision = DigetherJoinRules::Evaluate(Check);
	if (!DigetherJoinRules::IsAccepted(Decision))
	{
		// 거절 사유는 참가자 쪽 연결 실패 안내로 전달된다.
		ErrorMessage = DigetherSessionText::Get(DigetherJoinRules::GetRejectionText(Decision, Check)).ToString();
	}
}

void ADigetherLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	if (ADigetherLobbyPlayerState* LobbyState = NewPlayer ? NewPlayer->GetPlayerState<ADigetherLobbyPlayerState>() : nullptr)
	{
		// 리슨 서버에서 로컬 컨트롤러는 호스트뿐이다.
		LobbyState->SetHost(NewPlayer->IsLocalController());
		LobbyState->SetReady(NewPlayer->IsLocalController());
	}
}

void ADigetherLobbyGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// 대기실에는 조종할 캐릭터가 없으므로 Pawn을 만들지 않는다.
}

void ADigetherLobbyGameMode::TryStartGame(APlayerController* Requester)
{
	const ADigetherLobbyPlayerState* RequesterState = Requester ? Requester->GetPlayerState<ADigetherLobbyPlayerState>() : nullptr;
	const ADigetherLobbyGameState* LobbyState = GetGameState<ADigetherLobbyGameState>();
	UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this);
	// 화면 버튼은 이미 같은 판정으로 막혀 있다. 콘솔 명령 등 다른 경로를 위해 서버에서 다시 확인한다.
	if (!RequesterState || !RequesterState->IsHost() || !LobbyState || !LobbyState->AreGuestsReady() || !Session)
	{
		return;
	}
	Session->StartGame();
}
