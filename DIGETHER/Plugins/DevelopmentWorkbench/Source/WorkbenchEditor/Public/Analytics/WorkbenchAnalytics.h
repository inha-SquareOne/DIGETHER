#pragma once

#include "CoreMinimal.h"

class SWidget;

namespace WorkbenchAnalytics
{
	// ToolId: T02(스폰 확률), T03(합성 잠재가치 분석), T04(룸 연결성). 대소문자 무시, 그 외는 안내 패널을 반환한다.
	WORKBENCHEDITOR_API TSharedRef<SWidget> MakePanel(FName ToolId);
}
