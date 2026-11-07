#include "Game/Framework/DigetherGameplayGameMode.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
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
#include "Game/Framework/DigetherGameplayRunFlowInternal.h"
#include "Game/Life/DigetherLifeRules.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Hub/DigetherHubStateActor.h"
#include "Game/Run/DigetherGameOverFlowComponent.h"
#include "Game/Run/DigetherRunCoordinatorComponent.h"
#include "Game/Run/DigetherRunPublicStateComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/ScopeExit.h"
#include "Templates/UnrealTemplate.h"

namespace DigetherGameplayRunFlowInternal
{
	// 관전·복귀 시점 전환, 이미 보는 대상이면 유지
	// 네트워크 연결 없는 원격 컨트롤러(자동 테스트 월드)는 ClientSetViewTarget 즉시 실행
	// 블렌드 중 같은 대상 복귀 시 엔진 SetViewTarget과 상호 재귀, 이 경우 블렌드 없이 전환
	void SetSpectateViewTarget(APlayerController* Controller, AActor* Target)
	{
		if (!IsValid(Controller) || !IsValid(Target))
		{
			return;
		}
		if (const APlayerCameraManager* Camera = Controller->PlayerCameraManager)
		{
			const AActor* Pending = Camera->PendingViewTarget.Target;
			if (Pending == Target || (Pending == nullptr && Camera->GetViewTarget() == Target))
			{
				return;
			}
		}
		const bool bCanBlend = Controller->IsLocalController() || Controller->GetNetConnection() != nullptr;
		Controller->SetViewTargetWithBlend(Target, bCanBlend ? DigetherFrameworkDefaults::SpectateBlendSeconds : 0.f);
	}

	// 같은 엘리베이터·같은 종류의 다른 레버 중 진행 중·확정 레버 검색
	// 기획 S15: 안팎 상승 레버 동시 당김은 먼저 시작한 한 건만 수락
	const ADigetherElevatorLeverActor* FindOccupiedSiblingLever(
		const TArray<TWeakObjectPtr<ADigetherElevatorLeverActor>>& Levers, const ADigetherElevatorLeverActor* Lever)
	{
		if (!IsValid(Lever))
		{
			return nullptr;
		}
		for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Candidate : Levers)
		{
			const ADigetherElevatorLeverActor* Sibling = Candidate.Get();
			if (!IsValid(Sibling) || Sibling == Lever || Sibling->GetLeverKind() != Lever->GetLeverKind()
				|| Sibling->GetElevatorActor() != Lever->GetElevatorActor())
			{
				continue;
			}
			const UDigetherElevatorLeverComponent* Component = Sibling->GetLeverComponent();
			FDigetherElevatorAscentConfirmed Confirmed;
			if (IsValid(Component) && (Component->GetActivePullerId().IsValid() || Component->TryGetConfirmedAscent(Confirmed)))
			{
				return Sibling;
			}
		}
		return nullptr;
	}
}

namespace
{
	// 도메인 확정 signal은 큐 입력, 표현 통지는 복합 변경 완료 뒤 송출
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

	// 레버 상호작용 시선 기준점 높이(IsWithinLeverRange와 같은 값), 맵 배치 경고 전용
	constexpr double LeverSightHeightCm = 80.0;

	// 게이지 확정 상승 레버 검색, 확정 수도 반환해 둘 이상인 비정상은 호출자가 거절
	ADigetherElevatorLeverActor* FindConfirmedAscentLever(
		const TArray<TWeakObjectPtr<ADigetherElevatorLeverActor>>& Levers, int32& OutConfirmedCount)
	{
		OutConfirmedCount = 0;
		ADigetherElevatorLeverActor* Found = nullptr;
		for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Candidate : Levers)
		{
			ADigetherElevatorLeverActor* Lever = Candidate.Get();
			FDigetherElevatorAscentConfirmed Confirmed;
			if (IsValid(Lever) && Lever->GetLeverKind() == EDigetherElevatorLeverKind::Ascent
				&& IsValid(Lever->GetLeverComponent()) && Lever->GetLeverComponent()->TryGetConfirmedAscent(Confirmed))
			{
				++OutConfirmedCount;
				Found = Lever;
			}
		}
		return Found;
	}

	// 레버 시선 기준점의 탑승 영역 상자 내부 여부, 맵 배치 경고 전용·판정 미사용
	bool IsLeverInsideBoardingVolume(const ADigetherElevatorLeverActor& Lever, const UBoxComponent& Volume)
	{
		const FVector Local = Volume.GetComponentTransform().InverseTransformPosition(
			Lever.GetActorLocation() + FVector(0.0, 0.0, LeverSightHeightCm));
		const FVector Extent = Volume.GetUnscaledBoxExtent();
		return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
	}
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
	int32 AscentInsideLevers = 0;
	int32 AscentOutsideLevers = 0;
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
			if (Lever->GetPlacement() == EDigetherElevatorLeverPlacement::Outside)
			{
				++AscentOutsideLevers;
			}
			else
			{
				++AscentInsideLevers;
			}
		}
		Levers.Add(Lever);
	}
	if (!DigetherElevatorRules::IsValidLeverLayout(DepartureLevers, AscentInsideLevers, AscentOutsideLevers))
	{
		bool bAllowInsideOnly = false;
#if !UE_BUILD_SHIPPING
		// 개발 빌드 임시 허용: 밖 레버 미배치 맵(L_GameTest·L_Hub 등)은 안쪽 레버만으로 시작
		// 기획상 안·밖 2개 필수, Shipping은 실패
		bAllowInsideOnly = AscentOutsideLevers == 0
			&& DigetherElevatorRules::IsValidLeverLayout(DepartureLevers, AscentInsideLevers, 1);
#endif
		if (!bAllowInsideOnly)
		{
			UE_LOG(LogDigetherFramework, Error,
				TEXT("Map configuration rejected: expected one Departure, one inside Ascent and one outside Ascent lever (found %d/%d/%d)."),
				DepartureLevers, AscentInsideLevers, AscentOutsideLevers);
			return false;
		}
		UE_LOG(LogDigetherFramework, Warning,
			TEXT("Map configuration warning: outside Ascent lever missing; continuing with the inside lever only (development build)."));
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
	// 안쪽 레버는 탑승 영역 안, 밖 레버는 밖이어야 기획 의도(밖 레버 조작자 미탑승) 성립
	// 아트 배치 조정 여지로 시작은 막지 않고 경고만
	for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Lever : Levers)
	{
		if (Lever->GetLeverKind() != EDigetherElevatorLeverKind::Ascent)
		{
			continue;
		}
		const bool bExpectInside = Lever->GetPlacement() == EDigetherElevatorLeverPlacement::Inside;
		if (IsLeverInsideBoardingVolume(*Lever, *Elevators[0]->GetBoardingVolume()) != bExpectInside)
		{
			UE_LOG(LogDigetherFramework, Warning,
				TEXT("Map configuration warning: Ascent lever %s placement %s does not match the boarding volume."),
				*Lever->GetName(), *UEnum::GetValueAsString(Lever->GetPlacement()));
		}
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
	Elevator->OnDoorClosed.AddUObject(this, &ThisClass::HandleDoorClosed);
	Elevator->OnDoorReopenedByOverweight.AddUObject(this, &ThisClass::HandleDoorReopenedByOverweight);
	RunCoordinatorComponent->OnTimeExpired.AddUObject(this, &ThisClass::HandleTimeExpired);
	return true;
}

void ADigetherGameplayGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutosaveRetryTimer);
	}
	// 호스트 종료 저장은 월드 정리 시작(HandleWorldBeginTearDown)에서 완료, 남은 재시도가 종료를 막지 않게 함
	FWorldDelegates::OnWorldBeginTearDown.Remove(WorldTearDownHandle);
	WorldTearDownHandle.Reset();
	PendingAutosave.Reset();
	bAutosaveRecapturePending = false;
	DepartureBaseline.Reset();
	// PreLoginAsync 계약상 남은 교체 대기의 완료 콜백도 호출
	CompletePendingLoginReplacementsOnEnd();
	AcceptedBeforeLockIds.Reset();
	JoinSpectators.Reset();
	// 게임 오버 세부 단계는 미저장, 확정 저장이 됐다면 다음 실행은 같은 슬롯 새 게임(S33 240행)
	GameOverFlowComponent->Cancel();
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
		ElevatorActor->GetElevatorComponent()->OnDoorClosed.RemoveAll(this);
		ElevatorActor->GetElevatorComponent()->OnDoorReopenedByOverweight.RemoveAll(this);
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
	for (APlayerState* State : InactivePlayerArray)
	{
		ADigetherGameplayPlayerState* Player = Cast<ADigetherGameplayPlayerState>(State);
		if (IsValid(Player) && Player->GetGameplayPlayerId().IsValid())
		{
			OutPlayers.AddUnique(Player);
		}
	}
}

void ADigetherGameplayGameMode::CollectDeparturePlayers(TArray<ADigetherGameplayPlayerState*>& OutPlayers) const
{
	OutPlayers.Reset();
	if (!HasAuthority() || !IsValid(GameState))
	{
		return;
	}
	// 거점에서 끊긴 플레이어는 자리 반환으로 명단에 없음(ReleaseHubSeat)
	// 비활성 PS는 이전 탐사 중 끊긴 뒤 미복귀 참가자, 잠금 중 확정된 관전 합류자도 그날 출발 인원 제외(S01)
	for (APlayerState* State : GameState->PlayerArray)
	{
		ADigetherGameplayPlayerState* Player = Cast<ADigetherGameplayPlayerState>(State);
		APlayerController* Controller = IsValid(Player) ? Cast<APlayerController>(Player->GetOwner()) : nullptr;
		if (IsValid(Player) && Player->HasAuthority() && Player->GetWorld() == GetWorld()
			&& Player->IsGameplayInitialized() && Player->GetGameplayPlayerId().IsValid()
			&& !Player->IsInactive() && !InactivePlayerArray.Contains(Player)
			&& IsValid(Controller) && Controller->GetWorld() == GetWorld()
			&& Controller->GetPlayerState<ADigetherGameplayPlayerState>() == Player
			&& !IsJoinSpectator(Controller))
		{
			OutPlayers.AddUnique(Player);
		}
	}
}

void ADigetherGameplayGameMode::CollectRunParticipants(TArray<ADigetherGameplayPlayerState*>& OutPlayers) const
{
	OutPlayers.Reset();
	if (!HasAuthority())
	{
		return;
	}
	// 출발 때 확정한 같은 원본만 정산, 탐사 중 끊겨 inactive여도 참가 자격 유지
	for (const TWeakObjectPtr<ADigetherGameplayPlayerState>& Participant : RunParticipants)
	{
		ADigetherGameplayPlayerState* Player = Participant.Get();
		if (IsValid(Player) && Player->HasAuthority() && Player->GetWorld() == GetWorld()
			&& Player->IsGameplayInitialized() && Player->GetGameplayPlayerId().IsValid())
		{
			OutPlayers.AddUnique(Player);
		}
	}
}

