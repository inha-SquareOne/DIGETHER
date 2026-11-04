#include "Game/Framework/DigetherFrameworkRules.h"

#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Elevator/DigetherElevatorRules.h"
#include "Game/Life/DigetherLifeRules.h"
#include "Game/Run/DigetherRunRules.h"

bool DigetherFrameworkRules::TryMakePlayerLoad(const TArray<FGuid>& PassengerIds,
	TArray<FDigetherElevatorLoadEntry>& OutEntries)
{
	TSet<FGuid> Seen;
	TArray<FDigetherElevatorLoadEntry> Entries;
	for (const FGuid& Id : PassengerIds)
	{
		if (!Id.IsValid() || Seen.Contains(Id))
		{
			return false;
		}
		Seen.Add(Id);
		FDigetherElevatorLoadEntry Entry;
		Entry.EntryId = Id;
		Entry.OwnerId = Id;
		Entry.Kind = EDigetherElevatorLoadKind::Player;
		// 시험 Pawn에는 인벤토리가 없으며 P3의 Player 신체 무게는 0이다.
		Entry.WeightKg = 0.f;
		Entries.Add(Entry);
	}
	OutEntries = MoveTemp(Entries);
	return true;
}

EDigetherRunElevatorState DigetherFrameworkRules::MakeRunElevatorState(const EDigetherElevatorPhase Phase)
{
	if (DigetherElevatorRules::IsMoving(Phase))
	{
		return EDigetherRunElevatorState::InTransit;
	}
	return Phase == EDigetherElevatorPhase::BottomWaiting
		? EDigetherRunElevatorState::StoppedInMine : EDigetherRunElevatorState::AtBase;
}

EDigetherRunPlayerLocation DigetherFrameworkRules::MakeRunPlayerLocation(const EDigetherLifeState Life,
	const bool bInBoardingVolume, const EDigetherElevatorPhase ElevatorPhase)
{
	if (Life == EDigetherLifeState::Escaped)
	{
		return EDigetherRunPlayerLocation::Base;
	}
	const bool bBoardingPhase = ElevatorPhase == EDigetherElevatorPhase::InitialDescent
		|| ElevatorPhase == EDigetherElevatorPhase::BottomWaiting;
	return bInBoardingVolume && bBoardingPhase
		? EDigetherRunPlayerLocation::Elevator : EDigetherRunPlayerLocation::MineOrDungeon;
}

bool DigetherFrameworkRules::TryMakeHiddenReveals(const TArray<FString>& PublicKeys,
	const TArray<FDigetherWalletState>& Wallets, TArray<FDigetherHiddenRevealEntry>& OutReveals)
{
	if (PublicKeys.Num() != Wallets.Num())
	{
		return false;
	}
	TSet<FString> Seen;
	TArray<FDigetherHiddenRevealEntry> Reveals;
	for (int32 Index = 0; Index < PublicKeys.Num(); ++Index)
	{
		if (PublicKeys[Index].IsEmpty() || Seen.Contains(PublicKeys[Index])
			|| Wallets[Index].HiddenLifetimeCredits < 0)
		{
			return false;
		}
		Seen.Add(PublicKeys[Index]);
		FDigetherHiddenRevealEntry& Entry = Reveals.AddDefaulted_GetRef();
		Entry.PlayerKey = PublicKeys[Index];
		Entry.HiddenLifetimeCredits = Wallets[Index].HiddenLifetimeCredits;
	}
	OutReveals = MoveTemp(Reveals);
	return true;
}

bool DigetherFrameworkRules::IsDepositClosed(const FDigetherRunPublicState& Run,
	const EDigetherElevatorPhase ElevatorPhase)
{
	return Run.bGameOver || Run.bPaymentDeadline || Run.Phase == EDigetherRunPhase::InProgress
		|| ElevatorPhase != EDigetherElevatorPhase::HubWaiting;
}

