#pragma once

#include "CoreMinimal.h"
#include "Core/WorkbenchCsvPatchManifest.h"
#include "Core/WorkbenchDataTableEditAdapter.h"
#include "Core/WorkbenchDraftSession.h"
#include "Core/WorkbenchValidationTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"

class IDetailsView;
class SMultiLineEditableTextBox;
class UDataTable;

DECLARE_DELEGATE_OneParam(FWorkbenchTableChosen, UDataTable*);

/** 한 줄 메시지(색 구분). 미리보기·검증·적용 결과 목록에 쓴다. */
struct FWorkbenchUiLine
{
	FText Text;
	FLinearColor Color = FLinearColor::White;
};

namespace WorkbenchCoreUi
{
	FLinearColor ColorForMessage(const FWorkbenchValidationMessage& Message);
	TSharedRef<FWorkbenchUiLine> MakeLine(const FText& Text, const FLinearColor& Color);
	TSharedRef<SWidget> MakeLineList(TArray<TSharedPtr<FWorkbenchUiLine>>* Source, TSharedPtr<SListView<TSharedPtr<FWorkbenchUiLine>>>& OutList);
	FText MessageToText(const FWorkbenchValidationMessage& Message);
}

/** 대상 DataTable 선택: 에셋 목록, 메모리 합성 테이블, 합성 에셋 생성. */
class SWorkbenchTablePicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorkbenchTablePicker) {}
		SLATE_EVENT(FWorkbenchTableChosen, OnTableChosen)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	UDataTable* GetTable() const { return Current.Get(); }

private:
	struct FOption
	{
		FString Label;
		FString ObjectPath;
	};

	void RefreshOptions();
	void ChooseOption(TSharedPtr<FOption> Option);
	void UseMemoryFixture();
	void CreateOrLoadDiskAsset();
	void SetCurrent(UDataTable* Table, const FText& Message);
	TSharedRef<SWidget> GenerateOption(TSharedPtr<FOption> Option);
	FText GetSelectedLabel() const;

	TArray<TSharedPtr<FOption>> Options;
	TSharedPtr<FOption> Selected;
	TSharedPtr<SComboBox<TSharedPtr<FOption>>> Combo;
	TWeakObjectPtr<UDataTable> Current;
	TStrongObjectPtr<UDataTable> KeepAlive;
	FText Status;
	FWorkbenchTableChosen OnTableChosen;
};

/** T01 아이템 편집기. */
class SWorkbenchItemEditorPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorkbenchItemEditorPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	struct FItemRow
	{
		FName RowName;
		FName DefinitionId;
		FString DisplayName;
		FName Category;
		int32 Price = 0;
		float Weight = 0.f;
		bool bHasError = false;
		bool bHasWarning = false;
	};

	class SItemRowWidget;

	void SetTable(UDataTable* NewTable);
	void ReloadSnapshot();
	void RebuildVisibleRows();
	void OnSortChanged(EColumnSortPriority::Type Priority, const FName& Column, EColumnSortMode::Type Mode);
	TSharedRef<ITableRow> GenerateRow(TSharedPtr<FItemRow> Item, const TSharedRef<STableViewBase>& Owner);
	void OnSelectionChanged(TSharedPtr<FItemRow> Item, ESelectInfo::Type SelectInfo);
	void OnSearchChanged(const FText& Text);
	void OnDraftEdited(const FPropertyChangedEvent& Event);
	void RefreshDraftMessages();
	void AddResultLine(const FText& Text, const FLinearColor& Color);

	FReply OnRevert();
	FReply OnRequery();
	FReply OnApply();
	FReply OnSave();
	FReply OnUndo();
	FReply OnReleaseOwnership();

	bool CanApply() const;
	bool CanSave() const;
	bool IsTableStale() const;
	FText GetBannerText() const;
	FSlateColor GetBannerColor() const;
	FText GetSummaryText() const;
	EActiveTimerReturnType PollStale(double InCurrentTime, float InDeltaTime);

	TSharedPtr<SWorkbenchTablePicker> Picker;
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<SListView<TSharedPtr<FItemRow>>> ListView;
	TSharedPtr<SListView<TSharedPtr<FWorkbenchUiLine>>> DraftList;
	TSharedPtr<SListView<TSharedPtr<FWorkbenchUiLine>>> ResultList;

	TWeakObjectPtr<UDataTable> Table;
	FWorkbenchTableSnapshot Snapshot;
	bool bHaveSnapshot = false;
	TArray<FWorkbenchValidationMessage> TableMessages;

	TArray<TSharedPtr<FItemRow>> AllRows;
	TArray<TSharedPtr<FItemRow>> VisibleRows;
	FString SearchText;
	bool bOnlyProblemRows = false;
	FName SortColumn = TEXT("RowName");
	EColumnSortMode::Type SortMode = EColumnSortMode::Ascending;

	TSharedPtr<FWorkbenchItemDraftSession> Session;
	TArray<TSharedPtr<FWorkbenchUiLine>> DraftLines;
	TArray<TSharedPtr<FWorkbenchUiLine>> ResultLines;
	FText EditabilityReason;
	bool bEditable = false;
	bool bTableStale = false;
	bool bDraftBlocking = false;
	int32 DraftChangeCount = 0;
};

/** T05 독립 데이터 검증 패널. */
class SWorkbenchValidationPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorkbenchValidationPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void RunValidation();
	FReply OnRun();

	TSharedPtr<SWorkbenchTablePicker> Picker;
	TSharedPtr<SListView<TSharedPtr<FWorkbenchUiLine>>> LinesList;
	TArray<TSharedPtr<FWorkbenchUiLine>> Lines;
	FText Summary;
};

/** T08 CSV 일괄 편집 패널. */
class SWorkbenchCsvPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorkbenchCsvPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnExport();
	FReply OnSaveFile();
	FReply OnLoadFile();
	FReply OnPreview();
	FReply OnApply();
	FReply OnSavePackage();
	FReply OnUndo();

	bool CanApply() const;
	void AddLine(const FText& Text, const FLinearColor& Color);

	TSharedPtr<SWorkbenchTablePicker> Picker;
	TSharedPtr<SMultiLineEditableTextBox> CsvBox;
	TSharedPtr<SListView<TSharedPtr<FWorkbenchUiLine>>> LinesList;
	TArray<TSharedPtr<FWorkbenchUiLine>> Lines;

	FWorkbenchCsvPatchManifest LastManifest;
	FWorkbenchEditPreview LastPreview;
	bool bPreviewReady = false;
	FString PreviewedCsv;
};
