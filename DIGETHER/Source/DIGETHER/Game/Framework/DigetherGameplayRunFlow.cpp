#include "Game/Framework/DigetherGameplayGameMode.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Economy/DigetherQuotaStateComponent.h"
#include "Game/Economy/DigetherWalletComponent.h"
#include "Game/Elevator/DigetherElevatorActor.h"
#include "Game/Elevator/DigetherElevatorComponent.h"
#include "Game/Elevator/DigetherElevatorLeverActor.h"
#include "Game/Elevator/DigetherElevatorLeverComponent.h"
#include "Game/Elevator/DigetherElevatorRules.h"
#include "Game/Framework/DigetherFrameworkDefaults.h"
#include "Game/Framework/DigetherFrameworkLog.h"
#include "Game/Framework/DigetherFrameworkRules.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Life/DigetherLifeRules.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Run/DigetherRunCoordinatorComponent.h"
#include "Game/Run/DigetherRunPublicStateComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Misc/ScopeExit.h"
#include "Templates/UnrealTemplate.h"

namespace
{
	// 도메인의 확정 signal은 큐 입력으로 받고, 표현 통지는 복합 변경이 끝난 뒤 내보낸다.
	class FScopedDigetherRunNotifications
	{
	public:
		FScopedDigetherRunNotifications(UDigetherRunCoordinatorComponent* InRun,
			UDigetherElevatorComponent* InElevator,
			const TArray<TWeakObjectPtr<ADigetherElevatorLeverActor>>& Actors)
			: Run(InRun), Elevator(InElevator)
		{
			for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Actor : Actors)
			{
				if (Actor.IsValid())
				{
					Levers.Add(Actor->GetLeverComponent());
				}
			}
			Begin();
		}

		~FScopedDigetherRunNotifications() { Complete(); }
		FScopedDigetherRunNotifications(const FScopedDigetherRunNotifications&) = delete;
		FScopedDigetherRunNotifications& operator=(const FScopedDigetherRunNotifications&) = delete;

		void Flush()
		{
			Complete();
			Begin();
		}

		void Complete()
		{
			if (!bOpen)
			{
				return;
			}
			bOpen = false;
			for (const TWeakObjectPtr<UDigetherElevatorLeverComponent>& Lever : Levers)
			{
				if (Lever.IsValid())
				{
					Lever->EndNotificationBatch();
				}
			}
			if (Elevator.IsValid())
			{
				Elevator->EndNotificationBatch();
			}
			if (Run.IsValid())
			{
				Run->EndNotificationBatch();
			}
		}

	private:
		void Begin()
		{
			bOpen = true;
			if (Run.IsValid())
			{
				Run->BeginNotificationBatch();
			}
			if (Elevator.IsValid())
			{
				Elevator->BeginNotificationBatch();
			}
			for (const TWeakObjectPtr<UDigetherElevatorLeverComponent>& Lever : Levers)
			{
				if (Lever.IsValid())
				{
					Lever->BeginNotificationBatch();
				}
			}
		}

		TWeakObjectPtr<UDigetherRunCoordinatorComponent> Run;
		TWeakObjectPtr<UDigetherElevatorComponent> Elevator;
		TArray<TWeakObjectPtr<UDigetherElevatorLeverComponent>> Levers;
		bool bOpen = false;
	};
}

bool ADigetherGameplayGameMode::InitializeRunActors()
{
	TArray<ADigetherElevatorActor*> Elevators;
	for (TActorIterator<ADigetherElevatorActor> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		Elevators.Add(*Iterator);
	}
	if (Elevators.Num() != 1)
	{
		UE_LOG(LogDigetherFramework, Error, TEXT("Map configuration rejected: expected one elevator, found %d."), Elevators.Num());
		return false;
	}
	TArray<TWeakObjectPtr<ADigetherElevatorLeverActor>> Levers;
	int32 DepartureLevers = 0;
	int32 AscentLevers = 0;
	for (TActorIterator<ADigetherElevatorLeverActor> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		ADigetherElevatorLeverActor* Lever = *Iterator;
		if (Lever->GetElevatorActor() != Elevators[0] || !IsValid(Lever->GetLeverComponent()))
		{
			UE_LOG(LogDigetherFramework, Error, TEXT("Map configuration rejected: lever %s has no matching elevator/component."), *Lever->GetName());
			return false;
		}
		if (Lever->GetLeverKind() == EDigetherElevatorLeverKind::Departure)
		{
			++DepartureLevers;
		}
		else if (Lever->GetLeverKind() == EDigetherElevatorLeverKind::Ascent)
		{
			++AscentLevers;
		}
		Levers.Add(Lever);
	}
	if (DepartureLevers != 1 || AscentLevers != 1)
	{
		UE_LOG(LogDigetherFramework, Error, TEXT("Map configuration rejected: expected one Departure and one Ascent lever."));
		return false;
	}
	ADigetherGameplayGameState* State = GetGameState<ADigetherGameplayGameState>();
	UDigetherElevatorComponent* Elevator = Elevators[0]->GetElevatorComponent();
	const FDigetherElevatorTuning ElevatorTuning;
	if (!IsValid(State) || !IsValid(State->GetRunPublicStateComponent()) || !IsValid(Elevator)
		|| !IsValid(Elevators[0]->GetBoardingVolume())
		|| !DigetherElevatorRules::IsValidTuning(ElevatorTuning)
		|| !DigetherElevatorRules::IsValidRunParameters(RunTuning.ElevatorCapacityKg,
			RunTuning.ElevatorTripLimit, RunTuning.ElevatorTravelSeconds))
	{
		UE_LOG(LogDigetherFramework, Error, TEXT("Run actor initialization rejected: invalid components/tuning."));
		return false;
	}
	if (RunCoordinatorComponent->SetPublicStateComponent(State->GetRunPublicStateComponent()) != EDigetherRunResult::Success
		|| Elevator->InitializeForRun(RunTuning, ElevatorTuning) != EDigetherElevatorResult::Success)
	{
		return false;
	}
	ElevatorActor = Elevators[0];
	LeverActors = MoveTemp(Levers);
	for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Lever : LeverActors)
	{
		UDigetherElevatorLeverComponent* Component = Lever->GetLeverComponent();
		if (Component->Configure(Lever->GetLeverKind(), ElevatorTuning) != EDigetherElevatorLeverResult::Released)
		{
			return false;
		}
		Component->OnDepartureConfirmed.AddUObject(this, &ThisClass::HandleDepartureConfirmed);
		Component->OnAscentConfirmed.AddUObject(this, &ThisClass::HandleAscentLeverConfirmed);
	}
	Elevator->OnAscentConfirmed.AddUObject(this, &ThisClass::HandleElevatorAscentConfirmed);
	Elevator->OnExplorationFinished.AddUObject(this, &ThisClass::HandleExplorationFinished);
	RunCoordinatorComponent->OnTimeExpired.AddUObject(this, &ThisClass::HandleTimeExpired);
	return true;
}

void ADigetherGameplayGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Lever : LeverActors)
	{
		if (Lever.IsValid())
		{
			Lever->GetLeverComponent()->OnDepartureConfirmed.RemoveAll(this);
			Lever->GetLeverComponent()->OnAscentConfirmed.RemoveAll(this);
		}
	}
	if (ElevatorActor.IsValid())
	{
		ElevatorActor->GetElevatorComponent()->OnAscentConfirmed.RemoveAll(this);
		ElevatorActor->GetElevatorComponent()->OnExplorationFinished.RemoveAll(this);
	}
	RunCoordinatorComponent->OnTimeExpired.RemoveAll(this);
	RunCoordinatorComponent->SetPublicStateComponent(nullptr);
	RunParticipants.Reset();
	LeverActors.Reset();
	ElevatorActor.Reset();
#if !UE_BUILD_SHIPPING
	bSmokeActive = false;
	SmokeController.Reset();
#endif
	Super::EndPlay(EndPlayReason);
}

void ADigetherGameplayGameMode::CollectPlayers(TArray<ADigetherGameplayPlayerState*>& OutPlayers) const
{
	OutPlayers.Reset();
	if (!IsValid(GameState))
	{
		return;
	}
	for (APlayerState* State : GameState->PlayerArray)
	{
		ADigetherGameplayPlayerState* Player = Cast<ADigetherGameplayPlayerState>(State);
		if (IsValid(Player) && Player->GetGameplayPlayerId().IsValid())
		{
			OutPlayers.Add(Player);
		}
	}
}

bool ADigetherGameplayGameMode::IsValidRequester(const ADigetherGameplayPlayerController* Requester) const
{
	const ADigetherGameplayPlayerState* Player = IsValid(Requester)
		? Requester->GetPlayerState<ADigetherGameplayPlayerState>() : nullptr;
	return HasAuthority() && bRunReady && IsValid(Requester) && Requester->HasAuthority()
		&& Requester->GetWorld() == GetWorld() && IsValid(Player) && Player->GetOwner() == Requester
		&& Player->GetGameplayPlayerId().IsValid() && IsValid(GameState) && GameState->PlayerArray.Contains(Player);
}

bool ADigetherGameplayGameMode::IsConfiguredLever(const ADigetherElevatorLeverActor* Lever) const
{
	if (!IsValid(Lever) || !ElevatorActor.IsValid() || Lever->GetWorld() != GetWorld()
		|| Lever->GetElevatorActor() != ElevatorActor.Get())
	{
		return false;
	}
	for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Configured : LeverActors)
	{
		if (Configured.Get() == Lever)
		{
			return true;
		}
	}
	return false;
}

bool ADigetherGameplayGameMode::TryMakePullContext(FDigetherElevatorPullContext& OutContext) const
{
	if (!ElevatorActor.IsValid())
	{
		return false;
	}
	const ADigetherGameplayGameState* State = GetGameState<ADigetherGameplayGameState>();
	if (!IsValid(State) || !IsValid(State->GetQuotaComponent()))
	{
		return false;
	}
	const FDigetherElevatorPublicState Elevator = ElevatorActor->GetElevatorComponent()->GetPublicState();
	FDigetherElevatorPullContext Context;
	Context.ElevatorPhase = Elevator.Phase;
	Context.CompletedAscents = Elevator.CompletedAscents;
	Context.TripLimit = RunTuning.ElevatorTripLimit;
	Context.CapacityKg = RunTuning.ElevatorCapacityKg;
	Context.DayNumber = State->GetQuotaComponent()->CaptureState().Quota.Day;
	Context.bAnyoneWatchingResultCredits = DigetherFrameworkDefaults::bAnyoneWatchingResultCredits;
	TArray<APawn*> Pawns;
	ElevatorActor->GetOverlappingPawns(Pawns);
	TArray<FGuid> PassengerIds;
	for (const APawn* Pawn : Pawns)
	{
		const ADigetherGameplayPlayerState* Player = Pawn->GetPlayerState<ADigetherGameplayPlayerState>();
		if (IsValid(Player) && IsValid(Player->GetLifeComponent())
			&& Player->GetLifeComponent()->CaptureState().PublicState.Life == EDigetherLifeState::Active)
		{
			if (RunCoordinatorComponent->CapturePublicState().Phase == EDigetherRunPhase::InProgress
				&& !RunParticipants.ContainsByPredicate([Player](const TWeakObjectPtr<ADigetherGameplayPlayerState>& Participant)
				{
					return Participant.Get() == Player;
				}))
			{
				continue;
			}
			PassengerIds.Add(Player->GetGameplayPlayerId());
		}
	}
	// 엔진 overlap 배열 순서 변화가 같은 탑승자의 매니페스트 순서를 바꾸지 않게 한다.
	PassengerIds.Sort([](const FGuid& Left, const FGuid& Right) { return Left.ToString() < Right.ToString(); });
	if (!DigetherFrameworkRules::TryMakePlayerLoad(PassengerIds, Context.LoadEntries))
	{
		return false;
	}
	OutContext = MoveTemp(Context);
	return true;
}

