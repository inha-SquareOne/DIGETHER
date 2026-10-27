#include "Analytics/WorkbenchSpawnProbability.h"

#include "Algo/BinarySearch.h"
#include "Math/RandomStream.h"
#include "Containers/StringConv.h"
#include <cmath>
#include <cstdlib>

namespace WorkbenchAnalytics
{
	namespace
	{
		FSpawnPoolIssue MakeIssue(ESpawnPoolIssueType Type, int32 Index, const FString& Id, const FString& Message)
		{
			FSpawnPoolIssue Issue;
			Issue.Type = Type;
			Issue.EntryIndex = Index;
			Issue.EntryId = Id;
			Issue.Message = Message;
			return Issue;
		}
	}

	bool TryParseDoubleStrict(const FString& Text, double& OutValue)
	{
		FString Trimmed = Text;
		Trimmed.TrimStartAndEndInline();
		if (Trimmed.IsEmpty())
		{
			return false;
		}
		for (const TCHAR Ch : Trimmed)
		{
			const bool bAllowed = (Ch >= TEXT('0') && Ch <= TEXT('9')) || Ch == TEXT('+') || Ch == TEXT('-')
				|| Ch == TEXT('.') || Ch == TEXT('e') || Ch == TEXT('E');
			if (!bAllowed)
			{
				return false;
			}
		}
		const FTCHARToUTF8 Utf8(*Trimmed);
		char* End = nullptr;
		const double Parsed = std::strtod(Utf8.Get(), &End);
		if (End == nullptr || End == Utf8.Get() || *End != '\0' || !std::isfinite(Parsed))
		{
			return false;
		}
		OutValue = Parsed;
		return true;
	}

	TArray<FSpawnPoolIssue> ValidateSpawnPool(const TArray<FSpawnPoolEntry>& Entries)
	{
		TArray<FSpawnPoolIssue> Issues;
		if (Entries.Num() == 0)
		{
			Issues.Add(MakeIssue(ESpawnPoolIssueType::EmptyPool, INDEX_NONE, FString(), TEXT("풀에 항목이 없습니다.")));
			return Issues;
		}

		TSet<FString> SeenIds;
		double TotalWeight = 0.0;
		bool bAnyPositive = false;
		bool bOverflowReported = false;

		for (int32 Index = 0; Index < Entries.Num(); ++Index)
		{
			const FSpawnPoolEntry& Entry = Entries[Index];

			if (Entry.EntryId.IsEmpty())
			{
				Issues.Add(MakeIssue(ESpawnPoolIssueType::EmptyEntryId, Index, Entry.EntryId,
					FString::Printf(TEXT("%d번째 항목: ID가 비어 있습니다."), Index + 1)));
			}
			else if (SeenIds.Contains(Entry.EntryId))
			{
				Issues.Add(MakeIssue(ESpawnPoolIssueType::DuplicateEntryId, Index, Entry.EntryId,
					FString::Printf(TEXT("%d번째 항목: ID '%s'가 중복됩니다."), Index + 1, *Entry.EntryId)));
			}
			else
			{
				SeenIds.Add(Entry.EntryId);
			}

			if (!FMath::IsFinite(Entry.Value))
			{
				Issues.Add(MakeIssue(ESpawnPoolIssueType::NonFiniteValue, Index, Entry.EntryId,
					FString::Printf(TEXT("%d번째 항목('%s'): Value가 유한수가 아닙니다."), Index + 1, *Entry.EntryId)));
			}

			if (!FMath::IsFinite(Entry.Weight))
			{
				Issues.Add(MakeIssue(ESpawnPoolIssueType::NonFiniteWeight, Index, Entry.EntryId,
					FString::Printf(TEXT("%d번째 항목('%s'): 가중치가 유한수가 아닙니다(NaN/Inf)."), Index + 1, *Entry.EntryId)));
				continue;
			}
			if (Entry.Weight < 0.0)
			{
				Issues.Add(MakeIssue(ESpawnPoolIssueType::NegativeWeight, Index, Entry.EntryId,
					FString::Printf(TEXT("%d번째 항목('%s'): 가중치가 음수입니다(%g)."), Index + 1, *Entry.EntryId, Entry.Weight)));
				continue;
			}

			bAnyPositive |= Entry.Weight > 0.0;
			TotalWeight += Entry.Weight;
			if (!bOverflowReported && !FMath::IsFinite(TotalWeight))
			{
				bOverflowReported = true;
				Issues.Add(MakeIssue(ESpawnPoolIssueType::OverflowTotalWeight, Index, Entry.EntryId,
					FString::Printf(TEXT("%d번째 항목('%s')까지 합산하면 총 가중치가 double 범위를 넘습니다."), Index + 1, *Entry.EntryId)));
			}
		}

		if (!bAnyPositive && Issues.Num() == 0)
		{
			Issues.Add(MakeIssue(ESpawnPoolIssueType::AllZeroWeight, INDEX_NONE, FString(), TEXT("모든 항목의 가중치가 0입니다. 선택 가능한 항목이 없습니다.")));
		}
		return Issues;
	}

