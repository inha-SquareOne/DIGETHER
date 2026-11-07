// 기획 S33 게임 오버 순서(검은 개인 정산 → 실패 사건 → 결과 크레딧 → 현재 슬롯 새 게임)의 서버 연결층이다.
// 단계 판정은 UDigetherGameOverFlowComponent·DigetherGameOverRules가 하고, 여기서는 월드(몸·저장·새 게임)에 적용한다.
#include "Game/Framework/DigetherGameplayGameMode.h"

#include "Engine/World.h"
#include "Game/Economy/DigetherQuotaStateComponent.h"
#include "Game/Elevator/DigetherElevatorActor.h"
#include "Game/Framework/DigetherFrameworkLog.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Hub/DigetherHubWellActor.h"
#include "Game/Run/DigetherGameOverFlowComponent.h"
#include "Game/Run/DigetherGameOverPublicStateComponent.h"
#include "Game/Run/DigetherGameOverRules.h"
#include "Game/Run/DigetherRunCoordinatorComponent.h"
#include "Game/Save/DigetherSaveRules.h"
#include "Game/Save/DigetherSaveTuning.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"

namespace
{
	// 임시값: 기획 미정(우물 미배치 맵의 대체 대기 지점, 엘리베이터 기준 cm), 2026-11-05. 맵에 우물을 두면 쓰지 않는다.
	const FVector FallbackWaitingOffsetCm(0.0, 0.0, -1500.0);
	// 임시값: 기획 미정(대체 대기 지점 간격, cm), 2026-11-05.
	constexpr double FallbackWaitingSpacingCm = 120.0;

	FDigetherGameResultInput MakeResultInput(const FDigetherSavePlayerRecord& Record)
	{
		FDigetherGameResultInput Input;
		Input.DisplayName = Record.DisplayName;
		Input.JoinOrder = Record.JoinOrder;
		Input.Wallet = Record.Wallet;
		return Input;
	}

}

bool ADigetherGameplayGameMode::IsGameOverActive() const
{
	return GameOverFlowComponent->IsActive() || RunCoordinatorComponent->CapturePublicState().bGameOver;
}

double ADigetherGameplayGameMode::GetGameOverNowSeconds() const
{
	if (IsValid(GameState))
	{
		return GameState->GetServerWorldTimeSeconds();
	}
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetTimeSeconds() : 0.0;
}

ADigetherGameplayPlayerController* ADigetherGameplayGameMode::FindHostController() const
{
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(Iterator->Get());
		if (IsHostRequester(Controller) && IsValid(Controller->PlayerState))
		{
			return Controller;
		}
	}
	return nullptr;
}

void ADigetherGameplayGameMode::PublishGameOverState()
{
	const ADigetherGameplayGameState* State = GetGameState<ADigetherGameplayGameState>();
	if (UDigetherGameOverPublicStateComponent* Public = IsValid(State) ? State->GetGameOverPublicStateComponent() : nullptr)
	{
		Public->SetPublicState(GameOverFlowComponent->GetPublicState());
	}
}

void ADigetherGameplayGameMode::CloseHubTradingForGameOver()
{
	// 게임 오버 뒤에는 이전 진행의 바닥 금화를 줍거나 붓지 못한다. 열린 거점 화면도 닫는다(구매·납부 불가, S33 234행).
	StopAllPours(EDigetherPourStopReason::DepositClosed);
	ClearFloorCoins();
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		if (ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(Iterator->Get()))
		{
			Controller->Client_CloseHubForRun();
		}
	}
}

