#include "Game/Run/DigetherGameOverRules.h"

#include "Algo/StableSort.h"
#include "Game/Economy/DigetherEconomyRules.h"

namespace
{
	bool IsPositiveFinite(const float Value)
	{
		return FMath::IsFinite(Value) && Value > 0.f;
	}

	bool IsNonNegativeFinite(const float Value)
	{
		return FMath::IsFinite(Value) && Value >= 0.f;
	}
}

bool DigetherGameOverRules::IsValidTuning(const FDigetherGameOverTuning& Tuning)
{
	return IsNonNegativeFinite(Tuning.SettlementScreenSeconds) && IsPositiveFinite(Tuning.FailMotionSeconds)
		&& FMath::IsFinite(Tuning.FloodRatio) && Tuning.FloodRatio >= 0.f && Tuning.FloodRatio <= 1.f
		&& IsPositiveFinite(Tuning.SuctionSpeedCmPerSecond) && IsNonNegativeFinite(Tuning.CreditStartDelaySeconds)
		&& IsPositiveFinite(Tuning.CreditScrollSpeed) && FMath::IsFinite(Tuning.CreditTiltDegrees)
		&& IsNonNegativeFinite(Tuning.CreditBlockGap) && FMath::IsFinite(Tuning.CreditFastForwardMultiplier)
		&& Tuning.CreditFastForwardMultiplier >= 1.f && IsPositiveFinite(Tuning.CreditLineHeight);
}

bool DigetherGameOverRules::CanTransition(const EDigetherGameOverPhase From, const EDigetherGameOverPhase To)
{
	switch (From)
	{
	case EDigetherGameOverPhase::None:
		return To == EDigetherGameOverPhase::SettlementScreen;
	case EDigetherGameOverPhase::SettlementScreen:
		return To == EDigetherGameOverPhase::FailureEvent;
	case EDigetherGameOverPhase::FailureEvent:
		return To == EDigetherGameOverPhase::Credits;
	case EDigetherGameOverPhase::Credits:
		return To == EDigetherGameOverPhase::None;
	}
	return false;
}

double DigetherGameOverRules::GetSettlementScreenEndSeconds(const double SettlementStartSeconds,
	const FDigetherGameOverTuning& Tuning)
{
	return SettlementStartSeconds + FMath::Max(0.f, Tuning.SettlementScreenSeconds);
}

double DigetherGameOverRules::GetSuctionStartSeconds(const double FailureStartSeconds,
	const FDigetherGameOverTuning& Tuning)
{
	return FailureStartSeconds + static_cast<double>(FMath::Max(0.f, Tuning.FailMotionSeconds))
		* FMath::Clamp(Tuning.FloodRatio, 0.f, 1.f);
}

double DigetherGameOverRules::GetFailureDeadlineSeconds(const double FailureStartSeconds,
	const FDigetherGameOverTuning& Tuning)
{
	return FailureStartSeconds + FMath::Max(0.f, Tuning.FailMotionSeconds);
}

double DigetherGameOverRules::CalculateAbsorbSeconds(const double FailureStartSeconds, const double DistanceCm,
	const FDigetherGameOverTuning& Tuning)
{
	const double Deadline = GetFailureDeadlineSeconds(FailureStartSeconds, Tuning);
	const double SuctionStart = GetSuctionStartSeconds(FailureStartSeconds, Tuning);
	const double SafeDistance = FMath::IsFinite(DistanceCm) ? FMath::Max(0.0, DistanceCm) : 0.0;
	const double Speed = IsPositiveFinite(Tuning.SuctionSpeedCmPerSecond) ? Tuning.SuctionSpeedCmPerSecond : 1.0;
	return FMath::Min(Deadline, SuctionStart + SafeDistance / Speed);
}

float DigetherGameOverRules::CalculateFloodAlpha(const double FailureStartSeconds, const double NowSeconds,
	const FDigetherGameOverTuning& Tuning)
{
	const double FloodSeconds = GetSuctionStartSeconds(FailureStartSeconds, Tuning) - FailureStartSeconds;
	if (FloodSeconds <= 0.0)
	{
		return NowSeconds >= FailureStartSeconds ? 1.f : 0.f;
	}
	return static_cast<float>(FMath::Clamp((NowSeconds - FailureStartSeconds) / FloodSeconds, 0.0, 1.0));
}

