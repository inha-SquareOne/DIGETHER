// 기획 S24 동시 붓기 납부와 바닥 금화(줍기·저장 복원)의 서버 연결층이다. 값 계산은 FDigetherEconomyRules,
// 세션 순서는 FDigetherPourSessionRegistry가 맡고 여기서는 조건 검사·적용·롤백·알림만 한다.
#include "Game/Framework/DigetherGameplayGameMode.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Economy/DigetherDepositStationActor.h"
#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Economy/DigetherFloorCoinActor.h"
#include "Game/Economy/DigetherQuotaStateComponent.h"
#include "Game/Economy/DigetherWalletComponent.h"
#include "Game/Framework/DigetherFrameworkLog.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Run/DigetherRunCoordinatorComponent.h"
#include "Game/Save/DigetherSaveRules.h"
#include "GameFramework/Pawn.h"

namespace
{
	// 한 처리 시점에 몰아서 처리하는 붓기 주기 상한이다. 긴 프레임 뒤에도 한 번에 과하게 처리하지 않고 남은 주기는 다음 시점에 한다.
	constexpr int32 MaxPourTicksPerFrame = 10;
	// 임시값(기획 미정, 2026-11-05): 시선이 투입구를 향한다고 보는 최소 코사인이다(약 60도 이내).
	constexpr double PourAimMinDot = 0.5;
	// 레버 시선 검사와 같은 허용 오차다. 투입구 바로 앞 표면에 맞은 경우를 가림으로 보지 않는다.
	constexpr double PourSightToleranceCm = 4.0;

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

	EDigetherEconomyResult ToStopResult(const EDigetherPourStopReason Reason)
	{
		switch (Reason)
		{
		case EDigetherPourStopReason::InteractionLost: return EDigetherEconomyResult::PourInteractionLost;
		case EDigetherPourStopReason::Disconnected: return EDigetherEconomyResult::PourInteractionLost;
		case EDigetherPourStopReason::DepositClosed: return EDigetherEconomyResult::DepositClosed;
		case EDigetherPourStopReason::PeriodChanged: return EDigetherEconomyResult::PeriodMismatch;
		case EDigetherPourStopReason::Loaded: return EDigetherEconomyResult::PeriodMismatch;
		case EDigetherPourStopReason::InsufficientFunds: return EDigetherEconomyResult::InsufficientFunds;
		case EDigetherPourStopReason::CreditTooSmall: return EDigetherEconomyResult::DepositCreditTooSmall;
		default: return EDigetherEconomyResult::InvalidInput;
		}
	}

	EDigetherPourStopReason ToStopReason(const EDigetherEconomyResult Result)
	{
		switch (Result)
		{
		case EDigetherEconomyResult::InsufficientFunds: return EDigetherPourStopReason::InsufficientFunds;
		case EDigetherEconomyResult::DepositCreditTooSmall: return EDigetherPourStopReason::CreditTooSmall;
		case EDigetherEconomyResult::DepositClosed: return EDigetherPourStopReason::DepositClosed;
		case EDigetherEconomyResult::QuotaAlreadySatisfied: return EDigetherPourStopReason::PeriodChanged;
		default: return EDigetherPourStopReason::Rejected;
		}
	}
}

bool ADigetherGameplayGameMode::ResolveDepositContext(ADigetherGameplayPlayerController* Requester, FString& OutPlayerKey,
	UDigetherWalletComponent*& OutWallet, UDigetherQuotaStateComponent*& OutQuota) const
{
	// 관전 합류자는 그날 정산에서 빠지므로 납부하지 않는다(S01).
	if (!HasAuthority() || !bRunReady || !IsValid(Requester) || !Requester->HasAuthority()
		|| Requester->GetWorld() != GetWorld() || IsJoinSpectator(Requester)
		|| !TryGetDepositPlayerKey(Requester, OutPlayerKey))
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

bool ADigetherGameplayGameMode::IsActivePourer(ADigetherGameplayPlayerController* Requester) const
{
	if (!IsValidRequester(Requester) || IsJoinSpectator(Requester))
	{
		return false;
	}
	const ADigetherGameplayPlayerState* Player = Requester->GetPlayerState<ADigetherGameplayPlayerState>();
	const APawn* Pawn = Requester->GetPawn();
	// TODO(S24 ①): 돈자루를 손에 든 조건은 손 아이템 시스템이 생기면 여기서 확인한다. 지금은 생략한다.
	return IsValid(Pawn) && Player->GetGameplayPawn() == Pawn && IsValid(Player->GetLifeComponent())
		&& Player->GetLifeComponent()->CaptureState().PublicState.Life == EDigetherLifeState::Active;
}

ADigetherGameplayPlayerController* ADigetherGameplayGameMode::FindControllerByDepositKey(const FString& PlayerKey) const
{
	if (!IsValid(GetWorld()))
	{
		return nullptr;
	}
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(Iterator->Get());
		FString Key;
		if (TryGetDepositPlayerKey(Controller, Key) && Key == PlayerKey)
		{
			return Controller;
		}
	}
	return nullptr;
}

