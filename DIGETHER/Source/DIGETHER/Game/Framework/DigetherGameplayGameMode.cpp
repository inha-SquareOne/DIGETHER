#include "Game/Framework/DigetherGameplayGameMode.h"

#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Economy/DigetherQuotaStateComponent.h"
#include "Game/Economy/DigetherWalletComponent.h"
#include "Game/Elevator/DigetherElevatorActor.h"
#include "Game/Elevator/DigetherElevatorComponent.h"
#include "Game/Elevator/DigetherElevatorLeverActor.h"
#include "Game/Framework/DigetherFrameworkDefaults.h"
#include "Game/Framework/DigetherFrameworkLog.h"
#include "Game/Framework/DigetherFrameworkRules.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Framework/DigetherTestPawn.h"
#include "Game/Online/DigetherJoinRules.h"
#include "Game/Online/DigetherRoomProtocol.h"
#include "Game/Online/DigetherSessionText.h"
#include "Game/Run/DigetherGameOverFlowComponent.h"
#include "Game/Run/DigetherRunCoordinatorComponent.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Save/DigetherSaveRules.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystem.h"
#include "TimerManager.h"

namespace
{
	// 같은 계정 교체 대기의 정리 확인 주기. 제한 시간(DigetherJoinRules::LoginReplacementTimeoutSeconds)보다 충분히 짧게 둔다.
	constexpr float LoginReplacementPollSeconds = 0.1f;
}

ADigetherGameplayGameMode::ADigetherGameplayGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	RunCoordinatorComponent = CreateDefaultSubobject<UDigetherRunCoordinatorComponent>(TEXT("RunCoordinatorComponent"));
	// 시간 갱신은 이 연결층의 Tick에서 운행·레버와 함께 한 번만 한다.
	RunCoordinatorComponent->PrimaryComponentTick.bStartWithTickEnabled = false;
	GameOverFlowComponent = CreateDefaultSubobject<UDigetherGameOverFlowComponent>(TEXT("GameOverFlowComponent"));
	GameStateClass = ADigetherGameplayGameState::StaticClass();
	PlayerStateClass = ADigetherGameplayPlayerState::StaticClass();
	PlayerControllerClass = ADigetherGameplayPlayerController::StaticClass();
	DefaultPawnClass = ADigetherTestPawn::StaticClass();
	// 기본 300초 만료와 최대 개수 퇴출은 세션 보존 계약에 맞지 않는다.
	InactivePlayerStateLifeSpan = 0.f;
	MaxInactivePlayers = MAX_int32;
}

bool ADigetherGameplayGameMode::TryMakeRunSnapshot(const UDigetherTuningProfile* Profile,
	FDigetherRunTuning& OutSnapshot, TArray<FText>& OutProblems)
{
	if (!IsValid(Profile))
	{
		OutProblems.Add(NSLOCTEXT("DigetherFramework", "MissingTuning", "GameMode의 TuningProfile이 지정되지 않았습니다."));
		return false;
	}
	TArray<FText> Problems;
	if (!Profile->Values.CollectProblems(Problems))
	{
		OutProblems.Append(Problems);
		return false;
	}
	OutSnapshot = Profile->MakeSnapshot();
	return true;
}

