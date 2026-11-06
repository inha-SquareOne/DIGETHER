// 개발용 스모크 자동 운전·크레딧 지급, Shipping 구현은 상태 불변
#include "Game/Framework/DigetherGameplayGameMode.h"

#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Economy/DigetherWalletComponent.h"
#include "Game/Elevator/DigetherElevatorActor.h"
#include "Game/Elevator/DigetherElevatorComponent.h"
#include "Game/Elevator/DigetherElevatorLeverActor.h"
#include "Game/Elevator/DigetherElevatorLeverComponent.h"
#include "Game/Framework/DigetherFrameworkLog.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Framework/DigetherGameplayRunFlowInternal.h"
#include "Game/Run/DigetherRunCoordinatorComponent.h"

bool ADigetherGameplayGameMode::GrantCredits(ADigetherGameplayPlayerController* Requester, const int32 Amount)
{
#if !UE_BUILD_SHIPPING
	// 기획 S33: 게임 오버 중 개발 지급도 거절(결과 기록 확정 뒤 잔고 불변)
	if (IsValidRequester(Requester) && Amount > 0 && !IsGameOverActive())
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
			// 개발 자동 운전, 첫 편 탈출 뒤에도 남은 빈 편을 구동해 최종 도착까지 검사
			// 안·밖 상승 레버 중 점유된 레버가 있으면 다른 레버 미시작(BeginLeverPull과 같은 규칙)
			FDigetherElevatorPullContext LeverContext = Context;
			LeverContext.bOtherLeverOccupied = DigetherGameplayRunFlowInternal::FindOccupiedSiblingLever(LeverActors, Lever.Get()) != nullptr;
			const EDigetherElevatorLeverResult Result = Lever->GetLeverComponent()->TryBeginPull(
				Controller->GetPlayerState<ADigetherGameplayPlayerState>()->GetGameplayPlayerId(), LeverContext);
			if (Result == EDigetherElevatorLeverResult::Started)
			{
				if (Kind == EDigetherElevatorLeverKind::Departure)
				{
					DepartureRequester = Controller;
				}
				LogRunStep(Kind == EDigetherElevatorLeverKind::Departure ? TEXT("DeparturePullStarted") : TEXT("AscentPullStarted"));
				break;
			}
		}
	}
}
#endif