bool ADigetherGameplayGameMode::IsDepositClosed() const
{
	return !ElevatorActor.IsValid() || DigetherFrameworkRules::IsDepositClosed(
		RunCoordinatorComponent->CapturePublicState(), ElevatorActor->GetElevatorComponent()->GetPublicState().Phase);
}

bool ADigetherGameplayGameMode::BeginLeverPull(ADigetherGameplayPlayerController* Requester, ADigetherElevatorLeverActor* Lever)
{
	if (!IsValidRequester(Requester) || !IsConfiguredLever(Lever)
		|| RunCoordinatorComponent->CapturePublicState().bGameOver)
	{
		return false;
	}
	ADigetherGameplayPlayerState* Player = Requester->GetPlayerState<ADigetherGameplayPlayerState>();
	const EDigetherLifeState Life = Player->GetLifeComponent()->CaptureState().PublicState.Life;
	bool bCanOperate = Life == EDigetherLifeState::Active;
#if !UE_BUILD_SHIPPING
	// DigetherRunSmoke 자동 시험 중에만 탈출한 시험자가 남은 빈 편을 구동할 수 있다.
	// 개발 빌드의 일반 플레이에서는 관전자가 레버를 쓰지 못한다.
	bCanOperate |= bSmokeActive && SmokeController.Get() == Requester && Life == EDigetherLifeState::Escaped;
#endif
	if (!IsValid(Requester->GetPawn()) || !bCanOperate)
	{
		return false;
	}
	FDigetherElevatorPullContext Context;
	if (!TryMakePullContext(Context))
	{
		return false;
	}
	const EDigetherElevatorLeverResult Result = Lever->GetLeverComponent()->TryBeginPull(Player->GetGameplayPlayerId(), Context);
	if (Result == EDigetherElevatorLeverResult::Started)
	{
		LogRunStep(Lever->GetLeverKind() == EDigetherElevatorLeverKind::Departure ? TEXT("DeparturePullStarted") : TEXT("AscentPullStarted"));
		return true;
	}
	UE_LOG(LogDigetherFramework, Verbose, TEXT("Lever request rejected: %s"), *UEnum::GetValueAsString(Result));
	return false;
}

bool ADigetherGameplayGameMode::ReleaseLeverPull(ADigetherGameplayPlayerController* Requester, ADigetherElevatorLeverActor* Lever)
{
	if (!IsValidRequester(Requester) || !IsConfiguredLever(Lever))
	{
		return false;
	}
	return Lever->GetLeverComponent()->ReleasePull(Requester->GetPlayerState<ADigetherGameplayPlayerState>()->GetGameplayPlayerId())
		== EDigetherElevatorLeverResult::Released;
}

// 도메인 확정 알림에서는 사건만 보관한다. 아래 조율 함수는 도메인 호출이 반환된 뒤 실행한다.
void ADigetherGameplayGameMode::HandleDepartureConfirmed(const FDigetherElevatorDepartureConfirmed& Confirmation)
{
	bDeparturePending = true;
}

void ADigetherGameplayGameMode::HandleAscentLeverConfirmed(const FDigetherElevatorAscentConfirmed& Confirmation)
{
	bAscentPending = true;
}

void ADigetherGameplayGameMode::HandleElevatorAscentConfirmed(const FDigetherElevatorAscentConfirmed& Confirmation)
{
	PendingRecordedAscent = Confirmation.TripNumber;
}

void ADigetherGameplayGameMode::HandleExplorationFinished(const FDigetherElevatorAscentConfirmed& Confirmation)
{
	bExplorationFinishedPending = true;
}

void ADigetherGameplayGameMode::HandleTimeExpired()
{
	bTimeExpiredPending = true;
}

void ADigetherGameplayGameMode::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !bRunReady || !ElevatorActor.IsValid())
	{
		return;
	}
	UDigetherElevatorComponent* Elevator = ElevatorActor->GetElevatorComponent();
	if (RunCoordinatorComponent->CapturePublicState().Phase == EDigetherRunPhase::InProgress)
	{
		const int32 PreviousAftershock = RunCoordinatorComponent->CapturePublicState().AftershockStage;
		RunCoordinatorComponent->UpdateTime(GetWorld()->GetTimeSeconds());
		if (PreviousAftershock != RunCoordinatorComponent->CapturePublicState().AftershockStage)
		{
			LogRunStep(TEXT("AftershockStageChanged"));
		}
		if (bTimeExpiredPending)
		{
			ProcessPendingRunEnd();
			return;
		}
	}
#if !UE_BUILD_SHIPPING
	AdvanceSmoke();
