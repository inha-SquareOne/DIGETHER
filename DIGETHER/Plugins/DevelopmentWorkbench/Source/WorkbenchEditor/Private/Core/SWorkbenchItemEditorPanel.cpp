#include "Core/SWorkbenchCoreWidgets.h"

#include "Core/WorkbenchItemFixture.h"
#include "Core/WorkbenchItemValidation.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "UObject/Package.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "WorkbenchItemEditorPanel"

namespace
{
	const FLinearColor ErrorColor(1.f, 0.4f, 0.4f);
	const FLinearColor WarningColor(1.f, 0.85f, 0.3f);
	const FLinearColor OkColor(0.5f, 0.9f, 0.5f);
	const FLinearColor InfoColor(0.75f, 0.75f, 0.75f);
}

class SWorkbenchItemEditorPanel::SItemRowWidget : public SMultiColumnTableRow<TSharedPtr<FItemRow>>
{
public:
	SLATE_BEGIN_ARGS(SItemRowWidget) {}
		SLATE_ARGUMENT(TSharedPtr<FItemRow>, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable)
	{
		Item = InArgs._Item;
		SMultiColumnTableRow<TSharedPtr<FItemRow>>::Construct(FSuperRowType::FArguments(), OwnerTable);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		FText Text;
		FLinearColor Color = FLinearColor::White;
		if (ColumnName == TEXT("RowName"))
		{
			Text = FText::FromName(Item->RowName);
		}
		else if (ColumnName == TEXT("DefinitionId"))
		{
			Text = FText::FromName(Item->DefinitionId);
		}
		else if (ColumnName == TEXT("DisplayName"))
		{
			Text = FText::FromString(Item->DisplayName);
		}
		else if (ColumnName == TEXT("Price"))
		{
			Text = FText::AsNumber(Item->Price);
		}
		else if (ColumnName == TEXT("Weight"))
		{
			Text = FText::FromString(FString::SanitizeFloat(Item->Weight));
		}
		else
		{
			if (Item->bHasError)
			{
				Text = LOCTEXT("RowError", "오류");
				Color = ErrorColor;
			}
			else if (Item->bHasWarning)
			{
				Text = LOCTEXT("RowWarning", "경고/결정 대기");
				Color = WarningColor;
			}
			else
			{
				Text = LOCTEXT("RowOk", "구조 정상");
				Color = OkColor;
			}
		}
		return SNew(STextBlock).Text(Text).ColorAndOpacity(Color);
	}

private:
	TSharedPtr<FItemRow> Item;
};

#define WB_COLUMN(Id, LabelText, Fill) \
	+ SHeaderRow::Column(TEXT(Id)).DefaultLabel(LabelText).FillWidth(Fill) \
	.SortMode_Lambda([this]() { return SortColumn == FName(TEXT(Id)) ? SortMode : EColumnSortMode::None; }) \
	.OnSort(FOnSortModeChanged::CreateSP(this, &SWorkbenchItemEditorPanel::OnSortChanged))

