#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Tools/WorkbenchToolsAlgo.h"

class AActor;

// T11: 선택 액터 중 첫 번째(시드)를 기준으로 나머지 선택 액터를 간격·회전·스케일 단계·시드 지터로 재배치한다.
// 계산은 미리보기 값으로만 표시하고, 확정 시에만 하나의 트랜잭션으로 적용한다(Undo 가능). 저장은 하지 않는다.
class SWorkbenchT11PlacementPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorkbenchT11PlacementPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	struct FPreviewRow
	{
		FString Label;
		FString Detail;
	};

	FReply OnReadSelectionClicked();
	FReply OnPreviewClicked();
	FReply OnApplyClicked();
	FReply OnCancelClicked();

	void RebuildPreview();
	void ClearPreview();
	bool IsApplyEnabled() const;
	bool IsPlayInEditorActive() const;
	FText GetStatusText() const;
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FPreviewRow> Row, const TSharedRef<STableViewBase>& OwnerTable);

	TArray<TWeakObjectPtr<AActor>> Targets; // 0번이 시드
	TArray<WorkbenchToolsAlgo::FPlacementActorState> States;
	FTransform CapturedSeed = FTransform::Identity;
	TArray<FTransform> CapturedOriginals; // 선택 읽기 시점 각 액터의 원본 변환(적용 시 충돌 검사용)
	TArray<FString> ParamErrors;

	FVector Spacing = FVector(200.0, 0.0, 0.0);
	FRotator RotationStep = FRotator::ZeroRotator;
	float UniformScaleStep = 0.f;
	float PositionJitter = 0.f;
	int32 RandomSeed = 1234;

	TArray<FTransform> Preview;
	TArray<WorkbenchToolsAlgo::FPlacementIssue> Issues;
	bool bPreviewValid = false;
	FText LastResult;

	TArray<TSharedPtr<FPreviewRow>> Rows;
	TSharedPtr<SListView<TSharedPtr<FPreviewRow>>> RowsView;
};
