#include "SWorkbenchSpawnProbabilityPanel.h"

#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Text/STextBlock.h"

using namespace WorkbenchAnalytics;

namespace
{
	constexpr int32 MaxNumDraws = 1000000000;

	const TCHAR* SamplePoolCsv = TEXT(
		"# 합성 예제입니다(실제 게임 수치 아님). 형식: EntryId,Weight[,Value]\n"
		"EntryId,Weight,Value\n"
		"A,1,10\n"
		"B,3,20\n"
		"미선택,0,0\n");

	FString DescribeIssueType(ESpawnPoolIssueType Type)
	{
		switch (Type)
		{
		case ESpawnPoolIssueType::EmptyEntryId: return TEXT("빈 ID");
		case ESpawnPoolIssueType::DuplicateEntryId: return TEXT("ID 중복");
		case ESpawnPoolIssueType::NegativeWeight: return TEXT("음수 가중치");
		case ESpawnPoolIssueType::NonFiniteWeight: return TEXT("비유한 가중치");
		case ESpawnPoolIssueType::NonFiniteValue: return TEXT("비유한 Value");
		case ESpawnPoolIssueType::EmptyPool: return TEXT("빈 풀");
		case ESpawnPoolIssueType::AllZeroWeight: return TEXT("전부 0 가중치");
		case ESpawnPoolIssueType::OverflowTotalWeight: return TEXT("총 가중치 오버플로");
		case ESpawnPoolIssueType::OverflowExpectedValue: return TEXT("기대값 오버플로");
		case ESpawnPoolIssueType::InvalidDrawCount: return TEXT("추출 수 범위 오류");
		default: return TEXT("알 수 없는 오류");
		}
	}
}

void SWorkbenchSpawnProbabilityPanel::Construct(const FArguments& InArgs)
{
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
				.Text(FText::FromString(TEXT(
					"T02 스폰 확률 — 조건을 고정한 단일 풀의 복원 추출(추출마다 독립이며 뽑은 항목을 다시 넣는 방식)만 계산합니다.\n"
					"지원하지 않음: 비복원 추출, 천장(pity) 보정, 조건부 가중치, 다중 풀. 이들의 결과로 해석하지 마세요.\n"
					"입력은 합성 예제이며 실제 게임 수치가 아닙니다. Value는 사용자가 넣은 임의 단위입니다.")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("풀 입력 (한 줄에 \"EntryId,Weight[,Value]\", '#'로 시작하면 주석, 첫 줄 헤더 허용):")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SAssignNew(CsvInputBox, SMultiLineEditableTextBox)
				.AllowMultiLine(true)
				.Text(FText::FromString(SamplePoolCsv))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(2).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("추출 횟수 N:")))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2)
				[
					SNew(SNumericEntryBox<int32>)
					.MinValue(0).MaxValue(MaxNumDraws)
					.Value_Lambda([this]() { return TOptional<int32>(NumDraws); })
					.OnValueCommitted_Lambda([this](int32 NewValue, ETextCommit::Type) { NumDraws = FMath::Clamp(NewValue, 0, MaxNumDraws); })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Monte Carlo 시드:")))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2)
				[
					SNew(SNumericEntryBox<int32>)
					.Value_Lambda([this]() { return TOptional<int32>(MonteCarloSeed); })
					.OnValueCommitted_Lambda([this](int32 NewValue, ETextCommit::Type) { MonteCarloSeed = NewValue; })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("Monte Carlo 추출 수(0~%d):"), MaxMonteCarloDraws)))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2)
				[
					SNew(SNumericEntryBox<int32>)
					.MinValue(0).MaxValue(MaxMonteCarloDraws)
					.Value_Lambda([this]() { return TOptional<int32>(MonteCarloDraws); })
					.OnValueCommitted_Lambda([this](int32 NewValue, ETextCommit::Type) { MonteCarloDraws = FMath::Clamp(NewValue, 0, MaxMonteCarloDraws); })
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(2)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("합성 예제 불러오기")))
					.OnClicked(this, &SWorkbenchSpawnProbabilityPanel::OnLoadSampleClicked)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("검증·계산 실행")))
					.OnClicked(this, &SWorkbenchSpawnProbabilityPanel::OnCalculateClicked)
				]
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
				SAssignNew(ResultText, STextBlock)
				.AutoWrapText(true)
				.Text(FText::GetEmpty())
			]
		]
	];
}

void SWorkbenchSpawnProbabilityPanel::SetOutput(const FString& Status, const FString& Detail)
{
	StatusText->SetText(FText::FromString(Status));
	ResultText->SetText(FText::FromString(Detail));
}

FReply SWorkbenchSpawnProbabilityPanel::OnLoadSampleClicked()
{
	CsvInputBox->SetText(FText::FromString(SamplePoolCsv));
	SetOutput(TEXT("상태: 합성 예제를 불러왔습니다. 아직 계산하지 않았습니다."), FString());
	return FReply::Handled();
}