ADigetherDepositStationActor* ADigetherGameplayGameMode::FindDepositStation(const FName StationId) const
{
	if (StationId.IsNone() || !IsValid(GetWorld()))
	{
		return nullptr;
	}
	ADigetherDepositStationActor* Matched = nullptr;
	for (TActorIterator<ADigetherDepositStationActor> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		if (IsValid(*Iterator) && Iterator->GetStationId() == StationId)
		{
			if (Matched != nullptr)
			{
				return nullptr;
			}
			Matched = *Iterator;
		}
	}
	return Matched;
}

bool ADigetherGameplayGameMode::IsPourInletReachable(const ADigetherGameplayPlayerController* Requester,
	const ADigetherDepositStationActor* Station) const
{
	const APawn* Pawn = IsValid(Requester) ? Requester->GetPawn() : nullptr;
	if (!IsValid(Pawn) || !IsValid(Station) || Station->GetWorld() != GetWorld() || RunTuning.InteractionDistanceCm <= 0.f)
	{
		return false;
	}
	const FTransform Inlet = Station->GetPourInletTransform();
	const FVector InletLocation = Inlet.GetLocation();
	const FVector PawnLocation = Pawn->GetActorLocation();
	if (FVector::DistSquared(PawnLocation, InletLocation) > FMath::Square(RunTuning.InteractionDistanceCm))
	{
		return false;
	}
	// 입구 바깥쪽(+X)에 서 있어야 한다. 정산기 뒤나 옆에서 벽 너머로 붓지 못하게 한다.
	if (FVector::DotProduct(Inlet.GetUnitAxis(EAxis::X), PawnLocation - InletLocation) <= 0.0)
	{
		return false;
	}
	FVector Eye;
	FRotator Rotation;
	Requester->GetPlayerViewPoint(Eye, Rotation);
	const FVector ToInlet = InletLocation - Eye;
	if (!ToInlet.IsNearlyZero() && FVector::DotProduct(Rotation.Vector(), ToInlet.GetSafeNormal()) < PourAimMinDot)
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PourInletSight), false, Pawn);
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, InletLocation, ECC_Visibility, Params)
		|| Hit.GetActor() == Station || Hit.Distance + PourSightToleranceCm >= ToInlet.Size();
}

FDigetherDepositResponse ADigetherGameplayGameMode::BeginPour(ADigetherGameplayPlayerController* Requester,
	ADigetherDepositStationActor* Station)
{
	FDigetherDepositResponse Response;
	Response.StationId = IsValid(Station) ? Station->GetStationId() : NAME_None;
	FString PlayerKey;
	UDigetherWalletComponent* Wallet = nullptr;
	UDigetherQuotaStateComponent* Quota = nullptr;
	if (!ResolveDepositContext(Requester, PlayerKey, Wallet, Quota) || !IsActivePourer(Requester))
	{
		return Response;
	}
	// 기획 S24 ②: 정상 거점 준비 상태이며 출발 게이지 완료 전이어야 한다. 마감 뒤 요청은 차감 없이 거절한다.
	if (IsDepositClosed())
	{
		Response.Result = EDigetherEconomyResult::DepositClosed;
		return Response;
	}
	if (!IsValid(Station) || FindDepositStation(Station->GetStationId()) != Station)
	{
		return Response;
	}
	if (!IsPourInletReachable(Requester, Station))
	{
		Response.Result = EDigetherEconomyResult::PourInteractionLost;
		return Response;
	}
	if (Wallet->CaptureState().BalanceCredits <= 0)
	{
		Response.Result = EDigetherEconomyResult::InsufficientFunds;
		return Response;
	}
	const FDigetherQuotaState QuotaState = Quota->CaptureState().Quota;
	FDigetherPourSession Session;
	Response.Result = PourSessions.BeginPour(PlayerKey, Station->GetStationId(), QuotaState.PeriodGeneration,
		PourClockSeconds, RunTuning.DepositIntervalSeconds, &Session);
	Response.PeriodGeneration = Session.PeriodGeneration;
#if !UE_BUILD_SHIPPING
	// 원격 수신 지연과 분리한 서버 붓기 경합 검증용 시각·활성 수
	if (Response.Result == EDigetherEconomyResult::Success)
	{
		UE_LOG(LogDigetherFramework, Display, TEXT("Pour session: Event=Begin Player=%d Active=%d"),
			Requester->GetPlayerState<ADigetherGameplayPlayerState>()->GetPlayerId(), PourSessions.CaptureSessions().Num());
	}
#endif
	return Response;
}

