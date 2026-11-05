#include "Core/SWorkbenchCoreWidgets.h"

#include "Core/WorkbenchItemFixture.h"
#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "Framework/Application/SlateApplication.h"
#include "IDesktopPlatform.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WorkbenchCsvPanel"

namespace
{
	const FLinearColor CsvErrorColor(1.f, 0.4f, 0.4f);
	const FLinearColor CsvWarningColor(1.f, 0.85f, 0.3f);
	const FLinearColor CsvOkColor(0.5f, 0.9f, 0.5f);
	const FLinearColor CsvInfoColor(0.75f, 0.75f, 0.75f);

	const void* GetDialogParent()
	{
		return FSlateApplication::IsInitialized() ? FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr) : nullptr;
	}
}

void SWorkbenchCsvPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
		[
			SAssignNew(Picker, SWorkbenchTablePicker)
			.OnTableChosen_Lambda([this](UDataTable*)
			{
				bPreviewReady = false;
				Lines.Reset();
				AddLine(LOCTEXT("TableChanged", "테이블이 바뀌었습니다. 내보내기 또는 CSV 입력 후 미리보기를 실행하세요."), CsvInfoColor);
			})
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("FormatHelp",
				"형식: 첫 줄(선택) #WorkbenchCsv,버전,기준해시 · 헤더 첫 열 RowId · 빈 셀은 변경 없음 · 헤더에 없는 열은 원본 유지 · "
				"Name 열 <NULL>, String 열 <BLANK>로 명시적 비움 · 중복 RowId, 알 수 없는 열·행, 로케일 숫자(1,5), NaN은 거부"))
			.ColorAndOpacity(CsvInfoColor)
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() -> FText
			{
				const UDataTable* Target = Picker.IsValid() ? Picker->GetTable() : nullptr;
				if (!Target)
				{
					return LOCTEXT("NoTable", "대상 테이블이 선택되지 않았습니다.");
				}
				const int32 Pending = WorkbenchDataTableEditAdapter::GetPendingChangeCount(Target);
				return FText::Format(
					LOCTEXT("StateLine", "{0} | 적용됨·저장 전 {1}건 | 미리보기 {2}"),
					Target->GetOutermost() == GetTransientPackage() ? LOCTEXT("MemoryTable", "메모리 테이블(저장 불가)") : LOCTEXT("DiskTable", "에셋 테이블"),
					FText::AsNumber(Pending),
					bPreviewReady ? LOCTEXT("PreviewReady", "준비됨") : LOCTEXT("PreviewNotReady", "없음 또는 입력이 바뀜"));
			})
		]
		+ SVerticalBox::Slot().FillHeight(0.5f).Padding(4.f)
		[
			SAssignNew(CsvBox, SMultiLineEditableTextBox)
			.HintText(LOCTEXT("CsvHint", "여기에 CSV를 붙여 넣거나 '현재 테이블 내보내기'로 채우세요."))
			.AutoWrapText(false)
			.OnTextChanged_Lambda([this](const FText&) { bPreviewReady = false; })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Export", "현재 테이블 내보내기"))
				.IsEnabled_Lambda([this]() { return Picker.IsValid() && Picker->GetTable() != nullptr; })
				.OnClicked(this, &SWorkbenchCsvPanel::OnExport)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("SaveFile", "CSV 파일로 저장"))
				.OnClicked(this, &SWorkbenchCsvPanel::OnSaveFile)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("LoadFile", "CSV 파일 불러오기"))
				.OnClicked(this, &SWorkbenchCsvPanel::OnLoadFile)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Preview", "미리보기"))
				.ToolTipText(LOCTEXT("PreviewTip", "CSV를 해석해 변경될 셀과 거부 사유를 보여 줍니다. 데이터는 바뀌지 않습니다."))
				.IsEnabled_Lambda([this]() { return Picker.IsValid() && Picker->GetTable() != nullptr; })
				.OnClicked(this, &SWorkbenchCsvPanel::OnPreview)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Apply", "적용 (메모리, Undo 가능)"))
				.ToolTipText(LOCTEXT("ApplyTip", "미리보기에서 오류가 없을 때만 활성화됩니다. 적용 직전에 현재 상태로 다시 검사합니다."))
				.IsEnabled(this, &SWorkbenchCsvPanel::CanApply)
				.OnClicked(this, &SWorkbenchCsvPanel::OnApply)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("SavePackage", "저장 (패키지)"))
				.IsEnabled_Lambda([this]()
				{
					const UDataTable* Target = Picker.IsValid() ? Picker->GetTable() : nullptr;
					return Target && Target->GetOutermost() != GetTransientPackage() && WorkbenchDataTableEditAdapter::GetPendingChangeCount(Target) > 0;
				})
				.OnClicked(this, &SWorkbenchCsvPanel::OnSavePackage)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Undo", "에디터 Undo"))
				.ToolTipText(LOCTEXT("UndoTip", "에디터의 마지막 트랜잭션을 되돌립니다(Ctrl+Z와 같음)."))
				.IsEnabled_Lambda([this]() { return GEditor != nullptr && Picker.IsValid() && Picker->GetTable() != nullptr; })
				.OnClicked(this, &SWorkbenchCsvPanel::OnUndo)
			]
		]
		+ SVerticalBox::Slot().FillHeight(0.5f).Padding(4.f)
		[
			WorkbenchCoreUi::MakeLineList(&Lines, LinesList)
		]
	];
}

