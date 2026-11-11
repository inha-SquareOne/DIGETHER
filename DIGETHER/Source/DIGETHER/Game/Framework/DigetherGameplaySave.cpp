#include "Game/Framework/DigetherGameplayGameMode.h"

#include "Algo/StableSort.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Economy/DigetherQuotaStateComponent.h"
#include "Game/Economy/DigetherWalletComponent.h"
#include "Game/Elevator/DigetherElevatorActor.h"
#include "Game/Elevator/DigetherElevatorComponent.h"
#include "Game/Elevator/DigetherElevatorLeverActor.h"
#include "Game/Elevator/DigetherElevatorLeverComponent.h"
#include "Game/Framework/DigetherFrameworkDefaults.h"
#include "Game/Framework/DigetherFrameworkLog.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Life/DigetherLifeRules.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Monster/DigetherMonsterDirectorSubsystem.h"
#include "Game/Run/DigetherRunCoordinatorComponent.h"
#include "Game/Run/DigetherRunPublicStateComponent.h"
#include "Game/Save/DigetherSaveRules.h"
#include "Game/Save/DigetherSaveSubsystem.h"
#include "Game/Hub/DigetherHubStateActor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "TimerManager.h"

bool ADigetherGameplayGameMode::IsHostRequester(const ADigetherGameplayPlayerController* Requester) const
{
	return HasAuthority() && bRunReady && IsValid(Requester) && Requester->HasAuthority()
		&& Requester->IsLocalController() && Requester->GetWorld() == GetWorld()
		&& GetNetMode() != NM_Client && GetNetMode() != NM_DedicatedServer;
}

void ADigetherGameplayGameMode::NotifySaveResult(ADigetherGameplayPlayerController* Requester,
	const bool bSucceeded, const FText& Message) const
{
	if (IsValid(Requester))
	{
		Requester->Client_SaveResult(bSucceeded, Message);
	}
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		ADigetherGameplayPlayerController* Host = Cast<ADigetherGameplayPlayerController>(Iterator->Get());
		if (IsHostRequester(Host) && Host != Requester)
		{
			Host->Client_SaveResult(bSucceeded, Message);
		}
	}
}

bool ADigetherGameplayGameMode::CaptureProgress(FDigetherSaveSnapshot& OutSnapshot) const
{
	const ADigetherGameplayGameState* State = GetGameState<ADigetherGameplayGameState>();
	// 공통 로딩 실패 복귀용 메모리 기준점은 거점 문이 완전히 닫힌 순간에 만들고, 호스트는 문 닫힘 중에도
	// 거점에서 종료할 수 있으므로 거점 문 닫힘도 거점 대기로 본다(기획 A3).
	// 런 진행 중이 아니면 DoorClosing은 거점에서만 생긴다. 엘리베이터 단계는 저장 대상이 아니다.
	const EDigetherElevatorPhase ElevatorPhase = ElevatorActor.IsValid()
		? ElevatorActor->GetElevatorComponent()->GetPublicState().Phase : EDigetherElevatorPhase::HubWaiting;
	if (!HasAuthority() || !bRunReady || !IsValid(State) || !ElevatorActor.IsValid()
		|| RunCoordinatorComponent->CapturePublicState().Phase == EDigetherRunPhase::InProgress
		|| (ElevatorPhase != EDigetherElevatorPhase::HubWaiting && ElevatorPhase != EDigetherElevatorPhase::DoorClosing))
	{
		return false;
	}
	FDigetherSaveRuntimeState Runtime;
	Runtime.GameId = GameId;
	Runtime.RunId = CurrentRunId;
	Runtime.Quota = State->GetQuotaComponent()->CaptureState();
	Runtime.RunPublicState = RunCoordinatorComponent->CapturePublicState();
	// 종료된 런의 서버 내부 시계는 새 월드로 가져가지 않고 완료 기록으로 재정산을 막는다.
	Runtime.RunPublicState.Phase = EDigetherRunPhase::NotStarted;
	Runtime.RunPublicState.AftershockStage = 0;
	Runtime.RunTuning = RunTuning;
	Runtime.EconomyTuning = EconomyTuning;
	Runtime.RunRuleTuning = FixedRunRuleTuning;
	Runtime.CompletedSettlementRunIds = CompletedSettlementRunIds;
	// 기획 S33: 게임 오버 확정 저장에는 이번 게임 결과를 함께 쓰고, 이전 게임 결과 보관 목록은 항상 쓴다.
	Runtime.bHasGameResult = bHasGameResult && Runtime.RunPublicState.bGameOver;
	Runtime.GameResult = Runtime.bHasGameResult ? GameResult : FDigetherGameResult();
	Runtime.ArchivedGameResults = ArchivedGameResults;
	if (const ADigetherHubStateActor* Hub = ADigetherHubStateActor::Find(GetWorld()))
	{
		Runtime.HubProgress = Hub->CaptureProgress();
	}
	CaptureFloorCoins(Runtime.FloorCoins);
	if (!Runtime.FloorCoins.IsEmpty() && !Runtime.HubProgress.bInitialized)
	{
		// 바닥 금화는 Hub 확장(v2)에만 저장한다. Hub 진행이 없는 맵에서는 저장하지 않고 진행 저장은 계속한다.
		UE_LOG(LogDigetherFramework, Warning, TEXT("Floor coins (%d piles) not saved: no initialized hub progress in this map."),
			Runtime.FloorCoins.Num());
		Runtime.FloorCoins.Reset();
	}
	Runtime.Players = SavedPlayerRecords;
	TArray<ADigetherGameplayPlayerState*> Players;
	CollectPlayers(Players);
	for (ADigetherGameplayPlayerState* Player : Players)
	{
		FDigetherSavePlayerRecord Record;
		if (DigetherSaveRules::MakeAuthenticatedPlayerKey(Player->GetUniqueId(), Record.AuthenticatedPlayerKey)
			!= EDigetherSaveResult::Success)
		{
			return false;
		}
		Record.GameplayPlayerId = Player->GetGameplayPlayerId();
		Record.Wallet = Player->CaptureState();
		Record.Life = Player->GetLifeComponent()->CaptureState();
		Record.DisplayName = DigetherSaveRules::MakeStoredDisplayName(Player->GetPlayerName());
		Record.JoinOrder = Player->GetJoinOrder();
		if (const APawn* Pawn = Player->GetGameplayPawn())
		{
			Record.bHasHubTransform = true;
			Record.HubTransform = Pawn->GetActorTransform();
		}
		const int32 Existing = Runtime.Players.IndexOfByPredicate([&Record](const FDigetherSavePlayerRecord& Item)
		{
			return Item.AuthenticatedPlayerKey == Record.AuthenticatedPlayerKey;
		});
		if (Existing == INDEX_NONE)
		{
			Runtime.Players.Add(Record);
		}
		else
		{
			Runtime.Players[Existing] = Record;
		}
		if (const ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(Player->GetOwner()))
		{
			if (IsHostRequester(Controller))
			{
				Runtime.HostAuthenticatedPlayerKey = Record.AuthenticatedPlayerKey;
			}
		}
	}
	return DigetherSaveRules::TryCaptureSnapshot(Runtime, OutSnapshot) == EDigetherSaveResult::Success;
}