FDigetherDepositResponse ADigetherGameplayGameMode::EndPour(ADigetherGameplayPlayerController* Requester)
{
	FDigetherDepositResponse Response;
	FString PlayerKey;
	UDigetherWalletComponent* Wallet = nullptr;
	UDigetherQuotaStateComponent* Quota = nullptr;
	if (!ResolveDepositContext(Requester, PlayerKey, Wallet, Quota))
	{
		return Response;
	}
	FDigetherPourSession Stopped;
	// 이미 확정한 주기는 유지한다. 손을 뗀 응답은 호출한 RPC가 직접 돌려준다.
	const bool bStopped = PourSessions.StopPour(PlayerKey, &Stopped);
	Response.Result = bStopped ? EDigetherEconomyResult::Success : EDigetherEconomyResult::SessionNotFound;
#if !UE_BUILD_SHIPPING
	if (bStopped)
	{
		UE_LOG(LogDigetherFramework, Display, TEXT("Pour session: Event=End Player=%d Active=%d"),
			Requester->GetPlayerState<ADigetherGameplayPlayerState>()->GetPlayerId(), PourSessions.CaptureSessions().Num());
	}
#endif
	Response.StationId = Stopped.StationId;
	Response.PeriodGeneration = Stopped.PeriodGeneration;
	return Response;
}

void ADigetherGameplayGameMode::StopPour(const FString& PlayerKey, const EDigetherPourStopReason Reason)
{
	FDigetherPourSession Stopped;
	if (PourSessions.StopPour(PlayerKey, &Stopped))
	{
		NotifyPourStopped(Stopped, Reason);
	}
}

void ADigetherGameplayGameMode::StopAllPours(const EDigetherPourStopReason Reason)
{
	for (const FDigetherPourSession& Session : PourSessions.StopAll())
	{
		NotifyPourStopped(Session, Reason);
	}
}

void ADigetherGameplayGameMode::StopPourForController(ADigetherGameplayPlayerController* Controller,
	const EDigetherPourStopReason Reason)
{
	FString PlayerKey;
	if (HasAuthority() && TryGetDepositPlayerKey(Controller, PlayerKey))
	{
		StopPour(PlayerKey, Reason);
	}
}

void ADigetherGameplayGameMode::NotifyPourStopped(const FDigetherPourSession& Session, const EDigetherPourStopReason Reason)
{
	UE_LOG(LogDigetherFramework, Verbose, TEXT("Pour stopped: Station=%s Reason=%d"),
		*Session.StationId.ToString(), static_cast<int32>(Reason));
	ADigetherGameplayPlayerController* Controller = FindControllerByDepositKey(Session.PlayerKey);
	if (!IsValid(Controller))
	{
		return;
	}
	FDigetherDepositResponse Response;
	Response.Result = ToStopResult(Reason);
	Response.StationId = Session.StationId;
	Response.PeriodGeneration = Session.PeriodGeneration;
	Controller->Client_DepositResult(EDigetherDepositOperation::PourStopped, Response);
}