bool DigetherFrameworkRules::TryPlanReturn(const FDigetherRunTerminationInput& Input,
	const TArray<FDigetherFrameworkPlayerSnapshot>& Players, const FDigetherQuotaState& Quota,
	const FDigetherRunTuning& Tuning, const int32 DepartureCount, FDigetherFrameworkReturnPlan& OutPlan)
{
	FDigetherFrameworkReturnPlan Plan;
	Plan.Termination = FDigetherRunRules::CalculateTermination(Input);
	if (Plan.Termination.Result != EDigetherRunResult::Success || Players.Num() != Input.Players.Num()
		|| DepartureCount <= 0 || Players.Num() > DepartureCount)
	{
		return false;
	}
	TArray<FDigetherPlayerSettlementInput> SettlementInputs;
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		const FDigetherFrameworkPlayerSnapshot& Player = Players[Index];
		if (Player.PlayerKey != Input.Players[Index].PlayerKey
			|| Player.Life.PublicState.Life != Input.Players[Index].Life
			|| !FDigetherLifeRules::IsValidState(Player.Life)
			|| !FDigetherEconomyRules::IsValidWalletState(Player.Wallet))
		{
			return false;
		}
		FDigetherFrameworkPlayerReturn Returned;
		Returned.LifeAtEnd = Player.Life;
		Returned.WalletAtEnd = Player.Wallet;
		const FDigetherRunPlayerOutcome& Outcome = Plan.Termination.Players[Index];
		if (Outcome.Outcome == EDigetherRunPlayerOutcome::MissingDeath)
		{
			const FDigetherLifeUpdateResult Death = FDigetherLifeRules::ApplyDeath(Player.Life, Outcome.DeathCause);
			const FDigetherWalletUpdateResult Loss = FDigetherEconomyRules::ApplyDeathLoss(Player.Wallet,
				Tuning.DeathLossPercent);
			if (Death.Result != EDigetherLifeResult::Success || Loss.Result != EDigetherEconomyResult::Success)
			{
				return false;
			}
			Returned.LifeAtEnd = Death.State;
			Returned.WalletAtEnd = Loss.State;
		}
		else if (Player.Life.PublicState.Life == EDigetherLifeState::Active)
		{
			const FDigetherLifeUpdateResult Escape = FDigetherLifeRules::ApplyEscape(Player.Life, EDigetherEscapeKind::Elevator);
			if (Escape.Result != EDigetherLifeResult::Success)
			{
				return false;
			}
			Returned.LifeAtEnd = Escape.State;
		}
		FDigetherPlayerSettlementInput Settlement;
		Settlement.bDied = Returned.LifeAtEnd.PublicState.Life == EDigetherLifeState::Dead;
		Settlement.bSurvived = !Settlement.bDied;
		Settlement.PreDeathBalanceCredits = Returned.WalletAtEnd.LastPreDeathBalanceCredits;
		SettlementInputs.Add(Settlement);
		Plan.Players.Add(Returned);
	}
	// 화물·히든·시체 회수는 아직 없으므로 0원. 분모는 출발 당시 인원으로 고정한다.
	const FDigetherRunSettlementResult Settlement = FDigetherEconomyRules::CalculateRunSettlement(
		0, DepartureCount, SettlementInputs, Tuning.CorpseRefundPercent);
	const FDigetherDayAdvanceResult Day = FDigetherEconomyRules::AdvanceDay(Quota);
	if (Settlement.Result != EDigetherEconomyResult::Success || Day.Result != EDigetherEconomyResult::Success)
	{
		return false;
	}
	const FDigetherRunQuotaResult Evaluated = FDigetherRunRules::EvaluateQuota(Day.State);
	if (Evaluated.Result != EDigetherRunResult::Success)
	{
		return false;
	}
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		FDigetherFrameworkPlayerReturn& Returned = Plan.Players[Index];
		const FDigetherWalletUpdateResult Payout = FDigetherEconomyRules::ApplyRunSettlement(Returned.WalletAtEnd,
			Settlement.Players[Index]);
		const FDigetherLifeUpdateResult Revived = FDigetherLifeRules::Revive(Returned.LifeAtEnd);
		if (Payout.Result != EDigetherEconomyResult::Success || Revived.Result != EDigetherLifeResult::Success)
		{
			return false;
		}
		Returned.SettledWallet = Payout.State;
		Returned.RevivedLife = Revived.State;
	}
	Plan.ReturnedQuota = Day.State;
	Plan.bGameOver = Evaluated.bGameOver;
	OutPlan = MoveTemp(Plan);
	return true;
}