#endif
	FDigetherElevatorPullContext Context;
	if (!TryMakePullContext(Context))
	{
		return;
	}
	if (Context.ElevatorPhase == EDigetherElevatorPhase::BottomWaiting)
	{
		Elevator->UpdateLoad(Context.LoadEntries);
	}
	for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Lever : LeverActors)
	{
		if (Lever.IsValid() && TryMakePullContext(Context))
		{
			UDigetherElevatorLeverComponent* Component = Lever->GetLeverComponent();
			// 게이지 확정 화면도 출발·상승의 다른 도메인 적용이 끝날 때까지 보류한다.
			Component->BeginNotificationBatch();
			Component->AdvancePull(DeltaSeconds, Context);
			if (bDeparturePending)
			{
				ProcessDeparture();
			}
			if (bAscentPending)
			{
				ProcessAscent();
			}
			Component->EndNotificationBatch();
		}
	}
	if (bTimeExpiredPending)
	{
		ProcessPendingRunEnd();
		return;
	}
	const EDigetherElevatorPhase PreviousPhase = Elevator->GetPublicState().Phase;
	Elevator->AdvanceTravel(DeltaSeconds);
	if (PreviousPhase != Elevator->GetPublicState().Phase)
	{
		LogRunStep(TEXT("TravelPhaseChanged"));
	}
	ProcessPendingRunEnd();
}

void ADigetherGameplayGameMode::ProcessDeparture()
{
	bDeparturePending = false;
	bool bDeparturePullConsumed = false;
	ON_SCOPE_EXIT
	{
		for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Lever : LeverActors)
		{
			if (!bDeparturePullConsumed && Lever.IsValid() && Lever->GetLeverKind() == EDigetherElevatorLeverKind::Departure)
			{
				Lever->GetLeverComponent()->ResetPull();
			}
		}
	};
	ADigetherGameplayGameState* State = GetGameState<ADigetherGameplayGameState>();
	UDigetherElevatorComponent* Elevator = ElevatorActor->GetElevatorComponent();
	FDigetherElevatorState ElevatorState;
	TArray<ADigetherGameplayPlayerState*> Players;
	CollectPlayers(Players);
	if (Players.IsEmpty() || !IsValid(State) || RunCoordinatorComponent->CapturePublicState().bGameOver
		|| RunCoordinatorComponent->CapturePublicState().Phase == EDigetherRunPhase::InProgress
		|| !Elevator->TryGetServerState(ElevatorState)
		|| DigetherElevatorRules::Transition(ElevatorState, EDigetherElevatorCommand::DepartHub, 0.f,
			RunTuning.ElevatorTripLimit, RunTuning.ElevatorTravelSeconds, FDigetherElevatorTuning()).Result != EDigetherElevatorResult::Success)
	{
		return;
	}
	const FDigetherQuotaState Quota = State->GetQuotaComponent()->CaptureState().Quota;
	FDigetherRunCoordinatorState Candidate;
	if (FDigetherRunRules::EvaluateQuota(Quota).Result != EDigetherRunResult::Success
		|| FDigetherRunCoordinatorRules::StartRun(Candidate, RunTuning, FDigetherRunRuleTuning(),
			GetWorld()->GetTimeSeconds()).Result != EDigetherRunResult::Success)
	{
		return;
	}
	UBoxComponent* Volume = ElevatorActor->GetBoardingVolume();
	const FVector Extent = Volume->GetUnscaledBoxExtent();
	const double FirstOffsetCm = -(Players.Num() - 1) * DigetherFrameworkDefaults::PlayerSpacingCm / 2.0;
	TArray<FVector> Destinations;
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		const ADigetherGameplayPlayerState* Player = Players[Index];
		APawn* Pawn = Player->GetPawn();
		const FVector Local(FirstOffsetCm + Index * DigetherFrameworkDefaults::PlayerSpacingCm, 0.0, 0.0);
		if (!IsValid(Pawn) || !IsValid(Player->GetLifeComponent())
			|| Player->GetLifeComponent()->CaptureState().PublicState.Life != EDigetherLifeState::Active
			|| FMath::Abs(Local.X) >= Extent.X)
		{
			UE_LOG(LogDigetherFramework, Error, TEXT("Departure rejected: active player Pawn/boarding space unavailable."));
			return;
		}
		Destinations.Add(Volume->GetComponentTransform().TransformPosition(Local));
	}
	// 모든 값·대상·공간 검사 뒤 같은 서버 호출 안에서 전원 이동과 런·운행을 확정한다.
	FScopedDigetherRunNotifications Notifications(RunCoordinatorComponent, Elevator, LeverActors);
	RunParticipants.Reset();
	DeparturePlayerCount = Players.Num();
	LastRecordedAscent = 0;
	PendingRecordedAscent = 0;
	RunCoordinatorComponent->HandleDepartureLeverCompleted(Quota);
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		RunParticipants.Add(Players[Index]);
		if (UPawnMovementComponent* Movement = Players[Index]->GetPawn()->GetMovementComponent())
		{
			Movement->StopMovementImmediately();
		}
		Players[Index]->GetPawn()->SetActorLocation(Destinations[Index], false, nullptr, ETeleportType::TeleportPhysics);
	}
	RunCoordinatorComponent->StartRun(RunTuning, FDigetherRunRuleTuning(), GetWorld()->GetTimeSeconds());
	Elevator->DepartHub();
	for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Lever : LeverActors)
	{
		if (Lever.IsValid() && Lever->GetLeverKind() == EDigetherElevatorLeverKind::Departure)
		{
			Lever->GetLeverComponent()->ResetPull();
		}
	}
	bDeparturePullConsumed = true;
	const FDigetherDepositSessionCloseResult Closed = DepositSessions.CloseAllSessions(EDigetherDepositSessionCloseReason::Departure);
	Notifications.Complete();
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		if (APlayerController* Controller = Cast<APlayerController>(Players[Index]->GetOwner()))
		{
			// DefaultPawn의 소유자 위치에도 서버 순간이동을 명시적으로 전달한다.
			Controller->ClientSetLocation(Destinations[Index], Players[Index]->GetPawn()->GetActorRotation());
		}
	}
	NotifyClosedSessions(Closed, EDigetherEconomyResult::DepositClosed);
	LogRunStep(TEXT("DepartureConfirmed"));
	ProcessPendingRunEnd();
}

