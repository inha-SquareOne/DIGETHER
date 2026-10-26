// T02 스폰 확률 패널. CSV 입력을 검증해 단일 풀 복원 추출의 정확 확률·기대값을 보이고 결정적 Monte Carlo와 대조한다.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Analytics/WorkbenchSpawnProbability.h"

class STextBlock;
class SMultiLineEditableTextBox;

class SWorkbenchSpawnProbabilityPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorkbenchSpawnProbabilityPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedPtr<SMultiLineEditableTextBox> CsvInputBox;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<STextBlock> ResultText;

	int32 NumDraws = 10;
	int32 MonteCarloSeed = 12345;
	int32 MonteCarloDraws = 100000;

	FReply OnCalculateClicked();
	FReply OnLoadSampleClicked();

	void SetOutput(const FString& Status, const FString& Detail);

	static FString BuildResultReport(const WorkbenchAnalytics::FSpawnPoolCalculationResult& CalcResult, const WorkbenchAnalytics::FSpawnPoolMonteCarloResult& McResult);
	static FString BuildIssueReport(const TArray<WorkbenchAnalytics::FSpawnPoolIssue>& Issues);
};
