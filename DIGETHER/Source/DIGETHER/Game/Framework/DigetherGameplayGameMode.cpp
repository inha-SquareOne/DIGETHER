#include "Game/Framework/DigetherGameplayGameMode.h"

#include "Engine/World.h"
#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Economy/DigetherQuotaStateComponent.h"
#include "Game/Economy/DigetherWalletComponent.h"
#include "Game/Elevator/DigetherElevatorLeverActor.h"
#include "Game/Framework/DigetherFrameworkDefaults.h"
#include "Game/Framework/DigetherFrameworkLog.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Framework/DigetherTestPawn.h"
#include "Game/Online/DigetherJoinRules.h"
#include "Game/Online/DigetherRoomProtocol.h"
#include "Game/Online/DigetherSessionText.h"
#include "Game/Run/DigetherRunCoordinatorComponent.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Save/DigetherSaveRules.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystem.h"

namespace
{
	bool TryGetDepositPlayerKey(const ADigetherGameplayPlayerController* Controller, FString& OutPlayerKey)
	{
		const ADigetherGameplayPlayerState* State = IsValid(Controller)
			? Controller->GetPlayerState<ADigetherGameplayPlayerState>() : nullptr;
		if (!IsValid(State))
		{
			return false;
		}
		const FUniqueNetIdRepl& UniqueId = State->GetUniqueId();
		if (UniqueId.IsValid())
		{
			return DigetherSaveRules::MakeAuthenticatedPlayerKey(UniqueId, OutPlayerKey) == EDigetherSaveResult::Success;
		}
		// 직접 실행의 로컬 플레이어만 연결 수명 키를 쓴다. 재접속 식별자로 보관하지 않는다.
		if (Controller->IsLocalController() && Controller->GetNetMode() != NM_DedicatedServer)
		{
			OutPlayerKey = FString::Printf(TEXT("Local:%d"), State->GetPlayerId());
			return true;
		}
		return false;
	}
}

ADigetherGameplayGameMode::ADigetherGameplayGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	RunCoordinatorComponent = CreateDefaultSubobject<UDigetherRunCoordinatorComponent>(TEXT("RunCoordinatorComponent"));
	// 시간 갱신은 이 연결층의 Tick에서 운행·레버와 함께 한 번만 한다.
	RunCoordinatorComponent->PrimaryComponentTick.bStartWithTickEnabled = false;
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
	if (!IsValid(Quota) || Quota->SetQuotaState(InitialQuota, false) != EDigetherEconomyResult::Success)
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
		if (UGameplayStatics::HasOption(OptionsString, TEXT("DigetherNewGame")))
		{
			StartNewGame(Host);
		}
		else
		{
			LoadProgress(Host, true);
		}
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
	if (!bRestoreRejected && !WriteProgress(true))
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("일차 시작 자동 저장 실패. 기준점 저장 성공 전에는 출발을 차단합니다(PQ-07 임시 정책)."));
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
	CheckPlayerAdmission(UniqueId, false, ErrorMessage);
}

bool ADigetherGameplayGameMode::CheckPlayerAdmission(const FUniqueNetIdRepl& UniqueId,
	const bool bAllowMissingId, FString& ErrorMessage) const
{
	const IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	DigetherJoinRules::FDigetherJoinCheck Check;
	// ID 없는 로컬 직접 실행의 기존 Login 예외에도 정원과 탐사 중 잠금은 적용한다.
	Check.bIdValid = UniqueId.IsValid() || bAllowMissingId;
	Check.bRequireSteamId = Subsystem && Subsystem->GetSubsystemName() == STEAM_SUBSYSTEM
		&& !(bAllowMissingId && !UniqueId.IsValid());
	Check.bIdIsSteam = UniqueId.IsValid() && UniqueId.IsV1() && UniqueId.GetType() == STEAM_SUBSYSTEM;
	Check.MaxPlayers = DigetherRoomProtocol::MaxPlayers;
	Check.bNewJoinLocked = RunCoordinatorComponent->CapturePublicState().Phase == EDigetherRunPhase::InProgress;
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
			}
		}
		Check.ConnectedPlayers = Connected.Num();
	}
	const DigetherJoinRules::EDigetherJoinDecision Decision = DigetherJoinRules::Evaluate(Check);
	if (!DigetherJoinRules::IsAccepted(Decision))
	{
		ErrorMessage = DigetherSessionText::Get(DigetherJoinRules::GetRejectionText(Decision, Check)).ToString();
		return false;
	}
	return true;
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
		if (UGameplayStatics::HasOption(OptionsString, TEXT("DigetherNewGame")))
		{
			StartNewGame(GameplayController);
		}
		else
		{
			LoadProgress(GameplayController, true);
		}
	}
	ADigetherGameplayPlayerState* State = IsValid(NewPlayer) ? NewPlayer->GetPlayerState<ADigetherGameplayPlayerState>() : nullptr;
	if (HasAuthority() && bRunReady && IsValid(State))
	{
		RestorePlayerRecord(State);
	}
	Super::PostLogin(NewPlayer);
	if (IsHostRequester(GameplayController) && !bRestoreRejected && !bApplyingRunEnd)
	{
		WriteProgress(true);
	}
}

void ADigetherGameplayGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	ADigetherGameplayPlayerState* Player = IsValid(NewPlayer) ? NewPlayer->GetPlayerState<ADigetherGameplayPlayerState>() : nullptr;
	if (!HasAuthority() || !bRunReady || !IsValid(Player) || bRestoreRejected)
	{
		return;
	}
	if (!Player->IsGameplayInitialized() && !RestorePlayerRecord(Player))
	{
		Player->InitializeGameplayIdentity();
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
	FString PlayerKey;
	if (HasAuthority() && TryGetDepositPlayerKey(Cast<ADigetherGameplayPlayerController>(Exiting), PlayerKey))
	{
		DepositSessions.ClosePlayerSession(PlayerKey, EDigetherDepositSessionCloseReason::Disconnected);
	}
	Super::Logout(Exiting);
	LoggedInControllers.Remove(Cast<APlayerController>(Exiting));
}

bool ADigetherGameplayGameMode::ResolveDepositContext(ADigetherGameplayPlayerController* Requester, FString& OutPlayerKey,
	UDigetherWalletComponent*& OutWallet, UDigetherQuotaStateComponent*& OutQuota) const
{
	if (!HasAuthority() || !bRunReady || !IsValid(Requester) || !Requester->HasAuthority()
		|| Requester->GetWorld() != GetWorld() || !TryGetDepositPlayerKey(Requester, OutPlayerKey))
	{
		return false;
	}
	ADigetherGameplayPlayerState* Player = Requester->GetPlayerState<ADigetherGameplayPlayerState>();
	ADigetherGameplayGameState* State = GetGameState<ADigetherGameplayGameState>();
	OutWallet = IsValid(Player) ? Player->GetWalletComponent() : nullptr;
	OutQuota = IsValid(State) ? State->GetQuotaComponent() : nullptr;
	return IsValid(OutWallet) && IsValid(OutQuota)
		&& Player->HasAuthority() && State->HasAuthority() && Player->GetOwner() == Requester;
}

FDigetherDepositResponse ADigetherGameplayGameMode::OpenDeposit(ADigetherGameplayPlayerController* Requester, const FName StationId)
{
	FDigetherDepositResponse Response;
	Response.StationId = StationId;
	FString PlayerKey;
	UDigetherWalletComponent* Wallet = nullptr;
	UDigetherQuotaStateComponent* Quota = nullptr;
	if (!ResolveDepositContext(Requester, PlayerKey, Wallet, Quota))
	{
		return Response;
	}
	if (IsDepositClosed())
	{
		Response.Result = EDigetherEconomyResult::DepositClosed;
		return Response;
	}
	FDigetherDepositSessionRegistry CandidateRegistry = DepositSessions;
	const double NowSeconds = GetWorld()->GetTimeSeconds();
	const FDigetherDepositSessionCloseResult Expired = CandidateRegistry.ExpireSessions(NowSeconds);
	const FDigetherDepositSessionOpenResult Opened = CandidateRegistry.OpenSession(PlayerKey, StationId,
		Quota->CaptureState().Quota.PeriodGeneration, NowSeconds, EconomyTuning.DepositSessionLifetimeSeconds);
	Response.Result = Opened.Result;
	if (Opened.Result == EDigetherEconomyResult::Success)
	{
		DepositSessions = MoveTemp(CandidateRegistry);
		Response.SessionId = Opened.Session.SessionId;
		Response.PeriodGeneration = Opened.Session.PeriodGeneration;
		NotifyClosedSessions(Expired, EDigetherEconomyResult::SessionExpired);
	}
	return Response;
}