bool ADigetherGameplayGameMode::ValidatePourSession(const FDigetherPourSession& Session,
	EDigetherPourStopReason& OutReason, ADigetherGameplayPlayerController*& OutController,
	ADigetherDepositStationActor*& OutStation) const
{
	OutController = FindControllerByDepositKey(Session.PlayerKey);
	OutStation = nullptr;
	if (!IsValid(OutController))
	{
		OutReason = EDigetherPourStopReason::Disconnected;
		return false;
	}
	FString PlayerKey;
	UDigetherWalletComponent* Wallet = nullptr;
	UDigetherQuotaStateComponent* Quota = nullptr;
	if (!ResolveDepositContext(OutController, PlayerKey, Wallet, Quota) || !IsActivePourer(OutController))
	{
		OutReason = EDigetherPourStopReason::InteractionLost;
		return false;
	}
	if (IsDepositClosed())
	{
		OutReason = EDigetherPourStopReason::DepositClosed;
		return false;
	}
	// 시작한 기간과 다른 목표에는 붓지 않는다. 새 목표에 내려면 다시 눌러야 한다(S24 ④).
	if (Quota->CaptureState().Quota.PeriodGeneration != Session.PeriodGeneration)
	{
		OutReason = EDigetherPourStopReason::PeriodChanged;
		return false;
	}
	OutStation = FindDepositStation(Session.StationId);
	if (!IsPourInletReachable(OutController, OutStation))
	{
		OutReason = EDigetherPourStopReason::InteractionLost;
		return false;
	}
	if (Wallet->CaptureState().BalanceCredits <= 0)
	{
		OutReason = EDigetherPourStopReason::InsufficientFunds;
		return false;
	}
	return true;
}

void ADigetherGameplayGameMode::ProcessPourTicks(const float DeltaSeconds)
{
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.f)
	{
		return;
	}
	PourClockSeconds += DeltaSeconds;
	if (PourSessions.IsEmpty())
	{
		return;
	}
	// 매 처리 시점에 모든 붓기의 조건을 다시 본다. 어긋난 사람만 멈추고 다른 사람의 붓기는 이어간다.
	for (const FDigetherPourSession& Session : PourSessions.CaptureSessions())
	{
		EDigetherPourStopReason Reason = EDigetherPourStopReason::Rejected;
		ADigetherGameplayPlayerController* Controller = nullptr;
		ADigetherDepositStationActor* Station = nullptr;
		if (!ValidatePourSession(Session, Reason, Controller, Station))
		{
			StopPour(Session.PlayerKey, Reason);
		}
	}
	const TArray<FDigetherPourDueTick> DueTicks = PourSessions.CollectDueTicks(PourClockSeconds,
		RunTuning.DepositIntervalSeconds, MaxPourTicksPerFrame);
	for (const FDigetherPourDueTick& Tick : DueTicks)
	{
		if (ProcessPourTick(Tick))
		{
			// 목표 달성으로 기간이 바뀌었다. 같은 처리 시점의 남은 주기는 이전 목표 것이므로 처리하지 않는다.
			break;
		}
	}
}