bool ADigetherGameplayGameMode::IsValidRequester(const ADigetherGameplayPlayerController* Requester) const
{
	const ADigetherGameplayPlayerState* Player = IsValid(Requester)
		? Requester->GetPlayerState<ADigetherGameplayPlayerState>() : nullptr;
	return HasAuthority() && bRunReady && !bRestoreRejected && IsValid(Requester) && Requester->HasAuthority()
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
	Context.PeriodGeneration = State->GetQuotaComponent()->CaptureState().Quota.PeriodGeneration;
	TArray<APawn*> Pawns;
	ElevatorActor->GetOverlappingPawns(Pawns);
	TArray<ADigetherGameplayPlayerState*> EligiblePlayers;
	if (RunCoordinatorComponent->CapturePublicState().Phase == EDigetherRunPhase::InProgress)
	{
		CollectRunParticipants(EligiblePlayers);
	}
	else
	{
		CollectDeparturePlayers(EligiblePlayers);
	}
	TArray<FGuid> PassengerIds;
	for (const APawn* Pawn : Pawns)
	{
		const ADigetherGameplayPlayerState* Player = Pawn->GetPlayerState<ADigetherGameplayPlayerState>();
		if (IsValid(Player) && EligiblePlayers.Contains(Player) && Player->GetGameplayPawn() == Pawn
			&& IsValid(Player->GetLifeComponent())
			&& Player->GetLifeComponent()->CaptureState().PublicState.Life == EDigetherLifeState::Active)
		{
			PassengerIds.Add(Player->GetGameplayPlayerId());
		}
	}
	// 엔진 overlap 배열 순서 변화가 같은 탑승자의 매니페스트 순서를 바꾸지 않게 함
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
	// 관전 합류자는 몸이 없고 그날 운행 불참, 레버 사용 불가(S01)
	if (!IsValidRequester(Requester) || !IsConfiguredLever(Lever) || IsJoinSpectator(Requester)
		|| RunCoordinatorComponent->CapturePublicState().bGameOver)
	{
		return false;
	}
	bool bRangeRequired = true;
#if !UE_BUILD_SHIPPING
	bRangeRequired = !(bSmokeActive && SmokeController.Get() == Requester);
#endif
	if (bRangeRequired && !IsWithinLeverRange(Requester, Lever))
	{
		return false;
	}
	ADigetherHubStateActor* Hub = ADigetherHubStateActor::Find(GetWorld());
	FTransform SelectedStageTransform;
	if (IsValid(Hub) && Lever->GetLeverKind() == EDigetherElevatorLeverKind::Departure
		&& !Hub->GetSelectedStageTransform(SelectedStageTransform))
	{
		return false;
	}
	ADigetherGameplayPlayerState* Player = Requester->GetPlayerState<ADigetherGameplayPlayerState>();
	if (RunCoordinatorComponent->CapturePublicState().Phase == EDigetherRunPhase::InProgress)
	{
		TArray<ADigetherGameplayPlayerState*> Participants;
		CollectRunParticipants(Participants);
		if (!Participants.Contains(Player))
		{
			// 거점에 남았다 재접속한 플레이어는 이번 탐사 운행 변경 불가
			return false;
		}
	}
	const EDigetherLifeState Life = Player->GetLifeComponent()->CaptureState().PublicState.Life;
	bool bCanOperate = Life == EDigetherLifeState::Active;
#if !UE_BUILD_SHIPPING
	// DigetherRunSmoke 자동 시험 중에만 탈출한 시험자의 남은 빈 편 구동 허용
	// 개발 빌드 일반 플레이에서도 관전자는 레버 사용 불가
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
	// 기획 답변 06·추가(2026-11-06): 마지막 상승은 안쪽 레버로만 시작한다.
	if (Lever->GetLeverKind() == EDigetherElevatorLeverKind::Ascent
		&& Lever->GetPlacement() == EDigetherElevatorLeverPlacement::Outside
		&& Context.CompletedAscents + 1 >= Context.TripLimit)
	{
		return false;
	}
	Context.bOtherLeverOccupied = DigetherGameplayRunFlowInternal::FindOccupiedSiblingLever(LeverActors, Lever) != nullptr;
	const EDigetherElevatorLeverResult Result = Lever->GetLeverComponent()->TryBeginPull(Player->GetGameplayPlayerId(), Context);
	if (Result == EDigetherElevatorLeverResult::Started)
	{
		if (Lever->GetLeverKind() == EDigetherElevatorLeverKind::Departure)
		{
			DepartureRequester = Requester;
			if (IsValid(Hub))
			{
				Hub->LockDestination(true);
			}
		}
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
	const bool bReleased = Lever->GetLeverComponent()->ReleasePull(Requester->GetPlayerState<ADigetherGameplayPlayerState>()->GetGameplayPlayerId())
		== EDigetherElevatorLeverResult::Released;
	if (bReleased && Lever->GetLeverKind() == EDigetherElevatorLeverKind::Departure
		&& ElevatorActor.IsValid() && ElevatorActor->GetElevatorComponent()->GetPublicState().Phase == EDigetherElevatorPhase::HubWaiting)
	{
		if (ADigetherHubStateActor* Hub = ADigetherHubStateActor::Find(GetWorld()))
		{
			Hub->LockDestination(false);
		}
	}
	return bReleased;
}

bool ADigetherGameplayGameMode::IsWithinLeverRange(const ADigetherGameplayPlayerController* Requester,
	const ADigetherElevatorLeverActor* Lever) const
{
	const APawn* Pawn = IsValid(Requester) ? Requester->GetPawn() : nullptr;
	if (!IsValid(Pawn) || !IsValid(Lever) || Lever->GetWorld() != GetWorld()
		|| FVector::DistSquared(Pawn->GetActorLocation(), Lever->GetActorLocation()) > FMath::Square(RunTuning.InteractionDistanceCm))
	{
		return false;
	}
	FVector Eye;
	FRotator Rotation;
	Requester->GetPlayerViewPoint(Eye, Rotation);
	const FVector Target = Lever->GetActorLocation() + FVector(0, 0, 80);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LeverInteractionSight), false, Pawn);
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Params)
		|| Hit.GetActor() == Lever || Hit.Distance + 4 >= FVector::Dist(Eye, Target);
}

// 도메인 확정 알림은 사건만 보관, 아래 조율 함수는 도메인 호출 반환 뒤 실행
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

void ADigetherGameplayGameMode::HandleDoorClosed(const EDigetherElevatorPhase OriginPhase)
{
	bDoorClosedPending = true;
	PendingDoorOriginPhase = OriginPhase;
}

