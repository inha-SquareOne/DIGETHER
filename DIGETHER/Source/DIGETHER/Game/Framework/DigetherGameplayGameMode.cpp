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
#include "Game/Run/DigetherGameOverFlowComponent.h"
#include "Game/Run/DigetherRunCoordinatorComponent.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Voice/DigetherVoiceRoutingComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

ADigetherGameplayGameMode::ADigetherGameplayGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	RunCoordinatorComponent = CreateDefaultSubobject<UDigetherRunCoordinatorComponent>(TEXT("RunCoordinatorComponent"));
	// 시간 갱신은 이 연결층 Tick에서 운행·레버와 함께 한 번만
	RunCoordinatorComponent->PrimaryComponentTick.bStartWithTickEnabled = false;
	GameOverFlowComponent = CreateDefaultSubobject<UDigetherGameOverFlowComponent>(TEXT("GameOverFlowComponent"));
	VoiceRoutingComponent = CreateDefaultSubobject<UDigetherVoiceRoutingComponent>(TEXT("VoiceRoutingComponent"));
	VoiceRoutingComponent->CollectParticipants.BindUObject(this, &ThisClass::CollectVoiceParticipants);
	GameStateClass = ADigetherGameplayGameState::StaticClass();
	PlayerStateClass = ADigetherGameplayPlayerState::StaticClass();
	PlayerControllerClass = ADigetherGameplayPlayerController::StaticClass();
	DefaultPawnClass = ADigetherTestPawn::StaticClass();
	// 기본 300초 만료·최대 개수 퇴출은 세션 보존 계약과 불일치
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
	// 신호기 경계는 이 연결층의 경제 튜닝 원본과 일치 필요
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
	// 검증 전 엔진 BeginPlay·초기 Pawn 스폰 방지
	Super::StartPlay();
	// 엔진 매치는 월드 수명, 탐사 시작·종료는 별도 런 상태
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
	// 기획 1.1.0: 일차 시작 자동 저장 폐기, 저장 시점은 탐사 정상 완료·할당량 달성·호스트 거점 종료
	// 호스트 종료 저장은 넷 드라이버 종료·호스트 컨트롤러 파괴 전인 월드 정리 시작 시점
	if (!WorldTearDownHandle.IsValid())
	{
		WorldTearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &ThisClass::HandleWorldBeginTearDown);
	}
}

void ADigetherGameplayGameMode::PostLogin(APlayerController* NewPlayer)
{
	// AGameMode::PostLogin 자동 스폰보다 먼저 기존 상태 연결
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
	// 기획 1.1.0: 접속만으로는 저장 안 함(출발 기준점 저장 폐기), 저장은 S30 자동 저장 시점만
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
			// S01 관전 합류: 잠금 해제(탐사 종료 귀환·출발 실패 재개방)까지 몸 생성·시작 지급 없이 관전
			// 저장 명단이 있는 기존 참가자는 PostLogin에서 장부만 복원된 상태
			EnterJoinSpectate(NewPlayer);
			return;
		}
		// 로딩 중 탐사가 끝났으면 관전 없이 거점 합류(S01)
		RemoveJoinSpectator(NewPlayer);
	}
	if (!Player->IsGameplayInitialized() && !RestorePlayerRecord(Player))
	{
		Player->InitializeGameplayIdentity();
		// 이 게임의 새 참가자, 결과 크레딧 표시 순서(참가 순서) 부여·통계 0 시작(S33 231행)
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
		// 보존 대상 소실 시 재접속 표식이 일반 생성 경로를 막지 않게 함, 지급은 위 초기화 가드가 차단
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
		// 이번 세션 재접속자라도 보존 Pawn이 없으면 탐사 중 새 몸 없이 귀환 후 생성
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
	// 엔진 ServerRestartPlayer 경로의 프로필 검증 우회 방지
	return HasAuthority() && bRunReady
		&& !bRestoreRejected
		&& IsValid(Player)
		&& IsValid(Player->GetPlayerState<ADigetherGameplayPlayerState>())
		&& Player->GetPlayerState<ADigetherGameplayPlayerState>()->IsGameplayInitialized()
		&& Player->GetPlayerState<ADigetherGameplayPlayerState>()->GetLifeComponent()->CaptureState().PublicState.Life == EDigetherLifeState::Active
		&& RunCoordinatorComponent->CapturePublicState().Phase != EDigetherRunPhase::InProgress
		// 관전 합류자는 문 닫힘 중에도 몸 미생성, 출발 인원 혼입 방지
		&& !IsJoinSpectator(Player)
		&& Super::PlayerCanRestart_Implementation(Player);
}

void ADigetherGameplayGameMode::Logout(AController* Exiting)
{
	if (APlayerController* Controller = Cast<APlayerController>(Exiting))
	{
		if (!LoggedInControllers.Contains(Controller))
		{
			// Super Login 초기화 실패도 Logout 호출, 아직 증가하지 않은 매치 인원은 감소시키지 않음
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