bool ADigetherGameplayGameMode::RestorePlayerRecord(ADigetherGameplayPlayerState* Player)
{
	if (!IsValid(Player) || !Player->HasAuthority())
	{
		return false;
	}
	if (Player->IsGameplayInitialized())
	{
		return true;
	}
	FDigetherSavePlayerRecord Record;
	bool bSkipInitialGrant = false;
	if (DigetherSaveRules::TryResolveReturningPlayer(SavedPlayerRecords, Player->GetUniqueId(), Record,
		bSkipInitialGrant) != EDigetherSaveResult::Success || !bSkipInitialGrant)
	{
		return false;
	}
	if (!FDigetherEconomyRules::IsValidWalletState(Record.Wallet) || !FDigetherLifeRules::IsValidState(Record.Life)
		|| !Record.GameplayPlayerId.IsValid() || !IsValid(Player->GetLifeComponent()))
	{
		return false;
	}
	Player->RestoreGameplayIdentity(Record.GameplayPlayerId);
	Player->RestoreState(Record.Wallet, false);
	Player->GetLifeComponent()->RestoreState(Record.Life, false);
	// 같은 게임의 기존 참가자는 처음 참가 순서를 유지한다. 순서를 모르는 이전 저장 기록은 지금 다음 번호를 받는다.
	Player->SetJoinOrder(Record.JoinOrder > 0 ? Record.JoinOrder : AllocateJoinOrder());
	Player->MarkGameplayInitialized();
	Player->GetWalletComponent()->NotifyStateChanged();
	Player->GetLifeComponent()->NotifyStateChanged();
	return true;
}

void ADigetherGameplayGameMode::RestorePlayerPawnTransform(ADigetherGameplayPlayerState* Player) const
{
	if (!HasAuthority() || !IsValid(Player) || !IsValid(Player->GetGameplayPawn()))
	{
		return;
	}
	FDigetherSavePlayerRecord Record;
	bool bSkipInitialGrant = false;
	if (DigetherSaveRules::TryResolveReturningPlayer(SavedPlayerRecords, Player->GetUniqueId(), Record,
		bSkipInitialGrant) == EDigetherSaveResult::Success && Record.bHasHubTransform)
	{
		APawn* Pawn = Player->GetGameplayPawn();
		TArray<FTransform> Reserved;
		if (bValidateSavedHubSpawn)
		{
			for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				const APlayerController* Other = Iterator->Get();
				if (IsValid(Other) && IsValid(Other->GetPawn()) && Other->GetPawn() != Pawn)
				{
					Reserved.Add(Other->GetPawn()->GetActorTransform());
				}
			}
		}
		FTransform Resolved;
		bool bUsedFallback = false;
		if (!ResolveHubSpawnTransform(Pawn, &Record.HubTransform, Reserved, Resolved, bUsedFallback))
		{
			UE_LOG(LogDigetherFramework, Warning, TEXT("Saved hub position rejected: no safe PlayerStart for player %s. Current spawn retained."),
				*Player->GetGameplayPlayerId().ToString());
			return;
		}
		if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
		{
			Movement->StopMovementImmediately();
		}
		if (!Pawn->SetActorTransform(Resolved, false, nullptr, ETeleportType::TeleportPhysics))
		{
			UE_LOG(LogDigetherFramework, Warning, TEXT("Saved hub position could not be applied for player %s."),
				*Player->GetGameplayPlayerId().ToString());
			return;
		}
		if (bUsedFallback)
		{
			UE_LOG(LogDigetherFramework, Log, TEXT("Saved hub position replaced with a safe PlayerStart for player %s; progress retained."),
				*Player->GetGameplayPlayerId().ToString());
		}
		if (APlayerController* Controller = Cast<APlayerController>(Player->GetOwner()))
		{
			Controller->ClientSetLocation(Resolved.GetLocation(), Resolved.GetRotation().Rotator());
		}
	}
}

