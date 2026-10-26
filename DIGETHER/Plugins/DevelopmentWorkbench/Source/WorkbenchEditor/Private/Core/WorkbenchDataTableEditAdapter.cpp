#include "Core/WorkbenchDataTableEditAdapter.h"
#include "Core/WorkbenchItemValidation.h"
#include "DataTableEditorUtils.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/DataTable.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/DateTime.h"
#include "Misc/FeedbackContext.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#define LOCTEXT_NAMESPACE "WorkbenchDataTableEditAdapter"

namespace
{
	// 이 도구가 만든 테이블 상태의 이력. [0]은 최초 기준, 이후는 각 적용 직후. Undo·Redo는 이력의 다른 항목으로 이동한다.
	struct FHistoryEntry
	{
		uint32 Hash = 0;
		int32 PendingCount = 0;
	};

	struct FPackageState
	{
		bool bOwned = false;
		TArray<FHistoryEntry> History;
		int32 Current = 0;
		FDateTime DiskTimestamp = FDateTime::MinValue();
		TArray<FString> Pending;
	};

	FString GStorageRootOverride;

	TMap<FName, FPackageState>& States()
	{
		static TMap<FName, FPackageState> Value;
		return Value;
	}

	FString PackageFilename(const FString& PackageName)
	{
		return FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	}

	bool IsTransientTable(const UDataTable* Table)
	{
		return Table && Table->GetOutermost() == GetTransientPackage();
	}

	bool CollectRows(const UDataTable* Table, TMap<FName, FWorkbenchSyntheticItemRow>& OutRows)
	{
		if (!Table || Table->RowStruct != FWorkbenchSyntheticItemRow::StaticStruct())
		{
			return false;
		}
		OutRows.Reset();
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			if (Pair.Value)
			{
				OutRows.Add(Pair.Key, *reinterpret_cast<const FWorkbenchSyntheticItemRow*>(Pair.Value));
			}
		}
		return true;
	}

	TArray<FName> SortedKeys(const TMap<FName, FWorkbenchSyntheticItemRow>& Rows)
	{
		TArray<FName> Keys;
		Rows.GetKeys(Keys);
		Keys.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
		return Keys;
	}

	// 현재 테이블 해시와 일치하는 이력 항목을 찾아 Current를 맞춘다. 없으면 외부 변경이다. 같은 해시가 여럿이면 Current에 가장 가까운 것을 쓴다.
	bool ResyncState(FPackageState& State, const UDataTable* Table)
	{
		uint32 Hash = 0;
		if (!WorkbenchDataTableEditAdapter::TryComputeTableHash(Table, Hash) || State.History.IsEmpty())
		{
			return false;
		}
		int32 Best = INDEX_NONE;
		for (int32 i = 0; i < State.History.Num(); ++i)
		{
			if (State.History[i].Hash == Hash && (Best == INDEX_NONE || FMath::Abs(i - State.Current) < FMath::Abs(Best - State.Current)))
			{
				Best = i;
			}
		}
		if (Best == INDEX_NONE)
		{
			return false;
		}
		State.Current = Best;
		return true;
	}

	void BeginOwnership(FPackageState& State, const UDataTable* Table, const FString& PackageName)
	{
		State = FPackageState();
		State.bOwned = true;
		uint32 Hash = 0;
		WorkbenchDataTableEditAdapter::TryComputeTableHash(Table, Hash);
		State.History.Add({ Hash, 0 });
		State.DiskTimestamp = IFileManager::Get().GetTimeStamp(*PackageFilename(PackageName));
	}

	FString MakeJobId()
	{
		return FString::Printf(TEXT("%s-%s"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S-%s")), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
	}

	// Undo로 가려진 뒤 Redo될 수 있으므로 Pending 원본은 유지하고 현재 이력 위치의 개수만큼만 본다.
	TArrayView<const FString> PendingView(const FPackageState& State)
	{
		return TArrayView<const FString>(State.Pending).Left(State.History[State.Current].PendingCount);
	}

	bool AppendJournal(const FString& JobId, const FString& Phase, const FString& PackageName, const FString& Result, TArrayView<const FString> Changes, uint32 TableHash, const FString& Backup)
	{
		FString Line;
		{
			TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line);
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("job"), JobId);
			Writer->WriteValue(TEXT("time"), FDateTime::UtcNow().ToIso8601());
			Writer->WriteValue(TEXT("phase"), Phase);
			Writer->WriteValue(TEXT("package"), PackageName);
			Writer->WriteValue(TEXT("user"), FString(FPlatformProcess::UserName()));
			Writer->WriteValue(TEXT("tableHash"), FString::Printf(TEXT("%08X"), TableHash));
			Writer->WriteValue(TEXT("backup"), Backup);
			Writer->WriteValue(TEXT("result"), Result);
			Writer->WriteArrayStart(TEXT("changes"));
			for (const FString& C : Changes)
			{
				Writer->WriteValue(C);
			}
			Writer->WriteArrayEnd();
			Writer->WriteObjectEnd();
			Writer->Close();
		}
		Line += TEXT("\n");
		const FString Path = WorkbenchDataTableEditAdapter::GetJournalPath();
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		return FFileHelper::SaveStringToFile(Line, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}
}

