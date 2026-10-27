#include "Core/WorkbenchDraftSession.h"
#include "Core/WorkbenchItemValidation.h"
#include "Engine/DataTable.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "WorkbenchItemDraftSession"

FWorkbenchItemDraftSession::FWorkbenchItemDraftSession(UDataTable* InTable, FName InRowName)
	: TargetTable(InTable)
	, RowName(InRowName)
{
	FWorkbenchSyntheticItemRow Row;
	if (WorkbenchDataTableEditAdapter::TryReadRow(InTable, InRowName, Row))
	{
		DraftObject.Reset(NewObject<UWorkbenchItemDraftObject>(GetTransientPackage(), NAME_None, RF_Transient));
		LoadBaseline(Row);
		bIsValid = true;
	}
}

void FWorkbenchItemDraftSession::LoadBaseline(const FWorkbenchSyntheticItemRow& Row)
{
	BaselineRow = Row;
	BaselineHash = WorkbenchItemFixture::ComputeRowHash(Row);
	bStale = false;
	if (DraftObject.IsValid())
	{
		DraftObject->CopyFromRow(RowName, Row);
	}
}

TArray<FWorkbenchFieldChange> FWorkbenchItemDraftSession::GetChanges() const
{
	TArray<FWorkbenchFieldChange> Changes;
	if (!DraftObject.IsValid())
	{
		return Changes;
	}
	const FWorkbenchRowValues Before = WorkbenchItemFixture::RowToValues(BaselineRow);
	const FWorkbenchRowValues After = WorkbenchItemFixture::RowToValues(DraftObject->ToRow());
	for (const FWorkbenchColumnSpec& Col : WorkbenchItemFixture::GetColumns())
	{
		if (Before[Col.Name] != After[Col.Name])
		{
			Changes.Add({ RowName, Col.Name, Before[Col.Name].ToDisplayString(), After[Col.Name].ToDisplayString() });
		}
	}
	return Changes;
}

bool FWorkbenchItemDraftSession::RecomputeStale()
{
	FWorkbenchSyntheticItemRow Current;
	const bool bPresent = WorkbenchDataTableEditAdapter::TryReadRow(TargetTable.Get(), RowName, Current);
	bStale = !bPresent || WorkbenchItemFixture::ComputeRowHash(Current) != BaselineHash;
	return bStale;
}

bool FWorkbenchItemDraftSession::CheckEditable(FText& OutReason) const
{
	return WorkbenchDataTableEditAdapter::EvaluateEditability(TargetTable.Get(), OutReason);
}

TArray<FWorkbenchValidationMessage> FWorkbenchItemDraftSession::Validate() const
{
	TArray<FWorkbenchValidationMessage> Out;
	FWorkbenchTableSnapshot Snapshot;
	FText Error;
	if (!DraftObject.IsValid() || !WorkbenchDataTableEditAdapter::TakeSnapshot(TargetTable.Get(), Snapshot, Error))
	{
		return Out;
	}
	Snapshot.Rows.Add(RowName, DraftObject->ToRow());
	for (const FWorkbenchValidationMessage& Msg : WorkbenchItemValidation::ValidateTable(Snapshot.Rows))
	{
		if (Msg.TargetId.IsNone() || Msg.TargetId == RowName)
		{
			Out.Add(Msg);
		}
	}
	return Out;
}

void FWorkbenchItemDraftSession::Revert()
{
	if (DraftObject.IsValid())
	{
		DraftObject->CopyFromRow(RowName, BaselineRow);
	}
}

bool FWorkbenchItemDraftSession::Requery()
{
	FWorkbenchSyntheticItemRow Current;
	if (!WorkbenchDataTableEditAdapter::TryReadRow(TargetTable.Get(), RowName, Current))
	{
		bStale = true;
		return false;
	}
	LoadBaseline(Current);
	return true;
}

TMap<FName, FWorkbenchRowValues> FWorkbenchItemDraftSession::BuildEdits() const
{
	TMap<FName, FWorkbenchRowValues> Edits;
	if (!DraftObject.IsValid())
	{
		return Edits;
	}
	const FWorkbenchRowValues Before = WorkbenchItemFixture::RowToValues(BaselineRow);
	const FWorkbenchRowValues After = WorkbenchItemFixture::RowToValues(DraftObject->ToRow());
	FWorkbenchRowValues Changed;
	for (const FWorkbenchColumnSpec& Col : WorkbenchItemFixture::GetColumns())
	{
		if (Before[Col.Name] != After[Col.Name])
		{
			Changed.Add(Col.Name, After[Col.Name]);
		}
	}
	if (!Changed.IsEmpty())
	{
		Edits.Add(RowName, MoveTemp(Changed));
	}
	return Edits;
}

FWorkbenchApplyResult FWorkbenchItemDraftSession::Apply()
{
	FWorkbenchApplyResult Result;
	if (!bIsValid)
	{
		Result.Errors.Add(LOCTEXT("SessionInvalid", "유효한 편집 세션이 없습니다."));
		return Result;
	}
	if (RecomputeStale())
	{
		Result.Errors.Add(LOCTEXT("SessionStale", "원본이 조회한 뒤 바뀌었습니다. 재조회한 다음 다시 편집하세요."));
		return Result;
	}

	TMap<FName, uint32> Expected;
	Expected.Add(RowName, BaselineHash);
	Result = WorkbenchDataTableEditAdapter::ApplyEdits(TargetTable.Get(), BuildEdits(), Expected, LOCTEXT("ApplyTransaction", "Workbench 아이템 편집 적용"));
	if (Result.bSuccess)
	{
		Requery();
	}
	return Result;
}

#undef LOCTEXT_NAMESPACE
