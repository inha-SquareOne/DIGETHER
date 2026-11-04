#include "Game/Framework/DigetherGameplayGameMode.h"

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
#include "Game/Run/DigetherRunCoordinatorComponent.h"
#include "Game/Run/DigetherRunPublicStateComponent.h"
#include "Game/Save/DigetherSaveRules.h"
#include "Game/Save/DigetherSaveSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"

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
	if (!HasAuthority() || !bRunReady || !IsValid(State) || !ElevatorActor.IsValid()
		|| RunCoordinatorComponent->CapturePublicState().Phase == EDigetherRunPhase::InProgress
		|| ElevatorActor->GetElevatorComponent()->GetPublicState().Phase != EDigetherElevatorPhase::HubWaiting)
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
		if (UPawnMovementComponent* Movement = Player->GetGameplayPawn()->GetMovementComponent())
		{
			Movement->StopMovementImmediately();
		}
		Player->GetGameplayPawn()->SetActorTransform(Record.HubTransform, false, nullptr, ETeleportType::TeleportPhysics);
		if (APlayerController* Controller = Cast<APlayerController>(Player->GetOwner()))
		{
			Controller->ClientSetLocation(Record.HubTransform.GetLocation(), Record.HubTransform.GetRotation().Rotator());
		}
	}
}

bool ADigetherGameplayGameMode::RestoreProgress(const FDigetherSaveSnapshot& Snapshot, const bool bValidateOnly)
{
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
	// 시작 전 미초기화 PS도 대조한다. 기준점 이후 참가한 다른 ID의 복구 정책은 기획 확인 대상이다.
	for (APlayerState* Entry : GameState->PlayerArray)
	{
		if (ADigetherGameplayPlayerState* Player = Cast<ADigetherGameplayPlayerState>(Entry))
		{
			Players.AddUnique(Player);
		}
	}
	TArray<FDigetherSavePlayerRecord> Records;
	TArray<FTransform> Transforms;
	APlayerStart* DefaultStart = nullptr;
	for (TActorIterator<APlayerStart> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		DefaultStart = *Iterator;
		break;
	}
	for (ADigetherGameplayPlayerState* Player : Players)
	{
		FDigetherSavePlayerRecord Record;
		bool bSkip = false;
		if (!Player->HasAuthority() || !IsValid(Player->GetWalletComponent()) || !IsValid(Player->GetLifeComponent())
			|| DigetherSaveRules::TryResolveReturningPlayer(Snapshot.Players, Player->GetUniqueId(), Record, bSkip)
				!= EDigetherSaveResult::Success || (!Record.bHasHubTransform && !IsValid(DefaultStart)))
		{
			return false;
		}
		Records.Add(Record);
		Transforms.Add(Record.bHasHubTransform ? Record.HubTransform : DefaultStart->GetActorTransform());
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
	const FDigetherDepositSessionCloseResult Closed = DepositSessions.CloseAllSessions(EDigetherDepositSessionCloseReason::PeriodChanged);
	RunCoordinatorComponent->OnTimeExpired.RemoveAll(this);
	RunCoordinatorComponent->SetPublicStateComponent(nullptr);
	RunCoordinatorComponent->DestroyComponent();
	RunCoordinatorComponent = CandidateRun;
	RunCoordinatorComponent->OnTimeExpired.AddUObject(this, &ThisClass::HandleTimeExpired);
	RunCoordinatorComponent->BeginNotificationBatch();
	RunTuning = Snapshot.RunTuning;
	EconomyTuning = Snapshot.EconomyTuning;
	FixedRunRuleTuning = Snapshot.RunRuleTuning;
	GameId = Snapshot.GameId;
	CurrentRunId = Snapshot.RunId;
	CompletedSettlementRunIds = Snapshot.CompletedSettlementRunIds;
	SavedPlayerRecords = Snapshot.Players;
	State->GetQuotaComponent()->RestoreState(Snapshot.Quota, false);
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		Players[Index]->RestoreGameplayIdentity(Records[Index].GameplayPlayerId);
		Players[Index]->RestoreState(Records[Index].Wallet, false);
		Players[Index]->GetLifeComponent()->RestoreState(Records[Index].Life, false);
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
	NotifyClosedSessions(Closed, EDigetherEconomyResult::PeriodMismatch);
	return true;
}

bool ADigetherGameplayGameMode::WriteProgress(const bool bCheckpoint, const bool bExplicitNewGame)
{
	FDigetherSaveSnapshot Snapshot;
	if (!CaptureProgress(Snapshot))
	{
		return false;
	}
	UDigetherSaveSubsystem* Save = GetGameInstance()->GetSubsystem<UDigetherSaveSubsystem>();
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		ADigetherGameplayPlayerController* Host = Cast<ADigetherGameplayPlayerController>(Iterator->Get());
		if (IsHostRequester(Host) && IsValid(Host->PlayerState) && IsValid(Save)
			&& Save->BindHostIdentity(Host->PlayerState->GetUniqueId()) == EDigetherSaveResult::Success)
		{
			const EDigetherSaveResult Result = Save->Write(Snapshot, bExplicitNewGame, bCheckpoint);
			UE_LOG(LogDigetherFramework, Log, TEXT("저장 결과=%d 기준점=%d"), static_cast<int32>(Result), bCheckpoint);
			return Result == EDigetherSaveResult::Success && (!bCheckpoint || Save->IsCheckpointCurrent(Snapshot));
		}
	}
	return false;
}