bool ADigetherGameplayGameMode::RestoreProgress(const FDigetherSaveSnapshot& Snapshot, const bool bValidateOnly)
{
	ADigetherHubStateActor* Hub = ADigetherHubStateActor::Find(GetWorld());
	if ((IsValid(Hub) && !Hub->ValidateProgress(Snapshot.HubProgress))
		|| (!IsValid(Hub) && Snapshot.HubProgress.bInitialized))
	{
		return false;
	}
	ADigetherGameplayGameState* State = GetGameState<ADigetherGameplayGameState>();
	if (!HasAuthority() || !bRunReady || !IsValid(State) || !IsValid(State->GetQuotaComponent())
		|| !IsValid(State->GetRunPublicStateComponent()) || !ElevatorActor.IsValid()
		|| !IsValid(ElevatorActor->GetElevatorComponent())
		|| DigetherSaveRules::ValidateSnapshot(Snapshot) != EDigetherSaveResult::Success)
	{
		return false;
	}
	for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Lever : LeverActors)
	{
		if (!Lever.IsValid() || !IsValid(Lever->GetLeverComponent()))
		{
			return false;
		}
	}
	TArray<ADigetherGameplayPlayerState*> Players;
	CollectPlayers(Players);
	// 시작 전 미초기화 PS도 대조한다. 저장·출발 기준 시점 이후 참가한 다른 ID의 복구 정책은 기획 확인 대상이다.
	// 예외: 관전 합류로 아직 캐릭터·시작 지급을 받지 않은 신규 참가자는 저장 명단에 없을 수 있다.
	// 공통 로딩 실패 복귀에서 이들은 복원 대상이 아니며, 잠금이 풀린 뒤 Tick의 관전 해제에서 합류한다(S01).
	for (APlayerState* Entry : GameState->PlayerArray)
	{
		ADigetherGameplayPlayerState* Player = Cast<ADigetherGameplayPlayerState>(Entry);
		if (IsValid(Player) && !(IsJoinSpectator(Cast<APlayerController>(Player->GetOwner()))
			&& !Player->IsGameplayInitialized()))
		{
			Players.AddUnique(Player);
		}
	}
	TArray<FDigetherSavePlayerRecord> Records;
	TArray<FTransform> Transforms;
	int32 CorrectedSpawnCount = 0;
	for (ADigetherGameplayPlayerState* Player : Players)
	{
		FDigetherSavePlayerRecord Record;
		bool bSkip = false;
		if (!Player->HasAuthority() || !IsValid(Player->GetWalletComponent()) || !IsValid(Player->GetLifeComponent())
			|| DigetherSaveRules::TryResolveReturningPlayer(Snapshot.Players, Player->GetUniqueId(), Record, bSkip)
				!= EDigetherSaveResult::Success)
		{
			return false;
		}
		FTransform Resolved;
		bool bUsedFallback = false;
		if (!ResolveHubSpawnTransform(Player->GetGameplayPawn(), Record.bHasHubTransform ? &Record.HubTransform : nullptr,
			Transforms, Resolved, bUsedFallback))
		{
			return false;
		}
		CorrectedSpawnCount += bUsedFallback ? 1 : 0;
		if (bValidateSavedHubSpawn)
		{
			Record.bHasHubTransform = true;
			Record.HubTransform = Resolved;
		}
		Records.Add(Record);
		Transforms.Add(Resolved);
	}
	if (bValidateOnly)
	{
		return true;
	}
	// Run 폴더를 바꾸지 않고 거점 시계를 새로 만든다. 등록 전에는 다른 도메인에 알리지 않는다.
	UDigetherRunCoordinatorComponent* CandidateRun = NewObject<UDigetherRunCoordinatorComponent>(this);
	if (!IsValid(CandidateRun))
	{
		return false;
	}
	CandidateRun->PrimaryComponentTick.bStartWithTickEnabled = false;
	CandidateRun->RegisterComponent();
	CandidateRun->SetComponentTickEnabled(false);
	UDigetherElevatorComponent* Elevator = ElevatorActor->GetElevatorComponent();
	Elevator->BeginNotificationBatch();
	for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Lever : LeverActors)
	{
		if (Lever.IsValid())
		{
			Lever->GetLeverComponent()->BeginNotificationBatch();
		}
	}
	// 여기까지 모든 저장 값·참가 ID·적용 대상을 검사했다. 이후 setter는 권한과 값만 검사한다.
	// 불러온 진행에는 붓기 세션이 없다. 알림은 상태 적용이 끝난 뒤 보낸다.
	const TArray<FDigetherPourSession> StoppedPours = PourSessions.StopAll();
	RunCoordinatorComponent->OnTimeExpired.RemoveAll(this);
	RunCoordinatorComponent->SetPublicStateComponent(nullptr);
	RunCoordinatorComponent->DestroyComponent();
	RunCoordinatorComponent = CandidateRun;
	RunCoordinatorComponent->OnTimeExpired.AddUObject(this, &ThisClass::HandleTimeExpired);
	RunCoordinatorComponent->BeginNotificationBatch();
	RunTuning = Snapshot.RunTuning;
	{
		// 저장 형식에 없는 경제 값(붓기 기준액·신호기 경계)은 현재 서버 값을 유지한다.
		const FDigetherEconomyTuning Current = EconomyTuning;
		EconomyTuning = Snapshot.EconomyTuning;
		EconomyTuning.PourUnitCredits = Current.PourUnitCredits;
		EconomyTuning.PaymentSignalOrangeRatio = Current.PaymentSignalOrangeRatio;
		EconomyTuning.PaymentSignalGreenRatio = Current.PaymentSignalGreenRatio;
	}
	FixedRunRuleTuning = Snapshot.RunRuleTuning;
	GameId = Snapshot.GameId;
	CurrentRunId = Snapshot.RunId;
	CompletedSettlementRunIds = Snapshot.CompletedSettlementRunIds;
	SavedPlayerRecords = Snapshot.Players;
	bHasGameResult = Snapshot.bHasGameResult;
	GameResult = Snapshot.bHasGameResult ? Snapshot.GameResult : FDigetherGameResult();
	ArchivedGameResults = Snapshot.ArchivedGameResults;
	LastJoinOrder = 0;
	if (IsValid(Hub))
	{
		Hub->EndAllRest();
		Hub->ReturnElevatorToHub();
		ensure(Hub->RestoreProgress(Snapshot.HubProgress));
	}
	if (bValidateSavedHubSpawn)
	{
		for (const FDigetherSavePlayerRecord& Record : Records)
		{
			FDigetherSavePlayerRecord* Saved = SavedPlayerRecords.FindByPredicate([&Record](const FDigetherSavePlayerRecord& Entry)
			{
				return Entry.GameplayPlayerId == Record.GameplayPlayerId;
			});
			if (Saved != nullptr)
			{
				*Saved = Record;
			}
		}
	}
	ensure(State->GetQuotaComponent()->SetSignalTuning(EconomyTuning, false) == EDigetherEconomyResult::Success);
	State->GetQuotaComponent()->RestoreState(Snapshot.Quota, false);
	// 기존 바닥 금화를 지우고 저장대로 다시 만든다(이어하기·공통 로딩 실패 복귀·새 게임은 빈 목록).
	ensureMsgf(RestoreFloorCoins(Snapshot.FloorCoins), TEXT("Floor coin restore spawned fewer piles than saved."));
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		Players[Index]->RestoreGameplayIdentity(Records[Index].GameplayPlayerId);
		Players[Index]->RestoreState(Records[Index].Wallet, false);
		Players[Index]->GetLifeComponent()->RestoreState(Records[Index].Life, false);
		Players[Index]->SetJoinOrder(Records[Index].JoinOrder);
		Players[Index]->MarkGameplayInitialized();
		if (APawn* Pawn = Players[Index]->GetGameplayPawn())
		{
			if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
			{
				Movement->StopMovementImmediately();
			}
			Pawn->SetActorTransform(Transforms[Index], false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
	for (ADigetherGameplayPlayerState* Player : Players)
	{
		// 참가 순서를 모르는 이전 저장 기록은 알려진 참가자 다음 번호를 받는다(결과 크레딧 표시 순서).
		if (Player->GetJoinOrder() <= 0)
		{
			Player->SetJoinOrder(AllocateJoinOrder());
		}
	}
	ElevatorActor->GetElevatorComponent()->InitializeForRun(RunTuning, FDigetherElevatorTuning());
	for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Lever : LeverActors)
	{
		if (Lever.IsValid())
		{
			Lever->GetLeverComponent()->ResetPull();
		}
	}
	RunParticipants.Reset();
	DeparturePlayerCount = LastRecordedAscent = PendingRecordedAscent = 0;
	bDeparturePending = bAscentPending = bTimeExpiredPending = bExplorationFinishedPending = false;
	DepartureRequester.Reset();
	DepartureBaseline.Reset();
#if !UE_BUILD_SHIPPING
	bSmokeActive = false;
	SmokeController.Reset();
#endif
	bRestoreRejected = false;
	RunCoordinatorComponent->SetPublicStateComponent(State->GetRunPublicStateComponent());
	if (Snapshot.RunPublicState.bPaymentDeadline && Snapshot.RunPublicState.bGameOver)
	{
		FDigetherQuotaState DeadlineQuota = Snapshot.Quota.Quota;
		DeadlineQuota.Day = DigetherRunRules::PaymentDeadlineDay;
		RunCoordinatorComponent->HandleDepartureLeverCompleted(DeadlineQuota);
	}
	RunCoordinatorComponent->HandleBaseReturn(Snapshot.Quota.Quota);
	if (Snapshot.RunPublicState.bPaymentDeadline && !Snapshot.RunPublicState.bGameOver)
	{
		RunCoordinatorComponent->HandleDepartureLeverCompleted(Snapshot.Quota.Quota);
	}
	for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Lever : LeverActors)
	{
		if (Lever.IsValid())
		{
			Lever->GetLeverComponent()->EndNotificationBatch();
		}
	}
	Elevator->EndNotificationBatch();
	RunCoordinatorComponent->EndNotificationBatch();
	State->GetQuotaComponent()->NotifyStateChanged();
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		Players[Index]->ClearRunResult();
		Players[Index]->GetWalletComponent()->NotifyStateChanged();
		Players[Index]->GetLifeComponent()->NotifyStateChanged();
		if (ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(Players[Index]->GetOwner()))
		{
			Controller->Client_SetLifeSpectating(Records[Index].Life.PublicState.bSpectating);
			if (APawn* Pawn = Controller->GetPawn())
			{
				Controller->ClientSetLocation(Transforms[Index].GetLocation(), Transforms[Index].GetRotation().Rotator());
			}
		}
	}
	for (const FDigetherPourSession& Stopped : StoppedPours)
	{
		NotifyPourStopped(Stopped, EDigetherPourStopReason::Loaded);
	}
	if (CorrectedSpawnCount > 0)
	{
		UE_LOG(LogDigetherFramework, Log, TEXT("Replaced %d saved hub positions with safe PlayerStarts; wallets and run progress retained."),
			CorrectedSpawnCount);
		NotifySaveResult(nullptr, true, FText::Format(NSLOCTEXT("DigetherSave", "HubSpawnCorrected",
			"거점 배치가 바뀌어 {0}명의 위치를 안전한 시작점으로 옮겼습니다. 저장된 진행은 유지됩니다."),
			FText::AsNumber(CorrectedSpawnCount)));
	}
	return true;
}

