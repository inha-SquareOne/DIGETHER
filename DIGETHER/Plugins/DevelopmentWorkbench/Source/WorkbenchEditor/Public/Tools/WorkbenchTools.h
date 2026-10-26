#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"

// 담당 C(T06/T07/T09/T10/T11) 진입점. Core UI는 이 함수만으로 패널을 얻는다.
// ToolId는 "T06" "T07" "T09" "T10" "T11" 중 하나다. 그 외 값은 사유를 담은 안내 위젯을 돌려준다.
namespace WorkbenchTools
{
	WORKBENCHEDITOR_API TSharedRef<SWidget> MakePanel(FName ToolId);
}
