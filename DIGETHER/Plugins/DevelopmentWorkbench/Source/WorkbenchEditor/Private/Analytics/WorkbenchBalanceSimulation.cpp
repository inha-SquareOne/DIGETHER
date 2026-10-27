#include "Analytics/WorkbenchBalanceSimulation.h"

#include "Math/RandomStream.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"

namespace WorkbenchAnalytics
{
	namespace
	{
		using FWriter = TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>;

		double SampleEpisodeTotal(const FBalanceSimRunState& State, int32 EpisodeIndex)
		{
			FRandomStream Stream(DeriveEpisodeSeed(State.Config.MasterSeed, EpisodeIndex));
			double Total = 0.0;
			for (int32 Draw = 0; Draw < State.Config.DrawsPerEpisode; ++Draw)
			{
				const int32 Picked = PickWeightedIndex(State.CumulativeWeights, Stream.GetFraction());
				Total += State.Config.Pool[Picked].Value;
			}
			return Total;
		}

		// bootstrap 재표집 전용 시드 파생. 시뮬레이션 에피소드 시드와 상수를 달리해 같은 숫자 시드가 서로 상관되지 않게 한다.
		int32 DeriveBootstrapResampleSeed(int32 ResampleIndex)
		{
			uint64 Z = ((static_cast<uint64>(static_cast<uint32>(BootstrapSeed)) << 32) | static_cast<uint64>(static_cast<uint32>(ResampleIndex))) + 0xD1B54A32D192ED03ULL;
			Z = (Z ^ (Z >> 33)) * 0xFF51AFD7ED558CCDULL;
			Z = (Z ^ (Z >> 33)) * 0xC4CEB9FE1A85EC53ULL;
			Z ^= Z >> 33;
			return static_cast<int32>(Z & 0x7FFFFFFFULL);
		}

		double MaxAbs(const TArray<double>& Values)
		{
			double Scale = 0.0;
			for (double V : Values)
			{
				Scale = FMath::Max(Scale, FMath::Abs(V));
			}
			return Scale;
		}

		bool AllFinite(const TArray<double>& Values)
		{
			for (double V : Values)
			{
				if (!FMath::IsFinite(V))
				{
					return false;
				}
			}
			return true;
		}

		FString FormatNumber(double Value)
		{
			return FString::Printf(TEXT("%.17g"), Value);
		}

