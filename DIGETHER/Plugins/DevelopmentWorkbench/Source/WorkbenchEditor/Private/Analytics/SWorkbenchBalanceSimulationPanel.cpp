#include "SWorkbenchBalanceSimulationPanel.h"

#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "IDesktopPlatform.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"

using namespace WorkbenchAnalytics;

namespace
{
	constexpr int32 DrawsPerTick = 100000; // 틱당 추출 수 상한(UI 멈춤 방지)
	constexpr int32 BootstrapWorkPerTick = 200000; // 틱당 bootstrap 표본 추출 수 상한

	const TCHAR* BootstrapCaveat = TEXT(
		"  신뢰구간 해석 주의: 이 구간은 '평균'의 불확실성이며 에피소드 결과 분포의 범위(분위수·최소·최대)와 다릅니다. "
		"관측되지 않은 희귀 꼬리는 반영하지 못하고, 에피소드가 독립이라는 가정과 합성 입력 풀에 한한 값입니다.\n");

	const TCHAR* SamplePoolA = TEXT(
		"# 합성 예제 A입니다(실제 게임 수치 아님). 형식: EntryId,Weight,Value\n"
		"EntryId,Weight,Value\n"
		"A,1,10\n"
		"B,3,20\n"
		"미선택,0,0\n");

	const TCHAR* SamplePoolB = TEXT(
		"# 합성 예제 B입니다. A와 같은 확률에 B 항목 Value만 30으로 바꾼 비교용입니다.\n"
		"EntryId,Weight,Value\n"
		"A,1,10\n"
		"B,3,30\n"
		"미선택,0,0\n");

	FString FormatCi(const FBalanceSimBootstrapResult& Ci)
	{
		if (Ci.bValid)
		{
			return FString::Printf(TEXT("95%% 신뢰구간(percentile bootstrap, 재표집 %d회, 고정 bootstrap 시드 %d) [%.8g, %.8g], 점추정 %.8g\n"),
				Ci.NumResamples, Ci.Seed, Ci.Lower, Ci.Upper, Ci.PointEstimate);
		}
		return FString::Printf(TEXT("신뢰구간 미표시: %s\n"), *Ci.NotComputedReason);
	}
}

