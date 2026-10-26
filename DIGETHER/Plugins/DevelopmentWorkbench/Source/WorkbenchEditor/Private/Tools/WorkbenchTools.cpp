#include "Tools/WorkbenchTools.h"

#include "Tools/SWorkbenchT06MultiRunPanel.h"
#include "Tools/SWorkbenchT07InspectorPanel.h"
#include "Tools/SWorkbenchT09AssetCleanupPanel.h"
#include "Tools/SWorkbenchT10SearchPanel.h"
#include "Tools/SWorkbenchT11PlacementPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WorkbenchTools"

namespace WorkbenchTools
{
	TSharedRef<SWidget> MakePanel(FName ToolId)
	{
		// "T06" 또는 "T06.MultiRun"처럼 접두 번호만 비교한다.
		FString Prefix = ToolId.ToString();
		int32 DotIndex = INDEX_NONE;
		if (Prefix.FindChar(TEXT('.'), DotIndex))
		{
			Prefix.LeftInline(DotIndex);
		}

		if (Prefix == TEXT("T06")) { return SNew(SWorkbenchT06MultiRunPanel); }
		if (Prefix == TEXT("T07")) { return SNew(SWorkbenchT07InspectorPanel); }
		if (Prefix == TEXT("T09")) { return SNew(SWorkbenchT09AssetCleanupPanel); }
		if (Prefix == TEXT("T10")) { return SNew(SWorkbenchT10SearchPanel); }
		if (Prefix == TEXT("T11")) { return SNew(SWorkbenchT11PlacementPanel); }

		return SNew(STextBlock)
			.AutoWrapText(true)
			.Text(FText::Format(LOCTEXT("UnknownTool", "이 진입점은 T06/T07/T09/T10/T11만 제공한다. 요청한 도구 ID: {0}"),
				FText::FromName(ToolId)));
	}
}

#undef LOCTEXT_NAMESPACE
