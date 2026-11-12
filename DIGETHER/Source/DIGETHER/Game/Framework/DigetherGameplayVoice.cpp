// 음성 수신 제한 명단(기획 S28, 2026-11-06 사용자 결정)
#include "Game/Framework/DigetherGameplayGameMode.h"

#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Monster/DigetherZoneVolume.h"
#include "GameFramework/Pawn.h"
#include "Game/Run/DigetherGameOverFlowComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"

namespace
{
	// 기획은 갱도↔던전 두 구역만 구분, 던전별 추가 차단은 적용하지 않음
	int32 ResolveVoiceZone(const ADigetherGameplayPlayerState* Player)
	{
		const APawn* Pawn = IsValid(Player) ? Player->GetGameplayPawn() : nullptr;
		return IsValid(Pawn) && ADigetherZoneVolume::GetZoneKindAt(Pawn->GetWorld(), Pawn->GetActorLocation())
			== EDigetherMonsterZone::Dungeon ? 1 : 0;
	}
}

void ADigetherGameplayGameMode::CollectVoiceParticipants(TArray<FDigetherVoiceRouteEntry>& OutEntries,
	EDigetherVoiceStage& OutStage) const
{
	OutEntries.Reset();
	// 결과 크레딧 시작 순간 전원 음성
	OutStage = GameOverFlowComponent != nullptr && GameOverFlowComponent->GetPhase() == EDigetherGameOverPhase::Credits
		? EDigetherVoiceStage::AllOpen : EDigetherVoiceStage::Separated;
	if (!IsValid(GameState))
	{
		return;
	}
	for (APlayerState* Entry : GameState->PlayerArray)
	{
		ADigetherGameplayPlayerState* Player = Cast<ADigetherGameplayPlayerState>(Entry);
		APlayerController* Controller = IsValid(Player) ? Cast<APlayerController>(Player->GetOwner()) : nullptr;
		if (IsValid(Player))
		{
			Player->RefreshVoiceEnvironment();
		}
		if (!IsValid(Player) || Player->IsInactive() || !IsValid(Controller) || !Player->GetUniqueId().IsValid())
		{
			continue;
		}
		FDigetherVoiceRouteEntry Route;
		Route.Controller = Controller;
		Route.UniqueId = Player->GetUniqueId();
		Route.Participant.PlayerKey = FString::FromInt(Player->GetPlayerId());
		Route.Participant.ZoneId = ResolveVoiceZone(Player);
		const FDigetherLifeComponentState Life = Player->GetLifeComponent()->CaptureState();
		if (Life.PublicState.bSpectating)
		{
			Route.Participant.Role = EDigetherVoiceRole::Spectator;
			Route.Participant.SpectateTargetPlayerKey = Life.PrivateState.SpectateTargetPlayerKey;
		}
		else if (const FDigetherJoinSpectator* Joined = JoinSpectators.FindByPredicate(
			[Controller](const FDigetherJoinSpectator& Spectator) { return Spectator.Controller.Get() == Controller; }))
		{
			Route.Participant.Role = EDigetherVoiceRole::Spectator;
			Route.Participant.SpectateTargetPlayerKey = Joined->TargetPlayerKey;
		}
		OutEntries.Add(Route);
	}
}