void SWorkbenchBalanceSimulationPanel::Construct(const FArguments& InArgs)
{
	DiffCi.NotComputedReason = TEXT("아직 실행하지 않았습니다.");

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(FText::FromString(FString::Printf(TEXT(
					"T03 합성 잠재가치 분석 / 실게임 수익 아님\n"
					"입력한 풀에서 가중 복원 추출을 반복해 Value 합의 분포를 보는 합성 실험입니다. 실게임 규칙·가격·승인된 계산 함수(provider)가 연결되어 있지 않아 실게임 수익·밸런스 결론으로 쓸 수 없습니다.\n"
					"상한: 반복 %d회, 반복당 추출 %d회, 총 추출 %lld회. 표준오차는 반복이 독립이라는 가정의 값입니다.\n"
					"평균·차이의 95%% 신뢰구간(percentile bootstrap, 재표집 %d회)은 완료된 독립 에피소드 %d~%d개에서만 계산하며, 취소·미완결·표본 부족이면 숨깁니다. 이 구간은 평균의 불확실성이지 결과 분포의 범위가 아니며 관측되지 않은 희귀 꼬리는 반영하지 못합니다."),
					MaxBalanceEpisodes, MaxBalanceDrawsPerEpisode, MaxBalanceTotalDraws,
					BootstrapResamples, BootstrapMinEpisodes, BootstrapMaxEpisodes)))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("시나리오 A 풀 (\"EntryId,Weight[,Value]\"):")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SAssignNew(PoolCsvA, SMultiLineEditableTextBox).AllowMultiLine(true).Text(FText::FromString(SamplePoolA))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bCompareB ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bCompareB = State == ECheckBoxState::Checked; })
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("시나리오 B와 같은 시드로 짝지어 비교 (시드·추출 수·반복 수는 A와 공유)")))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SAssignNew(PoolCsvB, SMultiLineEditableTextBox)
				.AllowMultiLine(true)
				.Visibility_Lambda([this]() { return bCompareB ? EVisibility::Visible : EVisibility::Collapsed; })
				.Text(FText::FromString(SamplePoolB))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(2).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("시드:")))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2)
				[
					SNew(SNumericEntryBox<int32>)
					.Value_Lambda([this]() { return TOptional<int32>(MasterSeed); })
					.OnValueCommitted_Lambda([this](int32 NewValue, ETextCommit::Type) { MasterSeed = NewValue; })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("반복(에피소드) 수:")))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2)
				[
					SNew(SNumericEntryBox<int32>)
					.MinValue(1).MaxValue(MaxBalanceEpisodes)
					.Value_Lambda([this]() { return TOptional<int32>(NumEpisodes); })
					.OnValueCommitted_Lambda([this](int32 NewValue, ETextCommit::Type) { NumEpisodes = FMath::Clamp(NewValue, 1, MaxBalanceEpisodes); })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("반복당 추출 수:")))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2)
				[
					SNew(SNumericEntryBox<int32>)
					.MinValue(1).MaxValue(MaxBalanceDrawsPerEpisode)
					.Value_Lambda([this]() { return TOptional<int32>(DrawsPerEpisode); })
					.OnValueCommitted_Lambda([this](int32 NewValue, ETextCommit::Type) { DrawsPerEpisode = FMath::Clamp(NewValue, 1, MaxBalanceDrawsPerEpisode); })
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(2)
				[
					SNew(SButton).Text(FText::FromString(TEXT("합성 예제 불러오기")))
					.IsEnabled_Lambda([this]() { return !bRunning; })
					.OnClicked(this, &SWorkbenchBalanceSimulationPanel::OnLoadSampleClicked)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2)
				[
					SNew(SButton).Text(FText::FromString(TEXT("실행")))
					.IsEnabled_Lambda([this]() { return !bRunning; })
					.OnClicked(this, &SWorkbenchBalanceSimulationPanel::OnRunClicked)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2)
				[
					SNew(SButton).Text(FText::FromString(TEXT("취소")))
					.IsEnabled_Lambda([this]() { return bRunning; })
					.OnClicked(this, &SWorkbenchBalanceSimulationPanel::OnCancelClicked)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2)
				[
					SNew(SButton).Text(FText::FromString(TEXT("원시 결과 JSON 저장")))
					.IsEnabled_Lambda([this]() { return !bRunning && ResultA.IsSet() && ResultA->CompletedEpisodes > 0; })
					.OnClicked(this, &SWorkbenchBalanceSimulationPanel::OnExportClicked, true)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2)
				[
					SNew(SButton).Text(FText::FromString(TEXT("원시 결과 CSV 저장")))
					.IsEnabled_Lambda([this]() { return !bRunning && ResultA.IsSet() && ResultA->CompletedEpisodes > 0; })
					.OnClicked(this, &SWorkbenchBalanceSimulationPanel::OnExportClicked, false)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.35f, 0.2f)))
				.Text(FText::FromString(TEXT("경고: 입력(풀 CSV·시드·반복 수·추출 수·비교 설정)이 마지막 실행 이후 바뀌었습니다. 아래 결과는 실행 당시 입력 스냅샷의 오래된 결과이므로 현재 입력과 일치하지 않습니다. 다시 실행하세요. 저장하면 실행 당시의 입력 지문이 기록됩니다.")))
				.Visibility_Lambda([this]()
				{
					return (!bRunning && ResultA.IsSet() && BuildInputSignature() != RunSignature) ? EVisibility::Visible : EVisibility::Collapsed;
				})
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SAssignNew(StatusText, STextBlock)
				.AutoWrapText(true)
				.Text(FText::FromString(TEXT("상태: 아직 실행하지 않음.")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(SSeparator)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SAssignNew(ResultText, STextBlock).AutoWrapText(true)
			]
		]
	];
}