EDigetherSaveResult ADigetherGameplayGameMode::WriteSnapshot(const FDigetherSaveSnapshot& Snapshot,
	const bool bExplicitNewGame)
{
	const UGameInstance* Instance = GetGameInstance();
	UDigetherSaveSubsystem* Save = IsValid(Instance) ? Instance->GetSubsystem<UDigetherSaveSubsystem>() : nullptr;
	if (!HasAuthority() || !IsValid(Save))
	{
		return EDigetherSaveResult::NotAuthority;
	}
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		ADigetherGameplayPlayerController* Host = Cast<ADigetherGameplayPlayerController>(Iterator->Get());
		if (!IsHostRequester(Host) || !IsValid(Host->PlayerState))
		{
			continue;
		}
		// 크레딧 뒤 새 게임은 슬롯에 아직 이전 게임이 있을 수 있다. 그 새 게임의 첫 쓰기가 성공할 때까지는 일반 저장도
		// 명시 새 게임으로 써야 이전 게임 ID 검사(TryBuildSlot)에 막히지 않는다. 다른 게임 ID에는 적용하지 않는다.
		const bool bWriteAsNewGame = bExplicitNewGame
			|| (ExplicitNewGamePendingGameId.IsValid() && Snapshot.GameId == ExplicitNewGamePendingGameId);
		EDigetherSaveResult Result = Save->BindHostIdentity(Host->PlayerState->GetUniqueId());
		if (Result == EDigetherSaveResult::Success)
		{
			Result = Save->Write(Snapshot, bWriteAsNewGame);
		}
		UE_LOG(LogDigetherFramework, Log, TEXT("저장 결과=%s"), LexToString(Result));
		if (Result == EDigetherSaveResult::Success)
		{
			// 이 쓰기가 실패한 이전 시점보다 최신 진행이다. 오래된 재시도가 덮어쓰지 않게 버린다(S30 예외).
			ClearPendingAutosave();
			if (bWriteAsNewGame && Snapshot.GameId == ExplicitNewGamePendingGameId)
			{
				ExplicitNewGamePendingGameId.Invalidate();
				// 같은 방에서 게임 맵을 다시 열어도 새 게임을 또 만들지 않고 이 슬롯을 이어 간다.
				Save->CompleteNewGameRequest();
			}
		}
		return Result;
	}
	return EDigetherSaveResult::HostIdentityMissing;
}