void ADigetherGameplayGameMode::StartPlay()
{
	if (!HasAuthority() || bRunReady)
	{
		return;
	}
	FDigetherRunTuning Snapshot;
	TArray<FText> Problems;
	if (!TryMakeRunSnapshot(TuningProfile, Snapshot, Problems))
	{
		for (const FText& Problem : Problems)
		{
			UE_LOG(LogDigetherFramework, Error, TEXT("Run initialization rejected: %s"), *Problem.ToString());
		}
		return;
	}
	ADigetherGameplayGameState* State = GetGameState<ADigetherGameplayGameState>();
	UDigetherQuotaStateComponent* Quota = IsValid(State) ? State->GetQuotaComponent() : nullptr;
	const FDigetherQuotaState InitialQuota = FDigetherEconomyRules::MakeInitialQuotaState(Snapshot);
	// 신호기 경계는 이 연결층이 보관하는 경제 튜닝 원본과 같아야 한다.
	if (!IsValid(Quota) || Quota->SetSignalTuning(EconomyTuning, false) != EDigetherEconomyResult::Success
		|| Quota->SetQuotaState(InitialQuota, false) != EDigetherEconomyResult::Success)
	{
		UE_LOG(LogDigetherFramework, Error, TEXT("Run initialization rejected: authoritative quota component unavailable."));
		return;
	}
	RunTuning = Snapshot;
	if (!InitializeRunActors())
	{
		return;
	}
	bRunReady = true;
	GameId = FGuid::NewGuid();
	CurrentRunId = FGuid::NewGuid();
	ADigetherGameplayPlayerController* Host = Cast<ADigetherGameplayPlayerController>(GetWorld()->GetFirstPlayerController());
	if (IsHostRequester(Host) && IsValid(Host->PlayerState) && Host->PlayerState->GetUniqueId().IsValid())
	{
		RunStartupSaveAction(Host);
	}
	UE_LOG(LogDigetherFramework, Log, TEXT("Run tuning fixed: Profile=%s Version=%d"),
		*RunTuning.SourceProfile.ToString(), RunTuning.ProfileVersion);
	Quota->NotifyStateChanged();
	// 검증 전에 엔진 BeginPlay나 초기 Pawn 스폰이 진행되지 않게 한다.
	Super::StartPlay();
	// 엔진 매치는 월드 수명이고, 탐사 시작·종료는 별도의 런 상태로 유지한다.
	if (!HasMatchStarted())
	{
		StartMatch();
	}
	RunCoordinatorComponent->SetComponentTickEnabled(false);
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* Controller = Iterator->Get();
		if (IsValid(Controller) && !IsValid(Controller->GetPawn()))
		{
			HandleStartingNewPlayer(Controller);
		}
		if (IsValid(Controller))
		{
			RestorePlayerPawnTransform(Controller->GetPlayerState<ADigetherGameplayPlayerState>());
		}
	}
	LogRunStep(TEXT("Ready"));
	// 기획 1.1.0: 일차 시작 자동 저장은 폐기됐다. 저장 시점은 탐사 정상 완료·할당량 달성·호스트 거점 종료뿐이다.
	// 호스트 종료 저장은 넷 드라이버 종료·호스트 컨트롤러 파괴 전인 월드 정리 시작 시점에 한다.
	if (!WorldTearDownHandle.IsValid())
	{
		WorldTearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &ThisClass::HandleWorldBeginTearDown);
	}
}

void ADigetherGameplayGameMode::PreLogin(const FString& Options, const FString& Address,
	const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}
	DigetherJoinRules::EDigetherJoinDecision Decision = DigetherJoinRules::EDigetherJoinDecision::RejectInvalidId;
	// 동기 경로는 이전 연결 정리를 기다릴 수 없어 교체 대상도 ②로 거절한다. 엔진 접속은 PreLoginAsync를 쓴다.
	CheckPlayerAdmission(UniqueId, false, false, ErrorMessage, &Decision);
	RecordAdmission(UniqueId, Decision);
}