void ADigetherGameplayGameMode::HandleDoorReopenedByOverweight(const EDigetherElevatorPhase OriginPhase)
{
	bDoorReopenedPending = true;
	PendingDoorOriginPhase = OriginPhase;
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
	// 잠금 해제(탐사 종료 귀환·출발 실패로 문 재개방) 시 관전 합류자를 일반 참가자로 합류
	if (!JoinSpectators.IsEmpty() && !IsNewJoinLockedNow())
	{
		ReleaseJoinSpectators();
	}
	// 기획 S33: 게임 오버 단계(정산 화면·실패 사건·크레딧)는 출발·납부·레버 없이 그 단계만 진행
	if (GameOverFlowComponent->IsActive())
	{
		TickGameOverAt(GetGameOverNowSeconds());
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
			// 기획: 갱도 문 닫힘 완료와 시간 종료가 같은 처리 시점이면 문 닫힘 완료 먼저 판정
			AdvanceMineDoorBeforeTimeExpiry(DeltaSeconds);
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
	// 문 닫힘 중에도 탑승·하차가 바뀌어 무게 갱신, 갱도 상승 문만 초과 시 재개방
	if (Context.ElevatorPhase == EDigetherElevatorPhase::BottomWaiting
		|| Context.ElevatorPhase == EDigetherElevatorPhase::DoorClosing)
	{
		Elevator->UpdateLoad(Context.LoadEntries);
		if (bDoorReopenedPending)
		{
			ProcessDoorReopenedByOverweight();
		}
	}
	// 기획 S24·T63·T78: 같은 처리 시점의 붓기 주기를 레버보다 먼저 판정
	// 달성 시 아래 레버 처리가 바뀐 기간 세대를 참조, 이번에 차지 않는 게이지는 취소·이번에 차는 레버는 새 기간 1일차 출발
	ProcessPourTicks(DeltaSeconds);
	for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Lever : LeverActors)
	{
		if (Lever.IsValid() && TryMakePullContext(Context))
		{
			UDigetherElevatorLeverComponent* Component = Lever->GetLeverComponent();
			const FGuid PullerId = Component->GetActivePullerId();
			const bool bCheckRange = PullerId.IsValid();
			if (bCheckRange)
			{
				bool bInRange = false;
				for (APlayerState* Entry : GameState->PlayerArray)
				{
					const ADigetherGameplayPlayerState* Player = Cast<ADigetherGameplayPlayerState>(Entry);
					const ADigetherGameplayPlayerController* Controller = Player ? Cast<ADigetherGameplayPlayerController>(Player->GetOwner()) : nullptr;
					bool bSmokeRequester = false;
#if !UE_BUILD_SHIPPING
					bSmokeRequester = bSmokeActive && SmokeController.Get() == Controller;
#endif
					const UDigetherLifeStateComponent* Life = Player ? Player->GetLifeComponent() : nullptr;
					const bool bAliveOperator = IsValid(Life) && (Life->CaptureState().PublicState.Life == EDigetherLifeState::Active
						|| (bSmokeRequester && Life->CaptureState().PublicState.Life == EDigetherLifeState::Escaped));
					bInRange |= Player && Player->GetGameplayPlayerId() == PullerId && IsValidRequester(Controller)
						&& bAliveOperator && (bSmokeRequester || IsWithinLeverRange(Controller, Lever.Get()));
				}
				if (!bInRange)
				{
					Component->ReleasePull(PullerId);
					if (Lever->GetLeverKind() == EDigetherElevatorLeverKind::Departure)
					{
						if (ADigetherHubStateActor* Hub = ADigetherHubStateActor::Find(GetWorld())) Hub->LockDestination(false);
					}
				}
			}
			// 게이지 확정 화면도 문 닫힘 시작 완료까지 보류
			Component->BeginNotificationBatch();
			const EDigetherElevatorLeverResult Advanced = Component->AdvancePull(DeltaSeconds, Context);
			if (Advanced == EDigetherElevatorLeverResult::InterruptedConditions
				&& Lever->GetLeverKind() == EDigetherElevatorLeverKind::Departure)
			{
				// 레버 조작 중 목표 달성 등으로 취소된 출발은 목적지 고정도 해제
				if (ADigetherHubStateActor* Hub = ADigetherHubStateActor::Find(GetWorld())) Hub->LockDestination(false);
			}
			if (bDeparturePending)
			{
				BeginDoorClose(EDigetherElevatorLeverKind::Departure);
			}
			if (bAscentPending)
			{
				BeginDoorClose(EDigetherElevatorLeverKind::Ascent);
			}
			Component->EndNotificationBatch();
		}
	}
	if (bTimeExpiredPending)
	{
		ProcessPendingRunEnd();
		return;
	}
	// 문 닫힘 중에도 걸어서 탑승 가능(기획), 문턱에서 밀어내지 않음
	const EDigetherElevatorPhase PreviousPhase = Elevator->GetPublicState().Phase;
	Elevator->AdvanceTravel(DeltaSeconds);
	if (PreviousPhase != Elevator->GetPublicState().Phase)
	{
		LogRunStep(TEXT("TravelPhaseChanged"));
	}
	if (PreviousPhase == EDigetherElevatorPhase::InitialDescent
		&& Elevator->GetPublicState().Phase == EDigetherElevatorPhase::BottomWaiting
		&& RunCoordinatorComponent->CapturePublicState().Phase == EDigetherRunPhase::InProgress
		&& !RunCoordinatorComponent->IsClockStarted())
	{
		// 기획: 제한 시간은 최초 하강 후 갱도 도착 순간부터(첫 사람이 나갈 때까지 대기 안 함)
		RunCoordinatorComponent->StartClock(GetWorld()->GetTimeSeconds());
		LogRunStep(TEXT("RunClockStarted"));
	}
	if (bDoorClosedPending)
	{
		ProcessDoorClosed();
	}
	ProcessPendingRunEnd();
}