void SWorkbenchCsvPanel::AddLine(const FText& Text, const FLinearColor& Color)
{
	Lines.Add(WorkbenchCoreUi::MakeLine(Text, Color));
	if (LinesList.IsValid())
	{
		LinesList->RequestListRefresh();
	}
}

FReply SWorkbenchCsvPanel::OnExport()
{
	UDataTable* Target = Picker.IsValid() ? Picker->GetTable() : nullptr;
	FString Csv;
	FText Error;
	if (Target && WorkbenchDataTableEditAdapter::ExportTableToCsv(Target, Csv, Error))
	{
		CsvBox->SetText(FText::FromString(Csv));
		bPreviewReady = false;
		AddLine(LOCTEXT("Exported", "현재 테이블 값을 CSV로 채웠습니다. 기준 해시가 메타 줄에 들어 있어 그 뒤 원본이 바뀌면 적용이 거부됩니다."), CsvOkColor);
	}
	else
	{
		AddLine(FText::Format(LOCTEXT("ExportFail", "내보내기 실패: {0}"), Error), CsvErrorColor);
	}
	return FReply::Handled();
}

FReply SWorkbenchCsvPanel::OnSaveFile()
{
	IDesktopPlatform* Platform = FDesktopPlatformModule::Get();
	if (!Platform)
	{
		AddLine(LOCTEXT("NoDialog", "파일 대화상자를 사용할 수 없습니다."), CsvErrorColor);
		return FReply::Handled();
	}
	TArray<FString> Files;
	if (Platform->SaveFileDialog(GetDialogParent(), TEXT("CSV 저장"), FPaths::ProjectSavedDir(), TEXT("WorkbenchItems.csv"), TEXT("CSV (*.csv)|*.csv"), EFileDialogFlags::None, Files) && !Files.IsEmpty())
	{
		const bool bOk = FFileHelper::SaveStringToFile(CsvBox->GetText().ToString(), *Files[0], FFileHelper::EEncodingOptions::ForceUTF8);
		AddLine(
			bOk ? FText::Format(LOCTEXT("FileSaved", "CSV 저장: {0}"), FText::FromString(Files[0]))
			    : FText::Format(LOCTEXT("FileSaveFail", "CSV 저장 실패: {0}"), FText::FromString(Files[0])),
			bOk ? CsvOkColor : CsvErrorColor);
	}
	return FReply::Handled();
}