void ADigetherGameplayGameMode::ProcessAscent()
{
	bAscentPending = false;
	if (RunCoordinatorComponent->CapturePublicState().Phase != EDigetherRunPhase::InProgress)
	{
		return;
	}
	ADigetherElevatorLeverActor* AscentLever = nullptr;
	for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Lever : LeverActors)
	{
		if (Lever.IsValid() && Lever->GetLeverKind() == EDigetherElevatorLeverKind::Ascent)
		{
			AscentLever = Lever.Get();
		}
	}
	FDigetherElevatorAscentConfirmed Ascent;
	bool bPullConsumed = false;
	ON_SCOPE_EXIT
	{
		if (!bPullConsumed && IsValid(AscentLever))
		{
			AscentLever->GetLeverComponent()->ResetPull();
		}
	};
	if (!IsValid(AscentLever) || !AscentLever->GetLeverComponent()->TryGetConfirmedAscent(Ascent)
		|| Ascent.TripNumber != LastRecordedAscent + 1)
	{
		return;
	}
	TArray<ADigetherGameplayPlayerState*> Passengers;
	for (const FGuid& Id : Ascent.PassengerIds)
	{
		ADigetherGameplayPlayerState* Found = nullptr;
		for (const TWeakObjectPtr<ADigetherGameplayPlayerState>& Player : RunParticipants)
		{
			if (Player.IsValid() && Player->GetGameplayPlayerId() == Id)
			{
				Found = Player.Get();
				break;
			}
		}
		if (!IsValid(Found) || FDigetherLifeRules::ApplyEscape(Found->GetLifeComponent()->CaptureState(),
			EDigetherEscapeKind::Elevator).Result != EDigetherLifeResult::Success)
		{
			return;
		}
		Passengers.Add(Found);
	}
	UDigetherElevatorComponent* Elevator = ElevatorActor->GetElevatorComponent();
	FScopedDigetherRunNotifications Notifications(RunCoordinatorComponent, Elevator, LeverActors);
	if (Elevator->ConfirmAscent(Ascent) != EDigetherElevatorResult::Success)
	{
		return;
	}
	AscentLever->GetLeverComponent()->ResetPull();
	bPullConsumed = true;
	for (ADigetherGameplayPlayerState* Player : Passengers)
	{
		Player->GetLifeComponent()->ApplyEscape(EDigetherEscapeKind::Elevator, false);
	}
	// 레버의 통지가 아니라 실제 운행 컴포넌트의 확정 사건을 한 편에 한 번 소비한다.
	if (PendingRecordedAscent == Ascent.TripNumber && PendingRecordedAscent > LastRecordedAscent)
	{
		LastRecordedAscent = PendingRecordedAscent;
		RunCoordinatorComponent->RecordConfirmedAscent();
	}
	Notifications.Complete();
	LogRunStep(TEXT("AscentConfirmed"));
	for (ADigetherGameplayPlayerState* Player : Passengers)
	{
		Player->GetLifeComponent()->NotifyStateChanged();
		if (ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(Player->GetOwner()))
		{
			if (UPawnMovementComponent* Movement = Player->GetPawn() ? Player->GetPawn()->GetMovementComponent() : nullptr)
			{
				Movement->StopMovementImmediately();
			}
			Controller->Client_SetLifeSpectating(true);
			CycleSpectateTarget(Controller, true);
		}
	}
}

void ADigetherGameplayGameMode::ProcessPendingRunEnd()
{
	if (bApplyingRunEnd || RunCoordinatorComponent->CapturePublicState().Phase != EDigetherRunPhase::InProgress)
	{
		return;
	}
	if (bTimeExpiredPending || bExplorationFinishedPending)
	{
		const EDigetherRunTerminationTrigger Trigger = bTimeExpiredPending
			? EDigetherRunTerminationTrigger::TimeExpired : EDigetherRunTerminationTrigger::FinalAscentCompleted;
		FinishRunAndReturn(Trigger);
	}
}

