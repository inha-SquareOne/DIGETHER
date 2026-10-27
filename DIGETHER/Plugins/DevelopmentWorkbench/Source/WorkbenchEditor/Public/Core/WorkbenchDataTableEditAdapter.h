// DataTable 편집 어댑터. 조회(스냅샷) -> 변경 미리보기 -> 명시 적용(메모리, Undo 가능) -> 명시 저장(패키지별)을
// 서로 다른 단계로 제공한다. 지원 범위는 FWorkbenchSyntheticItemRow를 행 구조로 쓰는 UDataTable이며
// 배열·맵·UObject 참조·구조체 필드, 행 추가·삭제·이름 변경은 지원하지 않는다.
#pragma once

#include "CoreMinimal.h"
#include "Core/WorkbenchItemFixture.h"
#include "Core/WorkbenchCsvPatchManifest.h"
#include "Core/WorkbenchValidationTypes.h"

class UDataTable;

/** 조회 시점에 복사해 둔 읽기 전용 스냅샷. 원본 메모리를 가리키지 않는다. */
struct WORKBENCHEDITOR_API FWorkbenchTableSnapshot
{
	TWeakObjectPtr<UDataTable> Table;
	FString AssetPath;
	TArray<FName> RowOrder;
	TMap<FName, FWorkbenchSyntheticItemRow> Rows;
	TMap<FName, uint32> RowHashes;
	uint32 TableHash = 0;
	FDateTime CapturedAt;

	bool IsEmpty() const { return Rows.IsEmpty(); }
};

/** 필드 1개의 변경 전후. */
struct WORKBENCHEDITOR_API FWorkbenchFieldChange
{
	FName RowId;
	FName Column;
	FString OldText;
	FString NewText;
};

/** 적용 전 미리보기. 검증과 적용이 같은 결과를 쓰도록 편집 맵과 기준 해시를 함께 담는다. */
struct WORKBENCHEDITOR_API FWorkbenchEditPreview
{
	TMap<FName, FWorkbenchRowValues> Edits;
	TMap<FName, uint32> ExpectedRowHashes;
	TArray<FWorkbenchFieldChange> Changes;
	TArray<FWorkbenchValidationMessage> Messages;
	TArray<FText> Errors;
	TArray<FText> Warnings;

	bool CanApply() const { return Errors.IsEmpty() && !Changes.IsEmpty(); }
};

struct WORKBENCHEDITOR_API FWorkbenchApplyResult
{
	bool bSuccess = false;
	TArray<FName> AppliedRows;
	TArray<FText> Errors;
};

struct WORKBENCHEDITOR_API FWorkbenchSaveResult
{
	bool bSaved = false;
	FString PackageName;
	/** 저장 작업 1회의 고유 ID. 백업 폴더 이름과 저널 줄에 같은 값이 들어간다. */
	FString JobId;
	FString BackupPath;
	FString JournalPath;
	/** 마지막 결과 줄까지 저널에 기록됐는지. 저장 전 기록이 실패하면 저장 자체를 중단한다. */
	bool bJournalOk = true;
	FText JournalWarning;
	FText Message;
};

/** 이 도구가 패키지의 미저장 변경을 소유하는지. Broken이면 적용·저장이 모두 차단된다. */
enum class EWorkbenchOwnership : uint8
{
	None,
	Owned,
	Broken
};

namespace WorkbenchDataTableEditAdapter
{
	/** 행 구조가 FWorkbenchSyntheticItemRow인 테이블만 지원한다. */
	WORKBENCHEDITOR_API bool IsSupportedTable(const UDataTable* Table, FText& OutReason);

	WORKBENCHEDITOR_API bool TakeSnapshot(UDataTable* Table, FWorkbenchTableSnapshot& OutSnapshot, FText& OutError);

	/** 현재 테이블 해시 (스냅샷과 비교용). 테이블이 없으면 false. */
	WORKBENCHEDITOR_API bool TryComputeTableHash(const UDataTable* Table, uint32& OutHash);

	WORKBENCHEDITOR_API bool TryReadRow(const UDataTable* Table, FName RowName, FWorkbenchSyntheticItemRow& OutRow);

	/** 편집을 시작해도 되는지: PIE, 읽기 전용 파일, 다른 경로의 미저장 변경을 확인한다. */
	WORKBENCHEDITOR_API bool EvaluateEditability(const UDataTable* Table, FText& OutReason);

	WORKBENCHEDITOR_API bool IsPlayInEditorActive();