	FSpawnPoolCalculationResult CalculateSinglePoolProbabilities(const TArray<FSpawnPoolEntry>& Entries, int32 NumDraws)
	{
		FSpawnPoolCalculationResult Result;
		Result.NumDraws = FMath::Max(0, NumDraws);
		Result.Issues = ValidateSpawnPool(Entries);
		if (Result.Issues.Num() > 0)
		{
			return Result;
		}

		double Total = 0.0;
		for (const FSpawnPoolEntry& Entry : Entries)
		{
			Total += Entry.Weight;
		}
		Result.TotalWeight = Total;

		double ExpectedPerDraw = 0.0;
		Result.Entries.Reserve(Entries.Num());
		for (const FSpawnPoolEntry& Entry : Entries)
		{
			FSpawnPoolResultEntry Out;
			Out.EntryId = Entry.EntryId;
			Out.Weight = Entry.Weight;
			Out.Value = Entry.Value;
			Out.Probability = Entry.Weight / Total;
			if (Result.NumDraws > 0)
			{
				// 1-(1-p)^N을 작은 p에서도 정밀하게 계산한다.
				Out.ProbabilityAtLeastOnceInNDraws = Out.Probability >= 1.0
					? 1.0
					: -std::expm1(static_cast<double>(Result.NumDraws) * std::log1p(-Out.Probability));
				Out.ExpectedCountInNDraws = static_cast<double>(Result.NumDraws) * Out.Probability;
			}
			ExpectedPerDraw += Out.Probability * Entry.Value;
			Result.Entries.Add(Out);
		}

		Result.ExpectedValuePerDraw = ExpectedPerDraw;
		Result.ExpectedValueInNDraws = ExpectedPerDraw * static_cast<double>(Result.NumDraws);
		if (!FMath::IsFinite(Result.ExpectedValuePerDraw) || !FMath::IsFinite(Result.ExpectedValueInNDraws))
		{
			Result.Issues.Add(MakeIssue(ESpawnPoolIssueType::OverflowExpectedValue, INDEX_NONE, FString(), TEXT("기대값이 double 범위를 넘습니다. Value 또는 N을 줄이세요.")));
			Result.Entries.Reset();
			return Result;
		}

		Result.bSuccess = true;
		return Result;
	}

	bool ParseSpawnPoolCsv(const FString& CsvText, TArray<FSpawnPoolEntry>& OutEntries, TArray<FString>& OutParseErrors)
	{
		OutEntries.Reset();
		OutParseErrors.Reset();

		TArray<FString> Lines;
		CsvText.ParseIntoArrayLines(Lines, /*bCullEmpty=*/false);

		bool bSeenDataLine = false;
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			FString Line = Lines[LineIndex];
			Line.TrimStartAndEndInline();
			if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
			{
				continue;
			}

			TArray<FString> Columns;
			Line.ParseIntoArray(Columns, TEXT(","), /*bCullEmpty=*/false);
			for (FString& Column : Columns)
			{
				Column.TrimStartAndEndInline();
			}

			const bool bFirstDataLine = !bSeenDataLine;
			bSeenDataLine = true;

			if (Columns.Num() < 2 || Columns.Num() > 3)
			{
				OutParseErrors.Add(FString::Printf(TEXT("%d행: \"EntryId,Weight[,Value]\" 형식이 아닙니다(\"%s\")."), LineIndex + 1, *Line));
				continue;
			}

			double Weight = 0.0;
			const bool bWeightOk = TryParseDoubleStrict(Columns[1], Weight);
			if (bFirstDataLine && !bWeightOk && (Columns[0].Equals(TEXT("EntryId"), ESearchCase::IgnoreCase) || Columns[0].Equals(TEXT("ID"), ESearchCase::IgnoreCase)))
			{
				continue;
			}
			if (!bWeightOk)
			{
				OutParseErrors.Add(FString::Printf(TEXT("%d행: Weight '%s'를 숫자로 해석할 수 없습니다."), LineIndex + 1, *Columns[1]));
				continue;
			}

			double Value = 0.0;
			if (Columns.Num() == 3 && !Columns[2].IsEmpty() && !TryParseDoubleStrict(Columns[2], Value))
			{
				OutParseErrors.Add(FString::Printf(TEXT("%d행: Value '%s'를 숫자로 해석할 수 없습니다."), LineIndex + 1, *Columns[2]));
				continue;
			}

			FSpawnPoolEntry Entry;
			Entry.EntryId = Columns[0];
			Entry.Weight = Weight;
			Entry.Value = Value;
			OutEntries.Add(Entry);
		}