		FString OneLine(const FString& Text)
		{
			return Text.Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" "));
		}

		const TCHAR* StatusOf(const FBalanceSimScenarioResult& Result)
		{
			if (!Result.bSuccess)
			{
				return TEXT("failed");
			}
			if (Result.bCancelled)
			{
				return TEXT("cancelled");
			}
			return Result.bComplete ? TEXT("complete") : TEXT("incomplete");
		}

		void WriteFinite(FWriter& Writer, const TCHAR* Identifier, double Value)
		{
			if (FMath::IsFinite(Value))
			{
				Writer.WriteValue(FString(Identifier), Value);
			}
			else
			{
				Writer.WriteNull(FString(Identifier));
			}
		}

		void WriteBootstrap(FWriter& Writer, const TCHAR* Identifier, const FBalanceSimBootstrapResult& Boot)
		{
			Writer.WriteObjectStart(FString(Identifier));
			Writer.WriteValue(FString(TEXT("computed")), Boot.bValid);
			if (Boot.bValid)
			{
				Writer.WriteValue(FString(TEXT("method")), FString(TEXT("percentile bootstrap of the mean (independent episodes assumed)")));
				Writer.WriteValue(FString(TEXT("sampleCount")), Boot.SampleCount);
				Writer.WriteValue(FString(TEXT("resamples")), Boot.NumResamples);
				Writer.WriteValue(FString(TEXT("bootstrapSeed")), Boot.Seed);
				WriteFinite(Writer, TEXT("pointEstimate"), Boot.PointEstimate);
				WriteFinite(Writer, TEXT("lower2_5"), Boot.Lower);
				WriteFinite(Writer, TEXT("upper97_5"), Boot.Upper);
			}
			else
			{
				Writer.WriteValue(FString(TEXT("notComputedReason")), Boot.NotComputedReason);
			}
			Writer.WriteObjectEnd();
		}

		void WriteScenario(FWriter& Writer, const FString& Identifier, const FBalanceSimScenarioResult& Result)
		{
			Writer.WriteObjectStart(Identifier);
			Writer.WriteValue(FString(TEXT("label")), Result.Config.ScenarioLabel);
			Writer.WriteValue(FString(TEXT("runId")), Result.Config.RunId);
			Writer.WriteValue(FString(TEXT("inputFingerprint")), Result.InputFingerprint);
			Writer.WriteValue(FString(TEXT("status")), FString(StatusOf(Result)));
			Writer.WriteValue(FString(TEXT("masterSeed")), Result.Config.MasterSeed);
			Writer.WriteValue(FString(TEXT("drawsPerEpisode")), Result.Config.DrawsPerEpisode);
			Writer.WriteValue(FString(TEXT("requestedEpisodes")), Result.Config.NumEpisodes);
			Writer.WriteValue(FString(TEXT("completedEpisodes")), Result.CompletedEpisodes);
			Writer.WriteValue(FString(TEXT("cancelled")), Result.bCancelled);
			Writer.WriteValue(FString(TEXT("complete")), Result.bComplete);
			Writer.WriteArrayStart(FString(TEXT("consistencyWarnings")));
			for (const FString& Error : CheckResultConsistency(Result))
			{
				Writer.WriteValue(Error);
			}
			Writer.WriteArrayEnd();
			if (Result.bSuccess)
			{
				WriteFinite(Writer, TEXT("mean"), Result.Mean);
				if (Result.bHasVariance)
				{
					WriteFinite(Writer, TEXT("stdDev"), Result.StdDev);
					WriteFinite(Writer, TEXT("stdError"), Result.StdError);
				}
			}
			WriteBootstrap(Writer, TEXT("meanBootstrap"), Result.MeanBootstrap);
			Writer.WriteArrayStart(FString(TEXT("pool")));
			for (const FSpawnPoolEntry& Entry : Result.Config.Pool)
			{
				Writer.WriteObjectStart();
				Writer.WriteValue(FString(TEXT("id")), Entry.EntryId);
				WriteFinite(Writer, TEXT("weight"), Entry.Weight);
				WriteFinite(Writer, TEXT("value"), Entry.Value);
				Writer.WriteObjectEnd();
			}
			Writer.WriteArrayEnd();
			Writer.WriteArrayStart(FString(TEXT("episodeTotals")));
			for (double Total : Result.EpisodeTotals)
			{
				if (FMath::IsFinite(Total))
				{
					Writer.WriteValue(Total);
				}
				else
				{
					Writer.WriteValue(FString(TEXT("non-finite")));
				}
			}
			Writer.WriteArrayEnd();
			Writer.WriteObjectEnd();
		}

		FString CsvScenarioHeader(const TCHAR* Tag, const FBalanceSimScenarioResult& Result)
		{
			FString Lines = FString::Printf(TEXT("# [%s] label=%s, runId=%s, inputFingerprint=%s\n"), Tag,
				*OneLine(Result.Config.ScenarioLabel), *OneLine(Result.Config.RunId), *Result.InputFingerprint);
			Lines += FString::Printf(TEXT("# [%s] masterSeed=%d, drawsPerEpisode=%d, requestedEpisodes=%d, completedEpisodes=%d, status=%s, cancelled=%s\n"), Tag,
				Result.Config.MasterSeed, Result.Config.DrawsPerEpisode, Result.Config.NumEpisodes, Result.CompletedEpisodes, StatusOf(Result),
				Result.bCancelled ? TEXT("true") : TEXT("false"));
			if (Result.MeanBootstrap.bValid)
			{
				Lines += FString::Printf(TEXT("# [%s] meanBootstrap95CI=[%s, %s], pointEstimate=%s, resamples=%d, bootstrapSeed=%d, 독립 에피소드 가정\n"), Tag,
					*FormatNumber(Result.MeanBootstrap.Lower), *FormatNumber(Result.MeanBootstrap.Upper), *FormatNumber(Result.MeanBootstrap.PointEstimate),
					Result.MeanBootstrap.NumResamples, Result.MeanBootstrap.Seed);
			}
			else
			{
				Lines += FString::Printf(TEXT("# [%s] meanBootstrap95CI=미계산: %s\n"), Tag, *OneLine(Result.MeanBootstrap.NotComputedReason));
			}
			return Lines;
		}
	}

	const TCHAR* GetEpisodeSeedDerivationVersion()
	{
		return TEXT("splitmix64-v1");
	}

	TArray<FBalanceSimIssue> ValidateScenarioConfig(const FBalanceSimScenarioConfig& Config)
	{
		TArray<FBalanceSimIssue> Issues;
		for (const FSpawnPoolIssue& PoolIssue : ValidateSpawnPool(Config.Pool))
		{
			FBalanceSimIssue Issue;
			Issue.Type = EBalanceSimIssueType::InvalidPool;
			Issue.Message = PoolIssue.Message;
			Issues.Add(Issue);
		}

		auto AddIssue = [&Issues](EBalanceSimIssueType Type, const FString& Message)
		{
			FBalanceSimIssue Issue;
			Issue.Type = Type;
			Issue.Message = Message;
			Issues.Add(Issue);
		};

		if (Config.DrawsPerEpisode < 1 || Config.DrawsPerEpisode > MaxBalanceDrawsPerEpisode)
		{
			AddIssue(EBalanceSimIssueType::InvalidDrawsPerEpisode,
				FString::Printf(TEXT("에피소드당 추출 수는 1~%d여야 합니다(입력 %d)."), MaxBalanceDrawsPerEpisode, Config.DrawsPerEpisode));
		}
		if (Config.NumEpisodes < 1 || Config.NumEpisodes > MaxBalanceEpisodes)
		{
			AddIssue(EBalanceSimIssueType::InvalidNumEpisodes,
				FString::Printf(TEXT("에피소드 수는 1~%d여야 합니다(입력 %d)."), MaxBalanceEpisodes, Config.NumEpisodes));
		}
		if (static_cast<int64>(Config.DrawsPerEpisode) * static_cast<int64>(Config.NumEpisodes) > MaxBalanceTotalDraws)
		{
			AddIssue(EBalanceSimIssueType::WorkTooLarge,
				FString::Printf(TEXT("총 추출 수(에피소드 수 x 에피소드당 추출 수)가 상한 %lld를 넘습니다."), MaxBalanceTotalDraws));
		}
		return Issues;
	}

	int32 DeriveEpisodeSeed(int32 MasterSeed, int32 EpisodeIndex)
	{
		uint64 Z = ((static_cast<uint64>(static_cast<uint32>(MasterSeed)) << 32) | static_cast<uint64>(static_cast<uint32>(EpisodeIndex))) + 0x9E3779B97F4A7C15ULL;
		Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ULL;
		Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBULL;
		Z ^= Z >> 31;
		return static_cast<int32>(Z & 0x7FFFFFFFULL);
	}

	FString ComputeScenarioFingerprint(const FBalanceSimScenarioConfig& Config)
	{
		// 길이 접두 직렬화라 EntryId에 구분자가 들어 있어도 서로 다른 입력이 같은 문자열이 되지 않는다.
		FString Canonical = FString::Printf(TEXT("v1;seed=%d;draws=%d;episodes=%d;pool=%d;"), Config.MasterSeed, Config.DrawsPerEpisode, Config.NumEpisodes, Config.Pool.Num());
		for (const FSpawnPoolEntry& Entry : Config.Pool)
		{
			Canonical += FString::Printf(TEXT("%d:%s|%s|%s;"), Entry.EntryId.Len(), *Entry.EntryId, *FormatNumber(Entry.Weight), *FormatNumber(Entry.Value));
		}
		const FTCHARToUTF8 Utf8(*Canonical);
		return FMD5::HashBytes(reinterpret_cast<const uint8*>(Utf8.Get()), static_cast<uint64>(Utf8.Length()));
	}

	bool InitRunState(const FBalanceSimScenarioConfig& Config, FBalanceSimRunState& OutState)
	{
		if (ValidateScenarioConfig(Config).Num() > 0)
		{
			return false;
		}
		OutState = FBalanceSimRunState();
		OutState.Config = Config;
		OutState.EpisodeTotals.Reserve(Config.NumEpisodes);
		double Running = 0.0;
		OutState.CumulativeWeights.Reserve(Config.Pool.Num());
		for (const FSpawnPoolEntry& Entry : Config.Pool)
		{
			Running += Entry.Weight;
			OutState.CumulativeWeights.Add(Running);
		}
		OutState.bInitialized = true;
		return true;
	}

	void AdvanceRunState(FBalanceSimRunState& State, int32 MaxEpisodes)
	{
		if (State.IsFinished())
		{
			return;
		}
		const int32 Remaining = State.Config.NumEpisodes - State.EpisodesCompleted();
		const int32 StepCount = FMath::Min(FMath::Max(MaxEpisodes, 1), Remaining);
		for (int32 Step = 0; Step < StepCount; ++Step)
		{
			const double Total = SampleEpisodeTotal(State, State.EpisodesCompleted());
			if (!FMath::IsFinite(Total))
			{
				State.bNonFiniteTotal = true;
				return;
			}
			State.EpisodeTotals.Add(Total);
		}
	}

	void CancelRunState(FBalanceSimRunState& State)
	{
		State.bCancelled = true;
	}

	bool ComputeSampleStatistics(const TArray<double>& Values, double& OutMean, bool& bOutHasVariance, double& OutStdDev, double& OutStdError)
	{
		OutMean = 0.0;
		bOutHasVariance = false;
		OutStdDev = 0.0;
		OutStdError = 0.0;

		const int32 N = Values.Num();
		if (N == 0 || !AllFinite(Values))
		{
			return false;
		}
		const double Scale = MaxAbs(Values);
		if (Scale == 0.0)
		{
			bOutHasVariance = N > 1;
			return true;
		}

		double SumScaled = 0.0;
		for (double V : Values)
		{
			SumScaled += V / Scale;
		}
		const double MeanScaled = SumScaled / N;
		const double Mean = MeanScaled * Scale;

		double StdDev = 0.0;
		double StdError = 0.0;
		if (N > 1)
		{
			double SumSq = 0.0;
			for (double V : Values)
			{
				const double D = V / Scale - MeanScaled;
				SumSq += D * D;
			}
			const double StdDevScaled = FMath::Sqrt(SumSq / (N - 1));
			StdDev = StdDevScaled * Scale;
			StdError = StdDevScaled / FMath::Sqrt(static_cast<double>(N)) * Scale;
		}
		if (!FMath::IsFinite(Mean) || !FMath::IsFinite(StdDev) || !FMath::IsFinite(StdError))
		{
			return false;
		}
		OutMean = Mean;
		bOutHasVariance = N > 1;
		OutStdDev = StdDev;
		OutStdError = StdError;
		return true;
	}

	double ComputeQuantileSorted(const TArray<double>& Sorted, double Q)
	{
		const int32 N = Sorted.Num();
		if (N == 0)
		{
			return 0.0;
		}
		const double Position = FMath::Clamp(Q, 0.0, 1.0) * (N - 1);
		const int32 Lower = FMath::Clamp(FMath::FloorToInt(Position), 0, N - 1);
		const int32 Upper = FMath::Min(Lower + 1, N - 1);
		const double Fraction = Position - Lower;
		if (Fraction <= 0.0 || Upper == Lower)
		{
			return Sorted[Lower];
		}
		return Sorted[Lower] * (1.0 - Fraction) + Sorted[Upper] * Fraction;
	}

	TArray<FString> CheckResultConsistency(const FBalanceSimScenarioResult& Result)
	{
		TArray<FString> Errors;
		if (Result.CompletedEpisodes < 0 || Result.CompletedEpisodes != Result.EpisodeTotals.Num())
		{
			Errors.Add(FString::Printf(TEXT("완료 에피소드 수(%d)와 원시 결과 배열 길이(%d)가 다릅니다."), Result.CompletedEpisodes, Result.EpisodeTotals.Num()));
		}
		if (Result.EpisodeTotals.Num() > Result.Config.NumEpisodes)
		{
			Errors.Add(FString::Printf(TEXT("원시 결과 %d개가 목표 에피소드 수 %d를 넘습니다."), Result.EpisodeTotals.Num(), Result.Config.NumEpisodes));
		}
		if (!AllFinite(Result.EpisodeTotals))
		{
			Errors.Add(TEXT("원시 결과에 유한하지 않은 값이 있습니다."));
		}
		if (Result.bComplete && (Result.bCancelled || Result.EpisodeTotals.Num() != Result.Config.NumEpisodes))
		{
			Errors.Add(TEXT("완결로 표시됐지만 취소되었거나 목표 에피소드 수만큼 완료되지 않았습니다."));
		}
		return Errors;
	}

	FString GetBootstrapIneligibleReason(const TArray<double>& Values)
	{
		const int32 N = Values.Num();
		if (N < BootstrapMinEpisodes)
		{
			return FString::Printf(TEXT("표본 %d개는 최소 %d개보다 적어 신뢰구간을 계산하지 않고 숨깁니다."), N, BootstrapMinEpisodes);
		}
		if (N > BootstrapMaxEpisodes)
		{
			return FString::Printf(TEXT("표본 %d개는 UI 작업량 상한(%d개 x 재표집 %d회)을 넘어 신뢰구간을 계산하지 않았습니다. 평균·표준오차만 참고하세요."), N, BootstrapMaxEpisodes, BootstrapResamples);
		}
		if (!AllFinite(Values))
		{
			return TEXT("유한하지 않은 값이 있어 신뢰구간을 계산하지 않습니다.");
		}
		return FString();
	}

	FString GetMeanBootstrapIneligibleReason(const FBalanceSimScenarioResult& Result)
	{
		if (!Result.bSuccess)
		{
			return TEXT("실행이 성공하지 않아 신뢰구간을 계산하지 않습니다.");
		}
		if (Result.bCancelled)
		{
			return TEXT("실행이 취소되어 완결된 표본이 아니므로 신뢰구간을 숨깁니다.");
		}
		if (!Result.bComplete)
		{
			return TEXT("목표 에피소드를 모두 완료하지 않아 신뢰구간을 숨깁니다.");
		}
		const TArray<FString> Errors = CheckResultConsistency(Result);
		if (Errors.Num() > 0)
		{
			return TEXT("결과 일관성 검사 실패: ") + Errors[0];
		}
		return GetBootstrapIneligibleReason(Result.EpisodeTotals);
	}

	bool InitBootstrapState(const TArray<double>& Values, FBalanceSimBootstrapState& OutState, FString& OutReason)
	{
		OutState = FBalanceSimBootstrapState();
		OutReason = GetBootstrapIneligibleReason(Values);
		if (!OutReason.IsEmpty())
		{
			return false;
		}
		OutState.Scale = MaxAbs(Values);
		OutState.ScaledValues.Reserve(Values.Num());
		for (double V : Values)
		{
			OutState.ScaledValues.Add(OutState.Scale > 0.0 ? V / OutState.Scale : 0.0);
		}
		OutState.TotalResamples = BootstrapResamples;
		OutState.ResampleMeans.Reserve(BootstrapResamples);
		OutState.bInitialized = true;
		return true;
	}

	void AdvanceBootstrapState(FBalanceSimBootstrapState& State, int32 MaxResamples)
	{
		if (State.IsFinished())
		{
			return;
		}
		const int32 N = State.ScaledValues.Num();
		const int32 StepCount = FMath::Min(FMath::Max(MaxResamples, 1), State.TotalResamples - State.ResampleMeans.Num());
		for (int32 Step = 0; Step < StepCount; ++Step)
		{
			const FRandomStream Stream(DeriveBootstrapResampleSeed(State.ResampleMeans.Num()));
			double Sum = 0.0;
			for (int32 Draw = 0; Draw < N; ++Draw)
			{
				Sum += State.ScaledValues[FMath::Clamp(Stream.RandHelper(N), 0, N - 1)];
			}
			State.ResampleMeans.Add(Sum / N * State.Scale);
		}
	}

	void CancelBootstrapState(FBalanceSimBootstrapState& State)
	{
		State.bCancelled = true;
	}

	FBalanceSimBootstrapResult FinalizeBootstrapState(const FBalanceSimBootstrapState& State)
	{
		FBalanceSimBootstrapResult Result;
		if (!State.bInitialized)
		{
			Result.NotComputedReason = TEXT("신뢰구간 계산이 초기화되지 않았습니다.");
			return Result;
		}
		Result.SampleCount = State.ScaledValues.Num();
		if (State.bCancelled || State.ResampleMeans.Num() < State.TotalResamples)
		{
			Result.NotComputedReason = FString::Printf(TEXT("신뢰구간 계산이 완료되지 않았습니다(재표집 %d / %d%s). 구간을 표시하지 않습니다."),
				State.ResampleMeans.Num(), State.TotalResamples, State.bCancelled ? TEXT(", 취소됨") : TEXT(""));
			return Result;
		}

		double SumScaled = 0.0;
		for (double V : State.ScaledValues)
		{
			SumScaled += V;
		}
		Result.PointEstimate = SumScaled / Result.SampleCount * State.Scale;

		TArray<double> Sorted = State.ResampleMeans;
		Sorted.Sort();
		Result.Lower = ComputeQuantileSorted(Sorted, BootstrapLowerQuantile);
		Result.Upper = ComputeQuantileSorted(Sorted, BootstrapUpperQuantile);
		Result.NumResamples = State.TotalResamples;
		if (!FMath::IsFinite(Result.PointEstimate) || !FMath::IsFinite(Result.Lower) || !FMath::IsFinite(Result.Upper))
		{
			Result.NotComputedReason = TEXT("신뢰구간 계산 결과가 유한하지 않아 표시하지 않습니다.");
			return Result;
		}
		Result.bValid = true;
		return Result;
	}

	FBalanceSimBootstrapResult ComputeBootstrapMeanCi(const TArray<double>& Values)
	{
		FBalanceSimBootstrapState State;
		FString Reason;
		if (!InitBootstrapState(Values, State, Reason))
		{
			FBalanceSimBootstrapResult Hidden;
			Hidden.SampleCount = Values.Num();
			Hidden.NotComputedReason = Reason;
			return Hidden;
		}
		AdvanceBootstrapState(State, State.TotalResamples);
		return FinalizeBootstrapState(State);
	}

	FBalanceSimScenarioResult FinalizeRunState(const FBalanceSimRunState& State)
	{
		FBalanceSimScenarioResult Result;
		Result.Config = State.Config;
		Result.InputFingerprint = ComputeScenarioFingerprint(State.Config);
		Result.CompletedEpisodes = State.EpisodesCompleted();
		Result.bCancelled = State.bCancelled;
		Result.EpisodeTotals = State.EpisodeTotals;
		Result.MeanBootstrap.NotComputedReason = TEXT("신뢰구간을 아직 계산하지 않았습니다.");

		auto AddIssue = [&Result](EBalanceSimIssueType Type, const TCHAR* Message)
		{
			FBalanceSimIssue Issue;
			Issue.Type = Type;
			Issue.Message = Message;
			Result.Issues.Add(Issue);
		};

		if (State.bNonFiniteTotal)
		{
			AddIssue(EBalanceSimIssueType::NonFiniteEpisodeTotal, TEXT("에피소드 합이 double 범위를 넘어 중단했습니다. Value 또는 추출 수를 줄이세요. 완료분만 원시 결과에 남깁니다."));
		}
		if (Result.CompletedEpisodes == 0)
		{
			AddIssue(EBalanceSimIssueType::InvalidNumEpisodes, TEXT("완료된 에피소드가 없어 통계를 계산할 수 없습니다."));
			return Result;
		}

		if (!ComputeSampleStatistics(State.EpisodeTotals, Result.Mean, Result.bHasVariance, Result.StdDev, Result.StdError))
		{
			AddIssue(EBalanceSimIssueType::NonFiniteStatistic, TEXT("평균·표준편차·표준오차가 double 범위를 넘어 유한하게 계산되지 않았습니다. Value 크기를 줄이세요."));
			return Result;
		}

		TArray<double> Sorted = State.EpisodeTotals;
		Sorted.Sort();
		Result.Min = Sorted[0];
		Result.Max = Sorted.Last();
		Result.P05 = ComputeQuantileSorted(Sorted, 0.05);
		Result.P50 = ComputeQuantileSorted(Sorted, 0.50);
		Result.P95 = ComputeQuantileSorted(Sorted, 0.95);

		const FSpawnPoolCalculationResult Analytic = CalculateSinglePoolProbabilities(State.Config.Pool, 0);
		if (Analytic.bSuccess)
		{
			const double AnalyticMean = Analytic.ExpectedValuePerDraw * static_cast<double>(State.Config.DrawsPerEpisode);
			if (FMath::IsFinite(AnalyticMean))
			{
				Result.bHasAnalyticMean = true;
				Result.AnalyticMean = AnalyticMean;
			}
		}

		if (Result.CompletedEpisodes < 2)
		{
			Result.bSampleSizeWarning = true;
			Result.SampleSizeNote = TEXT("완료 에피소드가 1개여서 분산·표준오차를 계산하지 않습니다. 평균과 분위수는 의미가 없습니다.");
		}
		else if (Result.CompletedEpisodes < 30)
		{
			Result.bSampleSizeWarning = true;
			Result.SampleSizeNote = FString::Printf(TEXT("완료 에피소드 %d개는 표본이 부족합니다. 평균·분위수·표준오차가 불안정합니다."), Result.CompletedEpisodes);
		}
		else
		{
			Result.SampleSizeNote = FString::Printf(TEXT("완료 에피소드 %d개. 표준오차는 에피소드가 독립이라는 가정의 값입니다."), Result.CompletedEpisodes);
		}

		Result.bSuccess = !State.bNonFiniteTotal;
		Result.bComplete = Result.bSuccess && !State.bCancelled && Result.CompletedEpisodes == State.Config.NumEpisodes;
		return Result;
	}

	FBalanceSimScenarioResult RunScenarioToCompletion(const FBalanceSimScenarioConfig& Config)
	{
		FBalanceSimRunState State;
		if (!InitRunState(Config, State))
		{
			FBalanceSimScenarioResult Failed;
			Failed.Config = Config;
			Failed.InputFingerprint = ComputeScenarioFingerprint(Config);
			Failed.Issues = ValidateScenarioConfig(Config);
			return Failed;
		}
		AdvanceRunState(State, Config.NumEpisodes);
		return FinalizeRunState(State);
	}

	FBalanceSimPairedDiffResult ComparePairedResults(const FBalanceSimScenarioResult& ResultA, const FBalanceSimScenarioResult& ResultB)
	{
		FBalanceSimPairedDiffResult Diff;

		auto CheckSide = [&Diff](const TCHAR* Tag, const FBalanceSimScenarioResult& Side)
		{
			if (!Side.bSuccess)
			{
				Diff.Errors.Add(FString::Printf(TEXT("시나리오 %s가 성공하지 않아 비교할 수 없습니다."), Tag));
				return;
			}
			if (Side.bCancelled)
			{
				Diff.Errors.Add(FString::Printf(TEXT("시나리오 %s가 취소되어 부분 결과이므로 짝지은 비교를 하지 않습니다."), Tag));
			}
			else if (!Side.bComplete)
			{
				Diff.Errors.Add(FString::Printf(TEXT("시나리오 %s가 목표 에피소드를 모두 완료하지 않았습니다(%d / %d)."), Tag, Side.CompletedEpisodes, Side.Config.NumEpisodes));
			}
			for (const FString& Error : CheckResultConsistency(Side))
			{
				Diff.Errors.Add(FString::Printf(TEXT("시나리오 %s 결과 불일치: %s"), Tag, *Error));
			}
		};
		CheckSide(TEXT("A"), ResultA);
		CheckSide(TEXT("B"), ResultB);

		if (ResultA.Config.MasterSeed != ResultB.Config.MasterSeed)
		{
			Diff.Errors.Add(FString::Printf(TEXT("마스터 시드가 다릅니다(A=%d, B=%d). 같은 시드여야 에피소드가 짝지어집니다."), ResultA.Config.MasterSeed, ResultB.Config.MasterSeed));
		}
		if (ResultA.Config.DrawsPerEpisode != ResultB.Config.DrawsPerEpisode)
		{
			Diff.Errors.Add(FString::Printf(TEXT("에피소드당 추출 수가 다릅니다(A=%d, B=%d)."), ResultA.Config.DrawsPerEpisode, ResultB.Config.DrawsPerEpisode));
		}
		if (ResultA.Config.NumEpisodes != ResultB.Config.NumEpisodes)
		{
			Diff.Errors.Add(FString::Printf(TEXT("목표 에피소드 수가 다릅니다(A=%d, B=%d)."), ResultA.Config.NumEpisodes, ResultB.Config.NumEpisodes));
		}
		if (ResultA.CompletedEpisodes != ResultB.CompletedEpisodes)
		{
			Diff.Errors.Add(FString::Printf(TEXT("완료 에피소드 수가 다릅니다(A=%d, B=%d)."), ResultA.CompletedEpisodes, ResultB.CompletedEpisodes));
		}
		if (Diff.Errors.Num() == 0 && ResultA.EpisodeTotals.Num() == 0)
		{
			Diff.Errors.Add(TEXT("완료된 에피소드가 없습니다."));
		}
		if (Diff.Errors.Num() > 0)
		{
			return Diff;
		}

		const int32 N = ResultA.EpisodeTotals.Num();
		Diff.Diffs.Reserve(N);
		for (int32 Index = 0; Index < N; ++Index)
		{
			const double D = ResultB.EpisodeTotals[Index] - ResultA.EpisodeTotals[Index];
			if (!FMath::IsFinite(D))
			{
				Diff.Errors.Add(FString::Printf(TEXT("에피소드 %d의 B-A가 double 범위를 넘어 유한하지 않습니다."), Index));
				Diff.Diffs.Reset();
				return Diff;
			}
			Diff.Diffs.Add(D);
		}
		double StdDev = 0.0;
		if (!ComputeSampleStatistics(Diff.Diffs, Diff.MeanDiff, Diff.bHasVariance, StdDev, Diff.StdErrorDiff))
		{
			Diff.Errors.Add(TEXT("차이의 평균·표준오차가 유한하게 계산되지 않았습니다."));
			Diff.Diffs.Reset();
			return Diff;
		}
		Diff.PairedEpisodes = N;
		Diff.bSuccess = true;
		return Diff;
	}

	FString GetDiffBootstrapIneligibleReason(const FBalanceSimPairedDiffResult& Diff)
	{
		if (!Diff.bSuccess)
		{
			return TEXT("짝지은 차이가 유효하지 않아 신뢰구간을 계산하지 않습니다.");
		}
		if (Diff.Diffs.Num() != Diff.PairedEpisodes)
		{
			return TEXT("차이 배열 길이와 짝 수가 달라 신뢰구간을 계산하지 않습니다.");
		}
		return GetBootstrapIneligibleReason(Diff.Diffs);
	}

	FString ExportResultsToCsv(const FBalanceSimScenarioResult& ResultA, const FBalanceSimScenarioResult* ResultB, const FBalanceSimBootstrapResult* DiffCi)
	{
		FBalanceSimPairedDiffResult Diff;
		if (ResultB)
		{
			Diff = ComparePairedResults(ResultA, *ResultB);
		}
		const bool bPaired = ResultB && Diff.bSuccess;

		FString Csv = TEXT("# 합성 잠재가치 분석 원시 결과 - 실게임 수익 아님 (실게임 provider 미연결)\n");
		Csv += FString::Printf(TEXT("# seedDerivation=%s\n"), GetEpisodeSeedDerivationVersion());
		Csv += CsvScenarioHeader(TEXT("A"), ResultA);
		for (const FString& Error : CheckResultConsistency(ResultA))
		{
			Csv += TEXT("# [A] 경고 - 결과 불일치: ") + OneLine(Error) + TEXT("\n");
		}
		if (ResultB)
		{
			Csv += CsvScenarioHeader(TEXT("B"), *ResultB);
			if (bPaired)
			{
				Csv += FString::Printf(TEXT("# paired: 같은 시드의 같은 에피소드 %d쌍. B_minus_A 평균=%s\n"), Diff.PairedEpisodes, *FormatNumber(Diff.MeanDiff));
				if (DiffCi && DiffCi->bValid)
				{
					Csv += FString::Printf(TEXT("# [diff] bootstrap95CI=[%s, %s], pointEstimate=%s, resamples=%d, bootstrapSeed=%d, 독립 에피소드 가정\n"),
						*FormatNumber(DiffCi->Lower), *FormatNumber(DiffCi->Upper), *FormatNumber(DiffCi->PointEstimate), DiffCi->NumResamples, DiffCi->Seed);
				}
				else
				{
					Csv += FString::Printf(TEXT("# [diff] bootstrap95CI=미계산: %s\n"), DiffCi ? *OneLine(DiffCi->NotComputedReason) : TEXT("계산하지 않음"));
				}
			}
			else
			{
				Csv += TEXT("# not paired: 짝지은 차이를 계산하지 않았고 B의 원시 결과는 이 파일에 넣지 않았습니다.\n");
				for (const FString& Error : Diff.Errors)
				{
					Csv += TEXT("# not paired reason: ") + OneLine(Error) + TEXT("\n");
				}
			}
		}
		Csv += bPaired ? TEXT("EpisodeIndex,A,B,B_minus_A\n") : TEXT("EpisodeIndex,A\n");
		for (int32 Index = 0; Index < ResultA.EpisodeTotals.Num(); ++Index)
		{
			Csv += FString::Printf(TEXT("%d,%s"), Index, *FormatNumber(ResultA.EpisodeTotals[Index]));
			if (bPaired)
			{
				Csv += FString::Printf(TEXT(",%s,%s"), *FormatNumber(ResultB->EpisodeTotals[Index]), *FormatNumber(Diff.Diffs[Index]));
			}
			Csv += TEXT("\n");
		}
		return Csv;
	}

	FString ExportResultsToJson(const FBalanceSimScenarioResult& ResultA, const FBalanceSimScenarioResult* ResultB, const FBalanceSimBootstrapResult* DiffCi)
	{
		FString Out;
		TSharedRef<FWriter> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		Writer->WriteObjectStart();
		Writer->WriteValue(FString(TEXT("note")), FString(TEXT("합성 잠재가치 분석 원시 결과 - 실게임 수익 아님. 실게임 provider 미연결.")));
		Writer->WriteValue(FString(TEXT("seedDerivation")), FString(GetEpisodeSeedDerivationVersion()));
		Writer->WriteValue(FString(TEXT("bootstrapNote")), FString(TEXT("평균 신뢰구간은 에피소드 합의 평균에 대한 구간이며 결과 분포의 범위(분위수·최소·최대)와 다릅니다. 관측되지 않은 희귀 꼬리는 반영하지 않습니다.")));
		WriteScenario(*Writer, FString(TEXT("scenarioA")), ResultA);
		if (ResultB)
		{
			WriteScenario(*Writer, FString(TEXT("scenarioB")), *ResultB);
			const FBalanceSimPairedDiffResult Diff = ComparePairedResults(ResultA, *ResultB);
			Writer->WriteObjectStart(FString(TEXT("pairedDiff")));
			Writer->WriteValue(FString(TEXT("valid")), Diff.bSuccess);
			if (Diff.bSuccess)
			{
				Writer->WriteValue(FString(TEXT("pairedEpisodes")), Diff.PairedEpisodes);
				WriteFinite(*Writer, TEXT("meanDiff"), Diff.MeanDiff);
				if (Diff.bHasVariance)
				{
					WriteFinite(*Writer, TEXT("stdErrorDiff"), Diff.StdErrorDiff);
				}
				if (DiffCi)
				{
					WriteBootstrap(*Writer, TEXT("diffBootstrap"), *DiffCi);
				}
			}
			else
			{
				Writer->WriteArrayStart(FString(TEXT("errors")));
				for (const FString& Error : Diff.Errors)
				{
					Writer->WriteValue(Error);
				}
				Writer->WriteArrayEnd();
			}
			Writer->WriteObjectEnd();
		}
		Writer->WriteObjectEnd();
		Writer->Close();
		return Out;
	}
}
