#include "Game/Framework/DigetherGameplayGameMode.h"

#include "Engine/World.h"
#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Elevator/DigetherElevatorLeverActor.h"
#include "Game/Framework/DigetherFrameworkLog.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Life/DigetherLifeRules.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Run/DigetherRunCoordinatorComponent.h"
#include "Game/Save/DigetherSaveRules.h"
#include "Game/Save/DigetherSaveTuning.h"
#include "GameFramework/Pawn.h"

bool ADigetherGameplayGameMode::ReadyToStartMatch_Implementation()
{
	return bRunReady;
}

APlayerController* ADigetherGameplayGameMode::Login(UPlayer* NewPlayer, const ENetRole InRemoteRole,
	const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	// 참가 확정(Login) 재검사다. PreLogin과 Login 사이에 맵 로딩 시간이 지나므로 지금 상태로 다시 판정한다.
	// 무효 ID의 로컬 직접 실행도 정원·탐사 잠금을 검사하며 저장·재접속 키로 승격하지 않는다.
	DigetherJoinRules::EDigetherJoinDecision Decision = DigetherJoinRules::EDigetherJoinDecision::RejectInvalidId;
	if (UniqueId.IsValid())
	{
		// 같은 계정 교체(이전 연결 끊기)는 PreLoginAsync에서만 한다. 여기서 같은 ID가 활성이면 끊지 않고 ②로 거절한다.
		Super::PreLogin(Options, FString(), UniqueId, ErrorMessage);
		if (ErrorMessage.IsEmpty())
		{
			CheckPlayerAdmission(UniqueId, false, true, ErrorMessage, &Decision);
		}
		// PreLogin 수락 기록은 이 연결의 확정 판정 한 번에만 쓴다.
		AcceptedBeforeLockIds.Remove(UniqueId);
	}
	else if (!CheckPlayerAdmission(UniqueId, true, true, ErrorMessage, &Decision))
	{
		return nullptr;
	}
	if (!ErrorMessage.IsEmpty())
	{
		return nullptr;
	}
	APlayerController* Controller = Super::Login(NewPlayer, InRemoteRole, Portal, Options, UniqueId, ErrorMessage);
	if (IsValid(Controller) && ErrorMessage.IsEmpty())
	{
		// PostLogin 대기 중에도 임시 PS와 보존 PS가 같은 ID의 인원을 중복 예약하지 않게 연결한다.
		FindInactivePlayer(Controller);
		if (Decision == DigetherJoinRules::EDigetherJoinDecision::AcceptSpectator)
		{
			// 자리는 PlayerArray의 새 PS로 차지한다. 시작 훅이 몸 대신 관전을 시작한다.
			FDigetherJoinSpectator& Spectator = JoinSpectators.AddDefaulted_GetRef();
			Spectator.Controller = Controller;
			UE_LOG(LogDigetherFramework, Log, TEXT("Join %s confirmed during lock: spectating until base return"),
				*UniqueId.ToDebugString());
		}
	}
	return Controller;
}

bool ADigetherGameplayGameMode::IsJoinSpectator(const APlayerController* Controller) const
{
	if (Controller == nullptr)
	{
		return false;
	}
	for (const FDigetherJoinSpectator& Spectator : JoinSpectators)
	{
		if (Spectator.Controller.Get() == Controller)
		{
			return true;
		}
	}
	return false;
}

void ADigetherGameplayGameMode::RemoveJoinSpectator(const APlayerController* Controller)
{
	// 무효가 된 항목도 함께 정리한다.
	JoinSpectators.RemoveAll([Controller](const FDigetherJoinSpectator& Spectator)
	{
		return !Spectator.Controller.IsValid() || Spectator.Controller.Get() == Controller;
	});
}