FReply SWorkbenchCsvPanel::OnLoadFile()
{
	IDesktopPlatform* Platform = FDesktopPlatformModule::Get();
	if (!Platform)
	{
		AddLine(LOCTEXT("NoDialog2", "파일 대화상자를 사용할 수 없습니다."), CsvErrorColor);
		return FReply::Handled();
	}
	TArray<FString> Files;
	if (Platform->OpenFileDialog(GetDialogParent(), TEXT("CSV 불러오기"), FPaths::ProjectSavedDir(), TEXT(""), TEXT("CSV (*.csv)|*.csv"), EFileDialogFlags::None, Files) && !Files.IsEmpty())
	{
		FString Content;
		if (FFileHelper::LoadFileToString(Content, *Files[0]))
		{
			CsvBox->SetText(FText::FromString(Content));
			bPreviewReady = false;
			AddLine(FText::Format(LOCTEXT("FileLoaded", "CSV 불러옴: {0}. 미리보기를 실행하세요."), FText::FromString(Files[0])), CsvInfoColor);
		}
		else
		{
			AddLine(FText::Format(LOCTEXT("FileLoadFail", "CSV를 읽지 못했습니다: {0}"), FText::FromString(Files[0])), CsvErrorColor);
		}
	}
	return FReply::Handled();
}

FReply SWorkbenchCsvPanel::OnPreview()
{
	Lines.Reset();
	bPreviewReady = false;
	const UDataTable* Target = Picker.IsValid() ? Picker->GetTable() : nullptr;
	if (!Target)
	{
		AddLine(LOCTEXT("PreviewNoTable", "대상 테이블이 없습니다."), CsvErrorColor);
		return FReply::Handled();
	}

	PreviewedCsv = CsvBox->GetText().ToString();
	LastManifest = WorkbenchCsvPatchManifest::BuildFromCsv(PreviewedCsv, WorkbenchItemFixture::GetColumns());
	LastPreview = WorkbenchDataTableEditAdapter::PreviewCsv(Target, LastManifest);

	for (const FText& Error : LastManifest.RejectionErrors)
	{
		AddLine(FText::Format(LOCTEXT("ManifestError", "거부: {0}"), Error), CsvErrorColor);
	}
	for (const FText& Warning : LastManifest.Warnings)
	{
		AddLine(FText::Format(LOCTEXT("ManifestWarning", "경고: {0}"), Warning), CsvWarningColor);
	}
	for (const FText& Error : LastPreview.Errors)
	{
		AddLine(FText::Format(LOCTEXT("PreviewError", "오류: {0}"), Error), CsvErrorColor);
	}
	for (const FText& Warning : LastPreview.Warnings)
	{
		AddLine(FText::Format(LOCTEXT("PreviewWarning", "경고: {0}"), Warning), CsvWarningColor);
	}
	for (const FWorkbenchValidationMessage& Message : LastPreview.Messages)
	{
		AddLine(WorkbenchCoreUi::MessageToText(Message), WorkbenchCoreUi::ColorForMessage(Message));
	}
	for (const FWorkbenchFieldChange& Change : LastPreview.Changes)
	{
		AddLine(
			FText::Format(LOCTEXT("ChangeLine", "변경  {0}.{1}: {2} → {3}"), FText::FromName(Change.RowId), FText::FromName(Change.Column), FText::FromString(Change.OldText), FText::FromString(Change.NewText)),
			FLinearColor::White);
	}

	const bool bOk = LastManifest.IsAcceptable() && LastPreview.CanApply();
	AddLine(
		bOk ? FText::Format(LOCTEXT("PreviewOk", "미리보기 완료: 변경 {0}개 셀. 적용할 수 있습니다."), FText::AsNumber(LastPreview.Changes.Num()))
		    : (LastManifest.IsAcceptable() && LastPreview.Errors.IsEmpty() && LastPreview.Changes.IsEmpty()
		        ? LOCTEXT("PreviewNoChange", "미리보기 완료: 변경할 셀이 없습니다.")
		        : LOCTEXT("PreviewBlocked", "미리보기 완료: 오류가 있어 적용할 수 없습니다.")),
		bOk ? CsvOkColor : CsvWarningColor);
	bPreviewReady = LastManifest.IsAcceptable();
	return FReply::Handled();
}