FString SWorkbenchBalanceSimulationPanel::BuildInputSignature() const
{
	FString Signature = FString::Printf(TEXT("seed=%d;draws=%d;episodes=%d;compare=%d;"), MasterSeed, DrawsPerEpisode, NumEpisodes, bCompareB ? 1 : 0);
	Signature += PoolCsvA->GetText().ToString();
	if (bCompareB)
	{
		Signature += TEXT("\n--B--\n") + PoolCsvB->GetText().ToString();
	}
	return Signature;
}

void SWorkbenchBalanceSimulationPanel::SetOutput(const FString& Status, const FString& Detail)
{
	StatusText->SetText(FText::FromString(Status));
	ResultText->SetText(FText::FromString(Detail));
}

FReply SWorkbenchBalanceSimulationPanel::OnLoadSampleClicked()
{
	PoolCsvA->SetText(FText::FromString(SamplePoolA));
	PoolCsvB->SetText(FText::FromString(SamplePoolB));
	SetOutput(TEXT("상태: 합성 예제를 불러왔습니다. 아직 실행하지 않았습니다."), FString());
	return FReply::Handled();
}

bool SWorkbenchBalanceSimulationPanel::BuildConfig(const FString& CsvText, const FString& Label, int32 Seed, int32 Draws, int32 Episodes,
	FBalanceSimScenarioConfig& OutConfig, FString& OutError)
{
	TArray<FString> ParseErrors;
	OutConfig = FBalanceSimScenarioConfig();
	if (!ParseSpawnPoolCsv(CsvText, OutConfig.Pool, ParseErrors))
	{
		OutError = FString::Printf(TEXT("시나리오 %s CSV 파싱 실패:\n"), *Label);
		for (const FString& Error : ParseErrors)
		{
			OutError += TEXT("- ") + Error + TEXT("\n");
		}
		return false;
	}
	OutConfig.ScenarioLabel = Label;
	OutConfig.MasterSeed = Seed;
	OutConfig.DrawsPerEpisode = Draws;
	OutConfig.NumEpisodes = Episodes;

	const TArray<FBalanceSimIssue> Issues = ValidateScenarioConfig(OutConfig);
	if (Issues.Num() > 0)
	{
		OutError = FString::Printf(TEXT("시나리오 %s 검증 실패:\n"), *Label);
		for (const FBalanceSimIssue& Issue : Issues)
		{
			OutError += TEXT("- ") + Issue.Message + TEXT("\n");
		}
		return false;
	}
	return true;
}

FReply SWorkbenchBalanceSimulationPanel::OnRunClicked()
{
	if (bRunning)
	{
		return FReply::Handled();
	}

	// 실행 중 입력 편집이 섞이지 않도록 이 시점의 값을 한 번에 읽어 스냅샷으로 쓴다.
	const FString Signature = BuildInputSignature();
	FBalanceSimScenarioConfig ConfigA;
	FBalanceSimScenarioConfig ConfigB;
	FString Error;
	if (!BuildConfig(PoolCsvA->GetText().ToString(), TEXT("A"), MasterSeed, DrawsPerEpisode, NumEpisodes, ConfigA, Error)
		|| (bCompareB && !BuildConfig(PoolCsvB->GetText().ToString(), TEXT("B"), MasterSeed, DrawsPerEpisode, NumEpisodes, ConfigB, Error)))
	{
		SetOutput(TEXT("상태: 입력 오류. 실행하지 않았습니다."), Error);
		return FReply::Handled();
	}

	const FString RunId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	ConfigA.RunId = RunId;
	ConfigB.RunId = RunId;
	if (!InitRunState(ConfigA, RunState))
	{
		SetOutput(TEXT("상태: 실행 초기화에 실패했습니다."), FString());
		return FReply::Handled();
	}

	ResultA.Reset();
	ResultB.Reset();
	DiffCi = FBalanceSimBootstrapResult();
	DiffCi.NotComputedReason = TEXT("아직 계산하지 않았습니다.");
	BootState = FBalanceSimBootstrapState();
	PendingConfigB = ConfigB;
	bHasPendingB = bCompareB;
	RunSignature = Signature;
	RunSnapshotNote = FString::Printf(TEXT("실행 입력 스냅샷: runId %s, A 입력 지문 %s"), *RunId, *ComputeScenarioFingerprint(ConfigA));
	if (bCompareB)
	{
		RunSnapshotNote += FString::Printf(TEXT(", B 입력 지문 %s"), *ComputeScenarioFingerprint(ConfigB));
	}
	RunSnapshotNote += FString::Printf(TEXT(", 시드 파생 %s"), GetEpisodeSeedDerivationVersion());
	Phase = EPhase::SimA;
	bRunning = true;
	LastExportNote.Reset();
	++Generation;
	SetOutput(TEXT("상태: 시나리오 A 실행 중... 0%"), FString());
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SWorkbenchBalanceSimulationPanel::TickRun, Generation));
	return FReply::Handled();
}