FString WorkbenchDataTableEditAdapter::GetStorageRoot()
{
	if (!GStorageRootOverride.IsEmpty())
	{
		return GStorageRootOverride;
	}
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DevelopmentWorkbench")));
}

FString WorkbenchDataTableEditAdapter::GetBackupDirectory()
{
	return FPaths::Combine(GetStorageRoot(), TEXT("Backups"));
}

FString WorkbenchDataTableEditAdapter::GetJournalPath()
{
	return FPaths::Combine(GetStorageRoot(), TEXT("Journal"), TEXT("journal.jsonl"));
}

void WorkbenchDataTableEditAdapter::SetStorageRootOverrideForTesting(const FString& Root)
{
	GStorageRootOverride = Root.IsEmpty() ? FString() : FPaths::ConvertRelativePathToFull(Root);
}

void WorkbenchDataTableEditAdapter::ResetOwnershipStateForTesting()
{
	States().Empty();
}

bool WorkbenchDataTableEditAdapter::IsSupportedTable(const UDataTable* Table, FText& OutReason)
{
	if (!Table)
	{
		OutReason = LOCTEXT("NoTable", "DataTable이 선택되지 않았습니다.");
		return false;
	}
	if (Table->RowStruct != FWorkbenchSyntheticItemRow::StaticStruct())
	{
		OutReason = LOCTEXT("UnsupportedStruct", "이 도구는 합성 아이템 행 구조(FWorkbenchSyntheticItemRow)의 DataTable만 편집합니다. 다른 행 구조와 배열·맵·객체 참조 필드는 지원하지 않습니다.");
		return false;
	}
	return true;
}

bool WorkbenchDataTableEditAdapter::TakeSnapshot(UDataTable* Table, FWorkbenchTableSnapshot& OutSnapshot, FText& OutError)
{
	OutSnapshot = FWorkbenchTableSnapshot();
	if (!IsSupportedTable(Table, OutError))
	{
		return false;
	}
	CollectRows(Table, OutSnapshot.Rows);
	OutSnapshot.Table = Table;
	OutSnapshot.AssetPath = Table->GetPathName();
	OutSnapshot.RowOrder = SortedKeys(OutSnapshot.Rows);
	for (const TPair<FName, FWorkbenchSyntheticItemRow>& Pair : OutSnapshot.Rows)
	{
		OutSnapshot.RowHashes.Add(Pair.Key, WorkbenchItemFixture::ComputeRowHash(Pair.Value));
	}
	OutSnapshot.TableHash = WorkbenchItemFixture::ComputeTableHash(OutSnapshot.Rows);
	OutSnapshot.CapturedAt = FDateTime::Now();
	return true;
}

bool WorkbenchDataTableEditAdapter::TryComputeTableHash(const UDataTable* Table, uint32& OutHash)
{
	TMap<FName, FWorkbenchSyntheticItemRow> Rows;
	if (!CollectRows(Table, Rows))
	{
		return false;
	}
	OutHash = WorkbenchItemFixture::ComputeTableHash(Rows);
	return true;
}