void ADigetherGameplayGameMode::FinishRunAndReturn(const EDigetherRunTerminationTrigger Trigger)
{
	TGuardValue<bool> ApplyingGuard(bApplyingRunEnd, true);
	UDigetherElevatorComponent* Elevator = ElevatorActor->GetElevatorComponent();
	ADigetherGameplayGameState* State = GetGameState<ADigetherGameplayGameState>();
	if (!IsValid(State) || !IsValid(State->GetQuotaComponent()))
	{
		return;
	}
	FDigetherElevatorPullContext Context;
	if (!TryMakePullContext(Context))
	{
		return;
	}
	TArray<APawn*> BoardedPawns;
	ElevatorActor->GetOverlappingPawns(BoardedPawns);
	FDigetherRunTerminationInput Input;
	Input.Trigger = Trigger;
	Input.ElevatorState = DigetherFrameworkRules::MakeRunElevatorState(Context.ElevatorPhase);
	Input.bElevatorOverweight = DigetherElevatorRules::IsOverCapacity(
		DigetherElevatorRules::SumWeight(Context.LoadEntries), RunTuning.ElevatorCapacityKg);
	TArray<ADigetherGameplayPlayerState*> Players;
	TArray<FDigetherFrameworkPlayerSnapshot> Snapshots;
	TArray<FVector> ReturnLocations;
	for (const TWeakObjectPtr<ADigetherGameplayPlayerState>& WeakPlayer : RunParticipants)
	{
		ADigetherGameplayPlayerState* Player = WeakPlayer.Get();
		if (!IsValid(Player))
		{
			continue;
		}
		if (!Player->HasAuthority() || !IsValid(Player->GetLifeComponent()) || !IsValid(Player->GetWalletComponent()))
		{
			return;
		}
		FDigetherFrameworkPlayerSnapshot Snapshot;
		Snapshot.PlayerKey = Player->GetGameplayPlayerKey();
		Snapshot.Life = Player->GetLifeComponent()->CaptureState();
		Snapshot.Wallet = Player->GetWalletComponent()->CaptureState();
		FDigetherRunPlayerInput PlayerInput;
		PlayerInput.PlayerKey = Snapshot.PlayerKey;
		PlayerInput.Life = Snapshot.Life.PublicState.Life;
		PlayerInput.Location = DigetherFrameworkRules::MakeRunPlayerLocation(Snapshot.Life.PublicState.Life,
			BoardedPawns.Contains(Player->GetPawn()), Context.ElevatorPhase);
		AController* Controller = Cast<AController>(Player->GetOwner());
		AActor* Start = IsValid(Controller) ? FindPlayerStart(Controller) : nullptr;
		if (!IsValid(Start))
		{
			UE_LOG(LogDigetherFramework, Error, TEXT("Run return rejected: PlayerStart unavailable."));
			return;
		}
		ReturnLocations.Add(Start->GetActorLocation());
		Input.Players.Add(PlayerInput);
		Snapshots.Add(Snapshot);
		Players.Add(Player);
	}
	FDigetherFrameworkReturnPlan Plan;
	if (!DigetherFrameworkRules::TryPlanReturn(Input, Snapshots, State->GetQuotaComponent()->CaptureState().Quota,
		RunTuning, DeparturePlayerCount, Plan))
	{
		UE_LOG(LogDigetherFramework, Error, TEXT("Run return candidate rejected before commit."));
		return;
	}
	// DEV-027: 게임 오버면 히든 금액 공개 목록도 확정 전에 만든다. 키는 공개 PlayerId다.
	TArray<FDigetherHiddenRevealEntry> HiddenReveals;
	if (Plan.bGameOver)
	{
		TArray<FString> PublicKeys;
		TArray<FDigetherWalletState> SettledWallets;
		for (int32 Index = 0; Index < Players.Num(); ++Index)
		{
			PublicKeys.Add(FString::FromInt(Players[Index]->GetPlayerId()));
			SettledWallets.Add(Plan.Players[Index].SettledWallet);
		}
		if (!DigetherFrameworkRules::TryMakeHiddenReveals(PublicKeys, SettledWallets, HiddenReveals))
		{
			UE_LOG(LogDigetherFramework, Error, TEXT("Run return rejected: hidden reveal candidate invalid."));
			return;
		}
	}
	FDigetherElevatorAscentConfirmed ReturnAscent;
	if (Plan.Termination.ElevatorAction != EDigetherRunElevatorAction::Unchanged)
	{
		const TArray<FDigetherElevatorLoadEntry> ReturnLoad = Plan.Termination.bEjectCargo
			? TArray<FDigetherElevatorLoadEntry>() : Context.LoadEntries;
		ReturnAscent = DigetherElevatorRules::CaptureAscent(Context.CompletedAscents, Context.TripLimit,
			ReturnLoad, Context.CapacityKg, FDigetherElevatorTuning());
		FDigetherElevatorState ElevatorState;
		if (!Elevator->TryGetServerState(ElevatorState)
			|| DigetherElevatorRules::Transition(ElevatorState, EDigetherElevatorCommand::RequestAscent, 0.f,
				Context.TripLimit, RunTuning.ElevatorTravelSeconds, FDigetherElevatorTuning()).Result != EDigetherElevatorResult::Success)
		{
			return;
		}
	}
	// 여기까지 모든 생사·손실·정산·일차·귀환 후보 검사를 마쳤다. 다음 블록은 같은 서버 실행 안에서 적용한다.
	FScopedDigetherRunNotifications Notifications(RunCoordinatorComponent, Elevator, LeverActors);
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		UDigetherLifeStateComponent* Life = Players[Index]->GetLifeComponent();
		if (Plan.Termination.Players[Index].Outcome == EDigetherRunPlayerOutcome::MissingDeath)
		{
			Life->ApplyDeath(Plan.Termination.Players[Index].DeathCause, false);
		}
		else
		{
			Life->RestoreState(Plan.Players[Index].LifeAtEnd, false);
		}
		Players[Index]->GetWalletComponent()->RestoreState(Plan.Players[Index].WalletAtEnd, false);
	}
	const FDigetherRunTerminationResult Completed = RunCoordinatorComponent->CompleteRun(Input);
	if (!ensureMsgf(Completed.Result == EDigetherRunResult::Success, TEXT("Prevalidated run completion failed.")))
	{
		for (int32 Index = 0; Index < Players.Num(); ++Index)
		{
			Players[Index]->GetLifeComponent()->RestoreState(Snapshots[Index].Life, false);
			Players[Index]->GetWalletComponent()->RestoreState(Snapshots[Index].Wallet, false);
		}
		return;
	}
	if (Completed.ElevatorAction != EDigetherRunElevatorAction::Unchanged)
	{
		if (Completed.bEjectCargo)
		{
			Elevator->UpdateLoad(TArray<FDigetherElevatorLoadEntry>());
		}
		Elevator->ConfirmAscent(ReturnAscent);
		LogRunStep(Completed.bEjectCargo ? TEXT("EmptyAscentAfterEjection") : TEXT("TimeoutReturnAscent"));
	}
	Notifications.Flush();
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		if (Completed.Players[Index].Outcome == EDigetherRunPlayerOutcome::MissingDeath)
		{
			UE_LOG(LogDigetherFramework, Log, TEXT("Death applied: Cause=%s Drop skipped (inventory unavailable)."),
				*UEnum::GetValueAsString(Completed.Players[Index].DeathCause));
		}
		Players[Index]->GetLifeComponent()->NotifyStateChanged();
		Players[Index]->GetWalletComponent()->NotifyStateChanged();
	}
	LogRunStep(Trigger == EDigetherRunTerminationTrigger::TimeExpired ? TEXT("RunEnded.TimeExpired") : TEXT("RunEnded.FinalAscentCompleted"));

	// 임시 처리: 기획 미정(별도 귀환 장면 연결 전 즉시 거점 귀환), 2026-11-04
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		Players[Index]->GetWalletComponent()->RestoreState(Plan.Players[Index].SettledWallet, false);
	}
	State->GetQuotaComponent()->SetQuotaState(Plan.ReturnedQuota, false);
	if (Plan.bGameOver)
	{
		State->GetQuotaComponent()->SetHiddenReveals(HiddenReveals, false);
	}
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		Players[Index]->GetLifeComponent()->Revive(false);
		if (APawn* Pawn = Players[Index]->GetPawn())
		{
			if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
			{
				Movement->StopMovementImmediately();
			}
			Pawn->SetActorLocation(ReturnLocations[Index], false, nullptr, ETeleportType::TeleportPhysics);
		}
		if (ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(Players[Index]->GetOwner()))
		{
			Controller->Client_SetLifeSpectating(false);
			if (IsValid(Controller->GetPawn()))
			{
				Controller->SetViewTargetWithBlend(Controller->GetPawn(), DigetherFrameworkDefaults::SpectateBlendSeconds);
			}
		}
	}
	Elevator->InitializeForRun(RunTuning, FDigetherElevatorTuning());
	for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Lever : LeverActors)
	{
		if (Lever.IsValid())
		{
			Lever->GetLeverComponent()->ResetPull();
		}
	}
	bTimeExpiredPending = false;
	bExplorationFinishedPending = false;
	bDeparturePending = false;
	bAscentPending = false;
	PendingRecordedAscent = 0;
	LastRecordedAscent = 0;
	RunParticipants.Reset();
	// 공개 귀환 사건은 부활·이동·다음 운행 준비를 모두 마친 뒤 보낸다.
	RunCoordinatorComponent->HandleBaseReturn(Plan.ReturnedQuota);
	Notifications.Complete();
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		if (APlayerController* Controller = Cast<APlayerController>(Players[Index]->GetOwner()))
		{
			if (IsValid(Controller->GetPawn()))
			{
				Controller->ClientSetLocation(ReturnLocations[Index], Controller->GetPawn()->GetActorRotation());
			}
		}
	}
	State->GetQuotaComponent()->NotifyStateChanged();
	for (ADigetherGameplayPlayerState* Player : Players)
	{
		Player->GetWalletComponent()->NotifyStateChanged();
		Player->GetLifeComponent()->NotifyStateChanged();
	}
	LogRunStep(Plan.bGameOver ? TEXT("BaseReturned.GameOver") : TEXT("BaseReturned"));
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* Controller = Iterator->Get();
		if (IsValid(Controller) && !IsValid(Controller->GetPawn()))
		{
			HandleStartingNewPlayer(Controller);
		}
	}