EActiveTimerReturnType SWorkbenchBalanceSimulationPanel::TickRun(double InCurrentTime, float InDeltaTime, uint32 InGeneration)
{
	if (InGeneration != Generation || !bRunning)
	{
		return EActiveTimerReturnType::Stop;
	}

	if (Phase == EPhase::SimA || Phase == EPhase::SimB)
	{
		const int32 EpisodesPerTick = FMath::Max(1, DrawsPerTick / FMath::Max(1, RunState.Config.DrawsPerEpisode));
		AdvanceRunState(RunState, EpisodesPerTick);

		if (RunState.IsFinished())
		{
			FinishPhase();
			if (!bRunning)
			{
				return EActiveTimerReturnType::Stop;
			}
		}
		if (Phase == EPhase::SimA || Phase == EPhase::SimB)
		{
			const int32 Total = FMath::Max(1, RunState.Config.NumEpisodes);
			StatusText->SetText(FText::FromString(FString::Printf(TEXT("상태: 시나리오 %s 실행 중... %d / %d (%.0f%%)"),
				Phase == EPhase::SimB ? TEXT("B") : TEXT("A"), RunState.EpisodesCompleted(), Total, 100.0 * RunState.EpisodesCompleted() / Total)));
		}
	}
	else
	{
		const int32 ResamplesPerTick = FMath::Max(1, BootstrapWorkPerTick / FMath::Max(1, BootState.ScaledValues.Num()));
		AdvanceBootstrapState(BootState, ResamplesPerTick);
		if (BootState.IsFinished())
		{
			FinishBootPhase();
			if (!bRunning)
			{
				return EActiveTimerReturnType::Stop;
			}
		}
		const TCHAR* Target = Phase == EPhase::BootA ? TEXT("A 평균") : (Phase == EPhase::BootB ? TEXT("B 평균") : TEXT("B-A 차이"));
		StatusText->SetText(FText::FromString(FString::Printf(TEXT("상태: %s 신뢰구간 계산 중... 재표집 %d / %d"),
			Target, BootState.ResampleMeans.Num(), BootState.TotalResamples)));
	}
	return EActiveTimerReturnType::Continue;
}

void SWorkbenchBalanceSimulationPanel::FinishPhase()
{
	const FBalanceSimScenarioResult Result = FinalizeRunState(RunState);
	if (Phase == EPhase::SimA)
	{
		ResultA = Result;
		if (bHasPendingB && !RunState.bCancelled && Result.bSuccess && InitRunState(PendingConfigB, RunState))
		{
			bHasPendingB = false;
			Phase = EPhase::SimB;
			return;
		}
		bHasPendingB = false;
	}
	else
	{
		ResultB = Result;
	}

	if (RunState.bCancelled)
	{
		MarkBootstrapSkipped(EPhase::BootA, TEXT("실행이 취소되어 신뢰구간을 계산하지 않았습니다."));
		Phase = EPhase::Idle;
		bRunning = false;
		ShowReport();
		return;
	}
	BeginBootPhase(EPhase::BootA);
}