bool ADigetherGameplayGameMode::WriteProgress()
{
	FDigetherSaveSnapshot Snapshot;
	return CaptureProgress(Snapshot) && WriteSnapshot(Snapshot, false) == EDigetherSaveResult::Success;
}

void ADigetherGameplayGameMode::RequestAutosave(const TCHAR* Reason)
{
	FDigetherSaveSnapshot Snapshot;
	if (CaptureProgress(Snapshot))
	{
		RequestAutosaveSnapshot(Snapshot, Reason);
		return;
	}
	if (!HasAuthority() || !bRunReady || bRestoreRejected)
	{
		return;
	}
	// 저장 시점의 값을 만들지 못했다. 이전 유효 저장은 그대로 두고, 재시도 때 거점 상태에서 다시 캡처한다.
	UE_LOG(LogDigetherFramework, Warning, TEXT("자동 저장(%s) 캡처 실패. %.0f초 간격으로 다시 시도합니다."),
		Reason, DigetherSaveTuning::AutosaveRetrySeconds);
	PendingAutosave.Reset();
	bAutosaveRecapturePending = true;
	NotifySaveResult(nullptr, false, NSLOCTEXT("DigetherSave", "AutosaveFailedRetrying",
		"자동 저장에 실패했습니다. 진행은 계속되며 자동으로 다시 시도합니다."));
	GetWorldTimerManager().SetTimer(AutosaveRetryTimer, this, &ThisClass::RetryPendingAutosave,
		DigetherSaveTuning::AutosaveRetrySeconds, true);
}

void ADigetherGameplayGameMode::RequestAutosaveSnapshot(const FDigetherSaveSnapshot& Snapshot, const TCHAR* Reason)
{
	// 시작 시 거절한 저장(손상·미래 버전·다른 게임)은 자동 저장으로 덮지 않는다.
	if (!HasAuthority() || !bRunReady || bRestoreRejected)
	{
		return;
	}
	const EDigetherSaveResult Result = WriteSnapshot(Snapshot, false);
	if (Result == EDigetherSaveResult::Success)
	{
		UE_LOG(LogDigetherFramework, Log, TEXT("자동 저장(%s) 완료."), Reason);
		return;
	}
	// 기획 S30·T91: 실패를 정상 저장으로 표시하지 않고 알린 뒤 계속 재시도한다. 출발은 막지 않는다.
	// 이전 실패 건은 이 더 최신 저장 시점의 값으로 바꾼다.
	UE_LOG(LogDigetherFramework, Warning, TEXT("자동 저장(%s) 실패: 결과=%s. %.0f초 간격으로 다시 시도합니다."),
		Reason, LexToString(Result), DigetherSaveTuning::AutosaveRetrySeconds);
	PendingAutosave = Snapshot;
	bAutosaveRecapturePending = false;
	NotifySaveResult(nullptr, false, NSLOCTEXT("DigetherSave", "AutosaveFailedRetrying",
		"자동 저장에 실패했습니다. 진행은 계속되며 자동으로 다시 시도합니다."));
	GetWorldTimerManager().SetTimer(AutosaveRetryTimer, this, &ThisClass::RetryPendingAutosave,
		DigetherSaveTuning::AutosaveRetrySeconds, true);
}

void ADigetherGameplayGameMode::RetryPendingAutosave()
{
	if (!HasAuthority() || !bRunReady || bRestoreRejected
		|| (!PendingAutosave.IsSet() && !bAutosaveRecapturePending))
	{
		ClearPendingAutosave();
		return;
	}
	FDigetherSaveSnapshot Snapshot;
	if (PendingAutosave.IsSet())
	{
		Snapshot = PendingAutosave.GetValue();
	}
	else if (!CaptureProgress(Snapshot))
	{
		// 탐사 중에는 캡처할 수 없다. 거점에 돌아오면 다시 시도하거나 다음 저장 시점이 대신한다.
		return;
	}
	if (Snapshot.GameId != GameId)
	{
		// 불러오기·새 게임으로 다른 진행이 되었으면 이전 진행의 실패 건을 쓰지 않는다.
		ClearPendingAutosave();
		return;
	}
	const EDigetherSaveResult Result = WriteSnapshot(Snapshot, false);
	if (Result != EDigetherSaveResult::Success)
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("자동 저장 재시도 실패: 결과=%s. 이전 유효 저장을 보존합니다."),
			LexToString(Result));
		return;
	}
	UE_LOG(LogDigetherFramework, Log, TEXT("자동 저장 재시도 성공."));
	NotifySaveResult(nullptr, true, NSLOCTEXT("DigetherSave", "AutosaveRetrySucceeded",
		"자동 저장을 다시 시도해 저장했습니다."));
}