bool WorkbenchDataTableEditAdapter::TryReadRow(const UDataTable* Table, FName RowName, FWorkbenchSyntheticItemRow& OutRow)
{
	if (!Table || Table->RowStruct != FWorkbenchSyntheticItemRow::StaticStruct())
	{
		return false;
	}
	const uint8* const* Found = Table->GetRowMap().Find(RowName);
	if (!Found || !*Found)
	{
		return false;
	}
	OutRow = *reinterpret_cast<const FWorkbenchSyntheticItemRow*>(*Found);
	return true;
}

bool WorkbenchDataTableEditAdapter::IsPlayInEditorActive()
{
	return GEditor && GEditor->PlayWorld != nullptr;
}

void WorkbenchDataTableEditAdapter::MarkPackageOwnedByWorkbench(const UObject* Asset)
{
	if (!Asset)
	{
		return;
	}
	const UDataTable* Table = Cast<UDataTable>(Asset);
	if (!Table)
	{
		return;
	}
	const FName PackageName = Asset->GetOutermost()->GetFName();
	FPackageState& State = States().FindOrAdd(PackageName);
	if (!State.bOwned)
	{
		BeginOwnership(State, Table, PackageName.ToString());
	}
}

EWorkbenchOwnership WorkbenchDataTableEditAdapter::GetOwnership(const UDataTable* Table)
{
	if (!Table)
	{
		return EWorkbenchOwnership::None;
	}
	FPackageState* State = States().Find(Table->GetOutermost()->GetFName());
	if (!State || !State->bOwned)
	{
		return EWorkbenchOwnership::None;
	}
	if (!ResyncState(*State, Table))
	{
		return EWorkbenchOwnership::Broken;
	}
	return EWorkbenchOwnership::Owned;
}

void WorkbenchDataTableEditAdapter::MarkPackageCreatedByWorkbench(const UDataTable* Table)
{
	if (!Table)
	{
		return;
	}
	const FName PackageName = Table->GetOutermost()->GetFName();
	FPackageState& State = States().FindOrAdd(PackageName);
	BeginOwnership(State, Table, PackageName.ToString());
	State.Pending.Add(FString::Printf(TEXT("(생성) %s"), *PackageName.ToString()));
	State.History[0].PendingCount = 1;
}

bool WorkbenchDataTableEditAdapter::ReleaseWorkbenchOwnership(const UDataTable* Table, FText& OutMessage)
{
	if (!Table || !States().Contains(Table->GetOutermost()->GetFName()))
	{
		OutMessage = LOCTEXT("ReleaseNothing", "해제할 Workbench 소유 상태가 없습니다.");
		return false;
	}
	States().Remove(Table->GetOutermost()->GetFName());
	OutMessage = Table->GetOutermost()->IsDirty()
		? LOCTEXT("ReleasedDirty", "Workbench 소유권을 해제했습니다. 패키지에 저장되지 않은 변경이 남아 있으니 에디터에서 저장하거나 되돌리기 전까지 Workbench 편집은 차단됩니다.")
		: LOCTEXT("ReleasedClean", "Workbench 소유권을 해제했습니다. 다시 조회해 편집을 시작할 수 있습니다.");
	return true;
}

bool WorkbenchDataTableEditAdapter::IsPackageOwnedByWorkbench(const UObject* Asset)
{
	if (!Asset)
	{
		return false;
	}
	const FPackageState* State = States().Find(Asset->GetOutermost()->GetFName());
	return State && State->bOwned;
}