void SWorkbenchBalanceSimulationPanel::MarkBootstrapSkipped(EPhase From, const FString& Reason)
{
	auto Skip = [&Reason](FBalanceSimBootstrapResult& Ci)
	{
		Ci = FBalanceSimBootstrapResult();
		Ci.NotComputedReason = Reason;
	};
	if (From <= EPhase::BootA && ResultA.IsSet() && !ResultA->MeanBootstrap.bValid)
	{
		Skip(ResultA->MeanBootstrap);
	}
	if (From <= EPhase::BootB && ResultB.IsSet() && !ResultB->MeanBootstrap.bValid)
	{
		Skip(ResultB->MeanBootstrap);
	}
	if (From <= EPhase::BootDiff && !DiffCi.bValid)
	{
		Skip(DiffCi);
	}
}

void SWorkbenchBalanceSimulationPanel::BeginBootPhase(EPhase Next)
{
	for (EPhase P = Next; P != EPhase::Idle;)
	{
		FString Reason;
		if (P == EPhase::BootA || P == EPhase::BootB)
		{
			TOptional<FBalanceSimScenarioResult>& Target = (P == EPhase::BootA) ? ResultA : ResultB;
			if (Target.IsSet())
			{
				Reason = GetMeanBootstrapIneligibleReason(*Target);
				if (Reason.IsEmpty() && InitBootstrapState(Target->EpisodeTotals, BootState, Reason))
				{
					Phase = P;
					return;
				}
				Target->MeanBootstrap = FBalanceSimBootstrapResult();
				Target->MeanBootstrap.SampleCount = Target->EpisodeTotals.Num();
				Target->MeanBootstrap.NotComputedReason = Reason;
			}
			P = (P == EPhase::BootA) ? EPhase::BootB : EPhase::BootDiff;
		}
		else
		{
			DiffCi = FBalanceSimBootstrapResult();
			if (!ResultA.IsSet() || !ResultB.IsSet())
			{
				DiffCi.NotComputedReason = TEXT("시나리오 B 비교를 실행하지 않았습니다.");
			}
			else
			{
				const FBalanceSimPairedDiffResult Diff = ComparePairedResults(*ResultA, *ResultB);
				Reason = GetDiffBootstrapIneligibleReason(Diff);
				if (Reason.IsEmpty() && InitBootstrapState(Diff.Diffs, BootState, Reason))
				{
					Phase = EPhase::BootDiff;
					return;
				}
				DiffCi.SampleCount = Diff.Diffs.Num();
				DiffCi.NotComputedReason = Reason;
			}
			P = EPhase::Idle;
		}
	}

	Phase = EPhase::Idle;
	bRunning = false;
	ShowReport();
}

void SWorkbenchBalanceSimulationPanel::FinishBootPhase()
{
	const FBalanceSimBootstrapResult Ci = FinalizeBootstrapState(BootState);
	EPhase Next = EPhase::Idle;
	switch (Phase)
	{
	case EPhase::BootA:
		if (ResultA.IsSet()) { ResultA->MeanBootstrap = Ci; }
		Next = EPhase::BootB;
		break;
	case EPhase::BootB:
		if (ResultB.IsSet()) { ResultB->MeanBootstrap = Ci; }
		Next = EPhase::BootDiff;
		break;
	default:
		DiffCi = Ci;
		break;
	}
	BeginBootPhase(Next);
}

