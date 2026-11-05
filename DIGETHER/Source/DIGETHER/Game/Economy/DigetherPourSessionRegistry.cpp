#include "Game/Economy/DigetherPourSessionRegistry.h"

namespace
{
	bool IsValidServerTime(const double Seconds)
	{
		return FMath::IsFinite(Seconds) && Seconds >= 0.0;
	}

	bool IsValidInterval(const double IntervalSeconds)
	{
		return FMath::IsFinite(IntervalSeconds) && IntervalSeconds > 0.0;
	}
}

EDigetherEconomyResult FDigetherPourSessionRegistry::BeginPour(const FString& PlayerKey, const FName StationId,
	const int32 PeriodGeneration, const double NowServerSeconds, const double IntervalSeconds,
	FDigetherPourSession* OutSession)
{
	const double FirstTickSeconds = NowServerSeconds + IntervalSeconds;
	if (PlayerKey.IsEmpty() || StationId.IsNone() || PeriodGeneration < 0 || !IsValidServerTime(NowServerSeconds)
		|| !IsValidInterval(IntervalSeconds) || !FMath::IsFinite(FirstTickSeconds) || FirstTickSeconds <= NowServerSeconds
		|| NextStartSequence == MAX_int64)
	{
		return EDigetherEconomyResult::InvalidInput;
	}
	const int32 Index = Sessions.IndexOfByPredicate([&PlayerKey](const FDigetherPourSession& Session)
	{
		return Session.PlayerKey == PlayerKey;
	});
	if (Index != INDEX_NONE && Sessions[Index].StationId == StationId
		&& Sessions[Index].PeriodGeneration == PeriodGeneration)
	{
		if (OutSession != nullptr)
		{
			*OutSession = Sessions[Index];
		}
		return EDigetherEconomyResult::Success;
	}
	FDigetherPourSession Session;
	Session.PlayerKey = PlayerKey;
	Session.StationId = StationId;
	Session.PeriodGeneration = PeriodGeneration;
	Session.NextTickServerSeconds = FirstTickSeconds;
	Session.StartSequence = NextStartSequence++;
	if (Index != INDEX_NONE)
	{
		// 같은 사람의 이전 세션은 남기지 않는다. 한 사람은 한 정산기에만 붓는다.
		Sessions[Index] = Session;
	}
	else
	{
		Sessions.Add(Session);
	}
	if (OutSession != nullptr)
	{
		*OutSession = Session;
	}
	return EDigetherEconomyResult::Success;
}

bool FDigetherPourSessionRegistry::StopPour(const FString& PlayerKey, FDigetherPourSession* OutStopped)
{
	const int32 Index = Sessions.IndexOfByPredicate([&PlayerKey](const FDigetherPourSession& Session)
	{
		return Session.PlayerKey == PlayerKey;
	});
	if (Index == INDEX_NONE)
	{
		return false;
	}
	if (OutStopped != nullptr)
	{
		*OutStopped = Sessions[Index];
	}
	Sessions.RemoveAt(Index);
	return true;
}

TArray<FDigetherPourSession> FDigetherPourSessionRegistry::StopAll()
{
	TArray<FDigetherPourSession> Stopped = MoveTemp(Sessions);
	Sessions.Reset();
	return Stopped;
}

TArray<FDigetherPourDueTick> FDigetherPourSessionRegistry::CollectDueTicks(const double NowServerSeconds,
	const double IntervalSeconds, const int32 MaxTicks) const
{
	TArray<FDigetherPourDueTick> Due;
	if (!IsValidServerTime(NowServerSeconds) || !IsValidInterval(IntervalSeconds) || MaxTicks <= 0)
	{
		return Due;
	}
	for (const FDigetherPourSession& Session : Sessions)
	{
		// 세션마다 상한까지만 펼친다. 긴 정지 뒤에도 한 프레임에 무한히 쌓이지 않는다.
		double Scheduled = Session.NextTickServerSeconds;
		for (int32 Count = 0; Count < MaxTicks && Scheduled <= NowServerSeconds; ++Count)
		{
			FDigetherPourDueTick& Tick = Due.AddDefaulted_GetRef();
			Tick.PlayerKey = Session.PlayerKey;
			Tick.StartSequence = Session.StartSequence;
			Tick.ScheduledServerSeconds = Scheduled;
			Scheduled += IntervalSeconds;
		}
	}
	// 기획 S24 ①: 동시에 붓는 사람이 여럿이면 호스트가 처리한 순서대로 반영한다. 예정 시각, 같으면 먼저 시작한 순서다.
	Due.StableSort([](const FDigetherPourDueTick& Left, const FDigetherPourDueTick& Right)
	{
		return Left.ScheduledServerSeconds != Right.ScheduledServerSeconds
			? Left.ScheduledServerSeconds < Right.ScheduledServerSeconds : Left.StartSequence < Right.StartSequence;
	});
	if (Due.Num() > MaxTicks)
	{
		Due.SetNum(MaxTicks);
	}
	return Due;
}

bool FDigetherPourSessionRegistry::AdvanceAfterTick(const FDigetherPourDueTick& Tick, const double IntervalSeconds)
{
	FDigetherPourSession* Session = Sessions.FindByPredicate([&Tick](const FDigetherPourSession& Entry)
	{
		return Entry.PlayerKey == Tick.PlayerKey && Entry.StartSequence == Tick.StartSequence;
	});
	const double Next = Tick.ScheduledServerSeconds + IntervalSeconds;
	if (Session == nullptr || !IsValidInterval(IntervalSeconds) || !FMath::IsFinite(Next))
	{
		return false;
	}
	Session->NextTickServerSeconds = Next;
	return true;
}

const FDigetherPourSession* FDigetherPourSessionRegistry::Find(const FString& PlayerKey) const
{
	return Sessions.FindByPredicate([&PlayerKey](const FDigetherPourSession& Session)
	{
		return Session.PlayerKey == PlayerKey;
	});
}
