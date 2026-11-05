#include "Game/Frontend/Lobby/DigetherLobbyGameMode.h"

#include "Engine/World.h"
#include "Game/Frontend/Lobby/DigetherLobbyGameState.h"
#include "Game/Frontend/Lobby/DigetherLobbyPlayerController.h"
#include "Game/Frontend/Lobby/DigetherLobbyPlayerState.h"
#include "Game/Online/DigetherJoinRules.h"
#include "Game/Online/DigetherRoomProtocol.h"
#include "Game/Online/DigetherSessionSubsystem.h"
#include "Game/Online/DigetherSessionText.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "OnlineSubsystem.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDigetherLobby, Log, All);

namespace
{
	// 같은 계정 교체 대기의 정리 확인 주기. 제한 시간(DigetherJoinRules::LoginReplacementTimeoutSeconds)보다 충분히 짧게 둔다.
	constexpr float LobbyLoginReplacementPollSeconds = 0.1f;
}

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

void ADigetherLobbyGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld() != nullptr)
	{
		GetWorldTimerManager().ClearTimer(LoginReplacementTimer);
	}
	// PreLoginAsync 계약상 남은 교체 대기의 완료 콜백도 부른 뒤 끝낸다.
	const FString DuplicateMessage = DigetherJoinRules::MakeRejectionMessage(EDigetherSessionText::AccountAlreadyConnected);
	for (const FDigetherLoginReplacementQueue::FEntry& Entry : LoginReplacements.TakeAll())
	{
		Entry.OnComplete.ExecuteIfBound(DuplicateMessage);
	}
	Super::EndPlay(EndPlayReason);
}

void ADigetherLobbyGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}
	EvaluateAdmission(UniqueId, ErrorMessage, nullptr);
}

void ADigetherLobbyGameMode::PreLoginAsync(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId,
	const FOnPreLoginCompleteDelegate& OnComplete)
{
	// 엔진 계약(GameModeBase.h): PreLogin 검사와 GameModePreLoginEvent 방송(Super::PreLogin)을 함께 하고 OnComplete를 반드시 부른다.
	FString ErrorMessage;
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	DigetherJoinRules::EDigetherJoinDecision Decision = DigetherJoinRules::EDigetherJoinDecision::RejectInvalidId;
	APlayerController* Previous = nullptr;
	if (ErrorMessage.IsEmpty())
	{
		Decision = EvaluateAdmission(UniqueId, ErrorMessage, &Previous);
	}
	const UWorld* World = GetWorld();
	if (!DigetherJoinRules::ShouldDisconnectPreviousConnection(Decision) || World == nullptr || World->bIsTearingDown)
	{
		OnComplete.ExecuteIfBound(ErrorMessage);
		return;
	}
	// 기획 C2(2026-11-04 확정): 이전 접속을 끊고 새 접속을 받는다. 이전 연결의 로그아웃(세션 등록 해제·PlayerState 제거)이
	// 끝난 뒤 승인해야 같은 ID의 PlayerState가 둘이 되거나 늦은 등록 해제가 새 등록을 지우지 않는다.
	FDigetherLoginReplacementQueue::FEntry Entry;
	Entry.UniqueId = UniqueId;
	Entry.Previous = Previous;
	Entry.OnComplete = OnComplete;
	Entry.StartSeconds = FPlatformTime::Seconds();
	TArray<FDigetherLoginReplacementQueue::FEntry> Superseded;
	LoginReplacements.Add(MoveTemp(Entry), Superseded);
	const FString DuplicateMessage = DigetherJoinRules::MakeRejectionMessage(EDigetherSessionText::AccountAlreadyConnected);
	for (const FDigetherLoginReplacementQueue::FEntry& Older : Superseded)
	{
		Older.OnComplete.ExecuteIfBound(DuplicateMessage);
	}
	if (IsValid(Previous) && GameSession)
	{
		// 대기실에는 Pawn과 비활성 PlayerState 재접속 계약이 없어 엔진 KickPlayer를 그대로 쓴다. 새 접속의 PlayerState는
		// 새로 만들어지므로 준비 상태도 초기화된다. KickPlayer는 원격 연결 컨트롤러만 처리하고 PC를 바로 파괴한다.
		const bool bKicked = GameSession->KickPlayer(Previous, FText::GetEmpty());
		UE_LOG(LogDigetherLobby, Log, TEXT("Duplicate login %s: previous connection %s"),
			*UniqueId.ToDebugString(), bKicked ? TEXT("kicked") : TEXT("not kicked"));
	}
	PollLoginReplacements();
	if (!LoginReplacements.IsEmpty() && !GetWorldTimerManager().IsTimerActive(LoginReplacementTimer))
	{
		GetWorldTimerManager().SetTimer(LoginReplacementTimer, this, &ThisClass::PollLoginReplacements,
			LobbyLoginReplacementPollSeconds, true);
	}
}

