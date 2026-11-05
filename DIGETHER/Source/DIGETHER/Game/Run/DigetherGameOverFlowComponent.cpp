#include "Game/Run/DigetherGameOverFlowComponent.h"

#include "Game/Run/DigetherGameOverRules.h"
#include "Game/Run/DigetherRunLog.h"

UDigetherGameOverFlowComponent::UDigetherGameOverFlowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

bool UDigetherGameOverFlowComponent::Begin(const double NowSeconds, const FDigetherGameResult& Result,
	const FDigetherGameOverTuning& Tuning)
{
	if (State.Phase != EDigetherGameOverPhase::None || !Result.IsValid() || !FMath::IsFinite(NowSeconds)
		|| !DigetherGameOverRules::IsValidTuning(Tuning))
	{
		return false;
	}
	PendingResult = Result;
	AbsorbSchedule.Reset();
	bNewGameRequested = false;
	State.Tuning = Tuning;
	State.AbsorbedPlayerIds.Reset();
	State.WaitingPlayerCount = 0;
	State.Playback = FDigetherCreditPlayback();
	State.PublishedResult = FDigetherGameResult();
	return EnterPhase(EDigetherGameOverPhase::SettlementScreen, NowSeconds);
}

FDigetherGameOverStep UDigetherGameOverFlowComponent::Advance(const double NowSeconds,
	const TArray<FDigetherGameOverPresentPlayer>& PresentPlayers)
{
	FDigetherGameOverStep Step;
	if (!FMath::IsFinite(NowSeconds))
	{
		return Step;
	}
	switch (State.Phase)
	{
	case EDigetherGameOverPhase::SettlementScreen:
		if (NowSeconds >= DigetherGameOverRules::GetSettlementScreenEndSeconds(State.PhaseStartServerSeconds, State.Tuning)
			&& EnterPhase(EDigetherGameOverPhase::FailureEvent, NowSeconds))
		{
			AbsorbSchedule.Reset();
			State.AbsorbedPlayerIds.Reset();
			State.WaitingPlayerCount = PresentPlayers.Num();
			Step.bEnteredFailureEvent = true;
			Step.bPublicStateChanged = true;
		}
		break;
	case EDigetherGameOverPhase::FailureEvent:
		AdvanceFailureEvent(NowSeconds, PresentPlayers, Step);
		break;
	case EDigetherGameOverPhase::Credits:
		// 끝 판정은 서버만 한다. 종료 신호가 다시 와도 새 게임 요청은 한 번이다(S33 240행·T38).
		if (!bNewGameRequested && DigetherGameOverRules::IsCreditFinished(State.Playback, NowSeconds))
		{
			bNewGameRequested = true;
			Step.bCreditsFinished = true;
		}
		break;
	default:
		break;
	}
	return Step;
}

void UDigetherGameOverFlowComponent::AdvanceFailureEvent(const double NowSeconds,
	const TArray<FDigetherGameOverPresentPlayer>& PresentPlayers, FDigetherGameOverStep& OutStep)
{
	const double FailureStart = State.PhaseStartServerSeconds;
	const double SuctionStart = DigetherGameOverRules::GetSuctionStartSeconds(FailureStart, State.Tuning);
	const double Deadline = DigetherGameOverRules::GetFailureDeadlineSeconds(FailureStart, State.Tuning);
	int32 Waiting = 0;
	for (const FDigetherGameOverPresentPlayer& Player : PresentPlayers)
	{
		if (State.AbsorbedPlayerIds.Contains(Player.PlayerId))
		{
			continue;
		}
		bool bAbsorbNow = NowSeconds >= Deadline;
		if (!bAbsorbNow && NowSeconds >= SuctionStart)
		{
			// 흡입 시작 뒤 처음 본 사람의 흡입 시각을 정한다. 늦게 돌아온 사람은 지금 거리 기준이라 바로 흡입될 수 있다.
			const double* Scheduled = AbsorbSchedule.Find(Player.PlayerId);
			const double AbsorbAt = Scheduled != nullptr ? *Scheduled
				: AbsorbSchedule.Add(Player.PlayerId,
					DigetherGameOverRules::CalculateAbsorbSeconds(FailureStart, Player.DistanceToWellCm, State.Tuning));
			bAbsorbNow = NowSeconds >= AbsorbAt;
		}
		if (bAbsorbNow)
		{
			State.AbsorbedPlayerIds.Add(Player.PlayerId);
			OutStep.NewlyAbsorbedPlayerIds.Add(Player.PlayerId);
			OutStep.bPublicStateChanged = true;
		}
		else
		{
			++Waiting;
		}
	}
	if (State.WaitingPlayerCount != Waiting)
	{
		State.WaitingPlayerCount = Waiting;
		OutStep.bPublicStateChanged = true;
	}
	// 기다릴 현재 참가자가 없으면(전원 흡입·퇴장) 함께 크레딧을 시작한다. 떠난 사람은 기다리지 않는다(T34).
	if (DigetherGameOverRules::ShouldStartCredits(Waiting, FailureStart, NowSeconds, State.Tuning)
		&& EnterPhase(EDigetherGameOverPhase::Credits, NowSeconds))
	{
		State.PublishedResult = PendingResult;
		State.Playback = DigetherGameOverRules::MakeCreditPlayback(NowSeconds, PendingResult.Players.Num(), State.Tuning);
		OutStep.bEnteredCredits = true;
		OutStep.bPublicStateChanged = true;
	}
}

bool UDigetherGameOverFlowComponent::SetCreditFastForward(const bool bFastForward, const double NowSeconds)
{
	if (State.Phase != EDigetherGameOverPhase::Credits || bNewGameRequested)
	{
		return false;
	}
	FDigetherCreditPlayback Next;
	if (!DigetherGameOverRules::TrySetFastForward(State.Playback, bFastForward, NowSeconds, State.Tuning, Next))
	{
		return false;
	}
	State.Playback = Next;
	return true;
}

bool UDigetherGameOverFlowComponent::CompleteNewGame(const double NowSeconds)
{
	if (!IsAwaitingNewGame() || !EnterPhase(EDigetherGameOverPhase::None, NowSeconds))
	{
		return false;
	}
	PendingResult = FDigetherGameResult();
	AbsorbSchedule.Reset();
	bNewGameRequested = false;
	State.AbsorbedPlayerIds.Reset();
	State.WaitingPlayerCount = 0;
	State.Playback = FDigetherCreditPlayback();
	State.PublishedResult = FDigetherGameResult();
	return true;
}

void UDigetherGameOverFlowComponent::Cancel()
{
	const int32 Sequence = State.Sequence;
	State = FDigetherGameOverPublicState();
	State.Sequence = Sequence + 1;
	PendingResult = FDigetherGameResult();
	AbsorbSchedule.Reset();
	bNewGameRequested = false;
}

bool UDigetherGameOverFlowComponent::EnterPhase(const EDigetherGameOverPhase Next, const double NowSeconds)
{
	if (!DigetherGameOverRules::CanTransition(State.Phase, Next))
	{
		return false;
	}
	UE_LOG(LogDigetherRun, Log, TEXT("GameOver phase %s -> %s at %.2f"), *UEnum::GetValueAsString(State.Phase),
		*UEnum::GetValueAsString(Next), NowSeconds);
	State.Phase = Next;
	State.PhaseStartServerSeconds = NowSeconds;
	++State.Sequence;
	return true;
}