void ADigetherGameplayGameMode::PreLoginAsync(const FString& Options, const FString& Address,
	const FUniqueNetIdRepl& UniqueId, const FOnPreLoginCompleteDelegate& OnComplete)
{
	// 엔진 계약(GameModeBase.h): 재정의하면 PreLogin 검사와 GameModePreLoginEvent 방송을 함께 실행하고 OnComplete를
	// 반드시 한 번 부른다. 방송은 Super::PreLogin이 한다. OnComplete가 UWorld::PreLoginComplete로 이어져 승인(Welcome)
	// 또는 NMT_Failure 거절을 보낸다.
	FString ErrorMessage;
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	DigetherJoinRules::EDigetherJoinDecision Decision = DigetherJoinRules::EDigetherJoinDecision::RejectInvalidId;
	APlayerController* Previous = nullptr;
	if (ErrorMessage.IsEmpty())
	{
		CheckPlayerAdmission(UniqueId, false, false, ErrorMessage, &Decision, &Previous);
	}
	const UWorld* World = GetWorld();
	if (DigetherJoinRules::ShouldDisconnectPreviousConnection(Decision) && World != nullptr && !World->bIsTearingDown)
	{
		// 기획 C2(2026-11-04 확정): 이전 접속을 끊고 새 접속을 받는다. 이전 연결의 Logout(탐사 중이면 비활성 PS 보관,
		// 거점이면 자리 반환)이 끝난 뒤 승인해야 같은 ID의 PS가 둘이 되거나 늦은 세션 등록 해제가 새 등록을 지우지 않는다.
		BeginLoginReplacement(UniqueId, Previous, OnComplete);
		return;
	}
	RecordAdmission(UniqueId, Decision);
	OnComplete.ExecuteIfBound(ErrorMessage);
}

void ADigetherGameplayGameMode::BeginLoginReplacement(const FUniqueNetIdRepl& UniqueId, APlayerController* Previous,
	const FOnPreLoginCompleteDelegate& OnComplete)
{
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
		// 같은 계정의 더 늦은 접속이 왔다. 앞선 대기는 ②로 끝내 완료 콜백을 잃지 않는다.
		Older.OnComplete.ExecuteIfBound(DuplicateMessage);
	}
	DisconnectPreviousConnection(Previous, UniqueId);
	UE_LOG(LogDigetherFramework, Log, TEXT("Duplicate login %s: waiting for previous connection cleanup"), *UniqueId.ToDebugString());
	// 연결 정리는 다음 NetDriver 처리에서 일어난다. 이미 정리된 경우도 있어 바로 한 번 보고, 남으면 짧은 주기로 다시 본다.
	PollLoginReplacements();
	if (!LoginReplacements.IsEmpty() && !GetWorldTimerManager().IsTimerActive(LoginReplacementTimer))
	{
		GetWorldTimerManager().SetTimer(LoginReplacementTimer, this, &ThisClass::PollLoginReplacements,
			LoginReplacementPollSeconds, true);
	}
}

void ADigetherGameplayGameMode::PollLoginReplacements()
{
	PollLoginReplacementsAt(FPlatformTime::Seconds());
}

void ADigetherGameplayGameMode::PollLoginReplacementsAt(const double NowSeconds)
{
	TArray<FDigetherLoginReplacementQueue::FEntry> Ready;
	TArray<FDigetherLoginReplacementQueue::FEntry> TimedOut;
	LoginReplacements.Collect(NowSeconds, DigetherJoinRules::LoginReplacementTimeoutSeconds,
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
		// 정리 뒤의 현재 상태로 다시 판정한다. 탐사 중이면 비활성 PS 재접속, 거점이면 반환된 자리의 참가가 된다.
		// 그 사이 같은 ID가 또 활성화됐으면 교체를 반복하지 않고 ②로 거절한다.
		FString ErrorMessage;
		DigetherJoinRules::EDigetherJoinDecision Decision = DigetherJoinRules::EDigetherJoinDecision::RejectInvalidId;
		CheckPlayerAdmission(Entry.UniqueId, false, false, ErrorMessage, &Decision);
		RecordAdmission(Entry.UniqueId, Decision);
		UE_LOG(LogDigetherFramework, Log, TEXT("Duplicate login %s: previous connection released, decision %d"),
			*Entry.UniqueId.ToDebugString(), static_cast<int32>(Decision));
		Entry.OnComplete.ExecuteIfBound(ErrorMessage);
	}
	const FString DuplicateMessage = DigetherJoinRules::MakeRejectionMessage(EDigetherSessionText::AccountAlreadyConnected);
	for (const FDigetherLoginReplacementQueue::FEntry& Entry : TimedOut)
	{
		// 기획 C4: ②는 이전 접속을 끊지 못한 경우에만 쓴다.
		UE_LOG(LogDigetherFramework, Warning, TEXT("Duplicate login %s: previous connection not released within %.1fs"),
			*Entry.UniqueId.ToDebugString(), DigetherJoinRules::LoginReplacementTimeoutSeconds);
		Entry.OnComplete.ExecuteIfBound(DuplicateMessage);
	}
}

