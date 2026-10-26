#include "Core/SWorkbenchCoreWidgets.h"

#include "Core/WorkbenchItemValidation.h"
#include "Engine/DataTable.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WorkbenchValidationPanel"

void SWorkbenchValidationPanel::Construct(const FArguments& InArgs)
{
	Summary = LOCTEXT("SummaryInitial", "아직 검증하지 않았습니다. 테이블을 고르고 '검증 실행'을 누르세요.");

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
		[
			SAssignNew(Picker, SWorkbenchTablePicker)
			.OnTableChosen_Lambda([this](UDataTable*)
			{
				Lines.Reset();
				Summary = LOCTEXT("SummaryChosen", "테이블이 바뀌었습니다. '검증 실행'을 누르세요.");
				if (LinesList.IsValid())
				{
					LinesList->RequestListRefresh();
				}
			})
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Run", "검증 실행"))
				.ToolTipText(LOCTEXT("RunTip", "UI와 엔진 DataValidation이 쓰는 같은 규칙으로 읽기 전용 검사를 합니다. 데이터를 바꾸지 않습니다."))
				.IsEnabled_Lambda([this]() { return Picker.IsValid() && Picker->GetTable() != nullptr; })
				.OnClicked(this, &SWorkbenchValidationPanel::OnRun)
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return Summary; })
				.AutoWrapText(true)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PolicyNote", "가격 0 허용 여부 등 승인된 정책이 없는 규칙은 '결정 대기'로 표시하며 통과로 세지 않습니다. 프로젝트 전역 DefinitionId 충돌은 검사하지 않습니다."))
			.ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f))
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(4.f)
		[
			WorkbenchCoreUi::MakeLineList(&Lines, LinesList)
		]
	];
}

FReply SWorkbenchValidationPanel::OnRun()
{
	RunValidation();
	return FReply::Handled();
}

void SWorkbenchValidationPanel::RunValidation()
{
	Lines.Reset();
	UDataTable* Target = Picker.IsValid() ? Picker->GetTable() : nullptr;

	FWorkbenchTableSnapshot Snapshot;
	FText Error;
	if (!Target || !WorkbenchDataTableEditAdapter::TakeSnapshot(Target, Snapshot, Error))
	{
		Summary = Target
			? FText::Format(LOCTEXT("SnapshotFail", "검증하지 못했습니다: {0}"), Error)
			: LOCTEXT("NoTable", "대상 테이블이 없습니다.");
		if (LinesList.IsValid())
		{
			LinesList->RequestListRefresh();
		}
		return;
	}

	const TArray<FWorkbenchValidationMessage> Messages = WorkbenchItemValidation::ValidateTable(Snapshot.Rows);
	TMap<EWorkbenchValidationState, int32> StateCounts;
	int32 Blocking = 0;
	for (const FWorkbenchValidationMessage& Message : Messages)
	{
		StateCounts.FindOrAdd(Message.State)++;
		Blocking += Message.IsBlocking() ? 1 : 0;
		Lines.Add(WorkbenchCoreUi::MakeLine(
			FText::Format(LOCTEXT("RuleLine", "{0} (규칙 v{1})"), WorkbenchCoreUi::MessageToText(Message), FText::AsNumber(Message.RuleVersion)),
			WorkbenchCoreUi::ColorForMessage(Message)));
	}

	TArray<FText> Parts;
	Parts.Add(FText::Format(LOCTEXT("SummaryRows", "{0}행 검사"), FText::AsNumber(Snapshot.Rows.Num())));
	Parts.Add(FText::Format(LOCTEXT("SummaryBlocking", "차단 오류 {0}건"), FText::AsNumber(Blocking)));
	for (const TPair<EWorkbenchValidationState, int32>& Pair : StateCounts)
	{
		Parts.Add(FText::Format(LOCTEXT("SummaryState", "{0} {1}건"), WorkbenchItemValidation::StateToText(Pair.Key), FText::AsNumber(Pair.Value)));
	}
	Summary = FText::Join(FText::FromString(TEXT(" · ")), Parts);

	if (Messages.IsEmpty())
	{
		Lines.Add(WorkbenchCoreUi::MakeLine(LOCTEXT("NoMessages", "메시지가 없습니다."), FLinearColor(0.5f, 0.9f, 0.5f)));
	}
	if (LinesList.IsValid())
	{
		LinesList->RequestListRefresh();
	}
}

#undef LOCTEXT_NAMESPACE
