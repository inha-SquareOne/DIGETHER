#include "Game/Online/DigetherLoginReplacementQueue.h"

#include "Game/Online/DigetherJoinRules.h"
#include "GameFramework/PlayerController.h"

void FDigetherLoginReplacementQueue::Add(FEntry&& Entry, TArray<FEntry>& OutSuperseded)
{
	for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
	{
		if (Entries[Index].UniqueId == Entry.UniqueId)
		{
			OutSuperseded.Add(MoveTemp(Entries[Index]));
			Entries.RemoveAt(Index);
		}
	}
	Entries.Add(MoveTemp(Entry));
}

void FDigetherLoginReplacementQueue::Collect(const double NowSeconds, const double TimeoutSeconds,
	const TFunctionRef<bool(const FEntry&)> IsPreviousReleased, TArray<FEntry>& OutReady, TArray<FEntry>& OutTimedOut)
{
	for (int32 Index = 0; Index < Entries.Num();)
	{
		const DigetherJoinRules::EDigetherReplacementWait Wait = DigetherJoinRules::EvaluateReplacementWait(
			IsPreviousReleased(Entries[Index]), NowSeconds - Entries[Index].StartSeconds, TimeoutSeconds);
		if (Wait == DigetherJoinRules::EDigetherReplacementWait::Waiting)
		{
			++Index;
			continue;
		}
		TArray<FEntry>& Target = Wait == DigetherJoinRules::EDigetherReplacementWait::Ready ? OutReady : OutTimedOut;
		Target.Add(MoveTemp(Entries[Index]));
		Entries.RemoveAt(Index);
	}
}

TArray<FDigetherLoginReplacementQueue::FEntry> FDigetherLoginReplacementQueue::TakeAll()
{
	TArray<FEntry> Taken = MoveTemp(Entries);
	Entries.Reset();
	return Taken;
}