#if !UE_BUILD_SHIPPING
	if (bSmokeActive)
	{
		LogRunStep(Trigger == EDigetherRunTerminationTrigger::FinalAscentCompleted
			? TEXT("SmokeCompleted") : TEXT("SmokeFailed.TimeExpired"));
		bSmokeActive = false;
		SmokeController.Reset();
	}
#endif
}

void ADigetherGameplayGameMode::CycleSpectateTarget(ADigetherGameplayPlayerController* Requester, const bool bNext)
{
	if (!IsValidRequester(Requester))
	{
		return;
	}
	ADigetherGameplayPlayerState* Self = Requester->GetPlayerState<ADigetherGameplayPlayerState>();
	if (!Self->GetLifeComponent()->CaptureState().PublicState.bSpectating)
	{
		return;
	}
	TArray<ADigetherGameplayPlayerState*> Players;
	CollectPlayers(Players);
	TArray<FDigetherSpectateCandidate> Candidates;
	for (const ADigetherGameplayPlayerState* Player : Players)
	{
		if (IsValid(Player->GetPawn()))
		{
			FDigetherSpectateCandidate Candidate;
			// 관전 대상 키는 공개 PlayerId만 사용한다. 런 매니페스트의 서버 GUID는 전달하지 않는다.
			Candidate.PlayerKey = FString::FromInt(Player->GetPlayerId());
			Candidate.Life = Player->GetLifeComponent()->CaptureState().PublicState.Life;
			Candidates.Add(Candidate);
		}
	}
	if (Self->GetLifeComponent()->SelectSpectateTarget(Candidates, FString::FromInt(Self->GetPlayerId()), bNext
		? EDigetherSpectateDirection::Next : EDigetherSpectateDirection::Previous) != EDigetherLifeResult::Success)
	{
		return;
	}
	const FString TargetKey = Self->GetLifeComponent()->CaptureState().PrivateState.SpectateTargetPlayerKey;
	AActor* Target = ElevatorActor.Get();
	for (ADigetherGameplayPlayerState* Player : Players)
	{
		if (FString::FromInt(Player->GetPlayerId()) == TargetKey && IsValid(Player->GetPawn()))
		{
			Target = Player->GetPawn();
			break;
		}
	}
	if (IsValid(Target))
	{
		Requester->SetViewTargetWithBlend(Target, DigetherFrameworkDefaults::SpectateBlendSeconds);
	}
}