	/** 패키지의 미저장 변경을 이 도구가 만든 것으로 표시한다. */
	WORKBENCHEDITOR_API void MarkPackageOwnedByWorkbench(const UObject* Asset);

	/** 이 도구가 새로 만든 에셋을 소유 상태로 시작하고 생성 자체를 미저장 변경 1건으로 기록한다(별도 편집 없이 최초 저장 가능). */
	WORKBENCHEDITOR_API void MarkPackageCreatedByWorkbench(const UDataTable* Table);

	WORKBENCHEDITOR_API bool IsPackageOwnedByWorkbench(const UObject* Asset);

	/**
	 * 소유 상태를 현재 테이블과 대조한다. 이 도구가 만든 상태(최초 기준·각 적용 뒤)와 같으면 Owned(Undo·Redo 포함),
	 * 어느 것과도 다르면 외부 변경으로 보고 Broken이다. Broken에서 새 적용으로 소유권을 되찾을 수 없다.
	 */
	WORKBENCHEDITOR_API EWorkbenchOwnership GetOwnership(const UDataTable* Table);

	/**
	 * 소유권을 해제한다(복구 경로). 이 도구가 기록한 미저장 목록과 기준을 버리며 디스크·메모리 값은 건드리지 않는다.
	 * 패키지가 아직 dirty이면 에디터에서 저장하거나 되돌릴 때까지 편집이 차단된다.
	 */
	WORKBENCHEDITOR_API bool ReleaseWorkbenchOwnership(const UDataTable* Table, FText& OutMessage);

	/** 편집 맵을 현재 테이블에 대해 검사한다. 행 존재, 허용 열·타입, 기준 해시, 차단 검증을 확인한다. */
	WORKBENCHEDITOR_API FWorkbenchEditPreview PreviewEdits(
		const UDataTable* Table,
		const TMap<FName, FWorkbenchRowValues>& Edits,
		const TMap<FName, uint32>& ExpectedRowHashes);

	/** CSV manifest를 현재 테이블과 대조한 미리보기. 없는 행 ID, 기준 해시 불일치를 거부한다. */
	WORKBENCHEDITOR_API FWorkbenchEditPreview PreviewCsv(const UDataTable* Table, const FWorkbenchCsvPatchManifest& Manifest);

	/**
	 * 편집을 메모리에 적용한다. 적용 직전에 PIE, 편집 가능 여부, 기준 해시, 검증을 다시 확인한다.
	 * 하나의 Undo 트랜잭션으로 묶고 실패하면 바뀐 값을 되돌린다. 디스크에는 쓰지 않는다.
	 */
	WORKBENCHEDITOR_API FWorkbenchApplyResult ApplyEdits(
		UDataTable* Table,
		const TMap<FName, FWorkbenchRowValues>& Edits,
		const TMap<FName, uint32>& ExpectedRowHashes,
		const FText& TransactionLabel);

	/** CSV manifest를 적용한다. PreviewCsv를 다시 실행해 최신 상태로 검사한 뒤 ApplyEdits를 호출한다. */
	WORKBENCHEDITOR_API FWorkbenchApplyResult ApplyCsv(UDataTable* Table, const FWorkbenchCsvPatchManifest& Manifest);

	/** 테이블 현재 값을 CSV로 내보낸다. */
	WORKBENCHEDITOR_API bool ExportTableToCsv(const UDataTable* Table, FString& OutCsv, FText& OutError);

	/** 이 도구가 적용한 변경이 남아 있는 패키지를 디스크에 저장한다. 백업과 변경 저널을 남긴다. */
	WORKBENCHEDITOR_API FWorkbenchSaveResult SaveTablePackage(UDataTable* Table);

	/** 이 도구가 적용했지만 아직 저장하지 않은 변경 건수. */
	WORKBENCHEDITOR_API int32 GetPendingChangeCount(const UDataTable* Table);

	/** 백업·저널 루트. 기본은 <프로젝트>/Saved/DevelopmentWorkbench. */
	WORKBENCHEDITOR_API FString GetStorageRoot();
	WORKBENCHEDITOR_API FString GetBackupDirectory();
	WORKBENCHEDITOR_API FString GetJournalPath();

	/** 시험 전용: 백업·저널 루트를 바꾼다. 빈 문자열이면 기본값으로 돌아간다. */
	WORKBENCHEDITOR_API void SetStorageRootOverrideForTesting(const FString& Root);

	/** 시험 전용: 소유 상태 맵을 비운다. */
	WORKBENCHEDITOR_API void ResetOwnershipStateForTesting();
}