FReply SWorkbenchBalanceSimulationPanel::OnCancelClicked()
{
	if (!bRunning)
	{
		return FReply::Handled();
	}
	++Generation;

	if (Phase == EPhase::SimA || Phase == EPhase::SimB)
	{
		CancelRunState(RunState);
		const FBalanceSimScenarioResult Partial = FinalizeRunState(RunState);
		(Phase == EPhase::SimB ? ResultB : ResultA) = Partial;
		bHasPendingB = false;
		MarkBootstrapSkipped(EPhase::BootA, TEXT("실행이 취소되어 신뢰구간을 계산하지 않았습니다."));
	}
	else
	{
		CancelBootstrapState(BootState);
		const FBalanceSimBootstrapResult Ci = FinalizeBootstrapState(BootState);
		if (Phase == EPhase::BootA && ResultA.IsSet())
		{
			ResultA->MeanBootstrap = Ci;
		}
		else if (Phase == EPhase::BootB && ResultB.IsSet())
		{
			ResultB->MeanBootstrap = Ci;
		}
		else if (Phase == EPhase::BootDiff)
		{
			DiffCi = Ci;
		}
		// 시뮬레이션 결과 자체는 완결이므로 유지하고, 계산되지 않은 신뢰구간만 숨긴다.
		const EPhase After = Phase == EPhase::BootA ? EPhase::BootB : (Phase == EPhase::BootB ? EPhase::BootDiff : EPhase::Idle);
		if (After != EPhase::Idle)
		{
			MarkBootstrapSkipped(After, TEXT("신뢰구간 계산이 취소되어 계산하지 않았습니다."));
		}
	}
	Phase = EPhase::Idle;
	bRunning = false;
	ShowReport();
	return FReply::Handled();
}

FString SWorkbenchBalanceSimulationPanel::BuildScenarioReport(const FString& Title, const FBalanceSimScenarioResult& Result)
{
	const TCHAR* Status = !Result.bSuccess ? TEXT("실패") : (Result.bCancelled ? TEXT("취소됨, 부분 결과") : (Result.bComplete ? TEXT("완결") : TEXT("미완결")));
	FString Report = FString::Printf(TEXT("[%s] 시드 %d, 반복당 추출 %d회, 완료 %d / 목표 %d회 (%s)\n  runId %s, 입력 지문 %s\n"), *Title, Result.Config.MasterSeed,
		Result.Config.DrawsPerEpisode, Result.CompletedEpisodes, Result.Config.NumEpisodes, Status, *Result.Config.RunId, *Result.InputFingerprint);
	if (!Result.bSuccess)
	{
		for (const FBalanceSimIssue& Issue : Result.Issues)
		{
			Report += TEXT("  오류: ") + Issue.Message + TEXT("\n");
		}
		return Report;
	}
	if (Result.CompletedEpisodes == 0)
	{
		return Report + TEXT("  완료된 반복이 없어 통계를 계산하지 않았습니다.\n");
	}
	for (const FString& Error : CheckResultConsistency(Result))
	{
		Report += TEXT("  경고: 결과 불일치 - ") + Error + TEXT("\n");
	}
	Report += FString::Printf(TEXT("  평균 %.8g, 최소 %.8g, 최대 %.8g\n  분위수 P05 %.8g / P50 %.8g / P95 %.8g\n"),
		Result.Mean, Result.Min, Result.Max, Result.P05, Result.P50, Result.P95);
	if (Result.bHasVariance)
	{
		Report += FString::Printf(TEXT("  표준편차 %.8g, 표준오차 %.8g\n"), Result.StdDev, Result.StdError);
	}
	else
	{
		Report += TEXT("  표준편차·표준오차: 표본 1개라 계산하지 않음\n");
	}
	if (Result.bHasAnalyticMean)
	{
		Report += FString::Printf(TEXT("  닫힌 식 평균(추출 수 x 기대 Value) %.8g\n"), Result.AnalyticMean);
	}
	Report += TEXT("  평균 ") + FormatCi(Result.MeanBootstrap);
	Report += FString(Result.bSampleSizeWarning ? TEXT("  경고: ") : TEXT("  참고: ")) + Result.SampleSizeNote + TEXT("\n");
	return Report;
}