void ADigetherGameplayGameMode::AdvanceMineDoorBeforeTimeExpiry(const float DeltaSeconds)
{
	UDigetherElevatorComponent* Elevator = ElevatorActor.IsValid() ? ElevatorActor->GetElevatorComponent() : nullptr;
	if (!IsValid(Elevator) || Elevator->GetPublicState().Phase != EDigetherElevatorPhase::DoorClosing)
	{
		return;
	}
	FDigetherElevatorPullContext Context;
	if (TryMakePullContext(Context))
	{
		// 닫힘 직전 무게 초과면 평소처럼 재개방, 시간 종료는 갱도 대기 편으로 처리
		Elevator->UpdateLoad(Context.LoadEntries);
		if (bDoorReopenedPending)
		{
			ProcessDoorReopenedByOverweight();
		}
	}
	if (Elevator->GetPublicState().Phase != EDigetherElevatorPhase::DoorClosing)
	{
		return;
	}
	// 이번 처리 시점 안에 닫힘이 끝날 때만 편 고정, 아니면 시간 종료가 닫히는 중인 편 처리
	Elevator->AdvanceTravel(DeltaSeconds);
	if (bDoorClosedPending)
	{
		ProcessDoorClosed();
	}
}

void ADigetherGameplayGameMode::ResetLeverPull(const EDigetherElevatorLeverKind Kind)
{
	if (Kind == EDigetherElevatorLeverKind::Departure)
	{
		if (ADigetherHubStateActor* Hub = ADigetherHubStateActor::Find(GetWorld())) Hub->LockDestination(false);
	}
	for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Lever : LeverActors)
	{
		if (Lever.IsValid() && Lever->GetLeverKind() == Kind)
		{
			Lever->GetLeverComponent()->ResetPull();
		}
	}
}

void ADigetherGameplayGameMode::BeginDoorClose(const EDigetherElevatorLeverKind Kind)
{
	const bool bDeparture = Kind == EDigetherElevatorLeverKind::Departure;
	if (bDeparture)
	{
		bDeparturePending = false;
	}
	else
	{
		bAscentPending = false;
	}
	const FDigetherRunPublicState Run = RunCoordinatorComponent->CapturePublicState();
	const bool bRunInProgress = Run.Phase == EDigetherRunPhase::InProgress;
	const bool bAllowed = bDeparture ? !bRunInProgress && !Run.bGameOver : bRunInProgress;
	// 확정 게이지는 문 닫힘 동안 유지되어 레버 계속 잡기 불필요, 실패 시 게이지 0
	if (!bAllowed || ElevatorActor->GetElevatorComponent()->BeginDoorClose() != EDigetherElevatorResult::Success)
	{
		ResetLeverPull(Kind);
		return;
	}
	if (bDeparture)
	{
		CloseHubTradingForDeparture();
		// 기획 S02·S24(PQ-22 현재 기준): 4일차 납부 마감은 게이지 완료 순간 기록
		// 문 닫힘 뒤 출발 실패·공통 로딩 실패로 거점 복귀해도 기록 유지, 1~3일차는 마감 아님
		if (const ADigetherGameplayGameState* State = GetGameState<ADigetherGameplayGameState>())
		{
			RunCoordinatorComponent->HandleDepartureLeverCompleted(State->GetQuotaComponent()->CaptureState().Quota);
		}
	}
	LogRunStep(bDeparture ? TEXT("DepartureDoorClosing") : TEXT("AscentDoorClosing"));
}

void ADigetherGameplayGameMode::CloseHubTradingForDeparture()
{
	// 기획: 출발 게이지 완료 순간 구매·납부·신규 참가 접수 종료, 열린 상점·외형·납부 창 닫기
	// 새 요청은 문 닫힘 단계의 각 판정(시설·납부·참가)이 거절, 여기서는 진행 중 납부 입력·화면만 종료
	StopAllPours(EDigetherPourStopReason::DepositClosed);
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		if (ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(Iterator->Get()))
		{
			Controller->Client_CloseHubForRun();
		}
	}
}

void ADigetherGameplayGameMode::ProcessDoorReopenedByOverweight()
{
	bDoorReopenedPending = false;
	// 기획 A3: 재개방된 시도는 게이지 0, 상승 횟수 미포함
	ResetLeverPull(PendingDoorOriginPhase == EDigetherElevatorPhase::HubWaiting
		? EDigetherElevatorLeverKind::Departure : EDigetherElevatorLeverKind::Ascent);
	LogRunStep(TEXT("DoorReopenedOverweight"));
}