bool ADigetherGameplayGameMode::ProcessPourTick(const FDigetherPourDueTick& Tick)
{
	const FDigetherPourSession* Found = PourSessions.Find(Tick.PlayerKey);
	if (Found == nullptr || Found->StartSequence != Tick.StartSequence)
	{
		// 같은 처리 시점의 앞선 주기에서 멈췄거나 다시 시작했다.
		return false;
	}
	const FDigetherPourSession Session = *Found;
	EDigetherPourStopReason Reason = EDigetherPourStopReason::Rejected;
	ADigetherGameplayPlayerController* Controller = nullptr;
	ADigetherDepositStationActor* Station = nullptr;
	if (!ValidatePourSession(Session, Reason, Controller, Station))
	{
		StopPour(Session.PlayerKey, Reason);
		return false;
	}
	FString PlayerKey;
	UDigetherWalletComponent* Wallet = nullptr;
	UDigetherQuotaStateComponent* Quota = nullptr;
	if (!ResolveDepositContext(Controller, PlayerKey, Wallet, Quota))
	{
		StopPour(Session.PlayerKey, EDigetherPourStopReason::InteractionLost);
		return false;
	}
	const FDigetherWalletState PreviousWallet = Wallet->CaptureState();
	const FDigetherQuotaPublicState PreviousQuota = Quota->CaptureState();
	// 기획 S24 ①~③: 남은 목표·잔고 확인, 그 순간 일차 효율로 인정액 계산, 차감과 인정 반영을 함께 확정한다.
	FDigetherPourTickResult Result = FDigetherEconomyRules::CalculatePourTick(PreviousWallet, PreviousQuota.Quota,
		EconomyTuning, IsDepositClosed());
	if (Result.Result != EDigetherEconomyResult::Success)
	{
		StopPour(Session.PlayerKey, ToStopReason(Result.Result));
		return false;
	}
	ADigetherFloorCoinActor* Coin = nullptr;
	bool bCoinCreated = false;
	const int32 FloorCredits = Result.FloorCredits;
	if (FloorCredits > 0 && !AddFloorCoins(Station, FloorCredits, Coin, bCoinCreated))
	{
		// 바닥 금화를 만들지 못하면 초과분을 지갑에 남겨 돈이 사라지지 않게 한다.
		UE_LOG(LogDigetherFramework, Warning, TEXT("Floor coin spawn failed at station %s; overflow kept in wallet."),
			*Session.StationId.ToString());
		Result = FDigetherEconomyRules::ReclaimFloorCredits(Result);
	}
	const int32 AppliedFloorCredits = Result.FloorCredits;
	FDigetherQuotaPublicState CandidateQuota = PreviousQuota;
	CandidateQuota.Quota = Result.QuotaState;
	if (!FDigetherEconomyRules::IsValidWalletState(Result.WalletState)
		|| !FDigetherEconomyRules::IsValidPublicState(CandidateQuota))
	{
		UndoFloorCoins(Coin, AppliedFloorCredits, bCoinCreated);
		UE_LOG(LogDigetherFramework, Error, TEXT("Pour candidate rejected before commit."));
		StopPour(Session.PlayerKey, EDigetherPourStopReason::Rejected);
		return false;
	}
	// 두 setter는 권한·값 형태만 검사하며 bNotify=false에서 외부 콜백을 부르지 않는다.
	if (Wallet->RestoreState(Result.WalletState, false) != EDigetherEconomyResult::Success)
	{
		UndoFloorCoins(Coin, AppliedFloorCredits, bCoinCreated);
		StopPour(Session.PlayerKey, EDigetherPourStopReason::Rejected);
		return false;
	}
	if (Quota->RestoreState(CandidateQuota, false) != EDigetherEconomyResult::Success)
	{
		const EDigetherEconomyResult Rollback = Wallet->RestoreState(PreviousWallet, false);
		ensureMsgf(Rollback == EDigetherEconomyResult::Success, TEXT("Pour wallet rollback failed."));
		UndoFloorCoins(Coin, AppliedFloorCredits, bCoinCreated);
		StopPour(Session.PlayerKey, EDigetherPourStopReason::Rejected);
		return false;
	}
	PourSessions.AdvanceAfterTick(Tick, RunTuning.DepositIntervalSeconds);
	const auto NotifyTickCommitted = [&]()
	{
		Wallet->NotifyStateChanged();
		Quota->NotifyStateChanged();
		FDigetherDepositResponse Response;
		Response.Result = EDigetherEconomyResult::Success;
		Response.StationId = Session.StationId;
		Response.PeriodGeneration = Session.PeriodGeneration;
		Response.AcceptedCredits = Result.SpentCredits;
		Response.CreditedCredits = Result.CreditedCredits;
		Controller->Client_DepositResult(EDigetherDepositOperation::Submit, Response);
	};
	if (Result.bTargetCompleted)
	{
		CommitQuotaCompletion(*Quota, NotifyTickCommitted);
		return true;
	}
	NotifyTickCommitted();
	if (Result.WalletState.BalanceCredits <= 0)
	{
		// 잔고가 바닥나면 본인 붓기만 멈춘다(S24 입력/조건).
		StopPour(Session.PlayerKey, EDigetherPourStopReason::InsufficientFunds);
	}
	return false;
}

void ADigetherGameplayGameMode::CommitQuotaCompletion(UDigetherQuotaStateComponent& Quota,
	const TFunctionRef<void()> NotifyCommitted)
{
	// 달성 연출 카운터를 먼저 올려 서버 표시가 "달성 → 새 사이클 색" 순서가 되게 한다.
	Quota.MarkQuotaCompleted();
	NotifyCommitted();
	// 기획 S24 ④: 목표를 채운 순간 진행 중인 모든 붓기를 끊는다. 새 목표에 내려면 다시 눌러야 한다.
	StopAllPours(EDigetherPourStopReason::PeriodChanged);
	// 기획 S30 ②: 할당량 달성 시 관련 납부와 새 목표·1일차 전환을 한 번에 자동 저장한다.
	// 실패하면 알리고 자동 재시도하며 납부·출발을 막지 않는다.
	RequestAutosave(TEXT("QuotaCompleted"));
	LogRunStep(TEXT("QuotaCompleted"));
}