FString SWorkbenchSpawnProbabilityPanel::BuildIssueReport(const TArray<FSpawnPoolIssue>& Issues)
{
	FString Report = TEXT("검증 실패:\n");
	for (const FSpawnPoolIssue& Issue : Issues)
	{
		Report += FString::Printf(TEXT("- [%s] %s\n"), *DescribeIssueType(Issue.Type), *Issue.Message);
	}
	return Report;
}

FString SWorkbenchSpawnProbabilityPanel::BuildResultReport(const FSpawnPoolCalculationResult& CalcResult, const FSpawnPoolMonteCarloResult& McResult)
{
	FString Report = FString::Printf(TEXT("총 가중치 %.10g, N=%d회 (복원 추출·독립 전제)\n\n"), CalcResult.TotalWeight, CalcResult.NumDraws);

	for (int32 Index = 0; Index < CalcResult.Entries.Num(); ++Index)
	{
		const FSpawnPoolResultEntry& Entry = CalcResult.Entries[Index];
		Report += FString::Printf(TEXT("[%s] 가중치 %.10g, Value %.10g\n"), *Entry.EntryId, Entry.Weight, Entry.Value);
		Report += FString::Printf(TEXT("    1회 확률(정확) %.8f%%, N회 중 적어도 1회 %.8f%%, N회 기대 횟수 %.6g\n"),
			Entry.Probability * 100.0, Entry.ProbabilityAtLeastOnceInNDraws * 100.0, Entry.ExpectedCountInNDraws);

		if (McResult.bSuccess && McResult.TotalDraws > 0 && McResult.DrawCounts.IsValidIndex(Index))
		{
			const int32 Count = McResult.DrawCounts[Index];
			const double Observed = static_cast<double>(Count) / static_cast<double>(McResult.TotalDraws);
			const bool bWithin = IsObservedWithinTolerance(Entry.Probability, Count, McResult.TotalDraws);
			Report += FString::Printf(TEXT("    Monte Carlo 관측 %.8f%% (%d/%d), 허용 폭 ±%.6f%% → %s\n"),
				Observed * 100.0, Count, McResult.TotalDraws,
				MonteCarloTolerance(Entry.Probability, McResult.TotalDraws) * 100.0,
				bWithin ? TEXT("해석값과 일치(허용 폭 이내)") : TEXT("허용 폭 초과 — 확인 필요"));
		}
	}

	Report += FString::Printf(TEXT("\n기대 Value: 1회 %.10g, N회 %.10g (합성 단위)\n"), CalcResult.ExpectedValuePerDraw, CalcResult.ExpectedValueInNDraws);

	if (McResult.bSuccess && McResult.TotalDraws > 0)
	{
		Report += FString::Printf(TEXT("Monte Carlo: 시드 %d, 추출 %d회, 같은 시드·입력이면 같은 결과가 재현됩니다. 허용 폭은 도구 점검용이며 게임 밸런스 기준이 아닙니다.\n"), McResult.Seed, McResult.TotalDraws);
	}
	else if (McResult.bSuccess)
	{
		Report += TEXT("Monte Carlo 추출 수가 0이라 비교하지 않았습니다.\n");
	}
	else
	{
		Report += TEXT("Monte Carlo 비교를 수행하지 못했습니다:\n");
		for (const FSpawnPoolIssue& Issue : McResult.Issues)
		{
			Report += TEXT("- ") + Issue.Message + TEXT("\n");
		}
	}

	Report += TEXT("\n범위: 단일 풀 복원 추출만 해당합니다. 비복원·pity·조건부 가중치의 결과가 아닙니다.");
	return Report;
}

FReply SWorkbenchSpawnProbabilityPanel::OnCalculateClicked()
{
	TArray<FSpawnPoolEntry> Entries;
	TArray<FString> ParseErrors;
	if (!ParseSpawnPoolCsv(CsvInputBox->GetText().ToString(), Entries, ParseErrors))
	{
		FString Report = TEXT("CSV 파싱 실패:\n");
		for (const FString& Error : ParseErrors)
		{
			Report += TEXT("- ") + Error + TEXT("\n");
		}
		SetOutput(TEXT("상태: 파싱 오류. 계산하지 않았습니다."), Report);
		return FReply::Handled();
	}

	const FSpawnPoolCalculationResult CalcResult = CalculateSinglePoolProbabilities(Entries, NumDraws);
	if (!CalcResult.bSuccess)
	{
		SetOutput(TEXT("상태: 검증 실패. 계산하지 않았습니다."), BuildIssueReport(CalcResult.Issues));
		return FReply::Handled();
	}

	const FSpawnPoolMonteCarloResult McResult = RunMonteCarloSimulation(Entries, MonteCarloDraws, MonteCarloSeed);
	SetOutput(TEXT("상태: 계산 완료."), BuildResultReport(CalcResult, McResult));
	return FReply::Handled();
}
