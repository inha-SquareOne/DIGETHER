// CSV를 "패치 교환 형식"으로 다룬다. 엔진의 DataTable CSV 전체 덮어쓰기를 쓰지 않고, 원시 셀을 먼저 판정해
// 값이 있는 셀만 담은 manifest를 만든 뒤 어댑터가 미리보기와 적용을 수행한다.
//
// 형식
//  - 첫 줄(선택): #WorkbenchCsv,<스키마 버전>,<기준 해시 16진수>
//  - 헤더: 첫 열은 RowId(또는 ---), 이후 허용 열 이름. 열 순서는 자유이고 빠진 열은 원본을 보존한다.
//  - 빈 셀 = 변경 없음. 명시 토큰: Name 열의 <NULL>(None으로 설정), String 열의 <BLANK>(빈 문자열로 설정).
//  - 문자열/이름이 = + - @ 탭 CR ' 로 시작하면 내보낼 때 앞에 ' 를 붙이고 가져올 때 한 글자를 벗긴다.
#pragma once

#include "CoreMinimal.h"
#include "Core/WorkbenchScalarValue.h"

WORKBENCHEDITOR_API extern const TCHAR* const GWorkbenchCsvExplicitNullToken;
WORKBENCHEDITOR_API extern const TCHAR* const GWorkbenchCsvExplicitBlankToken;

/** 값이 있는 셀 1개. 값이 없는(빈) 셀은 패치에 넣지 않는다. */
struct WORKBENCHEDITOR_API FWorkbenchCsvCellPatch
{
	FName RowId;
	FName Column;
	FWorkbenchScalarValue Value;
	int32 SourceLine = 0;
};

struct WORKBENCHEDITOR_API FWorkbenchCsvPatchManifest
{
	static constexpr int32 CurrentSchemaVersion = 1;

	int32 SchemaVersion = CurrentSchemaVersion;
	bool bHasBaselineHash = false;
	uint32 BaselineHash = 0;

	/** 헤더에 나온 열(RowId 제외). 헤더에 없는 열은 원본을 보존한다. */
	TArray<FName> HeaderColumns;

	/** 파일에 나온 순서의 행 ID. */
	TArray<FName> RowOrder;

	TArray<FWorkbenchCsvCellPatch> Patches;

	TArray<FText> Warnings;
	TArray<FText> RejectionErrors;
	TArray<FName> UnknownColumns;
	TArray<FName> RejectedDuplicateIds;

	bool IsAcceptable() const { return RejectionErrors.IsEmpty(); }
};

namespace WorkbenchCsvPatchManifest
{
	/** CSV 원문을 판정해 manifest를 만든다. 거부 사유는 RejectionErrors에 모두 모은다. */
	WORKBENCHEDITOR_API FWorkbenchCsvPatchManifest BuildFromCsv(const FString& CsvContent, const TArray<FWorkbenchColumnSpec>& AllowedColumns);

	/** 행 목록을 CSV로 내보낸다. 모든 허용 열과 메타 줄(스키마 버전·기준 해시)을 쓴다. */
	WORKBENCHEDITOR_API FString ExportToCsv(
		const TArray<FName>& RowOrder,
		const TMap<FName, FWorkbenchRowValues>& Rows,
		const TArray<FWorkbenchColumnSpec>& Columns,
		uint32 BaselineHash);

	/** 값을 CSV 셀 텍스트로(토큰·수식 방지 접두 포함). 따옴표 처리는 하지 않는다. */
	WORKBENCHEDITOR_API FString EncodeCell(const FWorkbenchScalarValue& Value);
}
