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
	const FDigetherFrameworkPlayerReturn& Returned)
{
	FDigetherRunResult Result;
	Result.RunId = RunId;
	Result.CompletedDay = CompletedDay;
	Result.Outcome = Returned.ReturnOutcome;
	Result.GeneralCredits = Returned.Settlement.GeneralCredits;
	Result.HiddenCredits = Returned.Settlement.HiddenCredits;
	Result.DeathLossCredits = Returned.DeathLossCredits;
	Result.CorpseRefundCredits = Returned.Settlement.CorpseRefundCredits;
	Result.PayoutCredits = Returned.Settlement.PayoutCredits;
	Result.FinalBalanceCredits = Returned.SettledWallet.BalanceCredits;
	return Result;
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
	// 볼륨 경계에서 반 칸 안쪽까지만 쓴다. 중심 칸을 포함하는 대칭 격자이며 Z는 볼륨 중심 높이(0)다.
	const double HalfSpacing = SpacingCm / 2.0;
	const int32 HalfX = FMath::FloorToInt32((BoxExtent.X - HalfSpacing) / SpacingCm);
	const int32 HalfY = FMath::FloorToInt32((BoxExtent.Y - HalfSpacing) / SpacingCm);
	TArray<FVector> Candidates;
	for (int32 X = -HalfX; X <= HalfX; ++X)
	{
		for (int32 Y = -HalfY; Y <= HalfY; ++Y)
		{
			Candidates.Emplace(X * SpacingCm, Y * SpacingCm, 0.0);
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
		if (Settlement.bDied)
		{
			Returned.ReturnOutcome = Outcome.Outcome == EDigetherRunPlayerOutcome::MissingDeath
				? EDigetherRunReturnOutcome::Missing : EDigetherRunReturnOutcome::Died;
			// 누적 손실은 int32에서 포화될 수 있다. 이번 런의 사망 전 잔고로 같은 손실 규칙을 계산한다.
			Returned.DeathLossCredits = FDigetherEconomyRules::CalculateDeathLoss(
				Returned.WalletAtEnd.LastPreDeathBalanceCredits, Tuning.DeathLossPercent).LossCredits;
		}
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
		Returned.Settlement = Settlement.Players[Index];
	}
	Plan.ReturnedQuota = Day.State;
	Plan.bGameOver = Evaluated.bGameOver;
	OutPlan = MoveTemp(Plan);
	return true;
}
