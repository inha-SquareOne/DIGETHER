// T04 룸 연결성 패널. 입력된 룸/문/연결 매니페스트를 스키마·tolerance·그래프 기준으로 검사한다. 월드 충돌·NavMesh·PCG와는 연결되어 있지 않다.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Analytics/WorkbenchRoomConnectivity.h"

class STextBlock;
class SMultiLineEditableTextBox;

class SWorkbenchRoomConnectivityPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorkbenchRoomConnectivityPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedPtr<SMultiLineEditableTextBox> ManifestBox;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<STextBlock> ResultText;

	WorkbenchAnalytics::FRoomManifestSpec Spec;

	FReply OnAnalyzeClicked();
	FReply OnLoadSampleClicked(WorkbenchAnalytics::ERoomManifestSample Sample);

	void SetOutput(const FString& Status, const FString& Detail);

	TSharedRef<SWidget> MakeToleranceEntry(const FString& Label, double* Value, double MinValue);
	TSharedRef<SWidget> MakeSampleButton(const FString& Label, WorkbenchAnalytics::ERoomManifestSample Sample);

	static FString BuildReport(const WorkbenchAnalytics::FRoomManifestReport& Report);
};