ADigetherFloorCoinActor* ADigetherGameplayGameMode::FindFloorCoin(const FName StationId) const
{
	if (!IsValid(GetWorld()))
	{
		return nullptr;
	}
	for (TActorIterator<ADigetherFloorCoinActor> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		ADigetherFloorCoinActor* Coin = *Iterator;
		if (IsValid(Coin) && !Coin->IsActorBeingDestroyed() && !Coin->IsConsumed() && Coin->GetStationId() == StationId)
		{
			return Coin;
		}
	}
	return nullptr;
}

ADigetherFloorCoinActor* ADigetherGameplayGameMode::SpawnFloorCoin(const FGuid& CoinId, const FName StationId,
	const int32 Credits, const FTransform& Transform)
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !IsValid(World) || !CoinId.IsValid() || StationId.IsNone() || Credits < 1)
	{
		return nullptr;
	}
	UClass* CoinClass = FloorCoinClass.Get() != nullptr ? FloorCoinClass.Get() : ADigetherFloorCoinActor::StaticClass();
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADigetherFloorCoinActor* Coin = World->SpawnActor<ADigetherFloorCoinActor>(CoinClass, Transform, Parameters);
	if (!IsValid(Coin))
	{
		return nullptr;
	}
	if (!Coin->InitializeServerState(CoinId, StationId, Credits))
	{
		Coin->Destroy();
		return nullptr;
	}
	return Coin;
}

bool ADigetherGameplayGameMode::AddFloorCoins(ADigetherDepositStationActor* Station, const int32 Credits,
	ADigetherFloorCoinActor*& OutCoin, bool& bOutCreated)
{
	OutCoin = nullptr;
	bOutCreated = false;
	if (!IsValid(Station) || Credits < 1)
	{
		return false;
	}
	// 정산기당 금화 더미 하나에 합산한다.
	if (ADigetherFloorCoinActor* Existing = FindFloorCoin(Station->GetStationId()))
	{
		if (!Existing->AddAmount(Credits))
		{
			return false;
		}
		OutCoin = Existing;
		return true;
	}
	const FTransform Drop = Station->GetCoinDropTransform();
	OutCoin = SpawnFloorCoin(FGuid::NewGuid(), Station->GetStationId(), Credits,
		FTransform(Drop.GetRotation(), Drop.GetLocation()));
	bOutCreated = IsValid(OutCoin);
	return bOutCreated;
}

void ADigetherGameplayGameMode::UndoFloorCoins(ADigetherFloorCoinActor* Coin, const int32 Credits, const bool bCreated)
{
	if (!IsValid(Coin) || Credits < 1)
	{
		return;
	}
	if (bCreated)
	{
		Coin->Destroy();
		return;
	}
	ensureMsgf(Coin->RemoveAmount(Credits), TEXT("Floor coin merge rollback failed."));
}

void ADigetherGameplayGameMode::CaptureFloorCoins(TArray<FDigetherSaveFloorCoin>& OutCoins) const
{
	OutCoins.Reset();
	if (!IsValid(GetWorld()))
	{
		return;
	}
	for (TActorIterator<ADigetherFloorCoinActor> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		const ADigetherFloorCoinActor* Coin = *Iterator;
		if (!IsValid(Coin) || Coin->IsActorBeingDestroyed() || Coin->IsConsumed() || Coin->GetAmountCredits() < 1)
		{
			continue;
		}
		FDigetherSaveFloorCoin& Saved = OutCoins.AddDefaulted_GetRef();
		Saved.CoinId = Coin->GetCoinId();
		Saved.StationId = Coin->GetStationId();
		Saved.AmountCredits = Coin->GetAmountCredits();
		Saved.Transform = Coin->GetActorTransform();
	}
	// 월드 순회 순서가 바뀌어도 같은 바이트가 되게 정렬한다.
	OutCoins.Sort([](const FDigetherSaveFloorCoin& Left, const FDigetherSaveFloorCoin& Right)
	{
		return Left.CoinId.ToString() < Right.CoinId.ToString();
	});
}

void ADigetherGameplayGameMode::ClearFloorCoins()
{
	if (!HasAuthority() || !IsValid(GetWorld()))
	{
		return;
	}
	TArray<ADigetherFloorCoinActor*> Coins;
	for (TActorIterator<ADigetherFloorCoinActor> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		if (IsValid(*Iterator))
		{
			Coins.Add(*Iterator);
		}
	}
	for (ADigetherFloorCoinActor* Coin : Coins)
	{
		Coin->Destroy();
	}
}