bool ADigetherGameplayGameMode::IsPreviousConnectionReleased(const FUniqueNetIdRepl& UniqueId,
	const APlayerController* Previous) const
{
	// 컨트롤러 파괴(Destroyed → Logout → CleanupPlayerState)는 동기라 무효가 되면 Logout까지 끝난 상태다.
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

void ADigetherGameplayGameMode::CompletePendingLoginReplacementsOnEnd()
{
	if (GetWorld() != nullptr)
	{
		GetWorldTimerManager().ClearTimer(LoginReplacementTimer);
	}
	const FString DuplicateMessage = DigetherJoinRules::MakeRejectionMessage(EDigetherSessionText::AccountAlreadyConnected);
	for (const FDigetherLoginReplacementQueue::FEntry& Entry : LoginReplacements.TakeAll())
	{
		// 엔진 계약상 완료 콜백은 반드시 부른다. 월드가 끝나는 중이면 콜백 대상(UWorld)이 무효라 실행되지 않을 수 있다.
		Entry.OnComplete.ExecuteIfBound(DuplicateMessage);
	}
}

void ADigetherGameplayGameMode::RecordAdmission(const FUniqueNetIdRepl& UniqueId,
	const DigetherJoinRules::EDigetherJoinDecision Decision)
{
	if (!UniqueId.IsValid())
	{
		return;
	}
	// 기획 확인 대상(PQ-25) 판단: "참가 요청 수락 시점"을 이 연결의 PreLogin 판정 시점으로 본다. 신규 수락(AcceptNew)은
	// 잠금 전에만 나오므로 기록이 있으면 수락이 게이지 완료 전이다. Login 확정 때 잠겨 있으면 관전 합류로 바꾼다.
	// Login은 항상 같은 연결의 PreLogin 뒤에 오고, 잠금 중 다시 PreLogin하면 아래에서 지워지므로 오래된 기록이 잠금을 우회하지 않는다.
	if (Decision == DigetherJoinRules::EDigetherJoinDecision::AcceptNew)
	{
		AcceptedBeforeLockIds.AddUnique(UniqueId);
	}
	else
	{
		AcceptedBeforeLockIds.Remove(UniqueId);
	}
}

void ADigetherGameplayGameMode::DisconnectPreviousConnection(APlayerController* Previous,
	const FUniqueNetIdRepl& UniqueId) const
{
	if (!HasAuthority() || !IsValid(Previous))
	{
		return;
	}
	// KickPlayer는 Pawn을 먼저 파괴해 남긴 Pawn·비활성 PlayerState 재접속 계약을 깬다. PC를 직접 파괴해도
	// Player가 남아 있어 PawnLeavingGame 대신 UnPossess만 한다. 연결을 닫으면 다음 NetDriver 정리에서
	// CleanUp → OnNetCleanup(Player 해제 후 파괴) → PawnLeavingGame → Logout → AddInactivePlayer 순서로 처리된다.
	UNetConnection* Connection = Cast<UNetConnection>(Previous->Player);
	if (Connection == nullptr || Connection->GetConnectionState() == USOCK_Closed)
	{
		UE_LOG(LogDigetherFramework, Verbose, TEXT("Duplicate login %s: previous connection already closed or not remote"),
			*UniqueId.ToDebugString());
		return;
	}
	Connection->Close(ENetCloseResult::HostClosedConnection);
	UE_LOG(LogDigetherFramework, Log, TEXT("Duplicate login %s: previous connection closing"), *UniqueId.ToDebugString());
}

int32 ADigetherGameplayGameMode::AllocateJoinOrder()
{
	// 저장 명단(떠난 사람 포함)과 현재 참가자 중 가장 큰 순서 다음 번호다. 같은 게임 안에서 번호를 재사용하지 않는다.
	for (const FDigetherSavePlayerRecord& Record : SavedPlayerRecords)
	{
		LastJoinOrder = FMath::Max(LastJoinOrder, Record.JoinOrder);
	}
	TArray<ADigetherGameplayPlayerState*> Players;
	CollectPlayers(Players);
	for (const ADigetherGameplayPlayerState* Player : Players)
	{
		LastJoinOrder = FMath::Max(LastJoinOrder, Player->GetJoinOrder());
	}
	return ++LastJoinOrder;
}

bool ADigetherGameplayGameMode::IsNewJoinLockedNow() const
{
	return DigetherFrameworkRules::IsNewJoinLocked(RunCoordinatorComponent->CapturePublicState(),
		ElevatorActor.IsValid() ? ElevatorActor->GetElevatorComponent()->GetPublicState().Phase
			: EDigetherElevatorPhase::HubWaiting);
}

bool ADigetherGameplayGameMode::HasParticipantRecord(const FUniqueNetIdRepl& UniqueId) const
{
	FDigetherSavePlayerRecord Record;
	bool bSkipInitialGrant = false;
	return UniqueId.IsValid() && DigetherSaveRules::TryResolveReturningPlayer(SavedPlayerRecords, UniqueId, Record,
		bSkipInitialGrant) == EDigetherSaveResult::Success;
}

bool ADigetherGameplayGameMode::CheckPlayerAdmission(const FUniqueNetIdRepl& UniqueId,
	const bool bAllowMissingId, const bool bAtLogin, FString& ErrorMessage,
	DigetherJoinRules::EDigetherJoinDecision* OutDecision, APlayerController** OutPreviousToReplace) const
{
	if (OutPreviousToReplace != nullptr)
	{
		*OutPreviousToReplace = nullptr;
	}
	const IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	DigetherJoinRules::FDigetherJoinCheck Check;
	// ID 없는 로컬 직접 실행의 기존 Login 예외에도 정원과 탐사 중 잠금은 적용한다.
	Check.bIdValid = UniqueId.IsValid() || bAllowMissingId;
	Check.bRequireSteamId = Subsystem && Subsystem->GetSubsystemName() == STEAM_SUBSYSTEM
		&& !(bAllowMissingId && !UniqueId.IsValid());
	Check.bIdIsSteam = UniqueId.IsValid() && UniqueId.IsV1() && UniqueId.GetType() == STEAM_SUBSYSTEM;
	Check.MaxPlayers = DigetherRoomProtocol::MaxPlayers;
	Check.bNewJoinLocked = IsNewJoinLockedNow();
	Check.bHasParticipantRecord = HasParticipantRecord(UniqueId);
	Check.bAcceptedBeforeLock = bAtLogin && UniqueId.IsValid() && AcceptedBeforeLockIds.Contains(UniqueId);
	TSet<const APlayerState*> Reserved;
	for (const APlayerState* Inactive : InactivePlayerArray)
	{
		if (IsValid(Inactive))
		{
			Reserved.Add(Inactive);
			const ADigetherGameplayPlayerState* Player = Cast<ADigetherGameplayPlayerState>(Inactive);
			// 실제 FindInactivePlayer가 연결할 수 있는 원본만 정원·잠금 예외로 인정한다.
			Check.bIdInRoster |= IsValid(Player) && Player->GetWorld() == GetWorld() && Player->IsInactive()
				&& Player->IsGameplayInitialized() && Player->GetGameplayPlayerId().IsValid()
				&& UniqueId.IsValid() && Player->GetUniqueId().IsValid() && Player->GetUniqueId() == UniqueId;
		}
	}
	Check.ReservedPlayers = Reserved.Num();
	const APlayerState* DuplicatePlayer = nullptr;
	if (IsValid(GameState))
	{
		// 엔진 인원 카운터는 PostLogin에서 증가한다. 먼저 생성된 PS까지 세어 연속 Login의 초과 승인을 막는다.
		TSet<const APlayerState*> Connected;
		for (const APlayerState* Player : GameState->PlayerArray)
		{
			if (!IsValid(Player) || Reserved.Contains(Player))
			{
				continue;
			}
			Connected.Add(Player);
			if (UniqueId.IsValid() && Player->GetUniqueId().IsValid() && Player->GetUniqueId() == UniqueId)
			{
				Check.bIdAlreadyConnected = true;
				DuplicatePlayer = Player;
			}
		}
		Check.ConnectedPlayers = Connected.Num();
	}
	APlayerController* Previous = DuplicatePlayer != nullptr ? DuplicatePlayer->GetPlayerController() : nullptr;
	Check.bDuplicateIsLocal = IsValid(Previous) && Previous->IsLocalController();
	const DigetherJoinRules::EDigetherJoinDecision Decision = DigetherJoinRules::Evaluate(Check);
	if (OutDecision != nullptr)
	{
		*OutDecision = Decision;
	}
	if (DigetherJoinRules::IsAccepted(Decision))
	{
		return true;
	}
	// 참가자가 자기 언어 문구로 바꿔 보이도록 문구 대신 사유 키를 보낸다. 교체 대기는 대기를 끝내지 못했을 때의 ②다.
	ErrorMessage = DigetherJoinRules::MakeRejectionMessage(DigetherJoinRules::GetRejectionText(Decision, Check));
	if (OutPreviousToReplace != nullptr && IsValid(Previous) && DigetherJoinRules::ShouldDisconnectPreviousConnection(Decision))
	{
		*OutPreviousToReplace = Previous;
	}
	return false;
}

void ADigetherGameplayGameMode::PostLogin(APlayerController* NewPlayer)
{
	// AGameMode::PostLogin 안의 자동 스폰보다 먼저 기존 상태를 연결한다.
	FindInactivePlayer(NewPlayer);
	LoggedInControllers.AddUnique(NewPlayer);
	ADigetherGameplayPlayerController* GameplayController = Cast<ADigetherGameplayPlayerController>(NewPlayer);
	if (!bHasSaveStartupAttempted && IsHostRequester(GameplayController) && IsValid(NewPlayer->PlayerState)
		&& NewPlayer->PlayerState->GetUniqueId().IsValid())
	{
		RunStartupSaveAction(GameplayController);
	}
	ADigetherGameplayPlayerState* State = IsValid(NewPlayer) ? NewPlayer->GetPlayerState<ADigetherGameplayPlayerState>() : nullptr;
	if (HasAuthority() && bRunReady && IsValid(State))
	{
		RestorePlayerRecord(State);
	}
	// 기획 1.1.0: 접속만으로는 저장하지 않는다(폐기된 출발 기준점 저장). 저장 시점은 S30 자동 저장 시점뿐이다.
	Super::PostLogin(NewPlayer);
}

void ADigetherGameplayGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	ADigetherGameplayPlayerState* Player = IsValid(NewPlayer) ? NewPlayer->GetPlayerState<ADigetherGameplayPlayerState>() : nullptr;
	if (!HasAuthority() || !bRunReady || !IsValid(Player) || bRestoreRejected)
	{
		return;
	}
	if (IsJoinSpectator(NewPlayer))
	{
		if (IsNewJoinLockedNow())
		{
			// S01 관전 합류: 잠금이 풀릴 때(탐사 종료 귀환·출발 실패 재개방)까지 몸 생성·시작 지급 없이 관전한다.
			// 저장 명단이 있는 기존 참가자는 PostLogin에서 장부만 복원된 상태다.
			EnterJoinSpectate(NewPlayer);
			return;
		}
		// 로딩 중 탐사가 끝났으면 관전 없이 거점에 합류한다(S01).
		RemoveJoinSpectator(NewPlayer);
	}
	if (!Player->IsGameplayInitialized() && !RestorePlayerRecord(Player))
	{
		Player->InitializeGameplayIdentity();
		// 이 게임의 새 참가자다. 결과 크레딧 표시 순서(참가 순서)를 받고 통계는 0에서 시작한다(S33 231행).
		Player->SetJoinOrder(AllocateJoinOrder());
		FDigetherWalletState InitialWallet;
		InitialWallet.BalanceCredits = DigetherFrameworkDefaults::StartingBalanceCredits;
		if (Player->RestoreState(InitialWallet) != EDigetherEconomyResult::Success)
		{
			return;
		}
		Player->MarkGameplayInitialized();
	}
	if (ReconnectedPlayers.Contains(Player))
	{
		if (APawn* Pawn = Player->GetGameplayPawn())
		{
			NewPlayer->Possess(Pawn);
			if (NewPlayer->GetPawn() != Pawn)
			{
				return;
			}
			ReconnectedPlayers.Remove(Player);
			if (ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(NewPlayer))
			{
				Controller->Client_SetLifeSpectating(Player->GetLifeComponent()->CaptureState().PublicState.bSpectating);
			}
			return;
		}
		// 보존 대상이 사라졌으면 재접속 표식으로 일반 생성 경로를 막지 않는다. 지급은 위 초기화 가드가 막는다.
		ReconnectedPlayers.Remove(Player);
	}
	if (RunCoordinatorComponent->CapturePublicState().Phase != EDigetherRunPhase::InProgress
		&& Player->GetLifeComponent()->CaptureState().PublicState.Life == EDigetherLifeState::Active
		&& !IsValid(NewPlayer->GetPawn()))
	{
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
		RestorePlayerPawnTransform(Player);
		if (ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(NewPlayer))
		{
			Controller->Client_SetLifeSpectating(false);
		}
	}
	else if (RunCoordinatorComponent->CapturePublicState().Phase == EDigetherRunPhase::InProgress
		&& !IsValid(Player->GetGameplayPawn()))
	{
		// 이번 세션의 재접속자라도 보존 Pawn이 없으면 탐사에 새 몸을 만들지 않고 귀환 후 생성한다.
		if (ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(NewPlayer))
		{
			Controller->Client_SetLifeSpectating(true);
			Controller->Client_SaveResult(false, NSLOCTEXT("DigetherSave", "NoRetainedWorldPawn",
				"참가 기록은 복원했지만 현재 월드에 남은 캐릭터가 없어 생성을 보류했습니다."));
		}
	}
}