bool ADigetherGameplayGameMode::BuildGameResult(const FGuid& ResultId, FDigetherGameResult& OutResult) const
{
	const ADigetherGameplayGameState* State = GetGameState<ADigetherGameplayGameState>();
	if (!IsValid(State) || !IsValid(State->GetQuotaComponent()))
	{
		return false;
	}
	// 저장 명단(떠난 사람 포함)을 먼저 넣고, 지금 월드에 있는 참가자의 최신 장부로 덮는다. 키는 서버 전용 인증 키다.
	TArray<FDigetherGameResultInput> Inputs;
	TArray<FString> Keys;
	for (const FDigetherSavePlayerRecord& Record : SavedPlayerRecords)
	{
		Inputs.Add(MakeResultInput(Record));
		Keys.Add(Record.AuthenticatedPlayerKey);
	}
	TArray<ADigetherGameplayPlayerState*> Players;
	CollectPlayers(Players);
	for (const ADigetherGameplayPlayerState* Player : Players)
	{
		FString Key;
		if (DigetherSaveRules::MakeAuthenticatedPlayerKey(Player->GetUniqueId(), Key) != EDigetherSaveResult::Success)
		{
			// ID 없는 로컬 직접 실행도 결과에서 빠지지 않게 서버 GUID 키를 쓴다. 저장 명단과는 겹치지 않는다.
			Key = TEXT("Gameplay:") + Player->GetGameplayPlayerKey();
		}
		FDigetherGameResultInput Input;
		Input.DisplayName = DigetherSaveRules::MakeStoredDisplayName(Player->GetPlayerName());
		Input.JoinOrder = Player->GetJoinOrder();
		Input.Wallet = Player->CaptureState();
		const int32 Existing = Keys.IndexOfByKey(Key);
		if (Existing == INDEX_NONE)
		{
			Inputs.Add(Input);
			Keys.Add(Key);
			continue;
		}
		if (Input.JoinOrder <= 0)
		{
			Input.JoinOrder = Inputs[Existing].JoinOrder;
		}
		Inputs[Existing] = Input;
	}
	// 11/6 기획 답변 08: 같은 게임의 명단은 0원·중도 이탈 여부와 무관하게 결과에 남김
	const FDigetherQuotaState Quota = State->GetQuotaComponent()->CaptureState().Quota;
	return DigetherGameOverRules::TryMakeGameResult(ResultId, GameId, Quota.TotalDay, Quota.PeriodGeneration, Inputs,
		OutResult);
}

void ADigetherGameplayGameMode::BeginGameOverAfterReturn(const double NowSeconds)
{
	CloseHubTradingForGameOver();
	if (!IsValid(ADigetherHubWellActor::Find(GetWorld())))
	{
		// 맵 배치 전 임시 처리: 우물이 없거나 둘 이상이면 엘리베이터 위치를 흡입 중심으로 쓴다. 진행은 막지 않는다.
		UE_LOG(LogDigetherFramework, Warning,
			TEXT("Game over: exactly one ADigetherHubWellActor is required; using the elevator as the suction center."));
	}
	// 기획 S33 240행: 호스트가 마지막 정산을 끝낸 뒤 결과 기록 ID와 합산 값을 한 번 확정한다. 클라이언트가 계산하지 않는다.
	FDigetherGameResult Result;
	bHasGameResult = BuildGameResult(FGuid::NewGuid(), Result);
	if (!bHasGameResult)
	{
		// 결과를 만들 수 없어도 실패 사건·크레딧·새 게임으로는 넘어가야 한다(S33 244행 오류 복구). 빈 결과를 보여 준다.
		UE_LOG(LogDigetherFramework, Error, TEXT("Game over result could not be built; credits will show an empty result."));
		Result = FDigetherGameResult();
		Result.ResultId = FGuid::NewGuid();
		Result.GameId = GameId;
	}
	GameResult = bHasGameResult ? Result : FDigetherGameResult();
	bNewGameFailureReported = false;
	AbsorbedPawns.Reset();
	if (!GameOverFlowComponent->Begin(NowSeconds, Result, GameOverTuning))
	{
		UE_LOG(LogDigetherFramework, Error, TEXT("Game over flow could not start (already active or invalid tuning)."));
	}
	PublishGameOverState();
	LogRunStep(TEXT("GameOver.SettlementScreen"));
	// 기획 S33 220행 ⑦: 정산·통계·게임 오버 확정·결과 기록을 담은 일관된 상태로 저장한다. 실패하면 알리고 재시도하며
	// 진행(실패 사건·크레딧)은 막지 않는다. 확정 저장이 끝내 실패한 채 종료하면 다음 실행은 마지막 유효 저장에서 이어 간다.
	RequestAutosave(TEXT("GameOverReturn"));
}