void ADigetherLobbyGameMode::PollLoginReplacements()
{
	TArray<FDigetherLoginReplacementQueue::FEntry> Ready;
	TArray<FDigetherLoginReplacementQueue::FEntry> TimedOut;
	LoginReplacements.Collect(FPlatformTime::Seconds(), DigetherJoinRules::LoginReplacementTimeoutSeconds,
		[this](const FDigetherLoginReplacementQueue::FEntry& Entry)
		{
			return IsPreviousConnectionReleased(Entry.UniqueId, Entry.Previous.Get());
		}, Ready, TimedOut);
	if (LoginReplacements.IsEmpty() && GetWorld() != nullptr)
	{
		GetWorldTimerManager().ClearTimer(LoginReplacementTimer);
	}
	for (const FDigetherLoginReplacementQueue::FEntry& Entry : Ready)
	{
		// 정리 뒤 현재 인원으로 다시 판정한다. 그 사이 같은 ID가 또 들어왔으면 교체를 반복하지 않고 ②로 거절한다.
		FString ErrorMessage;
		EvaluateAdmission(Entry.UniqueId, ErrorMessage, nullptr);
		Entry.OnComplete.ExecuteIfBound(ErrorMessage);
	}
	const FString DuplicateMessage = DigetherJoinRules::MakeRejectionMessage(EDigetherSessionText::AccountAlreadyConnected);
	for (const FDigetherLoginReplacementQueue::FEntry& Entry : TimedOut)
	{
		// 기획 C4: ②는 이전 접속을 끊지 못한 경우에만 쓴다.
		UE_LOG(LogDigetherLobby, Warning, TEXT("Duplicate login %s: previous connection not released"), *Entry.UniqueId.ToDebugString());
		Entry.OnComplete.ExecuteIfBound(DuplicateMessage);
	}
}

bool ADigetherLobbyGameMode::IsPreviousConnectionReleased(const FUniqueNetIdRepl& UniqueId, const APlayerController* Previous) const
{
	if (IsValid(Previous) || !UniqueId.IsValid())
	{
		return false;
	}
	if (IsValid(GameState))
	{
		for (const APlayerState* Player : GameState->PlayerArray)
		{
			if (IsValid(Player) && Player->GetUniqueId().IsValid() && Player->GetUniqueId() == UniqueId)
			{
				return false;
			}
		}
	}
	return true;
}

DigetherJoinRules::EDigetherJoinDecision ADigetherLobbyGameMode::EvaluateAdmission(const FUniqueNetIdRepl& UniqueId,
	FString& ErrorMessage, APlayerController** OutPreviousToReplace)
{
	if (OutPreviousToReplace != nullptr)
	{
		*OutPreviousToReplace = nullptr;
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
	APlayerController* PreviousController = nullptr;
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
				PreviousController = Player->GetPlayerController();
				break;
			}
		}
	}
	// 호스트 자신(리슨 서버의 로컬 컨트롤러)과 같은 ID면 끊지 않고 ②로 거절한다.
	Check.bDuplicateIsLocal = IsValid(PreviousController) && PreviousController->IsLocalController();
	const DigetherJoinRules::EDigetherJoinDecision Decision = DigetherJoinRules::Evaluate(Check);
	if (DigetherJoinRules::IsAccepted(Decision))
	{
		return Decision;
	}
	// 참가자가 자기 언어 문구로 바꿔 보이도록 문구 대신 사유 키를 보낸다. 교체 대기는 대기를 끝내지 못했을 때의 ②다.
	ErrorMessage = DigetherJoinRules::MakeRejectionMessage(DigetherJoinRules::GetRejectionText(Decision, Check));
	if (OutPreviousToReplace != nullptr && IsValid(PreviousController)
		&& DigetherJoinRules::ShouldDisconnectPreviousConnection(Decision))
	{
		*OutPreviousToReplace = PreviousController;
	}
	return Decision;
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