bool ADigetherGameplayGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	// 엔진의 ServerRestartPlayer 경로도 프로필 검증을 우회하지 못하게 한다.
	return HasAuthority() && bRunReady
		&& !bRestoreRejected
		&& IsValid(Player)
		&& IsValid(Player->GetPlayerState<ADigetherGameplayPlayerState>())
		&& Player->GetPlayerState<ADigetherGameplayPlayerState>()->IsGameplayInitialized()
		&& Player->GetPlayerState<ADigetherGameplayPlayerState>()->GetLifeComponent()->CaptureState().PublicState.Life == EDigetherLifeState::Active
		&& RunCoordinatorComponent->CapturePublicState().Phase != EDigetherRunPhase::InProgress
		// 관전 합류자는 문 닫힘 중에도 몸을 만들지 않는다. 그 몸이 출발 인원에 섞이지 않게 한다.
		&& !IsJoinSpectator(Player)
		&& Super::PlayerCanRestart_Implementation(Player);
}

void ADigetherGameplayGameMode::Logout(AController* Exiting)
{
	if (APlayerController* Controller = Cast<APlayerController>(Exiting))
	{
		if (!LoggedInControllers.Contains(Controller))
		{
			// Super Login의 초기화 실패도 Logout을 부른다. 아직 증가하지 않은 매치 인원을 줄이지 않는다.
			AGameModeBase::Logout(Exiting);
			return;
		}
	}
	if (ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(Exiting))
	{
		for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Lever : LeverActors)
		{
			ReleaseLeverPull(Controller, Lever.Get());
		}
	}
	if (HasAuthority())
	{
		StopPourForController(Cast<ADigetherGameplayPlayerController>(Exiting), EDigetherPourStopReason::Disconnected);
	}
	Super::Logout(Exiting);
	LoggedInControllers.Remove(Cast<APlayerController>(Exiting));
	RemoveJoinSpectator(Cast<APlayerController>(Exiting));
}
