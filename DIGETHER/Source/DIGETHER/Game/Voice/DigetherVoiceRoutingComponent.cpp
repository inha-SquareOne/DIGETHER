#include "Game/Voice/DigetherVoiceRoutingComponent.h"

#include "Engine/World.h"
#include "Game/Voice/DigetherVoiceLog.h"
#include "Game/Voice/DigetherVoiceRules.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

UDigetherVoiceRoutingComponent::UDigetherVoiceRoutingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UDigetherVoiceRoutingComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner() != nullptr && GetOwner()->HasAuthority() && GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this, &ThisClass::Refresh,
			DigetherVoiceDefaults::RefreshSeconds, true);
	}
}

void UDigetherVoiceRoutingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
	}
	AppliedMutes.Reset();
	KnownIds.Reset();
	Super::EndPlay(EndPlayReason);
}

void UDigetherVoiceRoutingComponent::Refresh()
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || !CollectParticipants.IsBound())
	{
		return;
	}
	TArray<FDigetherVoiceRouteEntry> Entries;
	EDigetherVoiceStage Stage = EDigetherVoiceStage::Separated;
	CollectParticipants.Execute(Entries, Stage);

	TArray<FDigetherVoiceParticipant> Participants;
	TMap<FString, FString> IdStringByPlayerKey;
	for (const FDigetherVoiceRouteEntry& Entry : Entries)
	{
		if (Entry.Controller.IsValid() && Entry.UniqueId.IsValid() && !Entry.Participant.PlayerKey.IsEmpty())
		{
			Participants.Add(Entry.Participant);
			const FString IdString = Entry.UniqueId.ToString();
			IdStringByPlayerKey.Add(Entry.Participant.PlayerKey, IdString);
			// 떠난 발화자 ID도 해제할 수 있게 기록
			KnownIds.Add(IdString, Entry.UniqueId);
		}
	}

	for (const FDigetherVoiceRouteEntry& Entry : Entries)
	{
		APlayerController* Listener = Entry.Controller.Get();
		if (!IsValid(Listener) || !IdStringByPlayerKey.Contains(Entry.Participant.PlayerKey))
		{
			continue;
		}
		TSet<FString> Desired;
		for (const FString& BlockedKey : FDigetherVoiceRules::CollectBlockedTalkers(Entry.Participant, Participants, Stage))
		{
			Desired.Add(IdStringByPlayerKey[BlockedKey]);
		}
		TSet<FString>& Applied = AppliedMutes.FindOrAdd(Listener);
		const FDigetherVoiceMuteDiff Change = FDigetherVoiceRules::Diff(Applied, Desired);
		if (Change.ToMute.IsEmpty() && Change.ToUnmute.IsEmpty())
		{
			continue;
		}
		for (const FString& IdString : Change.ToMute)
		{
			Listener->GameplayMutePlayer(KnownIds[IdString]);
		}
		for (const FString& IdString : Change.ToUnmute)
		{
			if (const FUniqueNetIdRepl* Id = KnownIds.Find(IdString))
			{
				Listener->GameplayUnmutePlayer(*Id);
			}
		}
		Applied = Desired;
		UE_LOG(LogDigetherVoice, Verbose, TEXT("수신 제한 갱신: %s 음소거 %d명, 해제 %d건"),
			*Entry.Participant.PlayerKey, Desired.Num(), Change.ToUnmute.Num());
	}
	// 파괴된 PC만 제거, 잠시 명단에서 빠진 PC는 기록 유지(엔진 음소거가 남아 있음)
	for (auto It = AppliedMutes.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