void ADigetherGameplayGameMode::ProcessDoorClosed()
{
	bDoorClosedPending = false;
	if (PendingDoorOriginPhase == EDigetherElevatorPhase::HubWaiting)
	{
		ProcessDeparture();
	}
	else
	{
		ProcessAscent();
	}
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
		// 출발 실패는 거점 문 재개방으로 참가 잠금(C3)도 해제, 문 닫힘 없이 호출 시 변화 없음
		if (!bDeparturePullConsumed && ElevatorActor.IsValid())
		{
			ElevatorActor->GetElevatorComponent()->ReopenDoor();
			if (ADigetherHubStateActor* Hub = ADigetherHubStateActor::Find(GetWorld())) Hub->LockDestination(false);
		}
	};
	ADigetherGameplayGameState* State = GetGameState<ADigetherGameplayGameState>();
	UDigetherElevatorComponent* Elevator = ElevatorActor->GetElevatorComponent();
	FDigetherElevatorState ElevatorState;
	TArray<ADigetherGameplayPlayerState*> Players;
	CollectDeparturePlayers(Players);
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
		|| FDigetherRunCoordinatorRules::StartRun(Candidate, RunTuning, FixedRunRuleTuning,
			GetWorld()->GetTimeSeconds(), false).Result != EDigetherRunResult::Success)
	{
		return;
	}
	UBoxComponent* Volume = ElevatorActor->GetBoardingVolume();
	const FVector Extent = Volume->GetUnscaledBoxExtent();
	const FTransform SourceVolume = Volume->GetComponentTransform();
	ADigetherHubStateActor* Hub = ADigetherHubStateActor::Find(GetWorld());
	FTransform DestinationVolume = SourceVolume;
	if (IsValid(Hub))
	{
		FTransform StageTransform;
		if (!Hub->GetSelectedStageTransform(StageTransform)) return;
		DestinationVolume = Volume->GetRelativeTransform() * StageTransform;
	}
	// 기획 사슬 소집: 문이 완전히 닫힌 순간 엘리베이터 밖 참가자(자리 비운 사람 포함)를 바닥 안쪽 빈 위치로 이동
	// 직접 탑승자는 엘리베이터 안 상대 위치 유지
	// 거점에서 이미 끊긴 참가자는 CollectDeparturePlayers가 제외, 소집·출발 인원 미포함
	TArray<APawn*> BoardedPawns;
	ElevatorActor->GetOverlappingPawns(BoardedPawns);
	TArray<FVector> LocalDestinations;
	TArray<bool> SummonedFlags;
	TArray<FVector> OccupiedLocal;
	for (const ADigetherGameplayPlayerState* Player : Players)
	{
		APawn* Pawn = Player->GetGameplayPawn();
		const APlayerController* Controller = Cast<APlayerController>(Player->GetOwner());
		if (!IsValid(Pawn) || Pawn->GetWorld() != GetWorld() || Pawn->GetController() != Controller
			|| !IsValid(Player->GetLifeComponent())
			|| Player->GetLifeComponent()->CaptureState().PublicState.Life != EDigetherLifeState::Active)
		{
			UE_LOG(LogDigetherFramework, Error, TEXT("Departure rejected: active player Pawn unavailable."));
			return;
		}
		const bool bBoarded = BoardedPawns.Contains(Pawn);
		const FVector Local = bBoarded ? SourceVolume.InverseTransformPosition(Pawn->GetActorLocation()) : FVector::ZeroVector;
		LocalDestinations.Add(Local);
		SummonedFlags.Add(!bBoarded);
		if (bBoarded)
		{
			OccupiedLocal.Add(Local);
		}
	}
	const int32 SummonCount = SummonedFlags.FilterByPredicate([](const bool bSummoned) { return bSummoned; }).Num();
	TArray<FVector> SummonLocal;
	if (!DigetherFrameworkRules::TryPlanChainSummon(Extent, DigetherFrameworkDefaults::PlayerSpacingCm,
		OccupiedLocal, SummonCount, SummonLocal))
	{
		// 임시 처리: 기획은 빈 위치가 없으면 겹침 허용 후 밀어내기, MVP는 소집 위치 계산 실패를 출발 거절로 처리
		UE_LOG(LogDigetherFramework, Error, TEXT("Departure rejected: chain summon space unavailable (%d summoned)."), SummonCount);
		return;
	}
	TArray<FVector> Destinations;
	for (int32 Index = 0, SummonIndex = 0; Index < Players.Num(); ++Index)
	{
		const FVector Local = SummonedFlags[Index] ? SummonLocal[SummonIndex++] : LocalDestinations[Index];
		Destinations.Add(DestinationVolume.TransformPosition(Local));
	}
	// 기획 1.1.0(S30·T91): 출발은 저장 성공과 무관
	// 공통 로딩 실패 복귀용으로 확정 직전 진행을 메모리에만 보관(디스크 기준점 저장·복원 규칙 폐기), 캡처 실패해도 출발 허용
	FDigetherSaveSnapshot Baseline;
	if (CaptureProgress(Baseline))
	{
		DepartureBaseline = MoveTemp(Baseline);
	}
	else
	{
		DepartureBaseline.Reset();
		UE_LOG(LogDigetherFramework, Warning, TEXT("출발 기준점 캡처 실패: 이번 출발은 공통 로딩 실패 복귀를 할 수 없습니다."));
	}
	// 모든 값·대상·공간 검사 뒤 같은 서버 호출 안에서 전원 이동과 런·운행 확정
	FScopedDigetherRunNotifications Notifications(RunCoordinatorComponent, Elevator, LeverActors);
	if (IsValid(Hub))
	{
		Hub->EndAllRest();
		if (!Hub->MoveElevatorToSelectedStage()) return;
	}
	RunParticipants.Reset();
	DeparturePlayerCount = Players.Num();
	LastRecordedAscent = 0;
	PendingRecordedAscent = 0;
	// 4일차 납부 마감은 게이지 완료(BeginDoorClose) 때 기록 완료
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		RunParticipants.Add(Players[Index]);
		if (UPawnMovementComponent* Movement = Players[Index]->GetGameplayPawn()->GetMovementComponent())
		{
			Movement->StopMovementImmediately();
		}
		// 끌려가는 동안 충돌 판정이 없도록 sweep 없이 순간이동 1회
		Players[Index]->GetGameplayPawn()->SetActorLocation(Destinations[Index], false, nullptr, ETeleportType::TeleportPhysics);
	}
	// 제한 시간은 최초 하강 후 갱도 도착 때 StartClock으로 시작
	RunCoordinatorComponent->StartRun(RunTuning, FixedRunRuleTuning, GetWorld()->GetTimeSeconds(), false);
	Elevator->DepartHub();
	for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Lever : LeverActors)
	{
		if (Lever.IsValid() && Lever->GetLeverKind() == EDigetherElevatorLeverKind::Departure)
		{
			Lever->GetLeverComponent()->ResetPull();
		}
	}
	bDeparturePullConsumed = true;
	// 납부 입력·거점 화면은 게이지 완료 때 CloseHubTradingForDeparture가 이미 닫음
	Notifications.Complete();
	TArray<ADigetherGameplayPlayerState*> AllPlayers;
	CollectPlayers(AllPlayers);
	for (ADigetherGameplayPlayerState* Player : AllPlayers)
	{
		Player->ClearRunResult();
	}
	const float ChainSummonSeconds = FDigetherElevatorTuning().ChainSummonSeconds;
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		if (APlayerController* Controller = Cast<APlayerController>(Players[Index]->GetOwner()))
		{
			if (SummonedFlags[Index])
			{
				if (ADigetherGameplayPlayerController* Gameplay = Cast<ADigetherGameplayPlayerController>(Controller))
				{
					// 암전 연출은 소유 클라이언트 BP 담당, 위치 보정보다 먼저 보내 연출 시작 선행
					Gameplay->Client_ChainSummoned(ChainSummonSeconds);
				}
			}
			// DefaultPawn 소유자 위치에도 서버 순간이동 명시 전달
			Controller->ClientSetLocation(Destinations[Index], Players[Index]->GetGameplayPawn()->GetActorRotation());
		}
	}
	LogRunStep(TEXT("DepartureConfirmed"));
	ProcessPendingRunEnd();
}