bool WorkbenchDataTableEditAdapter::EvaluateEditability(const UDataTable* Table, FText& OutReason)
{
	if (!IsSupportedTable(Table, OutReason))
	{
		return false;
	}
	if (IsPlayInEditorActive())
	{
		OutReason = LOCTEXT("PieActive", "PIE가 실행 중입니다. 종료한 뒤 편집하세요.");
		return false;
	}
	if (IsTransientTable(Table))
	{
		return true;
	}

	const FString PackageName = Table->GetOutermost()->GetName();
	FString ExistingFile;
	if (FPackageName::DoesPackageExist(PackageName, &ExistingFile) && IFileManager::Get().IsReadOnly(*ExistingFile))
	{
		OutReason = LOCTEXT("ReadOnlyFile", "패키지 파일이 읽기 전용입니다. 이 도구는 소스 컨트롤 체크아웃을 하지 않으므로 먼저 쓰기 가능하게 만드세요.");
		return false;
	}
	if (GetOwnership(Table) == EWorkbenchOwnership::Broken)
	{
		OutReason = LOCTEXT("OwnershipBroken", "Workbench가 적용한 뒤 테이블이 다른 경로(에디터 직접 편집, 다른 도구, 외부 리로드)로 바뀌어 적용·저장을 차단했습니다. 소유권을 해제해 다시 시작하거나 에디터에서 되돌리세요.");
		return false;
	}
	if (Table->GetOutermost()->IsDirty() && !IsPackageOwnedByWorkbench(Table))
	{
		OutReason = LOCTEXT("ForeignDirty", "이 패키지에 다른 편집으로 생긴 저장되지 않은 변경이 있습니다. 먼저 저장하거나 되돌린 뒤 다시 시도하세요.");
		return false;
	}
	return true;
}

FWorkbenchEditPreview WorkbenchDataTableEditAdapter::PreviewEdits(
	const UDataTable* Table,
	const TMap<FName, FWorkbenchRowValues>& Edits,
	const TMap<FName, uint32>& ExpectedRowHashes)
{
	FWorkbenchEditPreview Preview;

	FText Reason;
	if (!IsSupportedTable(Table, Reason))
	{
		Preview.Errors.Add(Reason);
		return Preview;
	}

	TMap<FName, FWorkbenchSyntheticItemRow> Current;
	CollectRows(Table, Current);
	TMap<FName, FWorkbenchSyntheticItemRow> Candidate = Current;

	TArray<FName> EditedRows;
	Edits.GetKeys(EditedRows);
	EditedRows.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });

	for (const FName& RowId : EditedRows)
	{
		const FWorkbenchSyntheticItemRow* Existing = Current.Find(RowId);
		if (!Existing)
		{
			Preview.Errors.Add(FText::Format(LOCTEXT("RowMissing", "행 '{0}'이(가) 테이블에 없습니다. 행 추가는 지원하지 않습니다."), FText::FromName(RowId)));
			continue;
		}
		const uint32* Expected = ExpectedRowHashes.Find(RowId);
		if (!Expected)
		{
			Preview.Errors.Add(FText::Format(LOCTEXT("NoBaseline", "행 '{0}'의 기준 해시가 없어 적용할 수 없습니다."), FText::FromName(RowId)));
			continue;
		}
		if (*Expected != WorkbenchItemFixture::ComputeRowHash(*Existing))
		{
			Preview.Errors.Add(FText::Format(LOCTEXT("RowStale", "행 '{0}'이(가) 조회한 뒤 다른 곳에서 바뀌었습니다(오래된 기준). 재조회하세요."), FText::FromName(RowId)));
			continue;
		}

		FWorkbenchSyntheticItemRow Edited = *Existing;
		FText Error;
		if (!WorkbenchItemFixture::ApplyValuesToRow(Edited, Edits[RowId], Error))
		{
			Preview.Errors.Add(FText::Format(LOCTEXT("RowEditRejected", "행 '{0}': {1}"), FText::FromName(RowId), Error));
			continue;
		}

		const FWorkbenchRowValues Before = WorkbenchItemFixture::RowToValues(*Existing);
		const FWorkbenchRowValues After = WorkbenchItemFixture::RowToValues(Edited);
		FWorkbenchRowValues Effective;
		for (const FWorkbenchColumnSpec& Col : WorkbenchItemFixture::GetColumns())
		{
			if (!Edits[RowId].Contains(Col.Name))
			{
				continue;
			}
			const FWorkbenchScalarValue& Old = Before[Col.Name];
			const FWorkbenchScalarValue& New = After[Col.Name];
			if (Old != New)
			{
				Effective.Add(Col.Name, New);
				Preview.Changes.Add({ RowId, Col.Name, Old.ToDisplayString(), New.ToDisplayString() });
			}
		}
		if (!Effective.IsEmpty())
		{
			Preview.Edits.Add(RowId, MoveTemp(Effective));
			Preview.ExpectedRowHashes.Add(RowId, *Expected);
			Candidate.Add(RowId, Edited);
		}
	}

	if (Preview.Errors.IsEmpty())
	{
		for (const FWorkbenchValidationMessage& Msg : WorkbenchItemValidation::ValidateTable(Candidate))
		{
			if (Msg.TargetId.IsNone() || Preview.Edits.Contains(Msg.TargetId))
			{
				Preview.Messages.Add(Msg);
				if (Msg.IsBlocking())
				{
					Preview.Errors.Add(FText::Format(LOCTEXT("BlockingRule", "행 '{0}' 검증 오류({1}): {2}"),
						FText::FromName(Msg.TargetId), FText::FromName(Msg.RuleId), Msg.Description));
				}
			}
		}
		if (Preview.Changes.IsEmpty())
		{
			Preview.Warnings.Add(LOCTEXT("NoChanges", "현재 값과 다른 변경이 없습니다."));
		}
	}
	return Preview;
}