void ADigetherGameplayGameMode::LogRunStep(const TCHAR* Step) const
{
	const ADigetherGameplayGameState* State = GetGameState<ADigetherGameplayGameState>();
	if (!IsValid(State))
	{
		return;
	}
	const FDigetherQuotaState Quota = State->GetQuotaComponent()->CaptureState().Quota;
	TArray<ADigetherGameplayPlayerState*> Players;
	CollectPlayers(Players);
	int32 Active = 0;
	int32 Dead = 0;
	int32 Escaped = 0;
	for (const ADigetherGameplayPlayerState* Player : Players)
	{
		switch (Player->GetLifeComponent()->CaptureState().PublicState.Life)
		{
		case EDigetherLifeState::Active: ++Active; break;
		case EDigetherLifeState::Dead: ++Dead; break;
		case EDigetherLifeState::Escaped: ++Escaped; break;
		}
	}
	const EDigetherElevatorPhase Phase = ElevatorActor.IsValid()
		? ElevatorActor->GetElevatorComponent()->GetPublicState().Phase : EDigetherElevatorPhase::HubWaiting;
	UE_LOG(LogDigetherFramework, Log, TEXT("Step=%s Phase=%s Run=%s Day=%d RemainingQuota=%d Life[Active=%d Dead=%d Escaped=%d] ConfirmedAscents=%d Aftershock=%d RemainingSeconds=%.2f"),
		Step, *UEnum::GetValueAsString(Phase), *UEnum::GetValueAsString(RunCoordinatorComponent->CapturePublicState().Phase),
		Quota.Day, FMath::Max(0, Quota.TargetCredits - Quota.DepositedCredits), Active, Dead, Escaped,
		RunCoordinatorComponent->GetConfirmedAscentCount(), RunCoordinatorComponent->CapturePublicState().AftershockStage,
		RunCoordinatorComponent->GetRemainingSeconds());
}

bool ADigetherGameplayGameMode::GrantCredits(ADigetherGameplayPlayerController* Requester, const int32 Amount)
{
#if !UE_BUILD_SHIPPING
	if (IsValidRequester(Requester) && Amount > 0)
	{
		UDigetherWalletComponent* Wallet = Requester->GetPlayerState<ADigetherGameplayPlayerState>()->GetWalletComponent();
		FDigetherWalletState Candidate = Wallet->CaptureState();
		Candidate.BalanceCredits = FDigetherEconomyRules::AddCredits(Candidate.BalanceCredits, Amount);
		const bool bApplied = Wallet->RestoreState(Candidate) == EDigetherEconomyResult::Success;
		UE_LOG(LogDigetherFramework, Log, TEXT("GrantCredits Result=%d (owner wallet only)."), bApplied);
		return bApplied;
	}
#endif
	return false;
}

bool ADigetherGameplayGameMode::StartRunSmoke(ADigetherGameplayPlayerController* Requester)
{
#if !UE_BUILD_SHIPPING
	if (!IsValidRequester(Requester) || bSmokeActive || !ElevatorActor.IsValid()
		|| RunCoordinatorComponent->CapturePublicState().bGameOver
		|| ElevatorActor->GetElevatorComponent()->GetPublicState().Phase != EDigetherElevatorPhase::HubWaiting)
	{
		return false;
	}
	SmokeController = Requester;
	bSmokeActive = true;
	LogRunStep(TEXT("SmokeStarted"));
	return true;
#else
	return false;
#endif
}

#if !UE_BUILD_SHIPPING
void ADigetherGameplayGameMode::AdvanceSmoke()
{
	if (!bSmokeActive)
	{
		return;
	}
	ADigetherGameplayPlayerController* Controller = SmokeController.Get();
	if (!IsValidRequester(Controller))
	{
		bSmokeActive = false;
		SmokeController.Reset();
		UE_LOG(LogDigetherFramework, Warning, TEXT("SmokeAborted: requesting player disconnected."));
		return;
	}
	FDigetherElevatorPullContext Context;
	if (!TryMakePullContext(Context))
	{
		return;
	}
	if (Context.ElevatorPhase != EDigetherElevatorPhase::HubWaiting
		&& Context.ElevatorPhase != EDigetherElevatorPhase::BottomWaiting)
	{
		return;
	}
	const EDigetherElevatorLeverKind Kind = Context.ElevatorPhase == EDigetherElevatorPhase::HubWaiting
		? EDigetherElevatorLeverKind::Departure : EDigetherElevatorLeverKind::Ascent;
	for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Lever : LeverActors)
	{
		if (Lever.IsValid() && Lever->GetLeverKind() == Kind)
		{
			// 개발 자동 운전자다. 첫 편에 탈출한 뒤에도 남은 빈 편을 구동해 최종 도착까지 검사한다.
			const EDigetherElevatorLeverResult Result = Lever->GetLeverComponent()->TryBeginPull(
				Controller->GetPlayerState<ADigetherGameplayPlayerState>()->GetGameplayPlayerId(), Context);
			if (Result == EDigetherElevatorLeverResult::Started)
			{
				LogRunStep(Kind == EDigetherElevatorLeverKind::Departure ? TEXT("DeparturePullStarted") : TEXT("AscentPullStarted"));
			}
		}
	}
}
#endif