void ADigetherGameplayGameMode::ProcessAscent()
{
	// 안·밖 중 게이지 확정 레버로 이번 편 처리, 점유 규칙상 확정 레버는 하나
	int32 ConfirmedLevers = 0;
	ADigetherElevatorLeverActor* AscentLever = FindConfirmedAscentLever(LeverActors, ConfirmedLevers);
	UDigetherElevatorComponent* Elevator = ElevatorActor->GetElevatorComponent();
	bool bPullConsumed = false;
	ON_SCOPE_EXIT
	{
		if (!bPullConsumed)
		{
			// 성공·실패 모두 다음 편을 위해 안·밖 상승 레버 전체 0
			ResetLeverPull(EDigetherElevatorLeverKind::Ascent);
			// 고정·출발 실패 시도는 문 재개방, 상승 횟수 미포함(기획 A3)
			Elevator->ReopenDoor();
		}
	};
	if (RunCoordinatorComponent->CapturePublicState().Phase != EDigetherRunPhase::InProgress)
	{
		return;
	}
	if (ConfirmedLevers > 1)
	{
		UE_LOG(LogDigetherFramework, Error, TEXT("Ascent rejected: %d ascent levers confirmed at once."), ConfirmedLevers);
		return;
	}
	// 레버 게이지가 이번 편용으로 확정됐는지 확인, 게이지 완료 때 목록은 미사용
	FDigetherElevatorAscentConfirmed LeverAscent;
	if (!IsValid(AscentLever) || !AscentLever->GetLeverComponent()->TryGetConfirmedAscent(LeverAscent)
		|| LeverAscent.TripNumber != LastRecordedAscent + 1)
	{
		return;
	}
	// 기획 A3: 문이 완전히 닫힌 순간 안에 있는 생존자·화물로 편 고정
	FDigetherElevatorPullContext Context;
	if (!TryMakePullContext(Context))
	{
		return;
	}
	const FDigetherElevatorAscentConfirmed Ascent = DigetherElevatorRules::CaptureAscent(Context.CompletedAscents,
		Context.TripLimit, Context.LoadEntries, Context.CapacityKg, FDigetherElevatorTuning());
	if (Ascent.TripNumber != LastRecordedAscent + 1)
	{
		return;
	}
	TArray<ADigetherGameplayPlayerState*> Passengers;
	TArray<ADigetherGameplayPlayerState*> Participants;
	CollectRunParticipants(Participants);
	for (const FGuid& Id : Ascent.PassengerIds)
	{
		ADigetherGameplayPlayerState* Found = nullptr;
		for (ADigetherGameplayPlayerState* Player : Participants)
		{
			if (Player->GetGameplayPlayerId() == Id)
			{
				Found = Player;
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
	FScopedDigetherRunNotifications Notifications(RunCoordinatorComponent, Elevator, LeverActors);
	if (Elevator->ConfirmAscent(Ascent) != EDigetherElevatorResult::Success)
	{
		return;
	}
	ResetLeverPull(EDigetherElevatorLeverKind::Ascent);
	bPullConsumed = true;
	for (ADigetherGameplayPlayerState* Player : Passengers)
	{
		Player->GetLifeComponent()->ApplyEscape(EDigetherEscapeKind::Elevator, false);
	}
	// 레버 통지가 아닌 실제 운행 컴포넌트 확정 사건을 한 편에 한 번 소비
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
			if (UPawnMovementComponent* Movement = Player->GetGameplayPawn() ? Player->GetGameplayPawn()->GetMovementComponent() : nullptr)
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
	if (!CurrentRunId.IsValid() || CompletedSettlementRunIds.Contains(CurrentRunId))
	{
		UE_LOG(LogDigetherFramework, Error, TEXT("런 정산 거절: 런 ID가 없거나 이미 정산된 런입니다."));
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
	CollectRunParticipants(Players);
	TArray<FDigetherFrameworkPlayerSnapshot> Snapshots;
	TArray<FVector> ReturnLocations;
	for (ADigetherGameplayPlayerState* Player : Players)
	{
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
			BoardedPawns.Contains(Player->GetGameplayPawn()), Context.ElevatorPhase);
		AController* Controller = Cast<AController>(Player->GetOwner());
		AActor* Start = IsValid(Controller) ? FindPlayerStart(Controller) : nullptr;
		if (!IsValid(Start) && Player->IsInactive())
		{
			// 끊긴 참가자도 같은 귀환·정산, Controller 없이 남은 Pawn만 이동
			for (TActorIterator<APlayerStart> Iterator(GetWorld()); Iterator; ++Iterator)
			{
				Start = *Iterator;
				break;
			}
		}
		if (!IsValid(Start))
		{
			UE_LOG(LogDigetherFramework, Error, TEXT("Run return rejected: PlayerStart unavailable."));
			return;
		}
		ReturnLocations.Add(Start->GetActorLocation());
		Input.Players.Add(PlayerInput);
		Snapshots.Add(Snapshot);
	}
	FDigetherFrameworkReturnPlan Plan;
	const FDigetherQuotaState PreviousQuota = State->GetQuotaComponent()->CaptureState().Quota;
	if (!DigetherFrameworkRules::TryPlanReturn(Input, Snapshots, PreviousQuota,
		RunTuning, DeparturePlayerCount, Plan))
	{
		UE_LOG(LogDigetherFramework, Error, TEXT("Run return candidate rejected before commit."));
		return;
	}
	// 기획 S33 241행: 히든 수익·사망 손실은 귀환 순간이 아닌 결과 크레딧 진입 때 공개
	// 귀환 확정에서는 공개 목록(HiddenReveals) 미작성, 결과 기록은 귀환 적용 뒤 BeginGameOverAfterReturn이 생성
	FDigetherElevatorAscentConfirmed ReturnAscent;
	if (Plan.Termination.ElevatorAction != EDigetherRunElevatorAction::Unchanged)
	{
		const TArray<FDigetherElevatorLoadEntry> ReturnLoad = Plan.Termination.bEjectCargo
			? TArray<FDigetherElevatorLoadEntry>() : Context.LoadEntries;
		ReturnAscent = DigetherElevatorRules::CaptureAscent(Context.CompletedAscents, Context.TripLimit,
			ReturnLoad, Context.CapacityKg, FDigetherElevatorTuning());
		// 하부 대기·하부 문 닫힘 모두 갱도 대기로 귀환·사출(기획 A3), 닫힘 완료 대기 안 함
		FDigetherElevatorState ElevatorState;
		if (!Elevator->TryGetServerState(ElevatorState)
			|| DigetherElevatorRules::Transition(ElevatorState, EDigetherElevatorCommand::RequestAscent, 0.f,
				Context.TripLimit, RunTuning.ElevatorTravelSeconds, FDigetherElevatorTuning()).Result != EDigetherElevatorResult::Success)
		{
			return;
		}
	}
	// 여기까지 생사·손실·정산·일차·귀환 후보 검사 완료, 다음 블록은 같은 서버 실행 안에서 적용
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
	CompletedSettlementRunIds.AddUnique(CurrentRunId);
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

	if (ADigetherHubStateActor* Hub = ADigetherHubStateActor::Find(GetWorld())) Hub->ReturnElevatorToHub();
	// 임시 처리: 기획 미정(별도 귀환 장면 연결 전 즉시 거점 귀환), 2026-11-04
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		Players[Index]->GetWalletComponent()->RestoreState(Plan.Players[Index].SettledWallet, false);
	}
	State->GetQuotaComponent()->SetQuotaState(Plan.ReturnedQuota, false);
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		Players[Index]->GetLifeComponent()->Revive(false);
		if (APawn* Pawn = Players[Index]->GetGameplayPawn())
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
				DigetherGameplayRunFlowInternal::SetSpectateViewTarget(Controller, Controller->GetPawn());
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
	bDoorClosedPending = false;
	bDoorReopenedPending = false;
	PendingRecordedAscent = 0;
	LastRecordedAscent = 0;
	RunParticipants.Reset();
	// 공개 귀환 사건은 부활·이동·다음 운행 준비 완료 뒤 송출
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
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		ADigetherGameplayPlayerState* Player = Players[Index];
		Player->GetWalletComponent()->NotifyStateChanged();
		Player->GetLifeComponent()->NotifyStateChanged();
		// 기획 S33 220행 ④: 5일차 실패 귀환도 같은 검은 개인 정산 화면 경유
		// 게임 오버 단계가 정산 화면 시간 뒤 실패 사건으로 넘어가며 결과 닫음(ClearRunResult), 크레딧 진행은 미차단
		const FDigetherRunResult Result = DigetherFrameworkRules::MakeRunResult(
			CurrentRunId, PreviousQuota.Day, Plan.Players[Index]);
		const bool bPublished = Player->PublishRunResult(Result);
		ensureMsgf(bPublished, TEXT("Committed run result could not be published."));
	}
	LogRunStep(Plan.bGameOver ? TEXT("BaseReturned.GameOver") : TEXT("BaseReturned"));
	CurrentRunId = FGuid::NewGuid();
	DepartureBaseline.Reset();
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* Controller = Iterator->Get();
		if (IsValid(Controller) && !IsValid(Controller->GetPawn()))
		{
			HandleStartingNewPlayer(Controller);
		}
	}
	// 기획 S30 ①·④: 일차 변경·정산·복구금·지급 완료 기록·부활·거점 귀환 판정·귀환 합류 반영 뒤 한 번 저장
	// 실패 시 알림·자동 재시도, 다음 출발은 미차단
	// 기획 S33 220행 ⑦: 5일차 실패 귀환은 이 저장에 게임 오버 확정·결과 기록 포함(슬롯 초기화는 크레딧 뒤)
	if (Plan.bGameOver)
	{
		BeginGameOverAfterReturn(GetGameOverNowSeconds());
	}
	else
	{
		RequestAutosave(TEXT("RunCompleted"));
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
	UE_LOG(LogDigetherFramework, Log, TEXT("Step=%s Phase=%s Run=%s Day=%d Life[Active=%d Dead=%d Escaped=%d] ConfirmedAscents=%d Aftershock=%d RemainingSeconds=%.2f"),
		Step, *UEnum::GetValueAsString(Phase), *UEnum::GetValueAsString(RunCoordinatorComponent->CapturePublicState().Phase),
		Quota.Day, Active, Dead, Escaped,
		RunCoordinatorComponent->GetConfirmedAscentCount(), RunCoordinatorComponent->CapturePublicState().AftershockStage,
		RunCoordinatorComponent->GetRemainingSeconds());
	// 기획 S24: 정확한 공동 인정액 비공개, 호스트 개발 로그도 기본 표시 제외·Verbose만
	UE_LOG(LogDigetherFramework, Verbose, TEXT("Step=%s RemainingQuota=%d"), Step,
		FMath::Max(0, Quota.TargetCredits - Quota.DepositedCredits));
}