void ADigetherGameplayGameMode::ClearPendingAutosave()
{
	PendingAutosave.Reset();
	bAutosaveRecapturePending = false;
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutosaveRetryTimer);
	}
}

void ADigetherGameplayGameMode::HandleWorldBeginTearDown(UWorld* World)
{
	if (World != GetWorld() || bHostExitSaveHandled)
	{
		return;
	}
	bHostExitSaveHandled = true;
	SaveOnHostExit();
}

bool ADigetherGameplayGameMode::SaveOnHostExit()
{
	const TOptional<FDigetherSaveSnapshot> Pending = PendingAutosave;
	// 종료를 재시도 대기로 붙잡지 않는다(S30 예외). 아래 시도가 마지막이다.
	ClearPendingAutosave();
	if (!HasAuthority() || !bRunReady || bRestoreRejected)
	{
		return false;
	}
	const FDigetherRunPublicState Run = RunCoordinatorComponent->CapturePublicState();
	const bool bExploring = Run.Phase == EDigetherRunPhase::InProgress;
	if (bExploring || Run.bGameOver)
	{
		// 탐사 중 종료는 현재 탐사를 저장하지 않는다. 게임 오버 중에는 귀환 저장(새 게임 초기화)을 유지한다.
		// 그 정상 저장 시점이 실패해 재시도 중이었다면 그 값만 한 번 더 써 본다.
		const bool bWrote = Pending.IsSet() && Pending->GameId == GameId
			&& WriteSnapshot(Pending.GetValue(), false) == EDigetherSaveResult::Success;
		UE_LOG(LogDigetherFramework, Log, TEXT("호스트 종료: %s 중이라 현재 상태를 저장하지 않습니다. 대기 저장 기록=%d"),
			bExploring ? TEXT("탐사") : TEXT("게임 오버 처리"), bWrote);
		return bWrote;
	}
	FDigetherSaveSnapshot Snapshot;
	const EDigetherSaveResult Result = CaptureProgress(Snapshot)
		? WriteSnapshot(Snapshot, false) : EDigetherSaveResult::InvalidState;
	if (Result != EDigetherSaveResult::Success)
	{
		UE_LOG(LogDigetherFramework, Error, TEXT("호스트 종료 저장 실패: 결과=%s. 저장 없이 종료하며 이전 유효 저장을 보존합니다."),
			LexToString(Result));
		NotifySaveResult(nullptr, false, NSLOCTEXT("DigetherSave", "HostExitSaveFailed",
			"종료 저장에 실패했습니다. 저장하지 않고 종료하며, 다음 실행은 마지막 유효 저장에서 시작합니다."));
		return false;
	}
	UE_LOG(LogDigetherFramework, Log, TEXT("호스트 종료 저장 완료."));
	return true;
}

bool ADigetherGameplayGameMode::SaveProgress(ADigetherGameplayPlayerController* Requester)
{
	const bool bSucceeded = IsHostRequester(Requester) && !bRestoreRejected
		&& !RunCoordinatorComponent->CapturePublicState().bGameOver && WriteProgress();
	NotifySaveResult(Requester, bSucceeded, bSucceeded
		? NSLOCTEXT("DigetherSave", "Saved", "진행을 저장했습니다.")
		: NSLOCTEXT("DigetherSave", "SaveRejected", "저장을 거절했습니다. 호스트·인증 ID·거점 대기 상태와 저장 오류를 확인하세요."));
	return bSucceeded;
}

bool ADigetherGameplayGameMode::LoadProgress(ADigetherGameplayPlayerController* Requester, const bool bStartup)
{
	// 기획 S33: 실패 사건·크레딧 중에는 불러오기를 받지 않는다. 결과 기록·새 게임 생성을 우회하지 않게 한다.
	if (!IsHostRequester(Requester) || !IsValid(Requester->PlayerState)
		|| RunCoordinatorComponent->CapturePublicState().Phase == EDigetherRunPhase::InProgress || IsGameOverActive())
	{
		NotifySaveResult(Requester, false, NSLOCTEXT("DigetherSave", "LoadPhaseRejected", "불러오기는 호스트가 거점 대기 상태에서 요청해야 합니다."));
		return false;
	}
	const bool bStartupLoad = bStartup && !bHasSaveStartupAttempted;
	bHasSaveStartupAttempted = true;
	UDigetherSaveSubsystem* Save = GetGameInstance()->GetSubsystem<UDigetherSaveSubsystem>();
	FDigetherSaveSnapshot Snapshot;
	EDigetherSaveResult Result = Save->BindHostIdentity(Requester->PlayerState->GetUniqueId());
	if (Result == EDigetherSaveResult::Success)
	{
		Result = Save->Read(Snapshot);
	}
	if (Result == EDigetherSaveResult::NotFound)
	{
		// 시작 시 빈 슬롯은 새 진행이라 알림 생략
		if (!bStartup)
		{
			NotifySaveResult(Requester, false, NSLOCTEXT("DigetherSave", "LoadNotFound", "저장한 진행을 불러오지 못했습니다."));
		}
		return false;
	}
	if (Result == EDigetherSaveResult::Success && Snapshot.RunPublicState.bGameOver)
	{
		// 기획 S33 240행: 게임 오버 확정이 저장된 뒤 종료했으면 다음 실행에서 같은 슬롯 새 게임을 시작한다.
		// 실패 사건·크레딧은 다시 재생하지 않는다. 결과는 보관 목록에 남긴다.
		const bool bNewGame = StartNewGameAfterSavedGameOver(Requester, Snapshot);
		if (!bNewGame)
		{
			bRestoreRejected |= bStartupLoad;
			UE_LOG(LogDigetherFramework, Error, TEXT("게임 오버 확정 저장의 새 게임 전환 실패. 원본 슬롯을 보존합니다."));
		}
		NotifySaveResult(Requester, bNewGame, bNewGame
			? NSLOCTEXT("DigetherSave", "SavedGameOverNewGame", "지난 게임이 끝나 같은 슬롯에서 새 게임을 시작했습니다.")
			: NSLOCTEXT("DigetherSave", "LoadRejected", "불러오기를 거절했습니다. 기존 저장은 보존됩니다."));
		return bNewGame;
	}
	const bool bSucceeded = Result == EDigetherSaveResult::Success && RestoreProgress(Snapshot);
	if (!bSucceeded)
	{
		// 시작 시 거절한 저장만 새 게임으로 덮이는 것을 막는다. 수동 불러오기 실패는 현재 진행을 유지한다.
		bRestoreRejected |= bStartupLoad;
		UE_LOG(LogDigetherFramework, Error, TEXT("불러오기 거절: 결과=%s. 원본 슬롯을 보존합니다. 미래 버전은 새 게임으로도 초기화할 수 없습니다."), LexToString(Result));
	}
	else
	{
		// 불러온 진행이 현재 진행이다. 이전 진행의 실패 건을 다시 쓰지 않는다.
		ClearPendingAutosave();
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* Controller = Iterator->Get();
			if (IsValid(Controller) && !IsValid(Controller->GetPawn()))
			{
				HandleStartingNewPlayer(Controller);
			}
		}
	}
	NotifySaveResult(Requester, bSucceeded, bSucceeded
		? NSLOCTEXT("DigetherSave", "Loaded", "저장된 진행을 불러왔습니다.")
		: NSLOCTEXT("DigetherSave", "LoadRejected", "불러오기를 거절했습니다. 기존 저장은 보존됩니다."));
	return bSucceeded;
}

