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
			OutPlayerKey = UniqueId.GetType().ToString() + TEXT(":") + UniqueId.ToString();
			return true;
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
	UE_LOG(LogDigetherFramework, Log, TEXT("Run tuning fixed: Profile=%s Version=%d"),
		*RunTuning.SourceProfile.ToString(), RunTuning.ProfileVersion);
	Quota->NotifyStateChanged();
	// 검증 전에 엔진 BeginPlay나 초기 Pawn 스폰이 진행되지 않게 한다.
	Super::StartPlay();
	RunCoordinatorComponent->SetComponentTickEnabled(false);
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* Controller = Iterator->Get();
		if (IsValid(Controller) && !IsValid(Controller->GetPawn()))
		{
			HandleStartingNewPlayer(Controller);
		}
	}
	LogRunStep(TEXT("Ready"));
}

void ADigetherGameplayGameMode::PreLogin(const FString& Options, const FString& Address,
	const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
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
	Check.bNewJoinLocked = RunCoordinatorComponent->CapturePublicState().Phase == EDigetherRunPhase::InProgress;
	Check.bIdInRoster = false;
	if (Check.bIdValid && IsValid(GameState))
	{
		// 로비와 같은 계약이다. PlayerArray 이전의 동시 대기 연결은 P6에서 다룬다.
		for (const APlayerState* Player : GameState->PlayerArray)
		{
			if (IsValid(Player) && Player->GetUniqueId().IsValid() && Player->GetUniqueId() == UniqueId)
			{
				Check.bIdAlreadyConnected = true;
				break;
			}
		}
	}
	const DigetherJoinRules::EDigetherJoinDecision Decision = DigetherJoinRules::Evaluate(Check);
	if (!DigetherJoinRules::IsAccepted(Decision))
	{
		ErrorMessage = DigetherSessionText::Get(DigetherJoinRules::GetRejectionText(Decision, Check)).ToString();
	}
}

void ADigetherGameplayGameMode::PostLogin(APlayerController* NewPlayer)
{
	ADigetherGameplayPlayerState* State = IsValid(NewPlayer) ? NewPlayer->GetPlayerState<ADigetherGameplayPlayerState>() : nullptr;
	if (HasAuthority() && IsValid(State))
	{
		State->InitializeGameplayIdentity();
		FDigetherWalletState InitialWallet;
		InitialWallet.BalanceCredits = DigetherFrameworkDefaults::StartingBalanceCredits;
		if (State->RestoreState(InitialWallet) != EDigetherEconomyResult::Success)
		{
			UE_LOG(LogDigetherFramework, Error, TEXT("Player wallet initialization rejected."));
		}
	}
	Super::PostLogin(NewPlayer);
}

void ADigetherGameplayGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (HasAuthority() && bRunReady
		&& RunCoordinatorComponent->CapturePublicState().Phase != EDigetherRunPhase::InProgress)
	{
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	}
}

bool ADigetherGameplayGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	// 엔진의 ServerRestartPlayer 경로도 프로필 검증을 우회하지 못하게 한다.
	return HasAuthority() && bRunReady
		&& RunCoordinatorComponent->CapturePublicState().Phase != EDigetherRunPhase::InProgress
		&& Super::PlayerCanRestart_Implementation(Player);
}

void ADigetherGameplayGameMode::Logout(AController* Exiting)
{
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
