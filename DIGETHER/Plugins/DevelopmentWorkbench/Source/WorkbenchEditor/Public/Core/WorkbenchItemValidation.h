// UI, 자동화 테스트, 엔진 DataValidation이 함께 쓰는 순수 구조 검증 함수.
// 가격 0 허용 여부 같은 프로젝트 정책은 승인된 값이 없으므로 PendingDecision으로 남긴다.
#pragma once

#include "CoreMinimal.h"
#include "Core/WorkbenchItemFixture.h"
#include "Core/WorkbenchValidationTypes.h"

namespace WorkbenchItemValidation
{
	/** 테이블 안 DefinitionId별 등장 횟수. 행마다 다시 세지 않도록 호출자가 한 번 만들어 넘긴다. */
	WORKBENCHEDITOR_API TMap<FName, int32> BuildDefinitionIdCounts(const TMap<FName, FWorkbenchSyntheticItemRow>& Rows);

	/** 행 1개의 구조 검사. 프로젝트 전역 ID 충돌은 이 함수가 검사하지 않는다. */
	WORKBENCHEDITOR_API TArray<FWorkbenchValidationMessage> ValidateRow(
		FName RowName,
		const FWorkbenchSyntheticItemRow& Row,
		const TMap<FName, int32>& DefinitionIdCounts);

	/** 테이블 전체 검사. 행별 결과와 테이블 단위 안내(전역 ID 인덱스 미검사)를 합친다. */
	WORKBENCHEDITOR_API TArray<FWorkbenchValidationMessage> ValidateTable(const TMap<FName, FWorkbenchSyntheticItemRow>& Rows);

	/** Invalid + Error 메시지가 있는지. */
	WORKBENCHEDITOR_API bool HasBlocking(const TArray<FWorkbenchValidationMessage>& Messages);

	/** 상태 표시용 한국어 라벨. */
	WORKBENCHEDITOR_API FText StateToText(EWorkbenchValidationState State);
}