bool ADigetherGameplayGameMode::RunStartupSaveAction(ADigetherGameplayPlayerController* Host)
{
	const UGameInstance* Instance = GetGameInstance();
	const UDigetherSaveSubsystem* Save = IsValid(Instance) ? Instance->GetSubsystem<UDigetherSaveSubsystem>() : nullptr;
	const FDigetherSaveSlotSelection Selection = IsValid(Save) ? Save->GetSlotSelection() : FDigetherSaveSlotSelection();
	const bool bNewGame = DigetherSaveRules::ShouldStartNewGame(Selection,
		UGameplayStatics::HasOption(OptionsString, TEXT("DigetherNewGame")));
	UE_LOG(LogDigetherFramework, Log, TEXT("시작 진행 슬롯=%d 동작=%s"), Selection.SlotIndex,
		bNewGame ? TEXT("새 게임") : TEXT("이어하기"));
	if (bNewGame)
	{
		StartNewGame(Host);
	}
	else
	{
		// 비어 있는 슬롯은 새 진행으로 시작하고 첫 자동 저장이 이 슬롯에 쓴다. 손상·미래 버전은 거절해 보존한다.
		LoadProgress(Host, true);
	}
	return bNewGame;
}

bool ADigetherGameplayGameMode::BuildNewGameSnapshot(const ADigetherGameplayPlayerController* Host,
	FDigetherSaveSnapshot& OutSnapshot) const
{
	if (!IsHostRequester(Host) || !IsValid(Host->PlayerState) || !IsValid(GameState))
	{
		return false;
	}
	FDigetherSaveSnapshot Initial;
	Initial.GameId = FGuid::NewGuid();
	Initial.RunId = FGuid::NewGuid();
	Initial.RunTuning = RunTuning;
	Initial.Quota.Quota = FDigetherEconomyRules::MakeInitialQuotaState(RunTuning);
	// 이전 게임 결과는 새 게임과 따로 보관한다(S33 240행). 새 게임 자체의 결과·통계는 없다.
	Initial.ArchivedGameResults = ArchivedGameResults;
	TArray<ADigetherGameplayPlayerState*> Players;
	CollectPlayers(Players);
	for (APlayerState* Entry : GameState->PlayerArray)
	{
		if (ADigetherGameplayPlayerState* Player = Cast<ADigetherGameplayPlayerState>(Entry))
		{
			Players.AddUnique(Player);
		}
	}
	// 새 게임의 참가 순서는 지금 방에 있는 사람의 이전 참가 순서를 유지해 1부터 다시 매긴다. 모르는 사람은 뒤에 둔다.
	Algo::StableSortBy(Players, [](const ADigetherGameplayPlayerState* Player)
	{
		return Player->GetJoinOrder() > 0 ? Player->GetJoinOrder() : MAX_int32;
	});
	int32 NextJoinOrder = 0;
	for (ADigetherGameplayPlayerState* Player : Players)
	{
		FDigetherSavePlayerRecord Record;
		if (DigetherSaveRules::MakeAuthenticatedPlayerKey(Player->GetUniqueId(), Record.AuthenticatedPlayerKey)
			!= EDigetherSaveResult::Success)
		{
			return false;
		}
		// 통계·잔고·생존 상태는 새로 시작한다. 이전 참가자의 통계를 넘기지 않는다(S33 231행).
		Record.GameplayPlayerId = FGuid::NewGuid();
		Record.Wallet.BalanceCredits = DigetherFrameworkDefaults::StartingBalanceCredits;
		Record.DisplayName = DigetherSaveRules::MakeStoredDisplayName(Player->GetPlayerName());
		Record.JoinOrder = ++NextJoinOrder;
		Initial.Players.Add(Record);
		if (Player == Host->PlayerState)
		{
			Initial.HostAuthenticatedPlayerKey = Record.AuthenticatedPlayerKey;
		}
	}
	if (DigetherSaveRules::ValidateSnapshot(Initial) != EDigetherSaveResult::Success)
	{
		return false;
	}
	OutSnapshot = MoveTemp(Initial);
	return true;
}