void SWorkbenchItemEditorPanel::Construct(const FArguments& InArgs)
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = false;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = PropertyModule.CreateDetailView(DetailsArgs);
	DetailsView->OnFinishedChangingProperties().AddSP(this, &SWorkbenchItemEditorPanel::OnDraftEdited);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
		[
			SAssignNew(Picker, SWorkbenchTablePicker)
			.OnTableChosen_Lambda([this](UDataTable* Chosen) { SetTable(Chosen); })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
		[
			SNew(SBorder)
			.Padding(6.f)
			[
				SNew(STextBlock)
				.Text(this, &SWorkbenchItemEditorPanel::GetBannerText)
				.ColorAndOpacity(this, &SWorkbenchItemEditorPanel::GetBannerColor)
				.AutoWrapText(true)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SEditableTextBox)
				.HintText(LOCTEXT("SearchHint", "검색: 행 이름, DefinitionId, 표시 이름, 종류"))
				.OnTextChanged(this, &SWorkbenchItemEditorPanel::OnSearchChanged)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bOnlyProblemRows ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
				{
					bOnlyProblemRows = State == ECheckBoxState::Checked;
					RebuildVisibleRows();
				})
				[
					SNew(STextBlock).Text(LOCTEXT("OnlyProblems", "문제 행만 보기"))
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(4.f, 0.f)
		[
			SNew(STextBlock)
			.Text(this, &SWorkbenchItemEditorPanel::GetSummaryText)
			.ColorAndOpacity(InfoColor)
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(4.f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			+ SSplitter::Slot().Value(0.55f)
			[
				SAssignNew(ListView, SListView<TSharedPtr<FItemRow>>)
				.ListItemsSource(&VisibleRows)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &SWorkbenchItemEditorPanel::GenerateRow)
				.OnSelectionChanged(this, &SWorkbenchItemEditorPanel::OnSelectionChanged)
				.HeaderRow
				(
					SNew(SHeaderRow)
					WB_COLUMN("RowName", LOCTEXT("ColRowName", "행 이름"), 0.2f)
					WB_COLUMN("DefinitionId", LOCTEXT("ColDefinitionId", "DefinitionId"), 0.2f)
					WB_COLUMN("DisplayName", LOCTEXT("ColDisplayName", "표시 이름"), 0.25f)
					WB_COLUMN("Price", LOCTEXT("ColPrice", "가격"), 0.1f)
					WB_COLUMN("Weight", LOCTEXT("ColWeight", "중량"), 0.1f)
					WB_COLUMN("Check", LOCTEXT("ColCheck", "구조 검증"), 0.15f)
				)
			]
			+ SSplitter::Slot().Value(0.45f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().FillHeight(0.5f)
				[
					SNew(SBox)
					.IsEnabled_Lambda([this]() { return Session.IsValid() && bEditable; })
					[
						DetailsView.ToSharedRef()
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DraftHeader", "초안 변경과 검증 (원본은 '적용'을 누르기 전까지 바뀌지 않습니다)"))
					.ColorAndOpacity(InfoColor)
				]
				+ SVerticalBox::Slot().FillHeight(0.5f)
				[
					WorkbenchCoreUi::MakeLineList(&DraftLines, DraftList)
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Revert", "초안 되돌리기"))
				.ToolTipText(LOCTEXT("RevertTip", "초안을 조회 시점 값으로 되돌립니다."))
				.IsEnabled_Lambda([this]() { return Session.IsValid(); })
				.OnClicked(this, &SWorkbenchItemEditorPanel::OnRevert)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Requery", "재조회"))
				.ToolTipText(LOCTEXT("RequeryTip", "원본을 다시 읽습니다. 미적용 초안은 사라집니다."))
				.IsEnabled_Lambda([this]() { return Table.IsValid(); })
				.OnClicked(this, &SWorkbenchItemEditorPanel::OnRequery)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Apply", "적용 (메모리, Undo 가능)"))
				.ToolTipText(LOCTEXT("ApplyTip", "초안을 원본 테이블에 적용합니다. 디스크에는 저장하지 않습니다."))
				.IsEnabled(this, &SWorkbenchItemEditorPanel::CanApply)
				.OnClicked(this, &SWorkbenchItemEditorPanel::OnApply)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Save", "저장 (패키지)"))
				.ToolTipText(LOCTEXT("SaveTip", "이 도구가 적용한 변경이 있는 패키지를 백업과 저널을 남기고 저장합니다."))
				.IsEnabled(this, &SWorkbenchItemEditorPanel::CanSave)
				.OnClicked(this, &SWorkbenchItemEditorPanel::OnSave)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Undo", "에디터 Undo"))
				.ToolTipText(LOCTEXT("UndoTip", "에디터의 마지막 트랜잭션을 되돌립니다(Ctrl+Z와 같음). 적용 직후가 아니면 다른 작업이 되돌려질 수 있습니다."))
				.IsEnabled_Lambda([this]() { return GEditor != nullptr && Table.IsValid(); })
				.OnClicked(this, &SWorkbenchItemEditorPanel::OnUndo)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ReleaseOwnership", "소유권 해제"))
				.ToolTipText(LOCTEXT("ReleaseOwnershipTip", "외부 변경으로 적용·저장이 차단됐을 때 Workbench의 기록만 버립니다. 데이터는 바꾸지 않으며, 저장되지 않은 변경이 남아 있으면 에디터에서 저장하거나 되돌려야 편집을 다시 시작할 수 있습니다."))
				.IsEnabled_Lambda([this]() { return Table.IsValid() && WorkbenchDataTableEditAdapter::GetOwnership(Table.Get()) != EWorkbenchOwnership::None; })
				.OnClicked(this, &SWorkbenchItemEditorPanel::OnReleaseOwnership)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().MaxHeight(140.f).Padding(4.f)
		[
			WorkbenchCoreUi::MakeLineList(&ResultLines, ResultList)
		]
	];

	RegisterActiveTimer(1.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SWorkbenchItemEditorPanel::PollStale));
	RefreshDraftMessages();
}

#undef WB_COLUMN

void SWorkbenchItemEditorPanel::SetTable(UDataTable* NewTable)
{
	Table = NewTable;
	Session.Reset();
	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(nullptr);
	}
	ReloadSnapshot();
	RefreshDraftMessages();
}

void SWorkbenchItemEditorPanel::ReloadSnapshot()
{
	AllRows.Reset();
	TableMessages.Reset();
	bHaveSnapshot = false;
	bTableStale = false;

	UDataTable* Target = Table.Get();
	if (!Target)
	{
		RebuildVisibleRows();
		return;
	}

	FText Error;
	if (!WorkbenchDataTableEditAdapter::TakeSnapshot(Target, Snapshot, Error))
	{
		AddResultLine(FText::Format(LOCTEXT("SnapshotFail", "조회 실패: {0}"), Error), ErrorColor);
		RebuildVisibleRows();
		return;
	}
	bHaveSnapshot = true;
	bEditable = WorkbenchDataTableEditAdapter::EvaluateEditability(Target, EditabilityReason);

	TableMessages = WorkbenchItemValidation::ValidateTable(Snapshot.Rows);
	TSet<FName> ErrorRows;
	TSet<FName> WarningRows;
	for (const FWorkbenchValidationMessage& Message : TableMessages)
	{
		if (Message.TargetId.IsNone())
		{
			continue;
		}
		if (Message.IsBlocking())
		{
			ErrorRows.Add(Message.TargetId);
		}
		else if (Message.State == EWorkbenchValidationState::Invalid || Message.State == EWorkbenchValidationState::PendingDecision)
		{
			WarningRows.Add(Message.TargetId);
		}
	}

	for (const FName& RowName : Snapshot.RowOrder)
	{
		const FWorkbenchSyntheticItemRow* Row = Snapshot.Rows.Find(RowName);
		if (!Row)
		{
			continue;
		}
		TSharedPtr<FItemRow> Item = MakeShared<FItemRow>();
		Item->RowName = RowName;
		Item->DefinitionId = Row->DefinitionId;
		Item->DisplayName = Row->DisplayName;
		Item->Category = Row->Category;
		Item->Price = Row->Price;
		Item->Weight = Row->Weight;
		Item->bHasError = ErrorRows.Contains(RowName);
		Item->bHasWarning = WarningRows.Contains(RowName);
		AllRows.Add(Item);
	}
	RebuildVisibleRows();

	if (Session.IsValid())
	{
		const FName Selected = Session->GetRowName();
		const TSharedPtr<FItemRow>* Found = VisibleRows.FindByPredicate([Selected](const TSharedPtr<FItemRow>& Row) { return Row->RowName == Selected; });
		if (Found && ListView.IsValid())
		{
			ListView->SetSelection(*Found, ESelectInfo::Direct);
		}
	}
}

void SWorkbenchItemEditorPanel::RebuildVisibleRows()
{
	VisibleRows.Reset();
	for (const TSharedPtr<FItemRow>& Row : AllRows)
	{
		if (bOnlyProblemRows && !Row->bHasError && !Row->bHasWarning)
		{
			continue;
		}
		if (!SearchText.IsEmpty())
		{
			const bool bMatch = Row->RowName.ToString().Contains(SearchText)
				|| Row->DefinitionId.ToString().Contains(SearchText)
				|| Row->DisplayName.Contains(SearchText)
				|| Row->Category.ToString().Contains(SearchText);
			if (!bMatch)
			{
				continue;
			}
		}
		VisibleRows.Add(Row);
	}

	const FName Column = SortColumn;
	const bool bAscending = SortMode != EColumnSortMode::Descending;
	VisibleRows.Sort([Column, bAscending](const TSharedPtr<FItemRow>& A, const TSharedPtr<FItemRow>& B)
	{
		int32 Compare = 0;
		if (Column == TEXT("DefinitionId"))
		{
			Compare = A->DefinitionId.Compare(B->DefinitionId);
		}
		else if (Column == TEXT("DisplayName"))
		{
			Compare = A->DisplayName.Compare(B->DisplayName, ESearchCase::IgnoreCase);
		}
		else if (Column == TEXT("Price"))
		{
			Compare = A->Price < B->Price ? -1 : (A->Price > B->Price ? 1 : 0);
		}
		else if (Column == TEXT("Weight"))
		{
			Compare = A->Weight < B->Weight ? -1 : (A->Weight > B->Weight ? 1 : 0);
		}
		else if (Column == TEXT("Check"))
		{
			const int32 RankA = A->bHasError ? 2 : (A->bHasWarning ? 1 : 0);
			const int32 RankB = B->bHasError ? 2 : (B->bHasWarning ? 1 : 0);
			Compare = RankA - RankB;
		}
		else
		{
			Compare = A->RowName.Compare(B->RowName);
		}
		if (Compare != 0)
		{
			return bAscending ? Compare < 0 : Compare > 0;
		}
		return A->RowName.Compare(B->RowName) < 0;
	});

	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
}

void SWorkbenchItemEditorPanel::OnSortChanged(EColumnSortPriority::Type, const FName& Column, EColumnSortMode::Type Mode)
{
	SortColumn = Column;
	SortMode = Mode == EColumnSortMode::None ? EColumnSortMode::Ascending : Mode;
	RebuildVisibleRows();
}

TSharedRef<ITableRow> SWorkbenchItemEditorPanel::GenerateRow(TSharedPtr<FItemRow> Item, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(SItemRowWidget, Owner).Item(Item);
}

void SWorkbenchItemEditorPanel::OnSelectionChanged(TSharedPtr<FItemRow> Item, ESelectInfo::Type)
{
	if (!Item.IsValid() || !Table.IsValid())
	{
		return;
	}
	if (Session.IsValid() && Session->GetRowName() == Item->RowName)
	{
		return;
	}
	if (Session.IsValid() && Session->IsDirty())
	{
		AddResultLine(
			FText::Format(LOCTEXT("DraftDiscarded", "선택을 바꿔 '{0}'의 미적용 초안을 버렸습니다."), FText::FromName(Session->GetRowName())),
			WarningColor);
	}

	Session = MakeShared<FWorkbenchItemDraftSession>(Table.Get(), Item->RowName);	if (!Session->IsValid())
	{
		AddResultLine(FText::Format(LOCTEXT("RowReadFail", "행 '{0}'을 읽지 못했습니다. 재조회하세요."), FText::FromName(Item->RowName)), ErrorColor);
		Session.Reset();
	}
	DetailsView->SetObject(Session.IsValid() ? Session->GetDraftObject() : nullptr);
	RefreshDraftMessages();
}

void SWorkbenchItemEditorPanel::OnSearchChanged(const FText& Text)
{
	SearchText = Text.ToString();
	RebuildVisibleRows();
}

void SWorkbenchItemEditorPanel::OnDraftEdited(const FPropertyChangedEvent&)
{
	RefreshDraftMessages();
}

void SWorkbenchItemEditorPanel::RefreshDraftMessages()
{
	DraftLines.Reset();
	DraftChangeCount = 0;
	bDraftBlocking = false;

	if (Session.IsValid())
	{
		const TArray<FWorkbenchFieldChange> Changes = Session->GetChanges();
		DraftChangeCount = Changes.Num();
		if (Changes.IsEmpty())
		{
			DraftLines.Add(WorkbenchCoreUi::MakeLine(LOCTEXT("NoChanges", "초안에 변경이 없습니다."), InfoColor));
		}
		for (const FWorkbenchFieldChange& Change : Changes)
		{
			DraftLines.Add(WorkbenchCoreUi::MakeLine(
				FText::Format(LOCTEXT("ChangeLine", "변경  {0}: {1} → {2}"), FText::FromName(Change.Column), FText::FromString(Change.OldText), FText::FromString(Change.NewText)),
				FLinearColor::White));
		}
		const TArray<FWorkbenchValidationMessage> Messages = Session->Validate();
		bDraftBlocking = WorkbenchItemValidation::HasBlocking(Messages);
		for (const FWorkbenchValidationMessage& Message : Messages)
		{
			DraftLines.Add(WorkbenchCoreUi::MakeLine(WorkbenchCoreUi::MessageToText(Message), WorkbenchCoreUi::ColorForMessage(Message)));
		}
	}
	else
	{
		DraftLines.Add(WorkbenchCoreUi::MakeLine(LOCTEXT("SelectRow", "목록에서 행을 선택하면 초안이 만들어집니다. 아래는 테이블 단위 검증 결과입니다."), InfoColor));
		for (const FWorkbenchValidationMessage& Message : TableMessages)
		{
			if (Message.TargetId.IsNone())
			{
				DraftLines.Add(WorkbenchCoreUi::MakeLine(WorkbenchCoreUi::MessageToText(Message), WorkbenchCoreUi::ColorForMessage(Message)));
			}
		}
	}

	if (DraftList.IsValid())
	{
		DraftList->RequestListRefresh();
	}
}

void SWorkbenchItemEditorPanel::AddResultLine(const FText& Text, const FLinearColor& Color)
{
	ResultLines.Insert(WorkbenchCoreUi::MakeLine(Text, Color), 0);
	if (ResultLines.Num() > 200)
	{
		ResultLines.SetNum(200);
	}
	if (ResultList.IsValid())
	{
		ResultList->RequestListRefresh();
	}
}

FReply SWorkbenchItemEditorPanel::OnRevert()
{
	if (Session.IsValid())
	{
		Session->Revert();
		RefreshDraftMessages();
		if (DetailsView.IsValid())
		{
			DetailsView->ForceRefresh();
		}
		AddResultLine(LOCTEXT("Reverted", "초안을 조회 시점 값으로 되돌렸습니다."), InfoColor);
	}
	return FReply::Handled();
}

FReply SWorkbenchItemEditorPanel::OnRequery()
{
	if (Session.IsValid())
	{
		if (!Session->Requery())
		{
			AddResultLine(FText::Format(LOCTEXT("RequeryMissing", "행 '{0}'이 더 이상 없습니다."), FText::FromName(Session->GetRowName())), ErrorColor);
			Session.Reset();
			DetailsView->SetObject(nullptr);
		}
	}
	ReloadSnapshot();
	RefreshDraftMessages();
	if (DetailsView.IsValid())
	{
		DetailsView->ForceRefresh();
	}
	AddResultLine(LOCTEXT("Requeried", "원본을 다시 조회했습니다."), InfoColor);
	return FReply::Handled();
}

FReply SWorkbenchItemEditorPanel::OnApply()
{
	if (!CanApply())
	{
		return FReply::Handled();
	}
	const FWorkbenchApplyResult Result = Session->Apply();
	if (Result.bSuccess)
	{
		AddResultLine(
			FText::Format(LOCTEXT("Applied", "적용했습니다(행 {0}개). 아직 디스크에 저장되지 않았습니다."), FText::AsNumber(Result.AppliedRows.Num())),
			OkColor);
	}
	else
	{
		for (const FText& Error : Result.Errors)
		{
			AddResultLine(FText::Format(LOCTEXT("ApplyFail", "적용 실패: {0}"), Error), ErrorColor);
		}
	}
	ReloadSnapshot();
	RefreshDraftMessages();
	if (DetailsView.IsValid())
	{
		DetailsView->ForceRefresh();
	}
	return FReply::Handled();
}

FReply SWorkbenchItemEditorPanel::OnSave()
{
	UDataTable* Target = Table.Get();
	if (!Target)
	{
		return FReply::Handled();
	}
	const FWorkbenchSaveResult Result = WorkbenchDataTableEditAdapter::SaveTablePackage(Target);
	AddResultLine(Result.Message, Result.bSaved ? OkColor : ErrorColor);
	if (!Result.JobId.IsEmpty())
	{
		AddResultLine(FText::Format(LOCTEXT("SaveJob", "저장 작업 ID: {0}"), FText::FromString(Result.JobId)), InfoColor);
	}
	if (!Result.BackupPath.IsEmpty())
	{
		AddResultLine(FText::Format(LOCTEXT("SaveBackup", "백업: {0}"), FText::FromString(Result.BackupPath)), InfoColor);
	}
	if (!Result.JournalPath.IsEmpty() && (Result.bSaved || !Result.JobId.IsEmpty()))
	{
		AddResultLine(FText::Format(LOCTEXT("SaveJournal", "저널: {0}"), FText::FromString(Result.JournalPath)), InfoColor);
	}
	if (!Result.bJournalOk)
	{
		AddResultLine(FText::Format(LOCTEXT("SaveJournalWarn", "저널 경고: {0}"), Result.JournalWarning), ErrorColor);
	}
	ReloadSnapshot();
	RefreshDraftMessages();
	return FReply::Handled();
}

FReply SWorkbenchItemEditorPanel::OnUndo()
{
	if (GEditor)
	{
		const bool bUndone = GEditor->UndoTransaction();
		if (!bUndone)
		{
			AddResultLine(LOCTEXT("NothingToUndo", "되돌릴 트랜잭션이 없습니다."), InfoColor);
		}
		else if (WorkbenchDataTableEditAdapter::GetOwnership(Table.Get()) == EWorkbenchOwnership::Broken)
		{
			AddResultLine(LOCTEXT("UndoBroken", "되돌린 결과가 Workbench가 적용한 어떤 상태와도 달라(다른 트랜잭션이 되돌려졌을 수 있음) 적용·저장을 차단했습니다. '소유권 해제'로 복구하세요."), ErrorColor);
		}
		else
		{
			AddResultLine(LOCTEXT("Undone", "마지막 트랜잭션을 되돌렸습니다. 이 상태는 Workbench가 적용한 이력과 일치해 그대로 이어서 작업·저장할 수 있습니다. 초안은 새로 조회한 값으로 바뀝니다."), WarningColor);
		}
		if (Session.IsValid())
		{
			Session->Requery();
		}
		ReloadSnapshot();
		RefreshDraftMessages();
		if (DetailsView.IsValid())
		{
			DetailsView->ForceRefresh();
		}
	}
	return FReply::Handled();
}

FReply SWorkbenchItemEditorPanel::OnReleaseOwnership()
{
	UDataTable* Target = Table.Get();
	if (Target)
	{
		FText Message;
		const bool bReleased = WorkbenchDataTableEditAdapter::ReleaseWorkbenchOwnership(Target, Message);
		AddResultLine(Message, bReleased ? WarningColor : InfoColor);
		ReloadSnapshot();
		RefreshDraftMessages();
	}
	return FReply::Handled();
}

bool SWorkbenchItemEditorPanel::CanApply() const
{
	return Session.IsValid() && bEditable && !bTableStale && !Session->IsStale() && DraftChangeCount > 0 && !bDraftBlocking;
}

bool SWorkbenchItemEditorPanel::CanSave() const
{
	const UDataTable* Target = Table.Get();
	return Target && Target->GetOutermost() != GetTransientPackage()
		&& WorkbenchDataTableEditAdapter::GetOwnership(Target) == EWorkbenchOwnership::Owned
		&& WorkbenchDataTableEditAdapter::GetPendingChangeCount(Target) > 0;
}

bool SWorkbenchItemEditorPanel::IsTableStale() const
{
	return bTableStale;
}

FText SWorkbenchItemEditorPanel::GetBannerText() const
{
	const UDataTable* Target = Table.Get();
	if (!Target)
	{
		return LOCTEXT("BannerNoTable", "대상 테이블이 선택되지 않았습니다. 위에서 테이블을 고르거나 메모리 합성 테이블을 만드세요.");
	}

	TArray<FText> Parts;
	Parts.Add(FText::Format(LOCTEXT("BannerTable", "테이블: {0}"), FText::FromString(Target->GetPathName())));
	if (Target->GetOutermost() == GetTransientPackage())
	{
		Parts.Add(LOCTEXT("BannerMemory", "메모리 전용(저장 불가)"));
	}
	Parts.Add(bEditable ? LOCTEXT("BannerEditable", "편집 가능") : FText::Format(LOCTEXT("BannerReadOnly", "편집 불가: {0}"), EditabilityReason));
	if (bTableStale)
	{
		Parts.Add(LOCTEXT("BannerStale", "원본이 조회 뒤 바뀜 — 재조회 필요"));
	}
	else if (Session.IsValid() && Session->IsStale())
	{
		Parts.Add(LOCTEXT("BannerRowStale", "선택 행이 조회 뒤 바뀜 — 재조회 필요"));
	}
	if (Session.IsValid())
	{
		Parts.Add(FText::Format(LOCTEXT("BannerDraft", "초안 변경 {0}개 필드(미적용)"), FText::AsNumber(DraftChangeCount)));
	}
	if (WorkbenchDataTableEditAdapter::GetOwnership(Target) == EWorkbenchOwnership::Broken)
	{
		Parts.Add(LOCTEXT("BannerBroken", "소유권 깨짐 — 외부 변경으로 적용·저장 차단. '소유권 해제'로 복구"));
	}
	const int32 Pending = WorkbenchDataTableEditAdapter::GetPendingChangeCount(Target);
	Parts.Add(Pending > 0
		? FText::Format(LOCTEXT("BannerPending", "적용됨·저장 전 {0}건"), FText::AsNumber(Pending))
		: LOCTEXT("BannerNoPending", "저장 전 변경 없음"));
	return FText::Join(FText::FromString(TEXT("  |  ")), Parts);
}

FSlateColor SWorkbenchItemEditorPanel::GetBannerColor() const
{
	if (!Table.IsValid())
	{
		return FSlateColor(InfoColor);
	}
	if (!bEditable || bTableStale || (Session.IsValid() && Session->IsStale()))
	{
		return FSlateColor(WarningColor);
	}
	return FSlateColor(FLinearColor::White);
}

FText SWorkbenchItemEditorPanel::GetSummaryText() const
{
	int32 Errors = 0;
	int32 Warnings = 0;
	for (const TSharedPtr<FItemRow>& Row : AllRows)
	{
		Errors += Row->bHasError ? 1 : 0;
		Warnings += (!Row->bHasError && Row->bHasWarning) ? 1 : 0;
	}
	return FText::Format(
		LOCTEXT("Summary", "전체 {0}행 · 표시 {1}행 · 오류 행 {2} · 경고/결정 대기 행 {3} · 전역 DefinitionId 인덱스는 검사하지 않음"),
		FText::AsNumber(AllRows.Num()),
		FText::AsNumber(VisibleRows.Num()),
		FText::AsNumber(Errors),
		FText::AsNumber(Warnings));
}

EActiveTimerReturnType SWorkbenchItemEditorPanel::PollStale(double, float)
{
	UDataTable* Target = Table.Get();
	if (Target)
	{
		bEditable = WorkbenchDataTableEditAdapter::EvaluateEditability(Target, EditabilityReason);
		uint32 Hash = 0;
		if (bHaveSnapshot && WorkbenchDataTableEditAdapter::TryComputeTableHash(Target, Hash))
		{
			bTableStale = Hash != Snapshot.TableHash;
		}
		if (Session.IsValid())
		{
			Session->RecomputeStale();
		}
	}
	return EActiveTimerReturnType::Continue;
}

#undef LOCTEXT_NAMESPACE
