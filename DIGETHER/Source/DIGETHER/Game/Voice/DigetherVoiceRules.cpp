#include "Game/Voice/DigetherVoiceRules.h"

namespace
{
	const FDigetherVoiceParticipant* FindParticipant(const TArray<FDigetherVoiceParticipant>& Participants,
		const FString& PlayerKey)
	{
		if (PlayerKey.IsEmpty())
		{
			return nullptr;
		}
		return Participants.FindByPredicate([&PlayerKey](const FDigetherVoiceParticipant& Entry)
		{
			return Entry.PlayerKey == PlayerKey;
		});
	}
}

bool FDigetherVoiceRules::CanHear(const FDigetherVoiceParticipant& Listener, const FDigetherVoiceParticipant& Talker,
	const TArray<FDigetherVoiceParticipant>& Participants, const EDigetherVoiceStage Stage)
{
	if (Stage == EDigetherVoiceStage::AllOpen)
	{
		return true;
	}
	if (Talker.Role == EDigetherVoiceRole::Spectator)
	{
		return Listener.Role == EDigetherVoiceRole::Spectator;
	}
	if (Listener.Role == EDigetherVoiceRole::Survivor)
	{
		return Listener.ZoneId == Talker.ZoneId;
	}
	// 관전자는 관전 대상의 귀 기준, 대상이 생존자가 아니면 생존자 음성 없음
	const FDigetherVoiceParticipant* Target = FindParticipant(Participants, Listener.SpectateTargetPlayerKey);
	return Target != nullptr && Target->Role == EDigetherVoiceRole::Survivor && Target->ZoneId == Talker.ZoneId;
}

TSet<FString> FDigetherVoiceRules::CollectBlockedTalkers(const FDigetherVoiceParticipant& Listener,
	const TArray<FDigetherVoiceParticipant>& Participants, const EDigetherVoiceStage Stage)
{
	TSet<FString> Blocked;
	for (const FDigetherVoiceParticipant& Talker : Participants)
	{
		if (Talker.PlayerKey != Listener.PlayerKey && !CanHear(Listener, Talker, Participants, Stage))
		{
			Blocked.Add(Talker.PlayerKey);
		}
	}
	return Blocked;
}

FDigetherVoiceMuteDiff FDigetherVoiceRules::Diff(const TSet<FString>& Applied, const TSet<FString>& Desired)
{
	FDigetherVoiceMuteDiff Result;
	for (const FString& Key : Desired)
	{
		if (!Applied.Contains(Key))
		{
			Result.ToMute.Add(Key);
		}
	}
	for (const FString& Key : Applied)
	{
		if (!Desired.Contains(Key))
		{
			Result.ToUnmute.Add(Key);
		}
	}
	Result.ToMute.Sort();
	Result.ToUnmute.Sort();
	return Result;
}
