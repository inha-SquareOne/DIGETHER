#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

// T10: 엔진 기본 검색 창(Find in Blueprints / Reference Viewer)을 여는 진입점과, 콘텐츠 브라우저 선택 에셋의
// 패키지 dependency·referencer 목록(AssetRegistry의 Hard/Soft 구분). 새 검색 엔진은 만들지 않는다.
class SWorkbenchT10SearchPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorkbenchT10SearchPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	using FRow = TSharedPtr<FString>;

	FReply OnReadSelectionClicked();
	FReply OnListClicked();
	FReply OnOpenFindInBlueprintsClicked();
	FReply OnOpenReferenceViewerClicked();
	bool HasSubject() const { return SubjectPackage != NAME_None; }

	FText BuildIndexStateText() const;
	TSharedRef<ITableRow> OnGenerateRow(FRow Row, const TSharedRef<STableViewBase>& OwnerTable);

	FName SubjectPackage;
	FText SubjectLabel;
	FText EngineMessage;
	FText ListSummary;

	TArray<FRow> Rows;
	TSharedPtr<SListView<FRow>> RowsView;
};
