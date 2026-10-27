// T03 합성 잠재가치 분석. 입력한 풀에서 가중 복원 추출한 Value의 합을 에피소드 단위로 반복한다.
// 실게임 수익이 아니며 실게임 규칙·가격·승인 계산 함수(provider)와 연결되어 있지 않다.
#pragma once

#include "CoreMinimal.h"
#include "Analytics/WorkbenchSpawnProbability.h"

namespace WorkbenchAnalytics
{
	constexpr int32 MaxBalanceEpisodes = 200000;
	constexpr int32 MaxBalanceDrawsPerEpisode = 10000;
	constexpr int64 MaxBalanceTotalDraws = 20000000;

	/** 에피소드 시드 파생 방식 버전. DeriveEpisodeSeed 알고리즘이 바뀌면 올린다. 내보내기에 기록된다. */
	WORKBENCHEDITOR_API const TCHAR* GetEpisodeSeedDerivationVersion();

	struct WORKBENCHEDITOR_API FBalanceSimScenarioConfig
	{
		TArray<FSpawnPoolEntry> Pool;
		int32 DrawsPerEpisode = 1; // 에피소드당 복원 추출 횟수
		int32 NumEpisodes = 0; // 목표 에피소드 수
		int32 MasterSeed = 0; // A/B 짝지음 기준. 에피소드 i의 난수는 (MasterSeed, i)에서만 정해진다.
		FString ScenarioLabel;
		FString RunId; // 실행 식별자. 호출자(UI)가 실행마다 부여한다. 비어 있어도 계산에는 영향이 없다.
	};

	enum class EBalanceSimIssueType : uint8
	{
		InvalidPool,
		InvalidDrawsPerEpisode,
		InvalidNumEpisodes,
		WorkTooLarge,
		NonFiniteEpisodeTotal,
		NonFiniteStatistic,
		InconsistentResult,
	};

	struct WORKBENCHEDITOR_API FBalanceSimIssue
	{
		EBalanceSimIssueType Type = EBalanceSimIssueType::InvalidPool;
		FString Message;
	};

	/** 풀(T02 검증 공유)·추출 수·에피소드 수·총 작업량 상한을 검증한다. 실패하면 실행을 시작하지 않는다. */
	WORKBENCHEDITOR_API TArray<FBalanceSimIssue> ValidateScenarioConfig(const FBalanceSimScenarioConfig& Config);

	/** 에피소드 인덱스별 독립 시드. 청크 크기나 실행 순서와 무관하게 같은 (MasterSeed, Index)는 같은 값이다. */
	WORKBENCHEDITOR_API int32 DeriveEpisodeSeed(int32 MasterSeed, int32 EpisodeIndex);

	/**
	 * 입력 식별 문자열(풀·시드·추출 수·에피소드 수의 정규화 직렬화)의 MD5. 라벨과 RunId는 포함하지 않는다.
	 * 같은 입력이면 재시작 후에도 같은 값이다.
	 */
	WORKBENCHEDITOR_API FString ComputeScenarioFingerprint(const FBalanceSimScenarioConfig& Config);

	/** 단계적 실행 상태. UI는 틱마다 AdvanceRunState를 호출하고 취소는 CancelRunState로 한다. */
	struct WORKBENCHEDITOR_API FBalanceSimRunState
	{
		FBalanceSimScenarioConfig Config;
		TArray<double> CumulativeWeights;
		TArray<double> EpisodeTotals; // 완료된 에피소드별 Value 합
		bool bCancelled = false;
		bool bNonFiniteTotal = false;
		bool bInitialized = false;

		int32 EpisodesCompleted() const { return EpisodeTotals.Num(); }
		bool IsFinished() const { return !bInitialized || bCancelled || bNonFiniteTotal || EpisodeTotals.Num() >= Config.NumEpisodes; }
	};

	/** 검증을 통과한 Config로 상태를 초기화한다. 검증 실패 시 false이며 상태는 초기화되지 않는다. */
	WORKBENCHEDITOR_API bool InitRunState(const FBalanceSimScenarioConfig& Config, FBalanceSimRunState& OutState);

	/** 최대 MaxEpisodes개 에피소드를 추가로 실행한다. 취소·종료·비유한 합 발생 상태에서는 아무것도 하지 않는다. */
	WORKBENCHEDITOR_API void AdvanceRunState(FBalanceSimRunState& State, int32 MaxEpisodes);

	WORKBENCHEDITOR_API void CancelRunState(FBalanceSimRunState& State);