		return OutParseErrors.Num() == 0;
	}

	int32 PickWeightedIndex(const TArray<double>& CumulativeWeights, double Fraction)
	{
		const int32 Num = CumulativeWeights.Num();
		if (Num == 0)
		{
			return INDEX_NONE;
		}
		const double Roll = FMath::Clamp(Fraction, 0.0, 1.0) * CumulativeWeights.Last();
		int32 Index = Algo::UpperBound(CumulativeWeights, Roll);
		if (Index >= Num)
		{
			// 부동소수 반올림으로 Roll이 총합에 닿은 경우: 가중치가 양수인 마지막 항목으로 되돌린다.
			Index = Num - 1;
			while (Index > 0 && CumulativeWeights[Index] <= CumulativeWeights[Index - 1])
			{
				--Index;
			}
		}
		return Index;
	}

	FSpawnPoolMonteCarloResult RunMonteCarloSimulation(const TArray<FSpawnPoolEntry>& Entries, int32 TotalDraws, int32 Seed)
	{
		FSpawnPoolMonteCarloResult Result;
		Result.Seed = Seed;

		Result.Issues = ValidateSpawnPool(Entries);
		if (TotalDraws < 0 || TotalDraws > MaxMonteCarloDraws)
		{
			Result.Issues.Add(MakeIssue(ESpawnPoolIssueType::InvalidDrawCount, INDEX_NONE, FString(),
				FString::Printf(TEXT("Monte Carlo 추출 수는 0 이상 %d 이하여야 합니다(입력 %d)."), MaxMonteCarloDraws, TotalDraws)));
		}
		if (Result.Issues.Num() > 0)
		{
			return Result;
		}

		Result.TotalDraws = TotalDraws;
		TArray<double> Cumulative;
		Cumulative.Reserve(Entries.Num());
		double Running = 0.0;
		for (const FSpawnPoolEntry& Entry : Entries)
		{
			Running += Entry.Weight;
			Cumulative.Add(Running);
		}
		Result.DrawCounts.Init(0, Entries.Num());

		FRandomStream Stream(Seed);
		for (int32 Draw = 0; Draw < TotalDraws; ++Draw)
		{
			++Result.DrawCounts[PickWeightedIndex(Cumulative, Stream.GetFraction())];
		}

		Result.bSuccess = true;
		return Result;
	}

	double MonteCarloTolerance(double Probability, int32 TotalDraws)
	{
		if (TotalDraws <= 0)
		{
			return 1.0;
		}
		const double N = static_cast<double>(TotalDraws);
		return FMath::Max(5.0 * FMath::Sqrt(Probability * (1.0 - Probability) / N), 1.0 / N);
	}

	bool IsObservedWithinTolerance(double Probability, int32 ObservedCount, int32 TotalDraws)
	{
		if (TotalDraws <= 0)
		{
			return false;
		}
		if (Probability <= 0.0)
		{
			return ObservedCount == 0;
		}
		if (Probability >= 1.0)
		{
			return ObservedCount == TotalDraws;
		}
		const double Observed = static_cast<double>(ObservedCount) / static_cast<double>(TotalDraws);
		return FMath::Abs(Observed - Probability) <= MonteCarloTolerance(Probability, TotalDraws);
	}
}