bool DigetherGameOverRules::ShouldStartCredits(const int32 WaitingPlayerCount, const double FailureStartSeconds,
	const double NowSeconds, const FDigetherGameOverTuning& Tuning)
{
	return WaitingPlayerCount <= 0 || NowSeconds >= GetFailureDeadlineSeconds(FailureStartSeconds, Tuning);
}

int32 DigetherGameOverRules::CountCreditLines(const int32 PlayerCount)
{
	const int64 Players = FMath::Max(0, PlayerCount);
	const int64 Lines = FirstCreditBlockLines + static_cast<int64>(CreditBlockCount - 1) * (1 + Players);
	return static_cast<int32>(FMath::Min<int64>(Lines, MAX_int32));
}

float DigetherGameOverRules::CalculateCreditTotalSeconds(const int32 PlayerCount, const FDigetherGameOverTuning& Tuning)
{
	if (!IsValidTuning(Tuning))
	{
		return 0.f;
	}
	// 첫 블록이 화면 아래에서 나타나 마지막 블록이 화면 위로 사라질 때까지 화면 높이 1만큼 더 지나간다.
	constexpr double ScreenHeight = 1.0;
	const double ContentHeight = static_cast<double>(CountCreditLines(PlayerCount)) * Tuning.CreditLineHeight
		+ static_cast<double>(CreditBlockCount - 1) * Tuning.CreditBlockGap;
	return static_cast<float>(Tuning.CreditStartDelaySeconds + (ContentHeight + ScreenHeight) / Tuning.CreditScrollSpeed);
}

FDigetherCreditPlayback DigetherGameOverRules::MakeCreditPlayback(const double NowSeconds, const int32 PlayerCount,
	const FDigetherGameOverTuning& Tuning)
{
	FDigetherCreditPlayback Playback;
	Playback.AnchorServerSeconds = NowSeconds;
	Playback.AnchorProgressSeconds = 0.f;
	Playback.PlaybackRate = 1.f;
	Playback.TotalSeconds = CalculateCreditTotalSeconds(PlayerCount, Tuning);
	Playback.bFastForward = false;
	return Playback;
}

float DigetherGameOverRules::CalculateCreditProgressSeconds(const FDigetherCreditPlayback& Playback,
	const double NowSeconds)
{
	if (!FMath::IsFinite(Playback.TotalSeconds) || Playback.TotalSeconds <= 0.f || !FMath::IsFinite(NowSeconds))
	{
		return 0.f;
	}
	const double Rate = FMath::IsFinite(Playback.PlaybackRate) ? FMath::Max(0.f, Playback.PlaybackRate) : 1.0;
	const double Elapsed = FMath::Max(0.0, NowSeconds - Playback.AnchorServerSeconds);
	const double Progress = static_cast<double>(Playback.AnchorProgressSeconds) + Elapsed * Rate;
	return static_cast<float>(FMath::Clamp(Progress, 0.0, static_cast<double>(Playback.TotalSeconds)));
}

bool DigetherGameOverRules::IsCreditFinished(const FDigetherCreditPlayback& Playback, const double NowSeconds)
{
	return Playback.TotalSeconds <= 0.f || CalculateCreditProgressSeconds(Playback, NowSeconds) >= Playback.TotalSeconds;
}

bool DigetherGameOverRules::TrySetFastForward(const FDigetherCreditPlayback& Current, const bool bFastForward,
	const double NowSeconds, const FDigetherGameOverTuning& Tuning, FDigetherCreditPlayback& OutPlayback)
{
	if (Current.bFastForward == bFastForward || !IsValidTuning(Tuning) || !FMath::IsFinite(NowSeconds))
	{
		return false;
	}
	FDigetherCreditPlayback Next = Current;
	Next.AnchorProgressSeconds = CalculateCreditProgressSeconds(Current, NowSeconds);
	Next.AnchorServerSeconds = NowSeconds;
	Next.PlaybackRate = bFastForward ? Tuning.CreditFastForwardMultiplier : 1.f;
	Next.bFastForward = bFastForward;
	OutPlayback = Next;
	return true;
}