	/**
	 * 평균·표준편차·표준오차. 값을 최대 절댓값으로 나눠 계산하므로 각 값이 유한하면 합·제곱합이 넘치지 않는다.
	 * 결과(평균, 표준편차, 표준오차)가 하나라도 유한하지 않거나 입력에 비유한 값이 있으면 false를 반환하고 출력은 0이다.
	 * 값이 1개이면 bOutHasVariance=false, 표준편차·표준오차 0.
	 */
	WORKBENCHEDITOR_API bool ComputeSampleStatistics(const TArray<double>& Values, double& OutMean, bool& bOutHasVariance, double& OutStdDev, double& OutStdError);

	// ---- bootstrap 평균 신뢰구간(합성 범위) ----
	constexpr int32 BootstrapMinEpisodes = 100; // 이보다 작은 N에서는 CI를 숨긴다.
	constexpr int32 BootstrapMaxEpisodes = 2000; // 작업량 상한(N x 재표집 수). 이보다 큰 N은 CI를 계산하지 않는다.
	constexpr int32 BootstrapResamples = 10000;
	constexpr int32 BootstrapSeed = 20261027; // 시뮬레이션 시드와 무관한 고정 시드
	constexpr double BootstrapLowerQuantile = 0.025;
	constexpr double BootstrapUpperQuantile = 0.975;

	/** 단계적 bootstrap 상태. 재표집 r의 난수는 (BootstrapSeed, r)에서만 정해지므로 청크 크기와 무관하게 재현된다. */
	struct WORKBENCHEDITOR_API FBalanceSimBootstrapState
	{
		TArray<double> ScaledValues; // 원값 / Scale
		double Scale = 0.0;
		TArray<double> ResampleMeans; // 원래 단위로 환산한 재표집 평균
		int32 TotalResamples = 0;
		bool bCancelled = false;
		bool bInitialized = false;

		bool IsFinished() const { return !bInitialized || bCancelled || ResampleMeans.Num() >= TotalResamples; }
	};

	struct WORKBENCHEDITOR_API FBalanceSimBootstrapResult
	{
		bool bValid = false; // 완결 실행 후 구간이 계산된 경우만 true
		FString NotComputedReason; // bValid=false일 때 사유(한국어)
		int32 SampleCount = 0;
		int32 NumResamples = 0;
		int32 Seed = BootstrapSeed;
		double PointEstimate = 0.0; // 표본 평균
		double Lower = 0.0; // 2.5% 분위수
		double Upper = 0.0; // 97.5% 분위수
	};

	struct FBalanceSimScenarioResult;

	/**
	 * 값 배열이 bootstrap 대상인지 확인한다. 가능하면 빈 문자열, 아니면 사유를 돌려준다.
	 * 조건: 표본 수 BootstrapMinEpisodes~BootstrapMaxEpisodes, 모든 값 유한.
	 */
	WORKBENCHEDITOR_API FString GetBootstrapIneligibleReason(const TArray<double>& Values);

	/** 시나리오 결과가 평균 CI 대상인지 확인한다. 완결(취소·중단 아님)·성공·배열 일관성까지 함께 요구한다. */
	WORKBENCHEDITOR_API FString GetMeanBootstrapIneligibleReason(const FBalanceSimScenarioResult& Result);

	WORKBENCHEDITOR_API bool InitBootstrapState(const TArray<double>& Values, FBalanceSimBootstrapState& OutState, FString& OutReason);
	WORKBENCHEDITOR_API void AdvanceBootstrapState(FBalanceSimBootstrapState& State, int32 MaxResamples);
	WORKBENCHEDITOR_API void CancelBootstrapState(FBalanceSimBootstrapState& State);
	/** 모든 재표집이 끝났고 취소되지 않은 경우에만 bValid=true. 취소·미완결이면 사유와 함께 구간을 숨긴다. */
	WORKBENCHEDITOR_API FBalanceSimBootstrapResult FinalizeBootstrapState(const FBalanceSimBootstrapState& State);
	/** 동기 계산(테스트·소규모용). 대상이 아니면 사유가 담긴 bValid=false 결과를 반환한다. */
	WORKBENCHEDITOR_API FBalanceSimBootstrapResult ComputeBootstrapMeanCi(const TArray<double>& Values);

