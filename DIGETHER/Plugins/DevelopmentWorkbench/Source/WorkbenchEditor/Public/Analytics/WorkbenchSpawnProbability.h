// T02 스폰 확률: 조건을 고정한 단일 풀의 복원 추출 확률 계산. 비복원·pity·조건부 가중치는 지원하지 않는다.
#pragma once

#include "CoreMinimal.h"

namespace WorkbenchAnalytics
{
	/** 단일 풀 항목. Weight는 음수가 아닌 유한수여야 하며, Value는 사용자가 입력한 합성 단위 값이다(실제 가격 아님). */
	struct WORKBENCHEDITOR_API FSpawnPoolEntry
	{
		FString EntryId;
		double Weight = 0.0;
		double Value = 0.0;
	};

	enum class ESpawnPoolIssueType : uint8
	{
		EmptyEntryId,
		DuplicateEntryId,
		NegativeWeight,
		NonFiniteWeight,
		NonFiniteValue,
		EmptyPool,
		AllZeroWeight,
		OverflowTotalWeight,
		OverflowExpectedValue,
		InvalidDrawCount,
	};

	struct WORKBENCHEDITOR_API FSpawnPoolIssue
	{
		ESpawnPoolIssueType Type = ESpawnPoolIssueType::EmptyPool;
		int32 EntryIndex = INDEX_NONE; // 풀 전체에 해당하면 INDEX_NONE
		FString EntryId;
		FString Message; // 한국어 설명
	};

	/** 문자열 전체가 하나의 숫자일 때만 true. 앞뒤 공백은 허용하고 "12abc" 같은 꼬리 문자는 거부한다. "inf"/"nan"은 숫자로 읽히며 이후 검증이 거부한다. */
	WORKBENCHEDITOR_API bool TryParseDoubleStrict(const FString& Text, double& OutValue);

	/** 풀 구조 검증. 문제가 하나라도 있으면 확률 계산을 수행하지 않는다. */
	WORKBENCHEDITOR_API TArray<FSpawnPoolIssue> ValidateSpawnPool(const TArray<FSpawnPoolEntry>& Entries);

	struct WORKBENCHEDITOR_API FSpawnPoolResultEntry
	{
		FString EntryId;
		double Weight = 0.0;
		double Value = 0.0;
		double Probability = 0.0; // Weight / TotalWeight
		double ProbabilityAtLeastOnceInNDraws = 0.0; // 1-(1-p)^N, 독립 복원 추출 전제
		double ExpectedCountInNDraws = 0.0; // N * p
	};

	struct WORKBENCHEDITOR_API FSpawnPoolCalculationResult
	{
		bool bSuccess = false;
		TArray<FSpawnPoolIssue> Issues;
		TArray<FSpawnPoolResultEntry> Entries;
		double TotalWeight = 0.0;
		int32 NumDraws = 0;
		double ExpectedValuePerDraw = 0.0; // sum(p_i * Value_i)
		double ExpectedValueInNDraws = 0.0; // N * ExpectedValuePerDraw (추출 간 독립이므로 선형)
	};

	/** 단일 풀 복원 추출의 정확 확률. NumDraws<=0이면 N회 필드는 0이다. */
	WORKBENCHEDITOR_API FSpawnPoolCalculationResult CalculateSinglePoolProbabilities(const TArray<FSpawnPoolEntry>& Entries, int32 NumDraws);

	/**
	 * CSV 파싱. 한 줄당 "EntryId,Weight[,Value]". 빈 줄과 '#' 주석은 건너뛰고,
	 * 첫 데이터 줄의 두 번째 열이 숫자가 아니고 첫 열이 EntryId/ID이면 헤더로 취급한다.
	 */
	WORKBENCHEDITOR_API bool ParseSpawnPoolCsv(const FString& CsvText, TArray<FSpawnPoolEntry>& OutEntries, TArray<FString>& OutParseErrors);

	constexpr int32 MaxMonteCarloDraws = 5000000;

	struct WORKBENCHEDITOR_API FSpawnPoolMonteCarloResult
	{
		bool bSuccess = false;
		TArray<FSpawnPoolIssue> Issues;
		TArray<int32> DrawCounts; // 입력 Entries와 같은 순서의 관측 횟수
		int32 TotalDraws = 0;
		int32 Seed = 0;
	};

	/** 결정적 FRandomStream(Seed)으로 복원 추출을 TotalDraws회(0..MaxMonteCarloDraws) 수행한다. 해석값과 교차 검증하는 용도이며 실게임 추출기가 아니다. */
	WORKBENCHEDITOR_API FSpawnPoolMonteCarloResult RunMonteCarloSimulation(const TArray<FSpawnPoolEntry>& Entries, int32 TotalDraws, int32 Seed);

	/** 도구 시험 허용 폭 max(5*sqrt(p(1-p)/N), 1/N). 게임 밸런스 목표가 아니다. */
	WORKBENCHEDITOR_API double MonteCarloTolerance(double Probability, int32 TotalDraws);

	/** p가 0 또는 1이면 관측 횟수가 정확히 0/N이어야 하고, 그 외에는 허용 폭 이내여야 true. */
	WORKBENCHEDITOR_API bool IsObservedWithinTolerance(double Probability, int32 ObservedCount, int32 TotalDraws);

	/** 가중 선택에 쓰는 누적 가중치 구간에서 Fraction(0 이상 1 미만)에 해당하는 항목 인덱스. Weight가 0인 항목은 선택되지 않는다. */
	WORKBENCHEDITOR_API int32 PickWeightedIndex(const TArray<double>& CumulativeWeights, double Fraction);
}