bool ADigetherGameplayGameMode::StartNewGame(ADigetherGameplayPlayerController* Requester)
{
	// 기획 S33: 실패 사건·크레딧 중 새 게임 요청은 받지 않는다. 크레딧이 끝나면 호스트가 한 번 자동으로 만든다.
	if (!IsHostRequester(Requester) || !ElevatorActor.IsValid()
		|| RunCoordinatorComponent->CapturePublicState().Phase == EDigetherRunPhase::InProgress || IsGameOverActive())
	{
		NotifySaveResult(Requester, false, NSLOCTEXT("DigetherSave", "NewGameRejected", "새 게임을 시작하지 못했습니다."));
		return false;
	}
	FDigetherSaveSnapshot Initial;
	TArray<ADigetherGameplayPlayerState*> Players;
	CollectPlayers(Players);
	for (APlayerState* Entry : GameState->PlayerArray)
	{
		if (ADigetherGameplayPlayerState* Player = Cast<ADigetherGameplayPlayerState>(Entry))
		{
			Players.AddUnique(Player);
		}
	}
	UDigetherSaveSubsystem* Save = GetGameInstance()->GetSubsystem<UDigetherSaveSubsystem>();
	if (!BuildNewGameSnapshot(Requester, Initial) || !RestoreProgress(Initial, true)
		|| Save->BindHostIdentity(Requester->PlayerState->GetUniqueId()) != EDigetherSaveResult::Success)
	{
		NotifySaveResult(Requester, false, NSLOCTEXT("DigetherSave", "NewGameFailed", "새 게임 저장에 실패했습니다."));
		return false;
	}
	const EDigetherSaveResult WriteResult = Save->Write(Initial, true);
	if (WriteResult != EDigetherSaveResult::Success)
	{
		NotifySaveResult(Requester, false, WriteResult == EDigetherSaveResult::UnsupportedVersion
			? NSLOCTEXT("DigetherSave", "NewGameFutureVersionRejected", "미래 버전 저장이 있어 새 게임을 거절했습니다. 기존 저장은 보존됩니다.")
			: NSLOCTEXT("DigetherSave", "NewGameFailed", "새 게임 저장에 실패했습니다."));
		return false;
	}
	ClearPendingAutosave();
	ExplicitNewGamePendingGameId.Invalidate();
	// 슬롯에 새 게임을 저장했다. 같은 방에서 게임 맵을 다시 열어도 다시 초기화하지 않고 이 슬롯을 이어 간다.
	Save->CompleteNewGameRequest();
	const bool bSucceeded = RestoreProgress(Initial);
	bHasSaveStartupAttempted = true;
	if (bSucceeded)
	{
		for (ADigetherGameplayPlayerState* Player : Players)
		{
			if (APlayerController* Controller = Cast<APlayerController>(Player->GetOwner()))
			{
				HandleStartingNewPlayer(Controller);
			}
		}
	}
	NotifySaveResult(Requester, bSucceeded, NSLOCTEXT("DigetherSave", "NewGame", "새 게임을 초기화했습니다."));
	return bSucceeded;
}

bool ADigetherGameplayGameMode::RecoverFromCommonLoadingFailure()
{
	ADigetherGameplayPlayerController* Requester = DepartureRequester.Get();
	// 로딩 단계 = 출발 확정 뒤 갱도에 처음 도착(제한 시간 시작)하기 전이다. 그 뒤의 사건은 되돌리지 않는다.
	const bool bLoading = HasAuthority() && bRunReady && !bApplyingRunEnd
		&& RunCoordinatorComponent->CapturePublicState().Phase == EDigetherRunPhase::InProgress
		&& !RunCoordinatorComponent->IsClockStarted();
	const bool bHasBaseline = DepartureBaseline.IsSet() && DepartureBaseline->GameId == GameId
		&& DepartureBaseline->RunId == CurrentRunId;
	if (!bLoading || !bHasBaseline)
	{
		UE_LOG(LogDigetherFramework, Error, TEXT("공통 로딩 실패 복귀 거절: 로딩 단계=%d 기준점=%d."), bLoading, bHasBaseline);
		NotifySaveResult(Requester, false, NSLOCTEXT("DigetherSave", "LoadingFailureRecoveryRejected",
			"출발 로딩 실패 복귀를 처리하지 못했습니다. 현재 상태와 저장 파일을 보존합니다."));
		return false;
	}
	// 기획 S02·S30: 디스크 저장을 읽거나 쓰지 않는다. 문 닫힘 완료 순간의 확정된 준비(구매·납부·해금·배치)와
	// 게이지 완료 때 기록한 4일차 마감을 그대로 두고, 일차를 소비하지 않은 같은 일차 거점으로 돌아간다.
	const FDigetherSaveSnapshot Baseline = DepartureBaseline.GetValue();
	const FGuid AbandonedRunId = CurrentRunId;
	const bool bSucceeded = RestoreProgress(Baseline);
	if (bSucceeded)
	{
		// 출발 확정 때 시작한 몬스터 런(서식지 배치·예고 예약)을 거점 복귀와 함께 정리
		if (UDigetherMonsterDirectorSubsystem* Director = GetWorld()->GetSubsystem<UDigetherMonsterDirectorSubsystem>())
		{
			Director->EndRun(AbandonedRunId);
		}
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* Controller = Iterator->Get();
			// 관전 합류자는 잠금이 풀린 다음 Tick의 관전 해제에서 합류한다.
			if (IsValid(Controller) && !IsValid(Controller->GetPawn()) && !IsJoinSpectator(Controller))
			{
				HandleStartingNewPlayer(Controller);
			}
		}
		LogRunStep(TEXT("CommonLoadingFailureReturned"));
	}
	NotifySaveResult(Requester, bSucceeded, bSucceeded
		? NSLOCTEXT("DigetherSave", "LoadingFailureReturned", "출발 로딩에 실패해 같은 일차의 거점으로 돌아왔습니다. 준비 내용은 유지됩니다.")
		: NSLOCTEXT("DigetherSave", "LoadingFailureReturnFailed", "출발 로딩 실패 복귀에 실패했습니다. 현재 상태와 저장 파일을 보존합니다."));
	return bSucceeded;
}
