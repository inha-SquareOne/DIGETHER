// 접속 승인·같은 계정 교체(기획 C2·C4, PQ-25)·참가 순서
#include "Game/Framework/DigetherGameplayGameMode.h"

#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "Game/Elevator/DigetherElevatorActor.h"
#include "Game/Elevator/DigetherElevatorComponent.h"
#include "Game/Framework/DigetherFrameworkLog.h"
#include "Game/Framework/DigetherFrameworkRules.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Online/DigetherJoinRules.h"
#include "Game/Online/DigetherRoomProtocol.h"
#include "Game/Online/DigetherSessionText.h"
#include "Game/Run/DigetherRunCoordinatorComponent.h"
#include "Game/Save/DigetherSaveRules.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"
#include "OnlineSubsystem.h"
#include "TimerManager.h"

namespace
{
	// 같은 계정 교체 대기의 정리 확인 주기, 제한 시간(DigetherJoinRules::LoginReplacementTimeoutSeconds)보다 충분히 짧게
	constexpr float LoginReplacementPollSeconds = 0.1f;
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
	// 동기 경로는 이전 연결 정리 대기 불가, 교체 대상도 ②로 거절, 엔진 접속은 PreLoginAsync 사용
	CheckPlayerAdmission(UniqueId, false, false, ErrorMessage, &Decision);
	RecordAdmission(UniqueId, Decision);
}

void ADigetherGameplayGameMode::PreLoginAsync(const FString& Options, const FString& Address,
	const FUniqueNetIdRepl& UniqueId, const FOnPreLoginCompleteDelegate& OnComplete)
{
	// 엔진 계약(GameModeBase.h): 재정의 시 PreLogin 검사·GameModePreLoginEvent 방송 실행, OnComplete 반드시 한 번 호출
	// 방송은 Super::PreLogin 담당, OnComplete → UWorld::PreLoginComplete → 승인(Welcome) 또는 NMT_Failure 거절
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
		// 기획 C2(2026-11-04 확정): 이전 접속을 끊고 새 접속 수락
		// 이전 연결 Logout(탐사 중 비활성 PS 보관, 거점 자리 반환) 뒤 승인해야 같은 ID PS 중복·늦은 세션 등록 해제의 새 등록 삭제 방지
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
		// 같은 계정의 더 늦은 접속, 앞선 대기는 ②로 끝내 완료 콜백 유실 방지
		Older.OnComplete.ExecuteIfBound(DuplicateMessage);
	}
	DisconnectPreviousConnection(Previous, UniqueId);
	UE_LOG(LogDigetherFramework, Log, TEXT("Duplicate login %s: waiting for previous connection cleanup"), *UniqueId.ToDebugString());
	// 연결 정리는 다음 NetDriver 처리 시점, 이미 정리됐을 수 있어 즉시 한 번 확인 후 남으면 짧은 주기로 재확인
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
		// 정리 뒤 현재 상태로 재판정, 탐사 중이면 비활성 PS 재접속·거점이면 반환된 자리로 참가
		// 그 사이 같은 ID가 다시 활성화됐으면 교체 반복 없이 ②로 거절
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
		// 기획 C4: ②는 이전 접속을 끊지 못한 경우만
		UE_LOG(LogDigetherFramework, Warning, TEXT("Duplicate login %s: previous connection not released within %.1fs"),
			*Entry.UniqueId.ToDebugString(), DigetherJoinRules::LoginReplacementTimeoutSeconds);
		Entry.OnComplete.ExecuteIfBound(DuplicateMessage);
	}
}

bool ADigetherGameplayGameMode::IsPreviousConnectionReleased(const FUniqueNetIdRepl& UniqueId,
	const APlayerController* Previous) const
{
	// 컨트롤러 파괴(Destroyed → Logout → CleanupPlayerState)는 동기, 무효면 Logout까지 완료
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
		// 엔진 계약상 완료 콜백 필수 호출, 월드 종료 중이면 콜백 대상(UWorld) 무효로 미실행 가능
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
	// PQ-25 판단: "참가 요청 수락 시점" = 이 연결의 PreLogin 판정 시점
	// 신규 수락(AcceptNew)은 잠금 전에만 발생, 기록이 있으면 게이지 완료 전 수락, Login 확정 때 잠겨 있으면 관전 합류
	// Login은 항상 같은 연결 PreLogin 뒤, 잠금 중 재PreLogin이면 아래에서 삭제되어 오래된 기록의 잠금 우회 방지
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
	// KickPlayer는 Pawn을 먼저 파괴해 남긴 Pawn·비활성 PlayerState 재접속 계약 위반
	// PC 직접 파괴는 Player가 남아 PawnLeavingGame 대신 UnPossess만 실행
	// 연결 종료 시 다음 NetDriver 정리: CleanUp → OnNetCleanup(Player 해제 후 파괴) → PawnLeavingGame → Logout → AddInactivePlayer
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

void ADigetherGameplayGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	// 엔진이 대기실 PC·PS를 게임 클래스로 교체하면 C가 새 PC로 바뀜
	Super::HandleSeamlessTravelPlayer(C);
	// 교체 실패 시 엔진이 이전 PC를 파괴하고 그대로 반환
	APlayerController* Controller = Cast<APlayerController>(C);
	if (IsValid(Controller))
	{
		// Logout의 레버·붓기 정리와 매치 인원 감소 대상(AGameMode::InitSeamlessTravelPlayer가 인원 증가)
		LoggedInControllers.AddUnique(Controller);
		UE_LOG(LogDigetherFramework, Log, TEXT("Seamless travel player joined: %s"),
			IsValid(Controller->PlayerState) ? *Controller->PlayerState->GetPlayerName() : TEXT("(no PlayerState)"));
	}
}

int32 ADigetherGameplayGameMode::AllocateJoinOrder()
{
	// 저장 명단(떠난 사람 포함)·현재 참가자 중 최대 순서의 다음 번호, 같은 게임 안 재사용 없음
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
	// ID 없는 로컬 직접 실행의 Login 예외에도 정원·탐사 중 잠금 적용
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
			// 실제 FindInactivePlayer가 연결 가능한 원본만 정원·잠금 예외 인정
			Check.bIdInRoster |= IsValid(Player) && Player->GetWorld() == GetWorld() && Player->IsInactive()
				&& Player->IsGameplayInitialized() && Player->GetGameplayPlayerId().IsValid()
				&& UniqueId.IsValid() && Player->GetUniqueId().IsValid() && Player->GetUniqueId() == UniqueId;
		}
	}
	Check.ReservedPlayers = Reserved.Num();
	const APlayerState* DuplicatePlayer = nullptr;
	if (IsValid(GameState))
	{
		// 엔진 인원 카운터는 PostLogin에서 증가, 먼저 생성된 PS까지 세어 연속 Login 초과 승인 방지
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
	// 참가자 언어로 표시하도록 문구 대신 사유 키 전송, 교체 대기 미완료는 ②
	ErrorMessage = DigetherJoinRules::MakeRejectionMessage(DigetherJoinRules::GetRejectionText(Decision, Check));
	if (OutPreviousToReplace != nullptr && IsValid(Previous) && DigetherJoinRules::ShouldDisconnectPreviousConnection(Decision))
	{
		*OutPreviousToReplace = Previous;
	}
	return false;
}