FWorkbenchEditPreview WorkbenchDataTableEditAdapter::PreviewCsv(const UDataTable* Table, const FWorkbenchCsvPatchManifest& Manifest)
{
	FWorkbenchEditPreview Preview;
	Preview.Warnings = Manifest.Warnings;

	if (!Manifest.IsAcceptable())
	{
		Preview.Errors = Manifest.RejectionErrors;
		return Preview;
	}

	FText Reason;
	if (!IsSupportedTable(Table, Reason))
	{
		Preview.Errors.Add(Reason);
		return Preview;
	}

	TMap<FName, FWorkbenchSyntheticItemRow> Current;
	CollectRows(Table, Current);

	if (!Manifest.bHasBaselineHash)
	{
		Preview.Errors.Add(LOCTEXT("NoBaselineHash", "CSV에 기준 해시(#WorkbenchCsv 첫 줄)가 없어 적용할 수 없습니다. 이 도구의 '내보내기'로 만든 CSV를 편집해 사용하세요."));
		return Preview;
	}
	const uint32 CurrentHash = WorkbenchItemFixture::ComputeTableHash(Current);
	if (CurrentHash != Manifest.BaselineHash)
	{
		Preview.Errors.Add(FText::Format(LOCTEXT("BaselineMismatch", "CSV를 내보낸 뒤 테이블이 바뀌었습니다(CSV 기준 {0}, 현재 {1}). 다시 내보낸 CSV로 작업하세요."),
			FText::FromString(FString::Printf(TEXT("%08X"), Manifest.BaselineHash)), FText::FromString(FString::Printf(TEXT("%08X"), CurrentHash))));
		return Preview;
	}

	bool bMissingRow = false;
	for (const FName& RowId : Manifest.RowOrder)
	{
		if (!Current.Contains(RowId))
		{
			bMissingRow = true;
			Preview.Errors.Add(FText::Format(LOCTEXT("CsvUnknownRow", "CSV의 행 '{0}'이(가) 테이블에 없습니다. 행 자동 생성은 하지 않습니다."), FText::FromName(RowId)));
		}
	}
	if (bMissingRow)
	{
		return Preview;
	}

	TMap<FName, FWorkbenchRowValues> Edits;
	TMap<FName, uint32> Expected;
	for (const FWorkbenchCsvCellPatch& Patch : Manifest.Patches)
	{
		Edits.FindOrAdd(Patch.RowId).Add(Patch.Column, Patch.Value);
		Expected.Add(Patch.RowId, WorkbenchItemFixture::ComputeRowHash(Current[Patch.RowId]));
	}

	FWorkbenchEditPreview Inner = PreviewEdits(Table, Edits, Expected);
	Inner.Warnings.Append(Preview.Warnings);
	return Inner;
}