int32 DigetherGameOverRules::CalculateNetDeathLoss(const FDigetherWalletState& Wallet)
{
	const int64 Net = static_cast<int64>(Wallet.TotalDeathLossCredits) - Wallet.LifetimeCorpseRefundCredits;
	// 복구금은 사망 직전 잔고의 일부라 같은 게임의 손실보다 클 수 없다. 넘으면 장부 오류이므로 0으로 막고 알린다.
	ensureMsgf(Net >= 0, TEXT("Corpse refunds exceed death losses: loss=%d refund=%d"),
		Wallet.TotalDeathLossCredits, Wallet.LifetimeCorpseRefundCredits);
	return static_cast<int32>(FMath::Clamp<int64>(Net, 0, MAX_int32));
}

bool DigetherGameOverRules::TryMakeGameResult(const FGuid& ResultId, const FGuid& GameId, const int32 FinalTotalDay,
	const int32 CompletedPeriods, const TArray<FDigetherGameResultInput>& Players, FDigetherGameResult& OutResult)
{
	if (!ResultId.IsValid() || !GameId.IsValid() || Players.IsEmpty() || FinalTotalDay < 0 || CompletedPeriods < 0)
	{
		return false;
	}
	struct FOrdered
	{
		int32 InputIndex = 0;
		int32 SortKey = 0;
	};
	TArray<FOrdered> Order;
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		// 참가 순서를 모르는 구 저장 기록(0)은 알려진 참가자 뒤에 입력 순서대로 둔다.
		Order.Add({ Index, Players[Index].JoinOrder > 0 ? Players[Index].JoinOrder : MAX_int32 });
	}
	Algo::StableSortBy(Order, &FOrdered::SortKey);
	FDigetherGameResult Result;
	Result.ResultId = ResultId;
	Result.GameId = GameId;
	Result.FinalTotalDay = FinalTotalDay;
	Result.CompletedPeriods = CompletedPeriods;
	for (const FOrdered& Entry : Order)
	{
		const FDigetherGameResultInput& Input = Players[Entry.InputIndex];
		FDigetherGameResultPlayer Player;
		Player.DisplayName = Input.DisplayName;
		Player.JoinOrder = FMath::Max(0, Input.JoinOrder);
		Player.HiddenCredits = FMath::Max(0, Input.Wallet.HiddenLifetimeCredits);
		// 히든 수익은 지급 때 획득액에도 함께 쌓인다. 획득액 누적이 없던 구 저장(0으로 읽힘)은 확실한 하한인 히든 수익을 쓴다.
		Player.EarnedCredits = FMath::Max(FMath::Max(0, Input.Wallet.LifetimeEarnedCredits), Player.HiddenCredits);
		Player.DeathLossCredits = CalculateNetDeathLoss(Input.Wallet);
		Result.TotalEarnedCredits = FDigetherEconomyRules::AddCredits(Result.TotalEarnedCredits, Player.EarnedCredits);
		Result.Players.Add(MoveTemp(Player));
	}
	OutResult = MoveTemp(Result);
	return true;
}

bool DigetherGameOverRules::IsValidGameResult(const FDigetherGameResult& Result, const int32 MaxPlayers)
{
	if (!Result.IsValid() || Result.FinalTotalDay < 0 || Result.CompletedPeriods < 0 || Result.TotalEarnedCredits < 0
		|| Result.Players.IsEmpty() || Result.Players.Num() > MaxPlayers)
	{
		return false;
	}
	int32 Total = 0;
	for (const FDigetherGameResultPlayer& Player : Result.Players)
	{
		if (Player.JoinOrder < 0 || Player.EarnedCredits < 0 || Player.HiddenCredits < 0
			|| Player.HiddenCredits > Player.EarnedCredits || Player.DeathLossCredits < 0)
		{
			return false;
		}
		Total = FDigetherEconomyRules::AddCredits(Total, Player.EarnedCredits);
	}
	return Total == Result.TotalEarnedCredits;
}

void DigetherGameOverRules::ArchiveGameResult(TArray<FDigetherGameResult>& Archive, const FDigetherGameResult& Result,
	const int32 MaxResults)
{
	if (!Result.IsValid() || MaxResults <= 0
		|| Archive.ContainsByPredicate([&Result](const FDigetherGameResult& Entry) { return Entry.ResultId == Result.ResultId; }))
	{
		return;
	}
	Archive.Add(Result);
	while (Archive.Num() > MaxResults)
	{
		Archive.RemoveAt(0);
	}
}