void ADigetherGameplayGameMode::RememberDepartedPlayer(const ADigetherGameplayPlayerState* Player)
{
	FDigetherSavePlayerRecord Record;
	if (!IsValid(Player) || !Player->IsGameplayInitialized() || !Player->GetGameplayPlayerId().IsValid()
		|| !IsValid(Player->GetLifeComponent())
		|| DigetherSaveRules::MakeAuthenticatedPlayerKey(Player->GetUniqueId(), Record.AuthenticatedPlayerKey)
			!= EDigetherSaveResult::Success)
	{
		return;
	}
	Record.GameplayPlayerId = Player->GetGameplayPlayerId();
	Record.Wallet = Player->CaptureState();
	Record.Life = Player->GetLifeComponent()->CaptureState();
	// 기획 S33 231행: 중간에 나간 사람도 결과 크레딧에 남는다. 떠날 때의 이름과 참가 순서를 남긴다.
	Record.DisplayName = DigetherSaveRules::MakeStoredDisplayName(Player->GetPlayerName());
	Record.JoinOrder = Player->GetJoinOrder();
	// 몸을 제거하므로 위치는 남기지 않는다. 돌아오면 일반 시작 위치에 새 캐릭터를 만든다.
	Record.bHasHubTransform = false;
	if (!FDigetherEconomyRules::IsValidWalletState(Record.Wallet) || !FDigetherLifeRules::IsValidState(Record.Life))
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("Hub departure record skipped: invalid state for player %s."),
			*Record.GameplayPlayerId.ToString());
		return;
	}
	const int32 Existing = SavedPlayerRecords.IndexOfByPredicate([&Record](const FDigetherSavePlayerRecord& Entry)
	{
		return Entry.AuthenticatedPlayerKey == Record.AuthenticatedPlayerKey;
	});
	if (Existing != INDEX_NONE)
	{
		if (Record.JoinOrder <= 0)
		{
			Record.JoinOrder = SavedPlayerRecords[Existing].JoinOrder;
		}
		SavedPlayerRecords[Existing] = Record;
	}
	else if (SavedPlayerRecords.Num() < DigetherSaveTuning::MaxPlayerRecords)
	{
		SavedPlayerRecords.Add(Record);
	}
	else
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("Hub departure record skipped: saved roster full."));
	}
}

void ADigetherGameplayGameMode::ReleaseHubSeat(ADigetherGameplayPlayerState* Player)
{
	// S01·확정 규칙 53행: 거점에서 퇴장·연결 끊김은 캐릭터를 제거하고 자리를 반환한다. 비활성 PS로 예약하지 않으므로
	// 이어지는 엔진 CleanupPlayerState(OnDeactivated)가 이 PS를 파괴하고, 돌아오면 빈자리 검사를 받는다.
	// 최신 장부는 저장 명단에 남겨 재접속 때 시작 지급 없이 복원하고, 탐사 중 돌아오면 관전 합류 자격이 된다.
	RememberDepartedPlayer(Player);
	APawn* Pawn = Player->GetGameplayPawn();
	// TODO(인벤토리): 소지 장비를 몸이 있던 자리에 실물로 떨어뜨린다. 장비·인벤토리 시스템이 아직 없어 몸만 제거한다(T77).
	Player->RetainPawn(nullptr);
	if (IsValid(Pawn))
	{
		Pawn->Destroy();
	}
	ReconnectedPlayers.Remove(Player);
	InactivePlayerArray.Remove(Player);
	UE_LOG(LogDigetherFramework, Log, TEXT("Hub departure: player %s seat released."), *Player->GetGameplayPlayerId().ToString());
}

void ADigetherGameplayGameMode::NotifyPlayerDisconnecting(ADigetherGameplayPlayerController* Controller)
{
	if (!HasAuthority() || !IsValid(Controller) || Controller->GetWorld() != GetWorld())
	{
		return;
	}
	// Pawn 해제 전에 레버 점유를 풀어 끊긴 입력이 완료로 처리되지 않게 한다.
	for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Lever : LeverActors)
	{
		ReleaseLeverPull(Controller, Lever.Get());
	}
	// 기획 T17: 연결이 끊긴 사람의 붓기만 멈춘다. 이미 확정한 주기는 유지한다.
	StopPourForController(Controller, EDigetherPourStopReason::Disconnected);
}

