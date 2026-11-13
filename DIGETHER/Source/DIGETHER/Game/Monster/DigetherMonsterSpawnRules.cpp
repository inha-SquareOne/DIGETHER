#include "Game/Monster/DigetherMonsterSpawnRules.h"

namespace DigetherMonsterSpawnRules
{
	float EvaluateChance(const FCurveParams& Params, const float TimerSeconds)
	{
		const float Start = FMath::Clamp(Params.StartChance, 0.f, 1.f);
		const float Max = FMath::Clamp(Params.MaxChance, 0.f, 1.f);
		if (!FMath::IsFinite(TimerSeconds) || TimerSeconds < Params.GraceSeconds || Max <= 0.f)
		{
			return 0.f;
		}
		const float Span = Params.MaxChanceAtSeconds - Params.GraceSeconds;
		if (Span <= 0.f || TimerSeconds >= Params.MaxChanceAtSeconds)
		{
			return Max;
		}
		const float Progress = FMath::Clamp((TimerSeconds - Params.GraceSeconds) / Span, 0.f, 1.f);
		if (Start <= 0.f || Start >= Max)
		{
			return FMath::Lerp(Start, Max, Progress);
		}
		return FMath::Clamp(Start * FMath::Pow(Max / Start, Progress), 0.f, Max);
	}

	TArray<FCandidate> FilterCandidates(const TConstArrayView<FCandidate> Candidates, const int32 RemainingPower)
	{
		TArray<FCandidate> Result;
		for (const FCandidate& Candidate : Candidates)
		{
			if (!FMath::IsFinite(Candidate.Weight) || Candidate.Weight <= 0.f)
			{
				continue;
			}
			if (RemainingPower >= 0 && Candidate.PowerCost > RemainingPower)
			{
				continue;
			}
			if (Candidate.RunSpawnCap > 0 && Candidate.RunSpawned >= Candidate.RunSpawnCap)
			{
				continue;
			}
			if (Candidate.MaxAlive > 0 && Candidate.Alive >= Candidate.MaxAlive)
			{
				continue;
			}
			Result.Add(Candidate);
		}
		return Result;
	}

	int32 PickWeighted(const TConstArrayView<FCandidate> Candidates, const FRandomStream& Stream)
	{
		double Total = 0.0;
		for (const FCandidate& Candidate : Candidates)
		{
			Total += FMath::Max(0.f, Candidate.Weight);
		}
		if (Total <= 0.0)
		{
			return INDEX_NONE;
		}
		const double Roll = Stream.FRand() * Total;
		double Accumulated = 0.0;
		int32 LastPositive = INDEX_NONE;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			const double Weight = FMath::Max(0.f, Candidates[Index].Weight);
			if (Weight <= 0.0)
			{
				continue;
			}
			LastPositive = Index;
			Accumulated += Weight;
			if (Roll < Accumulated)
			{
				return Index;
			}
		}
		// 부동소수 누적 오차로 끝에 닿은 경우
		return LastPositive;
	}

	int32 EvaluatePowerLimit(const int32 Base, const int32 PerGoalAchieved, const int32 Max, const int32 GoalsAchieved)
	{
		const int64 Raw = static_cast<int64>(FMath::Max(0, Base))
			+ static_cast<int64>(FMath::Max(0, PerGoalAchieved)) * FMath::Max(0, GoalsAchieved);
		return static_cast<int32>(FMath::Clamp<int64>(Raw, 0, FMath::Max(Base, Max)));
	}

	void FLedger::Reset()
	{
		DungeonPowerUsed.Reset();
		RunSpawnedBySpecies.Reset();
		AliveBySpecies.Reset();
	}

	int32 FLedger::GetPowerUsed(const FName DungeonId) const
	{
		return DungeonPowerUsed.FindRef(DungeonId);
	}

	int32 FLedger::GetRunSpawned(const FName SpeciesId) const
	{
		return RunSpawnedBySpecies.FindRef(SpeciesId);
	}

	int32 FLedger::GetAlive(const FName SpeciesId) const
	{
		return AliveBySpecies.FindRef(SpeciesId);
	}

	void FLedger::RecordSpawn(const FName SpeciesId, const FName DungeonId, const int32 PowerCost)
	{
		++RunSpawnedBySpecies.FindOrAdd(SpeciesId);
		++AliveBySpecies.FindOrAdd(SpeciesId);
		if (!DungeonId.IsNone() && PowerCost > 0)
		{
			DungeonPowerUsed.FindOrAdd(DungeonId) += PowerCost;
		}
	}

	bool FLedger::RecordRemoval(const FName SpeciesId, const FName DungeonId, const int32 PowerCost)
	{
		int32* Alive = AliveBySpecies.Find(SpeciesId);
		if (!Alive || *Alive <= 0)
		{
			return false;
		}
		--(*Alive);
		if (!DungeonId.IsNone() && PowerCost > 0)
		{
			if (int32* Used = DungeonPowerUsed.Find(DungeonId))
			{
				*Used = FMath::Max(0, *Used - PowerCost);
			}
		}
		return true;
	}

	bool FLedger::RecordCancelledSpawn(const FName SpeciesId, const FName DungeonId, const int32 PowerCost)
	{
		int32* Spawned = RunSpawnedBySpecies.Find(SpeciesId);
		if (!Spawned || *Spawned <= 0 || !RecordRemoval(SpeciesId, DungeonId, PowerCost))
		{
			return false;
		}
		--(*Spawned);
		return true;
	}
}