void ADigetherGameplayGameMode::TickGameOverAt(const double NowSeconds)
{
	if (!GameOverFlowComponent->IsActive() || !IsValid(GameState))
	{
		return;
	}
	const ADigetherHubWellActor* Well = ADigetherHubWellActor::Find(GetWorld());
	const FVector Center = IsValid(Well) ? Well->GetSuctionCenter()
		: (ElevatorActor.IsValid() ? ElevatorActor->GetActorLocation() : FVector::ZeroVector);
	const double RadiusCm = IsValid(Well) ? Well->GetSuctionRadiusCm() : TNumericLimits<double>::Max();
	// 기다릴 대상은 지금 방에 연결된 참가자다. 떠난 사람(자리 반환)과 잠금 중 합류 관전자는 넣지 않는다(T34).
	TArray<FDigetherGameOverPresentPlayer> Present;
	for (APlayerState* Entry : GameState->PlayerArray)
	{
		const ADigetherGameplayPlayerState* Player = Cast<ADigetherGameplayPlayerState>(Entry);
		const APlayerController* Controller = IsValid(Player) ? Cast<APlayerController>(Player->GetOwner()) : nullptr;
		if (!IsValid(Player) || Player->IsInactive() || !Player->IsGameplayInitialized() || !IsValid(Controller)
			|| IsJoinSpectator(Controller))
		{
			continue;
		}
		FDigetherGameOverPresentPlayer Item;
		Item.PlayerId = Player->GetPlayerId();
		const APawn* Pawn = Player->GetGameplayPawn();
		Item.DistanceToWellCm = IsValid(Pawn) ? FMath::Min(FVector::Dist(Pawn->GetActorLocation(), Center), RadiusCm) : 0.0;
		Present.Add(Item);
	}
	const FDigetherGameOverStep Step = GameOverFlowComponent->Advance(NowSeconds, Present);
	if (Step.bEnteredFailureEvent)
	{
		// 검은 개인 정산 화면을 닫고 거점에서 각자 조작을 유지한 채 실패 사건을 본다(공통 컷신 없음).
		TArray<ADigetherGameplayPlayerState*> Players;
		CollectPlayers(Players);
		for (ADigetherGameplayPlayerState* Player : Players)
		{
			Player->ClearRunResult();
		}
		LogRunStep(TEXT("GameOver.FailureEvent"));
	}
	if (!Step.NewlyAbsorbedPlayerIds.IsEmpty())
	{
		AbsorbPlayers(Step.NewlyAbsorbedPlayerIds);
	}
	if (Step.bEnteredCredits)
	{
		LogRunStep(TEXT("GameOver.Credits"));
	}
	if (Step.bPublicStateChanged)
	{
		PublishGameOverState();
	}
	if (Step.bCreditsFinished || GameOverFlowComponent->IsAwaitingNewGame())
	{
		CreateNextGameAfterCredits();
	}
}

void ADigetherGameplayGameMode::AbsorbPlayers(const TArray<int32>& PlayerIds)
{
	const ADigetherHubWellActor* Well = ADigetherHubWellActor::Find(GetWorld());
	for (APlayerState* Entry : GameState->PlayerArray)
	{
		ADigetherGameplayPlayerState* Player = Cast<ADigetherGameplayPlayerState>(Entry);
		if (!IsValid(Player) || !PlayerIds.Contains(Player->GetPlayerId()))
		{
			continue;
		}
		APawn* Pawn = Player->GetGameplayPawn();
		ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(Player->GetOwner());
		if (IsValid(Pawn))
		{
			const int32 Seat = AbsorbedPawns.Num();
			FTransform Waiting = IsValid(Well) ? Well->GetWaitingTransform(Seat) : FTransform::Identity;
			if (!IsValid(Well))
			{
				const FVector Base = (ElevatorActor.IsValid() ? ElevatorActor->GetActorLocation() : FVector::ZeroVector)
					+ FallbackWaitingOffsetCm;
				Waiting.SetLocation(Base + FVector(FallbackWaitingSpacingCm * Seat, 0.0, 0.0));
			}
			// 흡입된 사람은 우물 아래에서 다른 사람을 기다린다. 이동·충돌을 끄고 시점 조작만 남긴다.
			if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
			{
				Movement->StopMovementImmediately();
				Movement->Deactivate();
			}
			Pawn->SetActorEnableCollision(false);
			Pawn->SetActorLocation(Waiting.GetLocation(), false, nullptr, ETeleportType::TeleportPhysics);
			AbsorbedPawns.Add(Pawn);
			if (IsValid(Controller))
			{
				Controller->ClientSetLocation(Waiting.GetLocation(), Pawn->GetActorRotation());
			}
		}
		if (IsValid(Controller))
		{
			Controller->Client_SetGameOverAbsorbed(true);
		}
	}
}

