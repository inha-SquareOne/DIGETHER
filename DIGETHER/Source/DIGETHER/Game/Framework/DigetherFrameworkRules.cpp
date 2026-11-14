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
	// 거점 문 닫힘은 출발 처리 전에만 있으므로 런 종료 판정에 오는 DoorClosing은 갱도 문 닫힘이다(기획 A3).
	return Phase == EDigetherElevatorPhase::BottomWaiting || Phase == EDigetherElevatorPhase::DoorClosing
		? EDigetherRunElevatorState::StoppedInMine : EDigetherRunElevatorState::AtBase;
}

EDigetherRunPlayerLocation DigetherFrameworkRules::MakeRunPlayerLocation(const EDigetherLifeState Life,
	const bool bInBoardingVolume, const EDigetherElevatorPhase ElevatorPhase)
{
	if (Life == EDigetherLifeState::Escaped)
	{
		return EDigetherRunPlayerLocation::Base;
	}
	// 문이 닫히는 동안에도 탈 수 있다(기획 A3).
	const bool bBoardingPhase = ElevatorPhase == EDigetherElevatorPhase::InitialDescent
		|| ElevatorPhase == EDigetherElevatorPhase::BottomWaiting
		|| ElevatorPhase == EDigetherElevatorPhase::DoorClosing;
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

bool DigetherFrameworkRules::IsNewJoinLocked(const FDigetherRunPublicState& Run,
	const EDigetherElevatorPhase ElevatorPhase)
{
	// 기획 C3: 출발 레버 게이지가 찬 순간(거점 문 닫힘 시작)부터 잠근다. 문이 다시 열리면 풀린다.
	// 기획 S33·04 9~10행: 게임 오버(실패 사건·크레딧)에도 신규 참가를 열지 않는다. 새 게임이 시작되면 풀린다.
	return Run.Phase == EDigetherRunPhase::InProgress || Run.bGameOver
		|| ElevatorPhase == EDigetherElevatorPhase::DoorClosing;
}

FDigetherRunResult DigetherFrameworkRules::MakeRunResult(const FGuid RunId, const int32 CompletedDay,
	const FDigetherFrameworkReturnPlan& Plan, const int32 PlayerIndex)
{
	FDigetherRunResult Result;
	if (!Plan.Players.IsValidIndex(PlayerIndex))
	{
		return Result;
	}
	const FDigetherFrameworkPlayerReturn& Returned = Plan.Players[PlayerIndex];
	Result.RunId = RunId;
	Result.CompletedDay = CompletedDay;
	Result.Outcome = Returned.ReturnOutcome;
	Result.FarmingCredits = Plan.GeneralCargoCredits;
	Result.ExcludedCredits = Plan.ExcludedGeneralCredits;
	Result.DeadPlayerNames = Plan.DeadPlayerNames;
	Result.MissingPlayerNames = Plan.MissingPlayerNames;
	Result.PreviousBalanceCredits = Returned.PreviousBalanceCredits;
	Result.GeneralCredits = Returned.Settlement.GeneralCredits;
	Result.HiddenCredits = Returned.Settlement.HiddenCredits;
	Result.DeathLossCredits = Returned.DeathLossCredits;
	Result.DeathLossPercent = Returned.DeathLossPercent;
	Result.bCorpseRecovered = Returned.bCorpseRecovered;
	Result.PayoutCredits = Returned.Settlement.PayoutCredits;
	Result.FinalBalanceCredits = Returned.SettledWallet.BalanceCredits;
	return Result;
}

EDigetherDepartureWarningKind DigetherFrameworkRules::ResolveDepartureWarning(
	const EDigetherDepartureWarningKind LeverWarning, const FDigetherRunPublicState& Run)
{
	// 마감 기록(bPaymentDeadline)은 4일차 게이지 완료 때 남고 탐사 출발(InProgress)·귀환 정리에서 종료
	if (Run.bPaymentDeadline && !Run.bGameOver && Run.Phase != EDigetherRunPhase::InProgress)
	{
		return EDigetherDepartureWarningKind::DeadlineConfirmed;
	}
	return LeverWarning;
}

bool DigetherFrameworkRules::TryPlanChainSummon(const FVector& BoxExtent, const double SpacingCm,
	const TArray<FVector>& OccupiedLocal, const int32 SummonCount, TArray<FVector>& OutLocations)
{
	if (SummonCount < 0 || !FMath::IsFinite(SpacingCm) || SpacingCm <= 0.0 || BoxExtent.ContainsNaN()
		|| BoxExtent.X <= 0.0 || BoxExtent.Y <= 0.0)
	{
		return false;
	}
	for (const FVector& Occupied : OccupiedLocal)
	{
		if (Occupied.ContainsNaN())
		{
			return false;
		}
	}
	// 경계에서 반 칸 여유를 둔 중앙 정렬 격자, 짝수 행·열도 수용하고 Z는 볼륨 중심 높이(0)
	const int32 CountX = FMath::FloorToInt32(2.0 * BoxExtent.X / SpacingCm);
	const int32 CountY = FMath::FloorToInt32(2.0 * BoxExtent.Y / SpacingCm);
	const double StartX = -(CountX - 1) * SpacingCm / 2.0;
	const double StartY = -(CountY - 1) * SpacingCm / 2.0;
	TArray<FVector> Candidates;
	for (int32 X = 0; X < CountX; ++X)
	{
		for (int32 Y = 0; Y < CountY; ++Y)
		{
			Candidates.Emplace(StartX + X * SpacingCm, StartY + Y * SpacingCm, 0.0);
		}
	}
	// 같은 입력이면 같은 배치가 나오도록 거리·X·Y 순으로 정렬한다.
	Candidates.Sort([](const FVector& Left, const FVector& Right)
	{
		const double LeftDistance = Left.SizeSquared2D();
		const double RightDistance = Right.SizeSquared2D();
		if (LeftDistance != RightDistance)
		{
			return LeftDistance < RightDistance;
		}
		return Left.X != Right.X ? Left.X < Right.X : Left.Y < Right.Y;
	});
	TArray<FVector> Taken = OccupiedLocal;
	TArray<FVector> Planned;
	const double MinDistanceSquared = FMath::Square(SpacingCm) - UE_KINDA_SMALL_NUMBER;
	for (const FVector& Candidate : Candidates)
	{
		if (Planned.Num() == SummonCount)
		{
			break;
		}
		const bool bFree = !Taken.ContainsByPredicate([&Candidate, MinDistanceSquared](const FVector& Other)
		{
			return FVector::DistSquared2D(Candidate, Other) < MinDistanceSquared;
		});
		if (bFree)
		{
			Planned.Add(Candidate);
			Taken.Add(Candidate);
		}
	}
	if (Planned.Num() != SummonCount)
	{
		return false;
	}
	OutLocations = MoveTemp(Planned);
	return true;
}

bool DigetherFrameworkRules::TryPlanReturn(const FDigetherRunTerminationInput& Input,
	const TArray<FDigetherFrameworkPlayerSnapshot>& Players, const FDigetherQuotaState& Quota,
	const FDigetherRunTuning& Tuning, const int32 DepartureCount, const int32 GeneralCargoCredits,
	FDigetherFrameworkReturnPlan& OutPlan)
{
	FDigetherFrameworkReturnPlan Plan;
	Plan.Termination = FDigetherRunRules::CalculateTermination(Input);
	if (Plan.Termination.Result != EDigetherRunResult::Success || Players.Num() != Input.Players.Num()
		|| DepartureCount <= 0 || Players.Num() > DepartureCount || GeneralCargoCredits < 0)
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
		Returned.PreviousBalanceCredits = Player.Wallet.BalanceCredits;
		const FDigetherRunPlayerOutcome& Outcome = Plan.Termination.Players[Index];
		if (Outcome.Outcome == EDigetherRunPlayerOutcome::MissingDeath)
		{
			const FDigetherLifeUpdateResult Death = FDigetherLifeRules::ApplyDeath(Player.Life, Outcome.DeathCause);
			if (Death.Result != EDigetherLifeResult::Success)
			{
				return false;
			}
			Returned.LifeAtEnd = Death.State;
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
		if (Player.bCorpseRecovered && !Settlement.bDied)
		{
			return false;
		}
		if (Settlement.bDied)
		{
			Returned.ReturnOutcome = Outcome.Outcome == EDigetherRunPlayerOutcome::MissingDeath
				? EDigetherRunReturnOutcome::Missing : EDigetherRunReturnOutcome::Died;
			// D25 개정(런 플로우 v1.1.0 9.4): 탐사 중 사망·잔류 사망 모두 여기서 한 번만 차감
			// 비율은 시체 유효 회수 여부로 확정, 전멸이어도 회수 시체는 회수 비율, 시체 없음은 미회수 비율
			Returned.bCorpseRecovered = Player.bCorpseRecovered;
			Returned.DeathLossPercent = FDigetherEconomyRules::GetExplorationDeathLossPercent(Tuning,
				Player.bCorpseRecovered);
			const FDigetherWalletUpdateResult Loss = FDigetherEconomyRules::ApplyDeathLoss(Player.Wallet,
				Returned.DeathLossPercent);
			if (Loss.Result != EDigetherEconomyResult::Success)
			{
				return false;
			}
			Returned.WalletAtEnd = Loss.State;
			// 누적 손실은 int32에서 포화될 수 있어 이번 차감액은 잔고 차이로 기록
			Returned.DeathLossCredits = Player.Wallet.BalanceCredits - Loss.State.BalanceCredits;
			TArray<FString>& Names = Returned.ReturnOutcome == EDigetherRunReturnOutcome::Missing
				? Plan.MissingPlayerNames : Plan.DeadPlayerNames;
			Names.Add(Player.DisplayName);
		}
		Settlement.bSurvived = !Settlement.bDied;
		Settlement.bCorpseRecovered = Player.bCorpseRecovered;
		SettlementInputs.Add(Settlement);
		Plan.Players.Add(Returned);
	}
	// 분모는 출발 당시 인원으로 고정, 화물 회수 연결 전에는 호출자가 0원 전달
	const FDigetherRunSettlementResult Settlement = FDigetherEconomyRules::CalculateRunSettlement(
		GeneralCargoCredits, DepartureCount, SettlementInputs);
	const FDigetherDayAdvanceResult Day = FDigetherEconomyRules::AdvanceDay(Quota);
	if (Settlement.Result != EDigetherEconomyResult::Success || Day.Result != EDigetherEconomyResult::Success)
	{
		return false;
	}
	Plan.GeneralCargoCredits = GeneralCargoCredits;
	Plan.ExcludedGeneralCredits = Settlement.LostGeneralCredits;
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
		Returned.Settlement = Settlement.Players[Index];
	}
	Plan.ReturnedQuota = Day.State;
	Plan.bGameOver = Evaluated.bGameOver;
	OutPlan = MoveTemp(Plan);
	return true;
}