FDigetherDepositResponse ADigetherGameplayGameMode::SubmitDeposit(ADigetherGameplayPlayerController* Requester, const FGuid SessionId,
	const FName StationId, const int32 PeriodGeneration, const int32 Amount)
{
	FDigetherDepositResponse Response;
	Response.SessionId = SessionId;
	Response.StationId = StationId;
	Response.PeriodGeneration = PeriodGeneration;
	FString PlayerKey;
	UDigetherWalletComponent* Wallet = nullptr;
	UDigetherQuotaStateComponent* Quota = nullptr;
	if (!ResolveDepositContext(Requester, PlayerKey, Wallet, Quota))
	{
		return Response;
	}
	const FDigetherWalletState PreviousWallet = Wallet->CaptureState();
	const FDigetherQuotaPublicState PreviousQuota = Quota->CaptureState();
	FDigetherWalletState CandidateWallet = PreviousWallet;
	FDigetherQuotaPublicState CandidateQuota = PreviousQuota;
	FDigetherDepositSessionRegistry CandidateRegistry = DepositSessions;
	FDigetherDepositSubmission Submission;
	Submission.PlayerKey = PlayerKey;
	Submission.SessionId = SessionId;
	Submission.StationId = StationId;
	Submission.PeriodGeneration = PeriodGeneration;
	Submission.AmountCredits = Amount;
	Submission.NowServerSeconds = GetWorld()->GetTimeSeconds();
	Submission.bDepositClosed = IsDepositClosed();
	const FDigetherDepositResult Result = CandidateRegistry.SubmitDeposit(Submission,
		CandidateWallet, CandidateQuota.Quota, EconomyTuning);
	Response.Result = Result.Result;
	if (Result.Result != EDigetherEconomyResult::Success)
	{
		return Response;
	}
	if (!FDigetherEconomyRules::IsValidWalletState(CandidateWallet)
		|| !FDigetherEconomyRules::IsValidPublicState(CandidateQuota))
	{
		Response.Result = EDigetherEconomyResult::InvalidInput;
		UE_LOG(LogDigetherFramework, Error, TEXT("Deposit candidate rejected before commit."));
		return Response;
	}
	FDigetherDepositSessionCloseResult PeriodClosed;
	if (Result.bTargetCompleted)
	{
		PeriodClosed = CandidateRegistry.CloseAllSessions(EDigetherDepositSessionCloseReason::PeriodChanged);
	}
	// 두 setter는 권한·값 형태만 검사하며 bNotify=false에서 외부 콜백을 부르지 않는다.
	Response.Result = Wallet->RestoreState(CandidateWallet, false);
	if (Response.Result != EDigetherEconomyResult::Success)
	{
		return Response;
	}
	Response.Result = Quota->RestoreState(CandidateQuota, false);
	if (Response.Result != EDigetherEconomyResult::Success)
	{
		const EDigetherEconomyResult Rollback = Wallet->RestoreState(PreviousWallet, false);
		ensureMsgf(Rollback == EDigetherEconomyResult::Success, TEXT("Deposit wallet rollback failed."));
		return Response;
	}
	DepositSessions = MoveTemp(CandidateRegistry);
	Response.AcceptedCredits = Result.AcceptedCredits;
	Response.CreditedCredits = Result.CreditedCredits;
	Response.PeriodGeneration = CandidateQuota.Quota.PeriodGeneration;
	Wallet->NotifyStateChanged();
	Quota->NotifyStateChanged();
	NotifyClosedSessions(PeriodClosed, EDigetherEconomyResult::PeriodMismatch);
	return Response;
}

FDigetherDepositResponse ADigetherGameplayGameMode::CancelDeposit(ADigetherGameplayPlayerController* Requester, const FGuid SessionId)
{
	FDigetherDepositResponse Response;
	Response.SessionId = SessionId;
	FString PlayerKey;
	UDigetherWalletComponent* Wallet = nullptr;
	UDigetherQuotaStateComponent* Quota = nullptr;
	if (!ResolveDepositContext(Requester, PlayerKey, Wallet, Quota))
	{
		return Response;
	}
	const FDigetherDepositSessionCloseResult Closed = DepositSessions.CloseSession(PlayerKey,
		SessionId, EDigetherDepositSessionCloseReason::ExplicitCancel);
	Response.Result = Closed.Result;
	return Response;
}

void ADigetherGameplayGameMode::NotifyClosedSessions(const FDigetherDepositSessionCloseResult& Closed,
	const EDigetherEconomyResult Result)
{
	for (const FDigetherDepositSession& Session : Closed.ClosedSessions)
	{
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(Iterator->Get());
			FString PlayerKey;
			if (TryGetDepositPlayerKey(Controller, PlayerKey) && PlayerKey == Session.PlayerKey)
			{
				FDigetherDepositResponse Response;
				Response.Result = Result;
				Response.SessionId = Session.SessionId;
				Response.StationId = Session.StationId;
				Response.PeriodGeneration = Session.PeriodGeneration;
				Controller->Client_DepositResult(EDigetherDepositOperation::Cancel, Response);
				break;
			}
		}
	}
}