void ADigetherGameplayGameMode::ReleaseAbsorbedPawns()
{
	for (const TWeakObjectPtr<APawn>& Weak : AbsorbedPawns)
	{
		APawn* Pawn = Weak.Get();
		if (!IsValid(Pawn))
		{
			continue;
		}
		Pawn->SetActorEnableCollision(true);
		if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
		{
			Movement->Activate(true);
		}
	}
	AbsorbedPawns.Reset();
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		if (ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(Iterator->Get()))
		{
			Controller->Client_SetGameOverAbsorbed(false);
		}
	}
}

bool ADigetherGameplayGameMode::SetCreditFastForward(ADigetherGameplayPlayerController* Requester, const bool bFastForward)
{
	return SetCreditFastForwardAt(Requester, bFastForward, GetGameOverNowSeconds());
}

bool ADigetherGameplayGameMode::SetCreditFastForwardAt(ADigetherGameplayPlayerController* Requester,
	const bool bFastForward, const double NowSeconds)
{
	// 기획 S33 239행·T33: 호스트만 빨리감기를 조작한다. 참가자 입력은 반영하지 않으며 개인별 빨리감기는 없다.
	if (!IsHostRequester(Requester) || !GameOverFlowComponent->SetCreditFastForward(bFastForward, NowSeconds))
	{
		return false;
	}
	PublishGameOverState();
	return true;
}

bool ADigetherGameplayGameMode::ApplyNewGame(ADigetherGameplayPlayerController* Host, const FDigetherSaveSnapshot& Initial,
	const TCHAR* Reason)
{
	if (!RestoreProgress(Initial))
	{
		return false;
	}
	bHasSaveStartupAttempted = true;
	// 이전 진행의 재시도 건은 쓰지 않는다. 새 게임의 첫 쓰기가 성공할 때까지 이 게임 ID의 저장은 명시 새 게임으로 쓴다.
	ClearPendingAutosave();
	ExplicitNewGamePendingGameId = Initial.GameId;
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* Controller = Iterator->Get();
		// 잠금 중 합류 관전자는 잠금이 풀린 뒤 관전 해제에서 합류한다.
		if (IsValid(Controller) && !IsValid(Controller->GetPawn()) && !IsJoinSpectator(Controller))
		{
			HandleStartingNewPlayer(Controller);
		}
	}
	// 현재 슬롯만 새 게임으로 쓴다(다른 슬롯 유지, 새 슬롯 자동 생성 없음). 실패하면 알리고 같은 스냅샷을 재시도한다.
	RequestAutosaveSnapshot(Initial, Reason);
	return true;
}

