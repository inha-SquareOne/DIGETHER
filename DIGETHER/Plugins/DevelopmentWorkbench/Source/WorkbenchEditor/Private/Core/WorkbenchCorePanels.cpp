#include "Core/WorkbenchCorePanels.h"

#include "Core/SWorkbenchCoreWidgets.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WorkbenchCorePanels"

TSharedRef<SWidget> WorkbenchCore::MakePanel(FName ToolId)
{
	if (ToolId == TEXT("T01.ItemEditor") || ToolId == TEXT("T01"))
	{
		return SNew(SWorkbenchItemEditorPanel);
	}
	if (ToolId == TEXT("T05.DataValidation") || ToolId == TEXT("T05"))
	{
		return SNew(SWorkbenchValidationPanel);
	}
	if (ToolId == TEXT("T08.CsvBulkEdit") || ToolId == TEXT("T08"))
	{
		return SNew(SWorkbenchCsvPanel);
	}

	return SNew(SBox)
		.Padding(12.f)
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("UnknownTool", "이 패널은 '{0}' 도구를 지원하지 않습니다. 지원 ID: T01.ItemEditor, T05.DataValidation, T08.CsvBulkEdit"), FText::FromName(ToolId)))
			.AutoWrapText(true)
		];
}

#undef LOCTEXT_NAMESPACE