bool ADigetherGameplayGameMode::RestoreFloorCoins(const TArray<FDigetherSaveFloorCoin>& Coins)
{
	ClearFloorCoins();
	bool bRestoredAll = true;
	for (const FDigetherSaveFloorCoin& Saved : Coins)
	{
		bRestoredAll &= IsValid(SpawnFloorCoin(Saved.CoinId, Saved.StationId, Saved.AmountCredits, Saved.Transform));
	}
	return bRestoredAll;
}

FDigetherDepositResponse ADigetherGameplayGameMode::PickupFloorCoin(ADigetherGameplayPlayerController* Requester,
	ADigetherFloorCoinActor* Coin)
{
	FDigetherDepositResponse Response;
	FString PlayerKey;
	UDigetherWalletComponent* Wallet = nullptr;
	UDigetherQuotaStateComponent* Quota = nullptr;
	if (!ResolveDepositContext(Requester, PlayerKey, Wallet, Quota) || !IsActivePourer(Requester))
	{
		return Response;
	}
	const FDigetherRunPublicState Run = RunCoordinatorComponent->CapturePublicState();
	// 바닥 금화는 거점에 있다. 탐사 중·게임 오버 처리 중에는 줍지 않는다.
	if (Run.Phase == EDigetherRunPhase::InProgress || Run.bGameOver)
	{
		return Response;
	}
	if (!IsValid(Coin) || Coin->GetWorld() != GetWorld() || Coin->IsActorBeingDestroyed() || Coin->IsConsumed()
		|| Coin->GetAmountCredits() < 1)
	{
		Response.Result = EDigetherEconomyResult::FloorCoinUnavailable;
		return Response;
	}
	Response.StationId = Coin->GetStationId();
	const APawn* Pawn = Requester->GetPawn();
	if (FVector::DistSquared(Pawn->GetActorLocation(), Coin->GetActorLocation())
		> FMath::Square(RunTuning.InteractionDistanceCm))
	{
		Response.Result = EDigetherEconomyResult::PourInteractionLost;
		return Response;
	}
	// 지갑 반영 전에 소비를 표시해 같은 처리 시점의 다른 줍기 요청이 같은 금화를 받지 못하게 한다.
	Coin->MarkConsumed();
	const int32 Amount = Coin->GetAmountCredits();
	FDigetherWalletState Candidate = Wallet->CaptureState();
	if (static_cast<int64>(Candidate.BalanceCredits) + Amount > MAX_int32)
	{
		// 잔고가 넘치면 일부가 사라지므로 줍지 않고 금화를 남긴다.
		Coin->ClearConsumed();
		Response.Result = EDigetherEconomyResult::InvalidAmount;
		return Response;
	}
	// 바닥 금화는 이미 누군가 번 돈이다. 런 지급·히든 통계에 넣지 않고 잔고만 늘린다.
	Candidate.BalanceCredits += Amount;
	Response.Result = Wallet->RestoreState(Candidate);
	if (Response.Result != EDigetherEconomyResult::Success)
	{
		Coin->ClearConsumed();
		return Response;
	}
	Response.AcceptedCredits = Amount;
	Coin->Destroy();
	return Response;
}

bool ADigetherGameplayGameMode::SetQuotaProgressForDev(ADigetherGameplayPlayerController* Requester,
	const int32 DepositedCredits)
{
#if !UE_BUILD_SHIPPING
	ADigetherGameplayGameState* State = GetGameState<ADigetherGameplayGameState>();
	UDigetherQuotaStateComponent* Quota = IsValid(State) ? State->GetQuotaComponent() : nullptr;
	// 기획 S33: 게임 오버 중에는 할당량 진행을 바꾸지 않는다(마감 결과·결과 기록 확정 뒤).
	if (!IsValidRequester(Requester) || !IsValid(Quota) || DepositedCredits < 0 || IsGameOverActive())
	{
		return false;
	}
	FDigetherQuotaState Candidate = Quota->CaptureState().Quota;
	// 목표 이상은 달성 처리(새 목표·자동 저장)를 건너뛰게 되므로 받지 않는다. 달성은 실제 붓기로 시험한다.
	if (DepositedCredits >= Candidate.TargetCredits)
	{
		return false;
	}
	Candidate.DepositedCredits = DepositedCredits;
	return Quota->SetQuotaState(Candidate) == EDigetherEconomyResult::Success;
#else
	return false;
#endif
}
