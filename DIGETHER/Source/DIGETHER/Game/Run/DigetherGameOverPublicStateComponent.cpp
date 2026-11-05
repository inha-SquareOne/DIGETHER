#include "Game/Run/DigetherGameOverPublicStateComponent.h"

#include "Engine/World.h"
#include "Game/Run/DigetherGameOverRules.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

UDigetherGameOverPublicStateComponent::UDigetherGameOverPublicStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDigetherGameOverPublicStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UDigetherGameOverPublicStateComponent, PublicState, COND_None);
}

bool UDigetherGameOverPublicStateComponent::SetPublicState(const FDigetherGameOverPublicState& NewState)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return false;
	}
	const FDigetherGameOverPublicState Previous = PublicState;
	PublicState = NewState;
	OwnerActor->ForceNetUpdate();
	BroadcastChanges(Previous);
	return true;
}

void UDigetherGameOverPublicStateComponent::OnRep_PublicState(const FDigetherGameOverPublicState& PreviousState)
{
	BroadcastChanges(PreviousState);
}

void UDigetherGameOverPublicStateComponent::BroadcastChanges(const FDigetherGameOverPublicState& PreviousState)
{
	const FDigetherGameOverPublicState Current = PublicState;
	const bool bPhaseChanged = Current.Phase != PreviousState.Phase || Current.Sequence != PreviousState.Sequence;
	if (Current.Phase != PreviousState.Phase)
	{
		OnGameOverPhaseChanged.Broadcast(Current.Phase);
		switch (Current.Phase)
		{
		case EDigetherGameOverPhase::FailureEvent:
		{
			const float FloodSeconds = static_cast<float>(DigetherGameOverRules::GetSuctionStartSeconds(
				Current.PhaseStartServerSeconds, Current.Tuning) - Current.PhaseStartServerSeconds);
			OnFailureEventStarted.Broadcast(FloodSeconds, Current.Tuning.FailMotionSeconds);
			break;
		}
		case EDigetherGameOverPhase::Credits:
			OnCreditsStarted.Broadcast(Current.PublishedResult);
			break;
		case EDigetherGameOverPhase::None:
			// 크레딧 뒤 새 게임으로만 None 진입, 월드 정리(Cancel)의 None에는 전송 안 함
			if (PreviousState.Phase == EDigetherGameOverPhase::Credits)
			{
				OnNewGameStarted.Broadcast();
			}
			break;
		default:
			break;
		}
	}
	// 늦은 접속·묶음 복제(이전 값이 빈 상태)여도 아직 알리지 않은 흡입 모두 전송
	for (const int32 PlayerId : Current.AbsorbedPlayerIds)
	{
		if (!PreviousState.AbsorbedPlayerIds.Contains(PlayerId))
		{
			OnPlayerAbsorbed.Broadcast(PlayerId);
		}
	}
	const FDigetherCreditPlayback& Before = PreviousState.Playback;
	const FDigetherCreditPlayback& After = Current.Playback;
	if (Current.Phase == EDigetherGameOverPhase::Credits && (bPhaseChanged || Before.bFastForward != After.bFastForward
		|| Before.AnchorServerSeconds != After.AnchorServerSeconds || Before.PlaybackRate != After.PlaybackRate))
	{
		OnCreditPlaybackChanged.Broadcast(After);
	}
}

double UDigetherGameOverPublicStateComponent::GetServerNowSeconds() const
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return 0.0;
	}
	const AGameStateBase* GameState = World->GetGameState();
	return IsValid(GameState) ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

float UDigetherGameOverPublicStateComponent::GetFloodAlpha() const
{
	if (PublicState.Phase != EDigetherGameOverPhase::FailureEvent)
	{
		return 0.f;
	}
	return DigetherGameOverRules::CalculateFloodAlpha(PublicState.PhaseStartServerSeconds, GetServerNowSeconds(),
		PublicState.Tuning);
}

float UDigetherGameOverPublicStateComponent::GetSecondsUntilSuction() const
{
	if (PublicState.Phase != EDigetherGameOverPhase::FailureEvent)
	{
		return 0.f;
	}
	const double SuctionStart = DigetherGameOverRules::GetSuctionStartSeconds(PublicState.PhaseStartServerSeconds,
		PublicState.Tuning);
	return static_cast<float>(FMath::Max(0.0, SuctionStart - GetServerNowSeconds()));
}

float UDigetherGameOverPublicStateComponent::GetCreditProgressSeconds() const
{
	if (PublicState.Phase != EDigetherGameOverPhase::Credits)
	{
		return 0.f;
	}
	return DigetherGameOverRules::CalculateCreditProgressSeconds(PublicState.Playback, GetServerNowSeconds());
}

float UDigetherGameOverPublicStateComponent::GetCreditScrollOffset() const
{
	const float Progress = GetCreditProgressSeconds();
	return FMath::Max(0.f, Progress - PublicState.Tuning.CreditStartDelaySeconds) * PublicState.Tuning.CreditScrollSpeed;
}

bool UDigetherGameOverPublicStateComponent::IsPlayerAbsorbed(const APlayerState* Player) const
{
	return IsValid(Player) && PublicState.AbsorbedPlayerIds.Contains(Player->GetPlayerId());
}
