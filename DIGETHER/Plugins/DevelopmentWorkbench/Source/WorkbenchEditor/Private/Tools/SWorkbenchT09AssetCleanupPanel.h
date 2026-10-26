#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Tools/WorkbenchToolsAlgo.h"

// T09: 경로 기준 에셋 조사, 접두사 규칙 이름 변경 계획, 참조 없음 후보 목록(삭제 기능 없음).
// 이름 변경은 계획이 충돌·읽기 전용 없이 통과하고 사용자가 대상을 확인한 뒤에만 AssetTools로 적용한다.
class SWorkbenchT09AssetCleanupPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorkbenchT09AssetCleanupPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	using FRow = TSharedPtr<FString>;

	FReply OnScanClicked();
	FReply OnPlanClicked();
	FReply OnCopyRecoveryClicked();
	FReply OnApplyClicked();

	void InvalidatePlan();
	bool IsApplyEnabled() const;
	bool IsPlayInEditorActive() const;
	FText GetStatusText() const;
	TSharedRef<ITableRow> OnGenerateRow(FRow Row, const TSharedRef<STableViewBase>& OwnerTable);

	FString RootPath = TEXT("/Game");
	bool bRecursive = true;
	FString Prefix;

	TArray<WorkbenchToolsAlgo::FRenameCandidate> Candidates;
	TMap<FString, FSoftObjectPath> ObjectPathByKey;
	bool bScanned = false;
	bool bScanTruncated = false;

	TArray<WorkbenchToolsAlgo::FRenamePlanEntry> Plan;
	bool bPlanFresh = false;

	FText LastMessage;
	TArray<FRow> PlanRows;
	TArray<FRow> UnreferencedRows;
	TSharedPtr<SListView<FRow>> PlanView;
	TSharedPtr<SListView<FRow>> UnreferencedView;
};
