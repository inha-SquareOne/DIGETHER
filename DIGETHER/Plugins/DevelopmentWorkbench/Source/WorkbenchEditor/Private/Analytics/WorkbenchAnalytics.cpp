#include "Analytics/WorkbenchAnalytics.h"

#include "SWorkbenchBalanceSimulationPanel.h"
#include "SWorkbenchRoomConnectivityPanel.h"
#include "SWorkbenchSpawnProbabilityPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace WorkbenchAnalytics
{
	TSharedRef<SWidget> MakePanel(FName ToolId)
	{
		const FString Id = ToolId.ToString();
		if (Id.Equals(TEXT("T02"), ESearchCase::IgnoreCase))
		{
			return SNew(SWorkbenchSpawnProbabilityPanel);
		}
		if (Id.Equals(TEXT("T03"), ESearchCase::IgnoreCase))
		{
			return SNew(SWorkbenchBalanceSimulationPanel);
		}
		if (Id.Equals(TEXT("T04"), ESearchCase::IgnoreCase))
		{
			return SNew(SWorkbenchRoomConnectivityPanel);
		}
		return SNew(STextBlock)
			.AutoWrapText(true)
			.Text(FText::FromString(FString::Printf(TEXT("분석 패널을 제공하지 않는 도구 ID입니다: %s (지원: T02, T03, T04)"), *Id)));
	}
}
