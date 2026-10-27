#include "Misc/AutomationTest.h"
#include "Analytics/WorkbenchSpawnProbability.h"
#include <limits>

using namespace WorkbenchAnalytics;

namespace
{
	TArray<FSpawnPoolEntry> MakeFixturePool()
	{
		// 합성 예제: A=1, B=3, 미선택=0 => 25% / 75% / 0%
		return {
			FSpawnPoolEntry{ TEXT("A"), 1.0 },
			FSpawnPoolEntry{ TEXT("B"), 3.0 },
			FSpawnPoolEntry{ TEXT("미선택"), 0.0 },
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchSpawnProbability_ExactFixture, "DevelopmentWorkbench.Analytics.T02.ExactFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchSpawnProbability_ExactFixture::RunTest(const FString& Parameters)
{
	const FSpawnPoolCalculationResult Result = CalculateSinglePoolProbabilities(MakeFixturePool(), /*NumDraws=*/5);
	TestTrue(TEXT("계산 성공"), Result.bSuccess);
	TestEqual(TEXT("총 가중치"), Result.TotalWeight, 4.0);
	TestEqual(TEXT("항목 수"), Result.Entries.Num(), 3);

	TestEqual(TEXT("A 확률"), Result.Entries[0].Probability, 0.25);
	TestEqual(TEXT("B 확률"), Result.Entries[1].Probability, 0.75);
	TestEqual(TEXT("미선택 확률"), Result.Entries[2].Probability, 0.0);

	const double ExpectedAtLeastOnceA = 1.0 - FMath::Pow(0.75, 5.0);
	TestTrue(TEXT("A N회 적어도1회"), FMath::IsNearlyEqual(Result.Entries[0].ProbabilityAtLeastOnceInNDraws, ExpectedAtLeastOnceA, 1e-9));
	TestEqual(TEXT("미선택 N회 적어도1회"), Result.Entries[2].ProbabilityAtLeastOnceInNDraws, 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchSpawnProbability_EmptyPool, "DevelopmentWorkbench.Analytics.T02.EmptyPool", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchSpawnProbability_EmptyPool::RunTest(const FString& Parameters)
{
	const FSpawnPoolCalculationResult Result = CalculateSinglePoolProbabilities(TArray<FSpawnPoolEntry>{}, 10);
	TestFalse(TEXT("빈 풀은 실패"), Result.bSuccess);
	TestTrue(TEXT("EmptyPool 이슈 포함"), Result.Issues.ContainsByPredicate([](const FSpawnPoolIssue& Issue) { return Issue.Type == ESpawnPoolIssueType::EmptyPool; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchSpawnProbability_AllZeroWeight, "DevelopmentWorkbench.Analytics.T02.AllZeroWeight", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchSpawnProbability_AllZeroWeight::RunTest(const FString& Parameters)
{
	TArray<FSpawnPoolEntry> Entries = { { TEXT("A"), 0.0 }, { TEXT("B"), 0.0 } };
	const FSpawnPoolCalculationResult Result = CalculateSinglePoolProbabilities(Entries, 10);
	TestFalse(TEXT("전부 0 가중치는 실패"), Result.bSuccess);
	TestTrue(TEXT("AllZeroWeight 이슈 포함"), Result.Issues.ContainsByPredicate([](const FSpawnPoolIssue& Issue) { return Issue.Type == ESpawnPoolIssueType::AllZeroWeight; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchSpawnProbability_NegativeAndNonFiniteWeight, "DevelopmentWorkbench.Analytics.T02.NegativeAndNonFiniteWeight", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchSpawnProbability_NegativeAndNonFiniteWeight::RunTest(const FString& Parameters)
{
	TArray<FSpawnPoolEntry> Entries = { { TEXT("A"), -1.0 }, { TEXT("B"), std::numeric_limits<double>::infinity() } };
	const TArray<FSpawnPoolIssue> Issues = ValidateSpawnPool(Entries);
	TestTrue(TEXT("음수 가중치 검출"), Issues.ContainsByPredicate([](const FSpawnPoolIssue& Issue) { return Issue.Type == ESpawnPoolIssueType::NegativeWeight; }));
	TestTrue(TEXT("비유한 가중치 검출"), Issues.ContainsByPredicate([](const FSpawnPoolIssue& Issue) { return Issue.Type == ESpawnPoolIssueType::NonFiniteWeight; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchSpawnProbability_DuplicateAndEmptyId, "DevelopmentWorkbench.Analytics.T02.DuplicateAndEmptyId", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchSpawnProbability_DuplicateAndEmptyId::RunTest(const FString& Parameters)
{
	TArray<FSpawnPoolEntry> Entries = { { TEXT("A"), 1.0 }, { TEXT("A"), 2.0 }, { TEXT(""), 1.0 } };
	const TArray<FSpawnPoolIssue> Issues = ValidateSpawnPool(Entries);
	TestTrue(TEXT("중복 ID 검출"), Issues.ContainsByPredicate([](const FSpawnPoolIssue& Issue) { return Issue.Type == ESpawnPoolIssueType::DuplicateEntryId; }));
	TestTrue(TEXT("빈 ID 검출"), Issues.ContainsByPredicate([](const FSpawnPoolIssue& Issue) { return Issue.Type == ESpawnPoolIssueType::EmptyEntryId; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchSpawnProbability_OverflowWeight, "DevelopmentWorkbench.Analytics.T02.OverflowWeight", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchSpawnProbability_OverflowWeight::RunTest(const FString& Parameters)
{
	const double Huge = TNumericLimits<double>::Max();
	TArray<FSpawnPoolEntry> Entries = { { TEXT("A"), Huge }, { TEXT("B"), Huge } };
	const TArray<FSpawnPoolIssue> Issues = ValidateSpawnPool(Entries);
	TestTrue(TEXT("오버플로 검출"), Issues.ContainsByPredicate([](const FSpawnPoolIssue& Issue) { return Issue.Type == ESpawnPoolIssueType::OverflowTotalWeight; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchSpawnProbability_CsvParseRoundTrip, "DevelopmentWorkbench.Analytics.T02.CsvParseRoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchSpawnProbability_CsvParseRoundTrip::RunTest(const FString& Parameters)
{
	const FString Csv = TEXT("# 주석\nEntryId,Weight\nA,1\nB,3\n");
	TArray<FSpawnPoolEntry> Entries;
	TArray<FString> Errors;
	TestTrue(TEXT("CSV 파싱 성공"), ParseSpawnPoolCsv(Csv, Entries, Errors));
	TestEqual(TEXT("파싱된 항목 수"), Entries.Num(), 2);
	TestEqual(TEXT("B 가중치"), Entries[1].Weight, 3.0);

	const FString BadCsv = TEXT("A,1,extra\nB,notanumber\n");
	TArray<FSpawnPoolEntry> BadEntries;
	TArray<FString> BadErrors;
	TestFalse(TEXT("잘못된 CSV는 실패"), ParseSpawnPoolCsv(BadCsv, BadEntries, BadErrors));
	TestEqual(TEXT("오류 2건"), BadErrors.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchSpawnProbability_MonteCarloDeterministicReproduction, "DevelopmentWorkbench.Analytics.T02.MonteCarloDeterministicReproduction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchSpawnProbability_MonteCarloDeterministicReproduction::RunTest(const FString& Parameters)
{
	const TArray<FSpawnPoolEntry> Pool = MakeFixturePool();
	const int32 Seed = 42;
	const int32 NumDraws = 10000;

	const FSpawnPoolMonteCarloResult First = RunMonteCarloSimulation(Pool, NumDraws, Seed);
	const FSpawnPoolMonteCarloResult Second = RunMonteCarloSimulation(Pool, NumDraws, Seed);

	TestTrue(TEXT("첫 실행 성공"), First.bSuccess);
	if (First.DrawCounts.Num() != 3 || Second.DrawCounts.Num() != 3)
	{
		AddError(TEXT("DrawCounts 길이가 항목 수와 다름"));
		return false;
	}
	TestEqual(TEXT("같은 시드는 동일 결과(A)"), First.DrawCounts[0], Second.DrawCounts[0]);
	TestEqual(TEXT("같은 시드는 동일 결과(B)"), First.DrawCounts[1], Second.DrawCounts[1]);
	TestEqual(TEXT("미선택은 항상 0회"), First.DrawCounts[2], 0);
	TestEqual(TEXT("총 횟수 보존"), First.DrawCounts[0] + First.DrawCounts[1], NumDraws);

	TestTrue(TEXT("A 관측 빈도가 해석값 허용폭 내"), IsObservedWithinTolerance(0.25, First.DrawCounts[0], NumDraws));
	TestTrue(TEXT("B 관측 빈도가 해석값 허용폭 내"), IsObservedWithinTolerance(0.75, First.DrawCounts[1], NumDraws));
	TestTrue(TEXT("미선택 정확 0회"), IsObservedWithinTolerance(0.0, First.DrawCounts[2], NumDraws));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchSpawnProbability_MonteCarloSeedAndLimits, "DevelopmentWorkbench.Analytics.T02.MonteCarloSeedAndLimits", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchSpawnProbability_MonteCarloSeedAndLimits::RunTest(const FString& Parameters)
{
	const TArray<FSpawnPoolEntry> Pool = MakeFixturePool();
	const FSpawnPoolMonteCarloResult SeedA = RunMonteCarloSimulation(Pool, 10000, 1);
	const FSpawnPoolMonteCarloResult SeedB = RunMonteCarloSimulation(Pool, 10000, 2);
	TestTrue(TEXT("두 시드 모두 성공"), SeedA.bSuccess && SeedB.bSuccess);
	TestTrue(TEXT("다른 시드는 다른 관측 횟수"), SeedA.DrawCounts != SeedB.DrawCounts);

	TestFalse(TEXT("추출 수 상한 초과는 실패"), RunMonteCarloSimulation(Pool, MaxMonteCarloDraws + 1, 1).bSuccess);
	TestFalse(TEXT("음수 추출 수는 실패"), RunMonteCarloSimulation(Pool, -1, 1).bSuccess);
	TestFalse(TEXT("검증 실패 풀은 실행하지 않음"), RunMonteCarloSimulation(TArray<FSpawnPoolEntry>{}, 10, 1).bSuccess);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchSpawnProbability_ExpectedValueAndToleranceHelpers, "DevelopmentWorkbench.Analytics.T02.ExpectedValueAndToleranceHelpers", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchSpawnProbability_ExpectedValueAndToleranceHelpers::RunTest(const FString& Parameters)
{
	// Value: A=10, B=20 => 0.25*10 + 0.75*20 = 17.5
	TArray<FSpawnPoolEntry> Entries = { { TEXT("A"), 1.0, 10.0 }, { TEXT("B"), 3.0, 20.0 }, { TEXT("미선택"), 0.0, 0.0 } };
	const FSpawnPoolCalculationResult Result = CalculateSinglePoolProbabilities(Entries, 4);
	TestTrue(TEXT("계산 성공"), Result.bSuccess);
	TestTrue(TEXT("1회 기대 Value"), FMath::IsNearlyEqual(Result.ExpectedValuePerDraw, 17.5, 1e-12));
	TestTrue(TEXT("N회 기대 Value는 선형"), FMath::IsNearlyEqual(Result.ExpectedValueInNDraws, 70.0, 1e-12));
	TestTrue(TEXT("B 기대 횟수"), FMath::IsNearlyEqual(Result.Entries[1].ExpectedCountInNDraws, 3.0, 1e-12));

	const FSpawnPoolCalculationResult ZeroDraws = CalculateSinglePoolProbabilities(Entries, 0);
	TestTrue(TEXT("N=0도 확률은 계산"), ZeroDraws.bSuccess && ZeroDraws.Entries[0].Probability == 0.25);
	TestEqual(TEXT("N=0이면 적어도 1회 확률 0"), ZeroDraws.Entries[0].ProbabilityAtLeastOnceInNDraws, 0.0);

	// p=1: 한 항목만 가중치가 있으면 항상 선택되어야 한다.
	TArray<FSpawnPoolEntry> Certain = { { TEXT("X"), 5.0 }, { TEXT("Y"), 0.0 } };
	const FSpawnPoolCalculationResult CertainResult = CalculateSinglePoolProbabilities(Certain, 3);
	TestEqual(TEXT("p=1이면 적어도 1회 확률 1"), CertainResult.Entries[0].ProbabilityAtLeastOnceInNDraws, 1.0);

	TestFalse(TEXT("p=0은 1회 관측도 불일치"), IsObservedWithinTolerance(0.0, 1, 1000));
	TestFalse(TEXT("p=1은 N 미만 관측 불일치"), IsObservedWithinTolerance(1.0, 999, 1000));
	TestFalse(TEXT("허용 폭을 벗어난 관측 불일치"), IsObservedWithinTolerance(0.25, 5000, 10000));
	TestTrue(TEXT("허용 폭은 항상 1/N 이상"), MonteCarloTolerance(0.5, 100) >= 0.01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchSpawnProbability_CsvValueColumnAndStrictNumbers, "DevelopmentWorkbench.Analytics.T02.CsvValueColumnAndStrictNumbers", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchSpawnProbability_CsvValueColumnAndStrictNumbers::RunTest(const FString& Parameters)
{
	TArray<FSpawnPoolEntry> Entries;
	TArray<FString> Errors;
	TestTrue(TEXT("Value 열 파싱"), ParseSpawnPoolCsv(TEXT("# 주석\nEntryId,Weight,Value\nA,1,10\nB,3,20.5\n"), Entries, Errors));
	TestEqual(TEXT("항목 수"), Entries.Num(), 2);
	if (Entries.Num() == 2)
	{
		TestEqual(TEXT("A Value"), Entries[0].Value, 10.0);
		TestEqual(TEXT("B Value"), Entries[1].Value, 20.5);
	}

	double Parsed = 0.0;
	TestTrue(TEXT("공백 포함 숫자 허용"), TryParseDoubleStrict(TEXT("  3.5 "), Parsed) && Parsed == 3.5);
	TestFalse(TEXT("꼬리 문자는 거부"), TryParseDoubleStrict(TEXT("12abc"), Parsed));
	TestFalse(TEXT("빈 문자열은 거부"), TryParseDoubleStrict(TEXT(""), Parsed));
	return true;
}