bool ADigetherGameplayGameMode::CreateNextGameAfterCredits()
{
	if (!GameOverFlowComponent->IsAwaitingNewGame())
	{
		return false;
	}
	ADigetherGameplayPlayerController* Host = FindHostController();
	// 이전 게임 결과를 따로 고정해 보관한다. 새 게임 스냅샷이 이 목록을 함께 저장한다.
	const TArray<FDigetherGameResult> PreviousArchive = ArchivedGameResults;
	if (bHasGameResult)
	{
		DigetherGameOverRules::ArchiveGameResult(ArchivedGameResults, GameResult, DigetherSaveTuning::MaxArchivedGameResults);
	}
	// 마지막 탐사 중 끊겨 비활성으로 남은 참가자는 떠난 사람이다. 결과에는 이미 남겼고 새 게임 기록에는 넘기지 않는다.
	TArray<APlayerState*> Inactive = InactivePlayerArray;
	for (APlayerState* Entry : Inactive)
	{
		if (ADigetherGameplayPlayerState* Player = Cast<ADigetherGameplayPlayerState>(Entry))
		{
			ReleaseHubSeat(Player);
			Player->Destroy();
		}
	}
	FDigetherSaveSnapshot Initial;
	if (!IsValid(Host) || !BuildNewGameSnapshot(Host, Initial))
	{
		ArchivedGameResults = PreviousArchive;
		if (!bNewGameFailureReported)
		{
			bNewGameFailureReported = true;
			UE_LOG(LogDigetherFramework, Error, TEXT("New game after credits could not be built; retrying every tick."));
			NotifySaveResult(Host, false, NSLOCTEXT("DigetherSave", "GameOverNewGameFailed",
				"새 게임을 만들지 못했습니다. 다시 시도합니다."));
		}
		return false;
	}
	ReleaseAbsorbedPawns();
	if (!ApplyNewGame(Host, Initial, TEXT("GameOverNewGame")))
	{
		ArchivedGameResults = PreviousArchive;
		if (!bNewGameFailureReported)
		{
			bNewGameFailureReported = true;
			UE_LOG(LogDigetherFramework, Error, TEXT("New game after credits could not be applied; retrying every tick."));
		}
		return false;
	}
	// 새 게임 ID는 BuildNewGameSnapshot에서 한 번만 만들어졌다. 완료 신호가 다시 와도 Flow가 None이라 반복하지 않는다(T38).
	GameOverFlowComponent->CompleteNewGame(GetGameOverNowSeconds());
	bNewGameFailureReported = false;
	PublishGameOverState();
	NotifySaveResult(Host, true, NSLOCTEXT("DigetherSave", "GameOverNewGame", "새 게임을 시작했습니다."));
	// 잠금(게임 오버)이 풀렸다. 잠금 중 확정된 합류 관전자를 새 거점에 합류시킨다.
	if (!JoinSpectators.IsEmpty() && !IsNewJoinLockedNow())
	{
		ReleaseJoinSpectators();
	}
	LogRunStep(TEXT("GameOver.NewGame"));
	return true;
}

bool ADigetherGameplayGameMode::StartNewGameAfterSavedGameOver(ADigetherGameplayPlayerController* Host,
	const FDigetherSaveSnapshot& Snapshot)
{
	if (!Snapshot.RunPublicState.bGameOver)
	{
		return false;
	}
	FDigetherGameResult Result;
	bool bHasResult = Snapshot.bHasGameResult;
	if (bHasResult)
	{
		Result = Snapshot.GameResult;
	}
	else
	{
		// 결과 기록 도입 전 게임 오버 저장이다. 저장된 플레이어 기록으로 결과를 다시 만든다(누적 획득은 0으로 읽힘).
		TArray<FDigetherGameResultInput> Inputs;
		for (const FDigetherSavePlayerRecord& Record : Snapshot.Players)
		{
			Inputs.Add(MakeResultInput(Record));
		}
		bHasResult = DigetherGameOverRules::TryMakeGameResult(FGuid::NewGuid(), Snapshot.GameId,
			Snapshot.Quota.Quota.TotalDay, Snapshot.Quota.Quota.PeriodGeneration, Inputs, Result);
	}
	const TArray<FDigetherGameResult> PreviousArchive = ArchivedGameResults;
	ArchivedGameResults = Snapshot.ArchivedGameResults;
	if (bHasResult)
	{
		DigetherGameOverRules::ArchiveGameResult(ArchivedGameResults, Result, DigetherSaveTuning::MaxArchivedGameResults);
	}
	FDigetherSaveSnapshot Initial;
	if (!BuildNewGameSnapshot(Host, Initial) || !ApplyNewGame(Host, Initial, TEXT("SavedGameOverNewGame")))
	{
		ArchivedGameResults = PreviousArchive;
		return false;
	}
	LogRunStep(TEXT("GameOver.SavedNewGame"));
	return true;
}