bool ADigetherGameplayGameMode::SaveProgress(ADigetherGameplayPlayerController* Requester)
{
	const bool bSucceeded = IsHostRequester(Requester) && !bRestoreRejected
		&& !RunCoordinatorComponent->CapturePublicState().bGameOver && WriteProgress(false);
	NotifySaveResult(Requester, bSucceeded, bSucceeded
		? NSLOCTEXT("DigetherSave", "Saved", "진행을 저장했습니다.")
		: NSLOCTEXT("DigetherSave", "SaveRejected", "저장을 거절했습니다. 호스트·인증 ID·거점 대기 상태와 저장 오류를 확인하세요."));
	return bSucceeded;
}

bool ADigetherGameplayGameMode::LoadProgress(ADigetherGameplayPlayerController* Requester, const bool bStartup)
{
	if (!IsHostRequester(Requester) || !IsValid(Requester->PlayerState)
		|| RunCoordinatorComponent->CapturePublicState().Phase == EDigetherRunPhase::InProgress)
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
		return false;
	}
	const bool bSucceeded = Result == EDigetherSaveResult::Success && RestoreProgress(Snapshot);
	if (!bSucceeded)
	{
		// 시작 시 거절한 저장만 새 게임으로 덮이는 것을 막는다. 수동 불러오기 실패는 현재 진행을 유지한다.
		bRestoreRejected |= bStartupLoad;
		UE_LOG(LogDigetherFramework, Error, TEXT("불러오기 거절: 결과=%d. 원본 슬롯을 보존합니다. 미래 버전은 새 게임으로도 초기화할 수 없습니다."), static_cast<int32>(Result));
	}
	else
	{
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

bool ADigetherGameplayGameMode::StartNewGame(ADigetherGameplayPlayerController* Requester)
{
	if (!IsHostRequester(Requester) || !ElevatorActor.IsValid()
		|| RunCoordinatorComponent->CapturePublicState().Phase == EDigetherRunPhase::InProgress)
	{
		return false;
	}
	FDigetherSaveSnapshot Initial;
	Initial.GameId = FGuid::NewGuid();
	Initial.RunId = FGuid::NewGuid();
	Initial.RunTuning = RunTuning;
	Initial.Quota.Quota = FDigetherEconomyRules::MakeInitialQuotaState(RunTuning);
	TArray<ADigetherGameplayPlayerState*> Players;
	CollectPlayers(Players);
	for (APlayerState* Entry : GameState->PlayerArray)
	{
		if (ADigetherGameplayPlayerState* Player = Cast<ADigetherGameplayPlayerState>(Entry))
		{
			Players.AddUnique(Player);
		}
	}
	for (ADigetherGameplayPlayerState* Player : Players)
	{
		FDigetherSavePlayerRecord Record;
		if (DigetherSaveRules::MakeAuthenticatedPlayerKey(Player->GetUniqueId(), Record.AuthenticatedPlayerKey)
			!= EDigetherSaveResult::Success)
		{
			return false;
		}
		Record.GameplayPlayerId = FGuid::NewGuid();
		Record.Wallet.BalanceCredits = DigetherFrameworkDefaults::StartingBalanceCredits;
		Initial.Players.Add(Record);
		if (Player == Requester->PlayerState)
		{
			Initial.HostAuthenticatedPlayerKey = Record.AuthenticatedPlayerKey;
		}
	}
	UDigetherSaveSubsystem* Save = GetGameInstance()->GetSubsystem<UDigetherSaveSubsystem>();
	if (DigetherSaveRules::ValidateSnapshot(Initial) != EDigetherSaveResult::Success || !RestoreProgress(Initial, true)
		|| Save->BindHostIdentity(Requester->PlayerState->GetUniqueId()) != EDigetherSaveResult::Success)
	{
		NotifySaveResult(Requester, false, NSLOCTEXT("DigetherSave", "NewGameFailed", "새 게임 저장에 실패했습니다."));
		return false;
	}
	const EDigetherSaveResult WriteResult = Save->Write(Initial, true, true);
	if (WriteResult != EDigetherSaveResult::Success)
	{
		NotifySaveResult(Requester, false, WriteResult == EDigetherSaveResult::UnsupportedVersion
			? NSLOCTEXT("DigetherSave", "NewGameFutureVersionRejected", "미래 버전 저장이 있어 새 게임을 거절했습니다. 기존 저장은 보존됩니다.")
			: NSLOCTEXT("DigetherSave", "NewGameFailed", "새 게임 저장에 실패했습니다."));
		return false;
	}
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

bool ADigetherGameplayGameMode::RollbackDepartureAfterLoadingFailure()
{
	if (!HasAuthority() || !bRunReady)
	{
		return false;
	}
	FDigetherSaveSnapshot Checkpoint;
	ADigetherGameplayPlayerController* Requester = DepartureRequester.Get();
	UDigetherSaveSubsystem* Save = GetGameInstance()->GetSubsystem<UDigetherSaveSubsystem>();
	const bool bSucceeded = Save->Read(Checkpoint, true) == EDigetherSaveResult::Success
		&& Checkpoint.GameId == GameId && Checkpoint.RunId == CurrentRunId && RestoreProgress(Checkpoint);
	if (bSucceeded)
	{
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
		? NSLOCTEXT("DigetherSave", "DepartureRestored", "출발 로딩 실패로 출발 전 저장을 복원했습니다.")
		: NSLOCTEXT("DigetherSave", "DepartureRestoreFailed", "출발 전 저장 복원에 실패했습니다. 상태와 저장 파일을 보존합니다."));
	return bSucceeded;
}

bool ADigetherGameplayGameMode::ResetProgressAfterGameOver()
{
	FDigetherSaveSnapshot Initial;
	if (!CaptureProgress(Initial))
	{
		return false;
	}
	// 게임 오버 화면의 확정 결과는 런타임에 남기고, 슬롯만 다음 시작의 초기 진행으로 만든다.
	// 자동 초기화는 유효한 같은 게임 파일에만 쓴다. 손상·미래 버전 거절을 우회하지 않는다.
	Initial.RunId = FGuid::NewGuid();
	Initial.Quota = FDigetherQuotaPublicState();
	Initial.Quota.Quota = FDigetherEconomyRules::MakeInitialQuotaState(RunTuning);
	Initial.RunPublicState = FDigetherRunPublicState();
	Initial.CompletedSettlementRunIds.Reset();
	for (FDigetherSavePlayerRecord& Player : Initial.Players)
	{
		Player.GameplayPlayerId = FGuid::NewGuid();
		Player.Wallet = FDigetherWalletState();
		Player.Wallet.BalanceCredits = DigetherFrameworkDefaults::StartingBalanceCredits;
		Player.Life = FDigetherLifeComponentState();
		Player.bHasHubTransform = false;
		Player.HubTransform = FTransform::Identity;
	}
	UDigetherSaveSubsystem* Save = GetGameInstance()->GetSubsystem<UDigetherSaveSubsystem>();
	return Save->Write(Initial, false, true) == EDigetherSaveResult::Success;
}
