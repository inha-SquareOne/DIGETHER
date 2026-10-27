#include "Core/WorkbenchItemRowValidator.h"

#include "Core/WorkbenchDataTableEditAdapter.h"
#include "Core/WorkbenchItemFixture.h"
#include "Core/WorkbenchItemValidation.h"
#include "Engine/DataTable.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "WorkbenchItemRowValidator"

namespace
{
	bool IsSyntheticAsset(const FAssetData& AssetData, const UObject* Asset)
	{
		const FString SyntheticPath = WorkbenchItemFixture::GetSyntheticAssetPackagePath();
		if (AssetData.PackageName.ToString() == SyntheticPath)
		{
			return true;
		}
		return Asset && Asset->GetOutermost()->GetName() == SyntheticPath;
	}
}

bool UWorkbenchItemRowValidator::CanValidateAsset_Implementation(
	const FAssetData& InAssetData,
	UObject* InAsset,
	FDataValidationContext& InContext) const
{
	const UDataTable* Table = Cast<UDataTable>(InAsset);
	if (!Table || Table->RowStruct != FWorkbenchSyntheticItemRow::StaticStruct())
	{
		return false;
	}
	return !IsSyntheticAsset(InAssetData, InAsset);
}

EDataValidationResult UWorkbenchItemRowValidator::ValidateLoadedAsset_Implementation(
	const FAssetData& InAssetData,
	UObject* InAsset,
	FDataValidationContext& InContext)
{
	UDataTable* Table = Cast<UDataTable>(InAsset);
	FWorkbenchTableSnapshot Snapshot;
	FText Error;
	if (!Table || !WorkbenchDataTableEditAdapter::TakeSnapshot(Table, Snapshot, Error))
	{
		InContext.AddError(Error.IsEmpty() ? LOCTEXT("SnapshotFailed", "아이템 테이블을 읽지 못해 검증하지 못했습니다.") : Error);
		return EDataValidationResult::Invalid;
	}

	bool bHasError = false;
	for (const FWorkbenchValidationMessage& Message : WorkbenchItemValidation::ValidateTable(Snapshot.Rows))
	{
		const bool bIsInvalid = Message.State == EWorkbenchValidationState::Invalid;
		const bool bIsPending = Message.State == EWorkbenchValidationState::PendingDecision;
		if (!bIsInvalid && !bIsPending)
		{
			continue;
		}

		const FText Line = FText::Format(
			LOCTEXT("MessageFormat", "[{0}] {1}: {2}"),
			FText::FromName(Message.RuleId),
			FText::FromName(Message.TargetId),
			Message.Description);

		if (Message.IsBlocking())
		{
			InContext.AddError(Line);
			bHasError = true;
		}
		else
		{
			InContext.AddWarning(Line);
		}
	}
	return bHasError ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}

#undef LOCTEXT_NAMESPACE