FWorkbenchApplyResult WorkbenchDataTableEditAdapter::ApplyEdits(
	UDataTable* Table,
	const TMap<FName, FWorkbenchRowValues>& Edits,
	const TMap<FName, uint32>& ExpectedRowHashes,
	const FText& TransactionLabel)
{
	FWorkbenchApplyResult Result;

	FText Reason;
	if (!EvaluateEditability(Table, Reason))
	{
		Result.Errors.Add(Reason);
		return Result;
	}

	const FWorkbenchEditPreview Preview = PreviewEdits(Table, Edits, ExpectedRowHashes);
	if (!Preview.Errors.IsEmpty())
	{
		Result.Errors = Preview.Errors;
		return Result;
	}
	if (Preview.Changes.IsEmpty())
	{
		Result.Errors.Add(LOCTEXT("ApplyNothing", "적용할 변경이 없습니다."));
		return Result;
	}

	TMap<FName, FWorkbenchSyntheticItemRow> Backup;
	CollectRows(Table, Backup);

	// 소유 상태는 변경 전에 확정한다. 이미 소유 중이면 EvaluateEditability가 이력과 현재 값이 일치함을 확인했다.
	if (!IsTransientTable(Table) && !IsPackageOwnedByWorkbench(Table))
	{
		MarkPackageOwnedByWorkbench(Table);
	}

	TOptional<FScopedTransaction> Transaction;
	if (GEditor)
	{
		Transaction.Emplace(TransactionLabel);
	}
	// Modify()는 RF_Transactional 객체만 기록하므로 Undo 약속을 지키려면 지원 대상 테이블에 플래그를 보장한다.
	Table->SetFlags(RF_Transactional);
	Table->Modify();
	FDataTableEditorUtils::BroadcastPreChange(Table, FDataTableEditorUtils::EDataTableChangeInfo::RowData);

	bool bFailed = false;
	for (const TPair<FName, FWorkbenchRowValues>& Pair : Preview.Edits)
	{
		uint8* const* Found = Table->GetRowMap().Find(Pair.Key);
		FText Error;
		if (!Found || !*Found || !WorkbenchItemFixture::ApplyValuesToRow(*reinterpret_cast<FWorkbenchSyntheticItemRow*>(*Found), Pair.Value, Error))
		{
			bFailed = true;
			Result.Errors.Add(FText::Format(LOCTEXT("ApplyRowFailed", "행 '{0}' 적용 중 오류가 나 전체를 되돌렸습니다."), FText::FromName(Pair.Key)));
			break;
		}
		Result.AppliedRows.Add(Pair.Key);
	}

	if (bFailed)
	{
		for (const TPair<FName, FWorkbenchSyntheticItemRow>& Pair : Backup)
		{
			if (uint8* const* Found = Table->GetRowMap().Find(Pair.Key); Found && *Found)
			{
				*reinterpret_cast<FWorkbenchSyntheticItemRow*>(*Found) = Pair.Value;
			}
		}
		FDataTableEditorUtils::BroadcastPostChange(Table, FDataTableEditorUtils::EDataTableChangeInfo::RowData);
		if (Transaction.IsSet())
		{
			Transaction->Cancel();
		}
		Result.AppliedRows.Reset();
		return Result;
	}

	FDataTableEditorUtils::BroadcastPostChange(Table, FDataTableEditorUtils::EDataTableChangeInfo::RowData);
	Table->MarkPackageDirty();

	if (!IsTransientTable(Table))
	{
		FPackageState& State = States().FindOrAdd(Table->GetOutermost()->GetFName());
		// Undo 뒤 새 적용이면 되돌린 가지(Redo 대상)를 버린다.
		State.History.SetNum(State.Current + 1);
		State.Pending.SetNum(State.History[State.Current].PendingCount);
		for (const FWorkbenchFieldChange& C : Preview.Changes)
		{
			State.Pending.Add(FString::Printf(TEXT("%s.%s: %s -> %s"), *C.RowId.ToString(), *C.Column.ToString(), *C.OldText, *C.NewText));
		}
		uint32 NewHash = 0;
		TryComputeTableHash(Table, NewHash);
		State.History.Add({ NewHash, State.Pending.Num() });
		State.Current = State.History.Num() - 1;
	}

	Result.bSuccess = true;
	return Result;
}