void SWorkbenchBalanceSimulationPanel::ShowReport()
{
	FString Report = TEXT("합성 잠재가치 분석 / 실게임 수익 아님. 실게임 provider 미연결.\n");
	if (!RunSnapshotNote.IsEmpty())
	{
		Report += RunSnapshotNote + TEXT("\n");
	}
	Report += TEXT("\n");

	bool bAnyCancelled = false;
	bool bAnyCi = false;
	if (ResultA.IsSet())
	{
		Report += BuildScenarioReport(TEXT("시나리오 A"), *ResultA) + TEXT("\n");
		bAnyCancelled |= ResultA->bCancelled;
		bAnyCi |= ResultA->MeanBootstrap.bValid;
	}
	if (ResultB.IsSet())
	{
		Report += BuildScenarioReport(TEXT("시나리오 B"), *ResultB) + TEXT("\n");
		bAnyCancelled |= ResultB->bCancelled;
		bAnyCi |= ResultB->MeanBootstrap.bValid;
	}
	if (ResultA.IsSet() && ResultB.IsSet())
	{
		const FBalanceSimPairedDiffResult Diff = ComparePairedResults(*ResultA, *ResultB);
		if (Diff.bSuccess)
		{
			Report += FString::Printf(TEXT("[짝지은 차이 B-A] 같은 시드의 같은 반복 %d쌍, 평균 차이 %.8g"), Diff.PairedEpisodes, Diff.MeanDiff);
			Report += Diff.bHasVariance ? FString::Printf(TEXT(", 차이의 표준오차 %.8g\n"), Diff.StdErrorDiff) : FString(TEXT(", 표본 1쌍이라 표준오차 없음\n"));
			Report += TEXT("  차이 ") + FormatCi(DiffCi);
			bAnyCi |= DiffCi.bValid;
			Report += TEXT("  A/B 각각의 구간 겹침으로 유의성을 판정하지 않습니다.\n");
		}
		else
		{
			Report += TEXT("[짝지은 차이] 계산하지 않음:\n");
			for (const FString& Error : Diff.Errors)
			{
				Report += TEXT("  - ") + Error + TEXT("\n");
			}
		}
	}
	if (bAnyCi)
	{
		Report += BootstrapCaveat;
	}
	if (!LastExportNote.IsEmpty())
	{
		Report += TEXT("\n") + LastExportNote;
	}
	SetOutput(bAnyCancelled ? TEXT("상태: 취소됨. 완료된 반복까지의 부분 결과입니다.") : TEXT("상태: 실행 종료."), Report);
}

FReply SWorkbenchBalanceSimulationPanel::OnExportClicked(bool bJson)
{
	if (!ResultA.IsSet() || bRunning)
	{
		return FReply::Handled();
	}

	const FBalanceSimScenarioResult* ResultBPtr = ResultB.IsSet() ? &*ResultB : nullptr;
	const FString Content = bJson ? ExportResultsToJson(*ResultA, ResultBPtr, &DiffCi) : ExportResultsToCsv(*ResultA, ResultBPtr, &DiffCi);

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		LastExportNote = TEXT("저장 실패: 파일 대화상자를 사용할 수 없습니다.");
		ShowReport();
		return FReply::Handled();
	}

	TArray<FString> Filenames;
	const bool bPicked = DesktopPlatform->SaveFileDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
		bJson ? TEXT("합성 분석 원시 결과(JSON) 저장") : TEXT("합성 분석 원시 결과(CSV) 저장"),
		FPaths::ProjectSavedDir(),
		bJson ? TEXT("synthetic-analysis.json") : TEXT("synthetic-analysis.csv"),
		bJson ? TEXT("JSON (*.json)|*.json") : TEXT("CSV (*.csv)|*.csv"),
		EFileDialogFlags::None, Filenames);

	if (!bPicked || Filenames.Num() == 0)
	{
		LastExportNote = TEXT("저장 취소: 파일을 쓰지 않았습니다.");
	}
	else if (FFileHelper::SaveStringToFile(Content, *Filenames[0], FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		LastExportNote = FString::Printf(TEXT("저장 완료(로컬 파일): %s"), *Filenames[0]);
	}
	else
	{
		LastExportNote = FString::Printf(TEXT("저장 실패: %s 에 쓸 수 없습니다."), *Filenames[0]);
	}
	ShowReport();
	return FReply::Handled();
}