	struct WORKBENCHEDITOR_API FBalanceSimScenarioResult
	{
		bool bSuccess = false;
		TArray<FBalanceSimIssue> Issues;
		FBalanceSimScenarioConfig Config;
		FString InputFingerprint; // ComputeScenarioFingerprint(Config)
		int32 CompletedEpisodes = 0;
		bool bCancelled = false;
		bool bComplete = false; // 목표 에피소드를 전부 완료했고 취소·중단이 없고 통계가 유한함
		double Mean = 0.0;
		bool bHasVariance = false; // 완료 에피소드가 2개 이상일 때만 true
		double StdDev = 0.0;
		double StdError = 0.0;
		double Min = 0.0;
		double Max = 0.0;
		double P05 = 0.0;
		double P50 = 0.0;
		double P95 = 0.0;
		bool bHasAnalyticMean = false;
		double AnalyticMean = 0.0; // DrawsPerEpisode * sum(p_i * Value_i). 표본 평균과의 대조용 닫힌 식. 비유한이면 없음.
		bool bSampleSizeWarning = false; // 완료 에피소드 < 30
		FString SampleSizeNote;
		TArray<double> EpisodeTotals; // 원시 결과
		FBalanceSimBootstrapResult MeanBootstrap; // 호출자가 계산해 채운다. 기본은 미계산.
	};

	/** 현재까지 완료된 에피소드만으로 통계를 집계한다(취소·미완료 상태에서도 호출 가능). 통계가 비유한이면 bSuccess=false. */
	WORKBENCHEDITOR_API FBalanceSimScenarioResult FinalizeRunState(const FBalanceSimRunState& State);

	/** 동기 실행(테스트·소규모용). 검증 실패 시 Issues를 채운 실패 결과를 반환한다. */
	WORKBENCHEDITOR_API FBalanceSimScenarioResult RunScenarioToCompletion(const FBalanceSimScenarioConfig& Config);

	/** 결과의 내부 일관성(완료 수와 배열 길이, 목표 수 이내, 값 유한)을 확인한다. 문제 설명 목록, 없으면 빈 배열. */
	WORKBENCHEDITOR_API TArray<FString> CheckResultConsistency(const FBalanceSimScenarioResult& Result);

	/** 오름차순 정렬된 값에서 선형 보간 분위수(Q는 0~1). 빈 배열이면 0. 보간은 (1-f)*lo+f*hi 형태라 넘치지 않는다. */
	WORKBENCHEDITOR_API double ComputeQuantileSorted(const TArray<double>& Sorted, double Q);

	struct WORKBENCHEDITOR_API FBalanceSimPairedDiffResult
	{
		bool bSuccess = false;
		TArray<FString> Errors;
		int32 PairedEpisodes = 0;
		double MeanDiff = 0.0; // mean(B_i - A_i)
		bool bHasVariance = false;
		double StdErrorDiff = 0.0;
		TArray<double> Diffs; // 에피소드 인덱스별 B-A
	};

	/**
	 * 둘 다 성공·완결(취소 아님)이고 배열 길이가 완료 수와 같으며, 마스터 시드·추출 수·목표/완료 에피소드 수가 같을 때만
	 * 에피소드 인덱스로 짝지어 B-A를 집계한다. B-A나 통계가 비유한이면 실패한다.
	 * A/B 각 구간의 겹침으로 유의성을 판정하지 않는다. 차이의 신뢰구간은 GetDiffBootstrapIneligibleReason이 빈 문자열일 때 ComputeBootstrapMeanCi(Diffs)로 계산한다.
	 */
	WORKBENCHEDITOR_API FBalanceSimPairedDiffResult ComparePairedResults(const FBalanceSimScenarioResult& ResultA, const FBalanceSimScenarioResult& ResultB);

	/** 짝지은 차이가 CI 대상인지(성공 + 표본 수/유한성 조건). 가능하면 빈 문자열. */
	WORKBENCHEDITOR_API FString GetDiffBootstrapIneligibleReason(const FBalanceSimPairedDiffResult& Diff);

	/**
	 * 원시 결과를 CSV 텍스트로 직렬화한다. A/B/차이 열은 ComparePairedResults가 성공한 경우에만 낸다.
	 * 짝지음이 불가능하면 A만 내고 이유를 주석으로 남긴다. 머리 주석에 RunId·입력 식별·시드 파생 버전·완료/취소 상태를 기록한다.
	 * DiffCi는 같은 쌍의 차이 CI(선택)이며 bValid일 때만 기록한다. 파일 저장은 호출자 책임.
	 */
	WORKBENCHEDITOR_API FString ExportResultsToCsv(const FBalanceSimScenarioResult& ResultA, const FBalanceSimScenarioResult* ResultB, const FBalanceSimBootstrapResult* DiffCi = nullptr);

	/** 원시 결과·입력 풀·시드·실행 식별·완료/취소 상태를 JSON 텍스트로 직렬화한다. ResultB가 있으면 시나리오 B와 짝지은 차이(가능할 때)를 포함한다. */
	WORKBENCHEDITOR_API FString ExportResultsToJson(const FBalanceSimScenarioResult& ResultA, const FBalanceSimScenarioResult* ResultB, const FBalanceSimBootstrapResult* DiffCi = nullptr);
}