FWorkbenchApplyResult WorkbenchDataTableEditAdapter::ApplyCsv(UDataTable* Table, const FWorkbenchCsvPatchManifest& Manifest)
{
	const FWorkbenchEditPreview Preview = PreviewCsv(Table, Manifest);
	if (!Preview.Errors.IsEmpty() || Preview.Changes.IsEmpty())
	{
		FWorkbenchApplyResult Result;
		Result.Errors = Preview.Errors;
		if (Result.Errors.IsEmpty())
		{
			Result.Errors.Add(LOCTEXT("CsvNothing", "CSV에서 현재 값과 다른 변경이 없습니다."));
		}
		return Result;
	}
	return ApplyEdits(Table, Preview.Edits, Preview.ExpectedRowHashes, LOCTEXT("CsvApplyTransaction", "Workbench CSV 일괄 적용"));
}

bool WorkbenchDataTableEditAdapter::ExportTableToCsv(const UDataTable* Table, FString& OutCsv, FText& OutError)
{
	if (!IsSupportedTable(Table, OutError))
	{
		return false;
	}
	TMap<FName, FWorkbenchSyntheticItemRow> Rows;
	CollectRows(Table, Rows);
	TMap<FName, FWorkbenchRowValues> Values;
	for (const TPair<FName, FWorkbenchSyntheticItemRow>& Pair : Rows)
	{
		Values.Add(Pair.Key, WorkbenchItemFixture::RowToValues(Pair.Value));
	}
	OutCsv = WorkbenchCsvPatchManifest::ExportToCsv(SortedKeys(Rows), Values, WorkbenchItemFixture::GetColumns(), WorkbenchItemFixture::ComputeTableHash(Rows));
	return true;
}

int32 WorkbenchDataTableEditAdapter::GetPendingChangeCount(const UDataTable* Table)
{
	if (!Table)
	{
		return 0;
	}
	GetOwnership(Table);
	const FPackageState* State = States().Find(Table->GetOutermost()->GetFName());
	return State && State->bOwned && !State->History.IsEmpty() ? PendingView(*State).Num() : 0;
}

