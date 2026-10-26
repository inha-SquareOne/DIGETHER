#include "Misc/AutomationTest.h"
#include "Analytics/WorkbenchBalanceSimulation.h"
#include <limits>

using namespace WorkbenchAnalytics;

namespace
{
	// 합성 예제: A=1(Value 10), B=3(Value 20), 미선택=0. 1회 기대 Value = 17.5
	FBalanceSimScenarioConfig MakeConfig(int32 Seed, int32 Draws, int32 Episodes, double ValueScale = 1.0)
	{
		FBalanceSimScenarioConfig Config;
		Config.Pool = {
			FSpawnPoolEntry{ TEXT("A"), 1.0, 10.0 * ValueScale },
			FSpawnPoolEntry{ TEXT("B"), 3.0, 20.0 * ValueScale },
			FSpawnPoolEntry{ TEXT("미선택"), 0.0, 0.0 },
		};
		Config.MasterSeed = Seed;
		Config.DrawsPerEpisode = Draws;
		Config.NumEpisodes = Episodes;
		Config.ScenarioLabel = TEXT("합성");
		return Config;
	}

	bool HasIssue(const TArray<FBalanceSimIssue>& Issues, EBalanceSimIssueType Type)
	{
		return Issues.ContainsByPredicate([Type](const FBalanceSimIssue& Issue) { return Issue.Type == Type; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchBalanceSim_MeanMatchesClosedForm, "DevelopmentWorkbench.Analytics.T03.MeanMatchesClosedForm", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchBalanceSim_MeanMatchesClosedForm::RunTest(const FString& Parameters)
{
	const FBalanceSimScenarioResult Result = RunScenarioToCompletion(MakeConfig(7, 10, 20000));
	TestTrue(TEXT("실행 성공"), Result.bSuccess);
	TestEqual(TEXT("완료 에피소드"), Result.CompletedEpisodes, 20000);
	TestTrue(TEXT("닫힌 식 평균 존재"), Result.bHasAnalyticMean);
	TestTrue(TEXT("닫힌 식 평균 = 10 x 17.5"), FMath::IsNearlyEqual(Result.AnalyticMean, 175.0, 1e-9));
	TestTrue(TEXT("분산 계산됨"), Result.bHasVariance && Result.StdError > 0.0);
	// 도구 점검용 5 표준오차 허용 폭이며 게임 밸런스 기준이 아니다.
	TestTrue(TEXT("표본 평균이 닫힌 식 평균의 5 표준오차 이내"), FMath::Abs(Result.Mean - Result.AnalyticMean) <= 5.0 * Result.StdError);
	TestTrue(TEXT("분위수 순서"), Result.Min <= Result.P05 && Result.P05 <= Result.P50 && Result.P50 <= Result.P95 && Result.P95 <= Result.Max);
	TestFalse(TEXT("표본이 충분하면 소표본 경고 없음"), Result.bSampleSizeWarning);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchBalanceSim_SeedReproducibility, "DevelopmentWorkbench.Analytics.T03.SeedReproducibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchBalanceSim_SeedReproducibility::RunTest(const FString& Parameters)
{
	const FBalanceSimScenarioResult First = RunScenarioToCompletion(MakeConfig(42, 10, 200));
	const FBalanceSimScenarioResult Second = RunScenarioToCompletion(MakeConfig(42, 10, 200));
	const FBalanceSimScenarioResult Other = RunScenarioToCompletion(MakeConfig(43, 10, 200));
	TestTrue(TEXT("같은 시드는 동일한 원시 결과"), First.EpisodeTotals == Second.EpisodeTotals);
	TestTrue(TEXT("다른 시드는 다른 원시 결과"), First.EpisodeTotals != Other.EpisodeTotals);
	TestEqual(TEXT("에피소드 시드는 결정적"), DeriveEpisodeSeed(5, 9), DeriveEpisodeSeed(5, 9));
	TestNotEqual(TEXT("에피소드 인덱스가 다르면 시드가 다름"), DeriveEpisodeSeed(5, 9), DeriveEpisodeSeed(5, 10));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchBalanceSim_ChunkedAndCancel, "DevelopmentWorkbench.Analytics.T03.ChunkedAndCancel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchBalanceSim_ChunkedAndCancel::RunTest(const FString& Parameters)
{
	const FBalanceSimScenarioConfig Config = MakeConfig(11, 10, 100);
	const FBalanceSimScenarioResult OneShot = RunScenarioToCompletion(Config);

	FBalanceSimRunState State;
	TestTrue(TEXT("초기화 성공"), InitRunState(Config, State));
	int32 Ticks = 0;
	while (!State.IsFinished())
	{
		AdvanceRunState(State, 7);
		++Ticks;
	}
	TestTrue(TEXT("청크로 나눠 여러 틱에 실행"), Ticks > 1);
	TestTrue(TEXT("청크 실행은 한 번에 실행한 결과와 동일"), FinalizeRunState(State).EpisodeTotals == OneShot.EpisodeTotals);

	FBalanceSimRunState Cancelled;
	InitRunState(Config, Cancelled);
	AdvanceRunState(Cancelled, 5);
	CancelRunState(Cancelled);
	AdvanceRunState(Cancelled, 50);
	const FBalanceSimScenarioResult Partial = FinalizeRunState(Cancelled);
	TestEqual(TEXT("취소 뒤에는 더 실행하지 않음"), Partial.CompletedEpisodes, 5);
	TestTrue(TEXT("취소 표시"), Partial.bCancelled);
	TestTrue(TEXT("부분 결과는 전체 결과의 앞부분과 동일"), Partial.EpisodeTotals == TArray<double>(OneShot.EpisodeTotals.GetData(), 5));
	TestTrue(TEXT("소표본 경고"), Partial.bSampleSizeWarning);

	FBalanceSimRunState Empty;
	InitRunState(Config, Empty);
	CancelRunState(Empty);
	const FBalanceSimScenarioResult None = FinalizeRunState(Empty);
	TestEqual(TEXT("즉시 취소는 0개"), None.CompletedEpisodes, 0);
	TestFalse(TEXT("0개이면 성공이 아님"), None.bSuccess);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchBalanceSim_PairedDifference, "DevelopmentWorkbench.Analytics.T03.PairedDifference", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchBalanceSim_PairedDifference::RunTest(const FString& Parameters)
{
	const FBalanceSimScenarioResult A = RunScenarioToCompletion(MakeConfig(3, 10, 50));
	const FBalanceSimScenarioResult Same = RunScenarioToCompletion(MakeConfig(3, 10, 50));
	const FBalanceSimScenarioResult Doubled = RunScenarioToCompletion(MakeConfig(3, 10, 50, 2.0));

	const FBalanceSimPairedDiffResult Zero = ComparePairedResults(A, Same);
	TestTrue(TEXT("동일 풀 비교 성공"), Zero.bSuccess);
	TestEqual(TEXT("동일 풀 평균 차이 정확히 0"), Zero.MeanDiff, 0.0);
	TestEqual(TEXT("동일 풀 표준오차 0"), Zero.StdErrorDiff, 0.0);

	const FBalanceSimPairedDiffResult Scaled = ComparePairedResults(A, Doubled);
	TestTrue(TEXT("Value 2배 비교 성공"), Scaled.bSuccess);
	TestTrue(TEXT("같은 시드로 같은 항목이 뽑히므로 차이는 A 자체"), Scaled.Diffs == A.EpisodeTotals);
	TestTrue(TEXT("평균 차이 = A 평균"), FMath::IsNearlyEqual(Scaled.MeanDiff, A.Mean, 1e-9));

	TestFalse(TEXT("시드가 다르면 비교 거부"), ComparePairedResults(A, RunScenarioToCompletion(MakeConfig(4, 10, 50))).bSuccess);
	TestFalse(TEXT("추출 수가 다르면 비교 거부"), ComparePairedResults(A, RunScenarioToCompletion(MakeConfig(3, 11, 50))).bSuccess);
	TestFalse(TEXT("에피소드 수가 다르면 비교 거부"), ComparePairedResults(A, RunScenarioToCompletion(MakeConfig(3, 10, 49))).bSuccess);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchBalanceSim_ValidationLimits, "DevelopmentWorkbench.Analytics.T03.ValidationLimits", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchBalanceSim_ValidationLimits::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("정상 설정은 이슈 없음"), ValidateScenarioConfig(MakeConfig(1, 10, 100)).Num(), 0);
	TestTrue(TEXT("에피소드 0"), HasIssue(ValidateScenarioConfig(MakeConfig(1, 10, 0)), EBalanceSimIssueType::InvalidNumEpisodes));
	TestTrue(TEXT("에피소드 상한 초과"), HasIssue(ValidateScenarioConfig(MakeConfig(1, 1, MaxBalanceEpisodes + 1)), EBalanceSimIssueType::InvalidNumEpisodes));
	TestTrue(TEXT("추출 수 0"), HasIssue(ValidateScenarioConfig(MakeConfig(1, 0, 10)), EBalanceSimIssueType::InvalidDrawsPerEpisode));
	TestTrue(TEXT("추출 수 상한 초과"), HasIssue(ValidateScenarioConfig(MakeConfig(1, MaxBalanceDrawsPerEpisode + 1, 10)), EBalanceSimIssueType::InvalidDrawsPerEpisode));
	TestTrue(TEXT("총 작업량 상한 초과"), HasIssue(ValidateScenarioConfig(MakeConfig(1, MaxBalanceDrawsPerEpisode, MaxBalanceEpisodes)), EBalanceSimIssueType::WorkTooLarge));

	FBalanceSimScenarioConfig BadPool = MakeConfig(1, 10, 10);
	BadPool.Pool.Empty();
	TestTrue(TEXT("빈 풀"), HasIssue(ValidateScenarioConfig(BadPool), EBalanceSimIssueType::InvalidPool));

	FBalanceSimRunState State;
	TestFalse(TEXT("검증 실패 설정은 초기화하지 않음"), InitRunState(BadPool, State));
	TestFalse(TEXT("검증 실패 설정은 실행하지 않음"), RunScenarioToCompletion(BadPool).bSuccess);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchBalanceSim_OverflowAndSingleEpisode, "DevelopmentWorkbench.Analytics.T03.OverflowAndSingleEpisode", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchBalanceSim_OverflowAndSingleEpisode::RunTest(const FString& Parameters)
{
	FBalanceSimScenarioConfig Huge;
	Huge.Pool = { FSpawnPoolEntry{ TEXT("H"), 1.0, TNumericLimits<double>::Max() } };
	Huge.MasterSeed = 1;
	Huge.DrawsPerEpisode = 10;
	Huge.NumEpisodes = 5;
	TestFalse(TEXT("합이 double 범위를 넘으면 성공으로 보고하지 않음"), RunScenarioToCompletion(Huge).bSuccess);

	const FBalanceSimScenarioResult Single = RunScenarioToCompletion(MakeConfig(9, 10, 1));
	TestTrue(TEXT("에피소드 1개 실행 성공"), Single.bSuccess);
	TestFalse(TEXT("에피소드 1개는 분산을 계산하지 않음"), Single.bHasVariance);
	TestTrue(TEXT("에피소드 1개는 소표본 경고"), Single.bSampleSizeWarning);

	const FBalanceSimScenarioResult Small = RunScenarioToCompletion(MakeConfig(9, 10, 10));
	TestTrue(TEXT("30개 미만은 소표본 경고"), Small.bSampleSizeWarning);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchBalanceSim_QuantileAndExport, "DevelopmentWorkbench.Analytics.T03.QuantileAndExport", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchBalanceSim_QuantileAndExport::RunTest(const FString& Parameters)
{
	const TArray<double> Sorted = { 1.0, 2.0, 3.0, 4.0, 5.0 };
	TestEqual(TEXT("최소"), ComputeQuantileSorted(Sorted, 0.0), 1.0);
	TestEqual(TEXT("중앙값"), ComputeQuantileSorted(Sorted, 0.5), 3.0);
	TestEqual(TEXT("최대"), ComputeQuantileSorted(Sorted, 1.0), 5.0);
	TestTrue(TEXT("선형 보간"), FMath::IsNearlyEqual(ComputeQuantileSorted(Sorted, 0.125), 1.5, 1e-12));
	TestEqual(TEXT("빈 배열"), ComputeQuantileSorted(TArray<double>(), 0.5), 0.0);

	const FBalanceSimScenarioResult A = RunScenarioToCompletion(MakeConfig(3, 10, 4));
	const FBalanceSimScenarioResult B = RunScenarioToCompletion(MakeConfig(3, 10, 4, 2.0));

	const FString CsvSingle = ExportResultsToCsv(A, nullptr);
	TestTrue(TEXT("CSV에 실게임 수익 아님 표기"), CsvSingle.Contains(TEXT("실게임 수익 아님")));
	TestTrue(TEXT("CSV 단일 헤더"), CsvSingle.Contains(TEXT("EpisodeIndex,A\n")));

	const FString CsvPaired = ExportResultsToCsv(A, &B);
	TestTrue(TEXT("CSV 짝지음 헤더"), CsvPaired.Contains(TEXT("EpisodeIndex,A,B,B_minus_A")));
	TArray<FString> Lines;
	CsvPaired.ParseIntoArrayLines(Lines);
	int32 DataLines = 0;
	for (const FString& Line : Lines)
	{
		if (!Line.StartsWith(TEXT("#")) && !Line.StartsWith(TEXT("EpisodeIndex")))
		{
			++DataLines;
		}
	}
	TestEqual(TEXT("CSV 데이터 행 수 = 에피소드 수"), DataLines, 4);

	const FString Json = ExportResultsToJson(A, &B);
	TestTrue(TEXT("JSON에 시나리오 A"), Json.Contains(TEXT("scenarioA")));
	TestTrue(TEXT("JSON에 시나리오 B"), Json.Contains(TEXT("scenarioB")));
	TestTrue(TEXT("JSON에 짝지은 차이"), Json.Contains(TEXT("pairedDiff")));
	TestTrue(TEXT("JSON에 원시 결과"), Json.Contains(TEXT("episodeTotals")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchBalanceSim_StatisticsFiniteness, "DevelopmentWorkbench.Analytics.T03.StatisticsFiniteness", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchBalanceSim_StatisticsFiniteness::RunTest(const FString& Parameters)
{
	double Mean = 0.0, StdDev = 0.0, StdError = 0.0;
	bool bHasVariance = false;

	const double Big = 1e308;
	TestTrue(TEXT("각 값이 유한하면 합이 overflow해도 안정 계산으로 평균 성공"), ComputeSampleStatistics({ Big, Big, Big }, Mean, bHasVariance, StdDev, StdError));
	TestTrue(TEXT("평균은 유한하고 값과 같음"), FMath::IsFinite(Mean) && FMath::IsNearlyEqual(Mean, Big, Big * 1e-12));
	TestTrue(TEXT("분산 0도 유한"), FMath::IsFinite(StdDev) && FMath::IsFinite(StdError) && StdDev == 0.0);

	TestFalse(TEXT("표준편차가 double 범위를 넘으면 명시 실패"), ComputeSampleStatistics({ 1.7e308, -1.7e308 }, Mean, bHasVariance, StdDev, StdError));
	TestFalse(TEXT("NaN 입력 실패"), ComputeSampleStatistics({ 1.0, std::numeric_limits<double>::quiet_NaN() }, Mean, bHasVariance, StdDev, StdError));
	TestFalse(TEXT("Inf 입력 실패"), ComputeSampleStatistics({ 1.0, std::numeric_limits<double>::infinity() }, Mean, bHasVariance, StdDev, StdError));
	TestFalse(TEXT("빈 입력 실패"), ComputeSampleStatistics(TArray<double>(), Mean, bHasVariance, StdDev, StdError));

	TestTrue(TEXT("분위수 보간은 큰 값에서도 유한"), FMath::IsFinite(ComputeQuantileSorted({ -1.7e308, 1.7e308 }, 0.3)));

	// 에피소드 값이 유한하고 단일 값이면 sum이 아닌 안정 계산 덕에 성공하고 평균이 유한하다.
	FBalanceSimScenarioConfig Config;
	Config.Pool = { FSpawnPoolEntry{ TEXT("P"), 1.0, 1.0e307 } };
	Config.MasterSeed = 2;
	Config.DrawsPerEpisode = 10;
	Config.NumEpisodes = 50;
	const FBalanceSimScenarioResult Result = RunScenarioToCompletion(Config);
	TestTrue(TEXT("큰 유한 값 파이프라인 성공"), Result.bSuccess && Result.bComplete);
	TestTrue(TEXT("평균·표준오차 유한"), FMath::IsFinite(Result.Mean) && FMath::IsFinite(Result.StdError) && FMath::IsFinite(Result.P95));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchBalanceSim_PairedRejectsInconsistent, "DevelopmentWorkbench.Analytics.T03.PairedRejectsInconsistent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchBalanceSim_PairedRejectsInconsistent::RunTest(const FString& Parameters)
{
	const FBalanceSimScenarioResult A = RunScenarioToCompletion(MakeConfig(3, 10, 4));
	const FBalanceSimScenarioResult B = RunScenarioToCompletion(MakeConfig(3, 10, 4, 2.0));
	TestTrue(TEXT("기준: 정상 입력은 짝지어짐"), ComparePairedResults(A, B).bSuccess);
	TestEqual(TEXT("정상 결과는 일관성 오류 없음"), CheckResultConsistency(A).Num(), 0);

	FBalanceSimScenarioResult ShortArray = A;
	ShortArray.EpisodeTotals.Pop();
	TestTrue(TEXT("개수와 배열 길이 불일치를 감지"), CheckResultConsistency(ShortArray).Num() > 0);
	TestFalse(TEXT("길이 불일치 A는 범위 밖 접근 없이 거부"), ComparePairedResults(ShortArray, B).bSuccess);
	TestFalse(TEXT("길이 불일치 B도 거부"), ComparePairedResults(A, ShortArray).bSuccess);

	FBalanceSimScenarioResult LongArray = A;
	LongArray.EpisodeTotals.Add(1.0);
	TestFalse(TEXT("배열이 개수보다 길어도 거부"), ComparePairedResults(LongArray, B).bSuccess);

	FBalanceSimScenarioResult Cancelled = A;
	Cancelled.bCancelled = true;
	TestFalse(TEXT("취소된 결과는 짝지은 비교 거부"), ComparePairedResults(Cancelled, B).bSuccess);

	FBalanceSimScenarioResult Incomplete = A;
	Incomplete.bComplete = false;
	TestFalse(TEXT("미완결 결과는 거부"), ComparePairedResults(A, Incomplete).bSuccess);

	TestFalse(TEXT("시드가 다르면 거부"), ComparePairedResults(A, RunScenarioToCompletion(MakeConfig(4, 10, 4))).bSuccess);
	TestFalse(TEXT("추출 수가 다르면 거부"), ComparePairedResults(A, RunScenarioToCompletion(MakeConfig(3, 11, 4))).bSuccess);
	TestFalse(TEXT("에피소드 수가 다르면 거부"), ComparePairedResults(A, RunScenarioToCompletion(MakeConfig(3, 10, 5))).bSuccess);

	FBalanceSimScenarioResult NanTotal = A;
	NanTotal.EpisodeTotals[1] = std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("비유한 원시 값 거부"), ComparePairedResults(NanTotal, B).bSuccess);

	FBalanceSimScenarioResult Low = A;
	FBalanceSimScenarioResult High = B;
	Low.EpisodeTotals[0] = -1.7e308;
	High.EpisodeTotals[0] = 1.7e308;
	const FBalanceSimPairedDiffResult Overflow = ComparePairedResults(Low, High);
	TestFalse(TEXT("B-A가 overflow하면 성공으로 반환하지 않음"), Overflow.bSuccess);
	TestTrue(TEXT("오류 사유 제공"), Overflow.Errors.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchBalanceSim_ExportIdentityAndPairing, "DevelopmentWorkbench.Analytics.T03.ExportIdentityAndPairing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchBalanceSim_ExportIdentityAndPairing::RunTest(const FString& Parameters)
{
	FBalanceSimScenarioConfig ConfigA = MakeConfig(3, 10, 4);
	ConfigA.RunId = TEXT("run-synthetic-0001");
	FBalanceSimScenarioConfig ConfigB = MakeConfig(3, 10, 4, 2.0);
	ConfigB.RunId = ConfigA.RunId;
	const FBalanceSimScenarioResult A = RunScenarioToCompletion(ConfigA);
	const FBalanceSimScenarioResult B = RunScenarioToCompletion(ConfigB);
	TestEqual(TEXT("입력 지문은 32자 MD5"), A.InputFingerprint.Len(), 32);

	const FString Csv = ExportResultsToCsv(A, &B);
	TestTrue(TEXT("CSV에 runId"), Csv.Contains(TEXT("runId=run-synthetic-0001")));
	TestTrue(TEXT("CSV에 입력 지문"), Csv.Contains(A.InputFingerprint) && Csv.Contains(B.InputFingerprint));
	TestTrue(TEXT("CSV에 시드 파생 버전"), Csv.Contains(TEXT("seedDerivation=splitmix64-v1")));
	TestTrue(TEXT("CSV에 완결 상태"), Csv.Contains(TEXT("status=complete")));
	TestTrue(TEXT("CSV에 취소 여부"), Csv.Contains(TEXT("cancelled=false")));
	TestTrue(TEXT("유효한 쌍은 B-A 열 포함"), Csv.Contains(TEXT("EpisodeIndex,A,B,B_minus_A")));

	const FString Json = ExportResultsToJson(A, &B);
	TestTrue(TEXT("JSON에 runId"), Json.Contains(TEXT("\"runId\":\"run-synthetic-0001\"")));
	TestTrue(TEXT("JSON에 입력 지문"), Json.Contains(TEXT("\"inputFingerprint\":\"") + A.InputFingerprint));
	TestTrue(TEXT("JSON에 시드 파생 버전"), Json.Contains(TEXT("\"seedDerivation\":\"splitmix64-v1\"")));
	TestTrue(TEXT("JSON에 완결 상태"), Json.Contains(TEXT("\"status\":\"complete\"")));
	TestTrue(TEXT("JSON에 신뢰구간 해석 주의"), Json.Contains(TEXT("bootstrapNote")));

	// 시드가 다른 A/B는 짝지어진 차이를 만들지 않는다.
	const FBalanceSimScenarioResult OtherSeed = RunScenarioToCompletion(MakeConfig(99, 10, 4));
	const FString Unpaired = ExportResultsToCsv(A, &OtherSeed);
	TestTrue(TEXT("불일치 CSV는 not paired 표기"), Unpaired.Contains(TEXT("not paired")));
	TestFalse(TEXT("불일치 CSV는 B-A 열 없음"), Unpaired.Contains(TEXT("B_minus_A")));
	TestTrue(TEXT("불일치 CSV는 A만 출력"), Unpaired.Contains(TEXT("EpisodeIndex,A\n")));
	TestTrue(TEXT("불일치 JSON은 valid=false"), ExportResultsToJson(A, &OtherSeed).Contains(TEXT("\"valid\":false")));

	// 취소된 결과는 상태로 구분되고 짝지어지지 않는다.
	FBalanceSimRunState State;
	TestTrue(TEXT("초기화"), InitRunState(ConfigA, State));
	AdvanceRunState(State, 2);
	CancelRunState(State);
	const FBalanceSimScenarioResult Partial = FinalizeRunState(State);
	TestTrue(TEXT("취소 결과도 입력 지문 보유"), Partial.InputFingerprint == A.InputFingerprint);
	const FString PartialCsv = ExportResultsToCsv(Partial, &B);
	TestTrue(TEXT("취소 상태 표기"), PartialCsv.Contains(TEXT("status=cancelled")) && PartialCsv.Contains(TEXT("cancelled=true")));
	TestFalse(TEXT("취소 결과는 B-A 열 없음"), PartialCsv.Contains(TEXT("B_minus_A")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchBalanceSim_InputFingerprint, "DevelopmentWorkbench.Analytics.T03.InputFingerprint", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchBalanceSim_InputFingerprint::RunTest(const FString& Parameters)
{
	const FString Base = ComputeScenarioFingerprint(MakeConfig(3, 10, 4));
	TestEqual(TEXT("같은 입력은 같은 지문"), ComputeScenarioFingerprint(MakeConfig(3, 10, 4)), Base);
	TestNotEqual(TEXT("시드가 다르면 다른 지문"), ComputeScenarioFingerprint(MakeConfig(4, 10, 4)), Base);
	TestNotEqual(TEXT("추출 수가 다르면 다른 지문"), ComputeScenarioFingerprint(MakeConfig(3, 11, 4)), Base);
	TestNotEqual(TEXT("에피소드 수가 다르면 다른 지문"), ComputeScenarioFingerprint(MakeConfig(3, 10, 5)), Base);
	TestNotEqual(TEXT("풀 Value가 다르면 다른 지문"), ComputeScenarioFingerprint(MakeConfig(3, 10, 4, 2.0)), Base);

	FBalanceSimScenarioConfig WithRunId = MakeConfig(3, 10, 4);
	WithRunId.RunId = TEXT("x");
	TestEqual(TEXT("runId는 입력 지문에 포함되지 않음"), ComputeScenarioFingerprint(WithRunId), Base);

	FBalanceSimScenarioConfig Renamed = MakeConfig(3, 10, 4);
	Renamed.Pool[0].EntryId = TEXT("A2");
	TestNotEqual(TEXT("항목 ID가 다르면 다른 지문"), ComputeScenarioFingerprint(Renamed), Base);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchBalanceSim_BootstrapMeanCi, "DevelopmentWorkbench.Analytics.T03.BootstrapMeanCi", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchBalanceSim_BootstrapMeanCi::RunTest(const FString& Parameters)
{
	// 상수 표본: 구간이 상수로 붕괴한다.
	TArray<double> Constant;
	Constant.Init(5.0, 200);
	const FBalanceSimBootstrapResult ConstantCi = ComputeBootstrapMeanCi(Constant);
	TestTrue(TEXT("상수 표본 구간 계산됨"), ConstantCi.bValid);
	TestTrue(TEXT("상수 표본 구간 = 상수"), FMath::IsNearlyEqual(ConstantCi.Lower, 5.0, 1e-12) && FMath::IsNearlyEqual(ConstantCi.Upper, 5.0, 1e-12));
	TestEqual(TEXT("재표집 횟수 고정"), ConstantCi.NumResamples, BootstrapResamples);
	TestEqual(TEXT("bootstrap 시드 고정"), ConstantCi.Seed, BootstrapSeed);

	TArray<double> Zeros;
	Zeros.Init(0.0, 150);
	const FBalanceSimBootstrapResult ZeroCi = ComputeBootstrapMeanCi(Zeros);
	TestTrue(TEXT("0 표본 구간 [0,0]"), ZeroCi.bValid && ZeroCi.Lower == 0.0 && ZeroCi.Upper == 0.0);

	// 동일한 A/B: 차이가 모두 0이라 구간이 [0,0]
	const FBalanceSimScenarioResult A = RunScenarioToCompletion(MakeConfig(11, 10, 200));
	const FBalanceSimPairedDiffResult SameDiff = ComparePairedResults(A, A);
	TestTrue(TEXT("동일 A/B 짝지음 성공"), SameDiff.bSuccess);
	TestTrue(TEXT("차이 CI 자격 있음"), GetDiffBootstrapIneligibleReason(SameDiff).IsEmpty());
	const FBalanceSimBootstrapResult SameCi = ComputeBootstrapMeanCi(SameDiff.Diffs);
	TestTrue(TEXT("동일 A/B 차이 구간 [0,0]"), SameCi.bValid && SameCi.Lower == 0.0 && SameCi.Upper == 0.0);

	// 재현성과 청크 독립성
	TestTrue(TEXT("완결 N=200 평균 CI 자격 있음"), GetMeanBootstrapIneligibleReason(A).IsEmpty());
	const FBalanceSimBootstrapResult First = ComputeBootstrapMeanCi(A.EpisodeTotals);
	const FBalanceSimBootstrapResult Second = ComputeBootstrapMeanCi(A.EpisodeTotals);
	TestTrue(TEXT("변동 있는 표본은 구간이 계산됨"), First.bValid && First.Lower < First.Upper);
	TestTrue(TEXT("같은 입력은 같은 구간"), First.Lower == Second.Lower && First.Upper == Second.Upper && First.PointEstimate == Second.PointEstimate);
	TestTrue(TEXT("점추정은 표본 평균"), FMath::IsNearlyEqual(First.PointEstimate, A.Mean, FMath::Abs(A.Mean) * 1e-9));
	TestTrue(TEXT("구간이 점추정을 포함"), First.Lower <= First.PointEstimate && First.PointEstimate <= First.Upper);

	FBalanceSimBootstrapState Chunked;
	FString Reason;
	TestTrue(TEXT("청크 초기화"), InitBootstrapState(A.EpisodeTotals, Chunked, Reason));
	int32 Ticks = 0;
	while (!Chunked.IsFinished())
	{
		AdvanceBootstrapState(Chunked, 37);
		++Ticks;
	}
	TestTrue(TEXT("여러 틱으로 나뉘어 UI 스레드를 점유하지 않음"), Ticks > 1);
	const FBalanceSimBootstrapResult ChunkedCi = FinalizeBootstrapState(Chunked);
	TestTrue(TEXT("청크 실행 = 일괄 실행"), ChunkedCi.bValid && ChunkedCi.Lower == First.Lower && ChunkedCi.Upper == First.Upper);

	// 다른 데이터는 다른 구간
	const FBalanceSimScenarioResult Other = RunScenarioToCompletion(MakeConfig(12, 10, 200));
	const FBalanceSimBootstrapResult OtherCi = ComputeBootstrapMeanCi(Other.EpisodeTotals);
	TestTrue(TEXT("다른 표본은 다른 구간"), OtherCi.Lower != First.Lower || OtherCi.Upper != First.Upper);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchBalanceSim_BootstrapHidden, "DevelopmentWorkbench.Analytics.T03.BootstrapHidden", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchBalanceSim_BootstrapHidden::RunTest(const FString& Parameters)
{
	auto MakeValues = [](int32 Count)
	{
		TArray<double> Values;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Values.Add(static_cast<double>(Index % 7));
		}
		return Values;
	};

	const FBalanceSimBootstrapResult Small = ComputeBootstrapMeanCi(MakeValues(BootstrapMinEpisodes - 1));
	TestFalse(TEXT("N<100은 숨김"), Small.bValid);
	TestFalse(TEXT("숨김 사유 제공"), Small.NotComputedReason.IsEmpty());
	TestTrue(TEXT("N=100은 계산"), ComputeBootstrapMeanCi(MakeValues(BootstrapMinEpisodes)).bValid);
	TestTrue(TEXT("N=2000은 계산"), ComputeBootstrapMeanCi(MakeValues(BootstrapMaxEpisodes)).bValid);

	const FBalanceSimBootstrapResult Large = ComputeBootstrapMeanCi(MakeValues(BootstrapMaxEpisodes + 1));
	TestFalse(TEXT("N>2000은 숨김"), Large.bValid);
	TestFalse(TEXT("상한 초과 사유 제공"), Large.NotComputedReason.IsEmpty());

	TArray<double> NonFinite = MakeValues(150);
	NonFinite[3] = std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("비유한 값이 있으면 숨김"), ComputeBootstrapMeanCi(NonFinite).bValid);

	// 취소/미완결 시뮬레이션은 평균 CI 자격이 없다.
	const FBalanceSimScenarioResult Complete = RunScenarioToCompletion(MakeConfig(5, 10, 200));
	TestTrue(TEXT("완결 결과는 자격 있음"), GetMeanBootstrapIneligibleReason(Complete).IsEmpty());
	FBalanceSimScenarioResult Cancelled = Complete;
	Cancelled.bCancelled = true;
	TestFalse(TEXT("취소 결과는 자격 없음"), GetMeanBootstrapIneligibleReason(Cancelled).IsEmpty());
	FBalanceSimScenarioResult Incomplete = Complete;
	Incomplete.bComplete = false;
	TestFalse(TEXT("미완결 결과는 자격 없음"), GetMeanBootstrapIneligibleReason(Incomplete).IsEmpty());
	TestFalse(TEXT("소표본 시뮬레이션은 자격 없음"), GetMeanBootstrapIneligibleReason(RunScenarioToCompletion(MakeConfig(5, 10, 50))).IsEmpty());

	FBalanceSimRunState PartialState;
	TestTrue(TEXT("부분 실행 초기화"), InitRunState(MakeConfig(5, 10, 200), PartialState));
	AdvanceRunState(PartialState, 150);
	CancelRunState(PartialState);
	TestFalse(TEXT("취소된 부분 결과는 N>=100이어도 자격 없음"), GetMeanBootstrapIneligibleReason(FinalizeRunState(PartialState)).IsEmpty());

	// bootstrap 계산 도중 취소 또는 미완료는 구간을 표시하지 않는다.
	FBalanceSimBootstrapState Running;
	FString Reason;
	TestTrue(TEXT("bootstrap 초기화"), InitBootstrapState(MakeValues(200), Running, Reason));
	AdvanceBootstrapState(Running, 100);
	const FBalanceSimBootstrapResult Unfinished = FinalizeBootstrapState(Running);
	TestFalse(TEXT("재표집이 끝나지 않으면 숨김"), Unfinished.bValid);
	CancelBootstrapState(Running);
	AdvanceBootstrapState(Running, BootstrapResamples);
	const FBalanceSimBootstrapResult CancelledCi = FinalizeBootstrapState(Running);
	TestFalse(TEXT("취소된 bootstrap은 숨김"), CancelledCi.bValid);
	TestFalse(TEXT("취소 사유 제공"), CancelledCi.NotComputedReason.IsEmpty());
	return true;
}