bool SWorkbenchCsvPanel::CanApply() const
{
	return bPreviewReady && LastManifest.IsAcceptable() && LastPreview.CanApply() && CsvBox.IsValid() && CsvBox->GetText().ToString() == PreviewedCsv;
}

FReply SWorkbenchCsvPanel::OnApply()
{
	UDataTable* Target = Picker.IsValid() ? Picker->GetTable() : nullptr;
	if (!Target || !CanApply())
	{
		return FReply::Handled();
	}
	const FWorkbenchApplyResult Result = WorkbenchDataTableEditAdapter::ApplyCsv(Target, LastManifest);
	if (Result.bSuccess)
	{
		AddLine(FText::Format(LOCTEXT("Applied", "적용했습니다(행 {0}개). 아직 디스크에 저장되지 않았습니다."), FText::AsNumber(Result.AppliedRows.Num())), CsvOkColor);
	}
	else
	{
		for (const FText& Error : Result.Errors)
		{
			AddLine(FText::Format(LOCTEXT("ApplyFail", "적용 실패: {0}"), Error), CsvErrorColor);
		}
	}
	bPreviewReady = false;
	return FReply::Handled();
}

FReply SWorkbenchCsvPanel::OnSavePackage()
{
	UDataTable* Target = Picker.IsValid() ? Picker->GetTable() : nullptr;
	if (!Target)
	{
		return FReply::Handled();
	}
	const FWorkbenchSaveResult Result = WorkbenchDataTableEditAdapter::SaveTablePackage(Target);
	AddLine(Result.Message, Result.bSaved ? CsvOkColor : CsvErrorColor);
	if (!Result.JobId.IsEmpty())
	{
		AddLine(FText::Format(LOCTEXT("SaveJob", "저장 작업 ID: {0}"), FText::FromString(Result.JobId)), CsvInfoColor);
	}
	if (!Result.BackupPath.IsEmpty())
	{
		AddLine(FText::Format(LOCTEXT("Backup", "백업: {0}"), FText::FromString(Result.BackupPath)), CsvInfoColor);
	}
	if (!Result.JournalPath.IsEmpty() && (Result.bSaved || !Result.JobId.IsEmpty()))
	{
		AddLine(FText::Format(LOCTEXT("Journal", "저널: {0}"), FText::FromString(Result.JournalPath)), CsvInfoColor);
	}
	if (!Result.bJournalOk)
	{
		AddLine(FText::Format(LOCTEXT("JournalWarn", "저널 경고: {0}"), Result.JournalWarning), CsvErrorColor);
	}
	return FReply::Handled();
}

FReply SWorkbenchCsvPanel::OnUndo()
{
	if (GEditor)
	{
		const bool bUndone = GEditor->UndoTransaction();
		const UDataTable* Target = Picker.IsValid() ? Picker->GetTable() : nullptr;
		if (!bUndone)
		{
			AddLine(LOCTEXT("NothingToUndo", "되돌릴 트랜잭션이 없습니다."), CsvInfoColor);
		}
		else if (Target && WorkbenchDataTableEditAdapter::GetOwnership(Target) == EWorkbenchOwnership::Broken)
		{
			AddLine(LOCTEXT("UndoBroken", "되돌린 결과가 Workbench가 적용한 어떤 상태와도 달라 적용·저장을 차단했습니다. 편집기 패널의 '소유권 해제'로 복구하세요."), CsvErrorColor);
		}
		else
		{
			AddLine(LOCTEXT("Undone", "마지막 트랜잭션을 되돌렸습니다. 적용 이력과 일치해 이어서 작업·저장할 수 있으며, CSV는 다시 미리보기해야 합니다."), CsvWarningColor);
		}
		bPreviewReady = false;
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