FWorkbenchSaveResult WorkbenchDataTableEditAdapter::SaveTablePackage(UDataTable* Table)
{
	FWorkbenchSaveResult Result;
	Result.JournalPath = GetJournalPath();

	FText Reason;
	if (!IsSupportedTable(Table, Reason))
	{
		Result.Message = Reason;
		return Result;
	}
	if (IsPlayInEditorActive())
	{
		Result.Message = LOCTEXT("SavePie", "PIE가 실행 중이라 저장하지 않았습니다.");
		return Result;
	}
	if (IsTransientTable(Table))
	{
		Result.Message = LOCTEXT("SaveTransient", "메모리 전용 테이블은 디스크에 저장할 수 없습니다. 합성 에셋을 만든 뒤 저장하세요.");
		return Result;
	}

	UPackage* Package = Table->GetOutermost();
	const FName PackageFName = Package->GetFName();
	Result.PackageName = PackageFName.ToString();

	const EWorkbenchOwnership Ownership = GetOwnership(Table);
	if (Ownership == EWorkbenchOwnership::Broken)
	{
		Result.Message = LOCTEXT("SaveBroken", "Workbench가 적용한 뒤 테이블이 다른 경로(에디터 직접 편집, 외부 리로드 등)로 바뀌어 저장하지 않았습니다. 소유권을 해제하고 에디터에서 상태를 확인하세요.");
		return Result;
	}
	FPackageState* State = States().Find(PackageFName);
	if (Ownership != EWorkbenchOwnership::Owned || !State)
	{
		Result.Message = LOCTEXT("SaveNotOwned", "이 도구가 적용한 변경이 없는 패키지는 저장하지 않습니다.");
		return Result;
	}
	const TArrayView<const FString> PendingChanges = PendingView(*State);
	if (PendingChanges.IsEmpty())
	{
		Result.Message = LOCTEXT("SaveNoPending", "저장할 Workbench 변경이 없습니다(Undo로 모두 되돌린 상태).");
		return Result;
	}
	const uint32 CurrentHash = State->History[State->Current].Hash;

	const FString Filename = PackageFilename(Result.PackageName);
	const bool bFileExists = IFileManager::Get().FileExists(*Filename);
	if (bFileExists)
	{
		if (IFileManager::Get().IsReadOnly(*Filename))
		{
			Result.Message = LOCTEXT("SaveReadOnly", "패키지 파일이 읽기 전용이라 저장하지 않았습니다.");
			return Result;
		}
		if (IFileManager::Get().GetTimeStamp(*Filename) != State->DiskTimestamp)
		{
			Result.Message = LOCTEXT("SaveDiskChanged", "디스크의 패키지 파일이 편집 시작 이후 바뀌었습니다. 덮어쓰지 않았으니 파일 상태를 확인하세요.");
			return Result;
		}
	}
	else if (State->DiskTimestamp != FDateTime::MinValue())
	{
		Result.Message = LOCTEXT("SaveDiskGone", "편집 시작 시 있던 패키지 파일이 사라졌습니다. 덮어쓰지 않았습니다.");
		return Result;
	}

	// 같은 초에 두 번 저장해도 겹치지 않도록 작업 ID별 폴더에 백업한다.
	Result.JobId = MakeJobId();
	if (bFileExists)
	{
		const FString JobBackupDir = FPaths::Combine(GetBackupDirectory(), Result.JobId);
		Result.BackupPath = FPaths::Combine(JobBackupDir, FPackageName::GetShortName(Result.PackageName) + FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*JobBackupDir, true);
		if (IFileManager::Get().Copy(*Result.BackupPath, *Filename) != COPY_OK || !IFileManager::Get().FileExists(*Result.BackupPath))
		{
			Result.BackupPath.Reset();
			Result.Message = LOCTEXT("SaveBackupFail", "백업을 만들지 못해 저장하지 않았습니다.");
			return Result;
		}
	}

	// 저장 전 기록이 남지 않으면 복구 근거가 없으므로 저장하지 않는다.
	if (!AppendJournal(Result.JobId, TEXT("begin"), Result.PackageName, TEXT("pending"), PendingChanges, CurrentHash, Result.BackupPath))
	{
		Result.bJournalOk = false;
		Result.JournalWarning = LOCTEXT("JournalBeginFail", "변경 저널에 저장 시작 기록을 쓰지 못했습니다.");
		Result.Message = LOCTEXT("SaveJournalAbort", "변경 저널을 쓸 수 없어 저장을 중단했습니다. 디스크의 패키지는 바뀌지 않았고 메모리의 변경은 그대로입니다. 저널 경로의 쓰기 권한을 확인하세요.");
		return Result;
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	SaveArgs.Error = GWarn;
	const FSavePackageResultStruct SaveResult = UPackage::Save(Package, Table, *Filename, SaveArgs);

	if (!SaveResult.IsSuccessful())
	{
		if (!AppendJournal(Result.JobId, TEXT("result"), Result.PackageName, TEXT("failed"), PendingChanges, CurrentHash, Result.BackupPath))
		{
			Result.bJournalOk = false;
			Result.JournalWarning = LOCTEXT("JournalFailResultFail", "저장 실패 결과를 저널에 기록하지 못했습니다.");
		}
		Result.Message = LOCTEXT("SaveFailed", "패키지 저장에 실패했습니다. 메모리의 변경은 그대로이며 백업과 저널을 확인하세요.");
		return Result;
	}

	if (!AppendJournal(Result.JobId, TEXT("result"), Result.PackageName, TEXT("saved"), PendingChanges, CurrentHash, Result.BackupPath))
	{
		Result.bJournalOk = false;
		Result.JournalWarning = LOCTEXT("JournalResultFail", "저장은 끝났지만 결과를 저널에 기록하지 못했습니다. 저널에는 이 작업의 '시작' 줄만 남아 있습니다.");
	}
	States().Remove(PackageFName);
	Result.bSaved = true;
	Result.Message = LOCTEXT("SaveOk", "저장했습니다.");
	return Result;
}

#undef LOCTEXT_NAMESPACE