void ADigetherGameplayGameMode::AddInactivePlayer(APlayerState* PlayerState, APlayerController* PC)
{
	ADigetherGameplayPlayerState* Player = Cast<ADigetherGameplayPlayerState>(PlayerState);
	if (!HasAuthority() || !IsValid(Player) || !IsValid(PC) || PC->PlayerState != Player
		|| Player->GetWorld() != GetWorld() || PC->GetWorld() != GetWorld() || GetWorld()->bIsTearingDown)
	{
		return;
	}
	// S01: 탐사 출발 전(거점·문 닫힘)의 이탈은 자리를 반환하고, 출발 후의 이탈만 몸·자리를 보존한다.
	if (RunCoordinatorComponent->CapturePublicState().Phase != EDigetherRunPhase::InProgress)
	{
		ReleaseHubSeat(Player);
		return;
	}
	if (!Player->IsGameplayInitialized() || !Player->GetGameplayPlayerId().IsValid() || !Player->GetUniqueId().IsValid())
	{
		return;
	}
	// Duplicate하면 RunParticipants와 남은 Pawn이 이전 상태를 가리키므로 원본을 보존한다.
	Player->SetIsInactive(true);
	Player->SetReplicates(false);
	Player->SetLifeSpan(0.f);
	Player->SetOwner(nullptr);
	if (IsValid(GameState))
	{
		GameState->RemovePlayerState(Player);
	}
	InactivePlayerArray.AddUnique(Player);
	// 재빙의 대기는 현재 연결의 작업이다. 다시 끊기면 다음 로그인에서 새로 등록한다.
	ReconnectedPlayers.Remove(Player);
}

bool ADigetherGameplayGameMode::FindInactivePlayer(APlayerController* PC)
{
	ADigetherGameplayPlayerState* Fresh = IsValid(PC) ? PC->GetPlayerState<ADigetherGameplayPlayerState>() : nullptr;
	if (!HasAuthority() || !IsValid(Fresh) || PC->GetWorld() != GetWorld()
		|| Fresh->GetWorld() != GetWorld() || !Fresh->GetUniqueId().IsValid() || GetWorld()->bIsTearingDown)
	{
		return false;
	}
	for (int32 Index = 0; Index < InactivePlayerArray.Num(); ++Index)
	{
		ADigetherGameplayPlayerState* Existing = Cast<ADigetherGameplayPlayerState>(InactivePlayerArray[Index]);
		if (!IsValid(Existing) || Existing->GetWorld() != GetWorld() || !Existing->IsInactive()
			|| !Existing->IsGameplayInitialized() || !Existing->GetGameplayPlayerId().IsValid()
			|| !Existing->GetUniqueId().IsValid()
			|| Existing->GetUniqueId() != Fresh->GetUniqueId())
		{
			continue;
		}
		PC->SetPlayerState(Existing);
		Existing->SetOwner(PC);
		Existing->SetIsInactive(false);
		Existing->SetReplicates(true);
		Existing->SetLifeSpan(0.f);
		if (IsValid(GameState))
		{
			GameState->RemovePlayerState(Fresh);
			GameState->AddPlayerState(Existing);
		}
		InactivePlayerArray.RemoveAt(Index);
		ReconnectedPlayers.AddUnique(Existing);
		// 임시 로그인 PS가 같은 ID의 온라인 등록을 해제하지 않도록 엔진 순서를 따른다.
		Fresh->SetIsInactive(true);
		Fresh->SetUniqueId(FUniqueNetIdRepl());
		Fresh->Destroy();
		Existing->OnReactivated();
		return true;
	}
	return false;
}
