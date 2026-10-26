// T01/T05/T08 패널의 진입점. 다른 도구 그룹의 MakePanel(Analytics, Tools)과 같은 "ToolId -> 위젯" 계약을 따른다.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"

namespace WorkbenchCore
{
	/**
	 * ToolId별 패널 위젯을 만든다. 지원 ID:
	 *  - "T01.ItemEditor"       : 합성 아이템 목록·검색·정렬·선택·초안·검증·적용·저장 UI
	 *  - "T05.DataValidation"   : 독립 데이터 검증 결과 패널(규칙 ID·상태별 집계)
	 *  - "T08.CsvBulkEdit"      : CSV 내보내기·가져오기 미리보기·patch manifest 적용 UI
	 * 지원하지 않는 ID가 들어오면 그 사실을 보여주는 안내 위젯을 반환한다.
	 */
	WORKBENCHEDITOR_API TSharedRef<SWidget> MakePanel(FName ToolId);
}
