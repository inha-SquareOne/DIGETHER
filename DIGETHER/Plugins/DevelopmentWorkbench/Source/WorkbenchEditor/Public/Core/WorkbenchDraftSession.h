// 아이템 1행의 초안 편집 세션: 조회 -> 초안 -> 검증 -> 적용 -> 재기준화.
// 저장(디스크)은 WorkbenchDataTableEditAdapter가 맡고, 이 클래스는 메모리 상의 초안과 오래됨(stale) 판정을 다룬다.
#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Core/WorkbenchDataTableEditAdapter.h"
#include "Core/WorkbenchItemFixture.h"
#include "Core/WorkbenchValidationTypes.h"

class UDataTable;

class WORKBENCHEDITOR_API FWorkbenchItemDraftSession : public TSharedFromThis<FWorkbenchItemDraftSession>
{
public:
	FWorkbenchItemDraftSession(UDataTable* InTable, FName InRowName);

	/** 시작 시 행을 읽었는지. 행이 없거나 지원하지 않는 테이블이면 false. */
	bool IsValid() const { return bIsValid; }

	FName GetRowName() const { return RowName; }
	uint32 GetBaselineHash() const { return BaselineHash; }
	const FWorkbenchSyntheticItemRow& GetBaseline() const { return BaselineRow; }

	/** Details에 바인딩하는 transient 초안. 원본 행에는 바인딩하지 않는다. */
	UWorkbenchItemDraftObject* GetDraftObject() const { return DraftObject.Get(); }

	/** 초안이 기준과 다른 필드 목록. */
	TArray<FWorkbenchFieldChange> GetChanges() const;
	bool IsDirty() const { return !GetChanges().IsEmpty(); }

	/** 원본이 기준 이후 바뀌었는지 현재 값을 다시 읽어 판정한다. 바뀌면 적용을 막는다. */
	bool RecomputeStale();
	bool IsStale() const { return bStale; }

	/** 현재 편집 가능 여부와 이유. */
	bool CheckEditable(FText& OutReason) const;

	/** 초안을 가정한 테이블 전체 검증 중 이 행에 해당하는 메시지(전역 안내 포함). */
	TArray<FWorkbenchValidationMessage> Validate() const;

	/** 초안을 기준값으로 되돌린다. */
	void Revert();

	/** 현재 원본 값으로 기준과 초안을 다시 만든다(오래됨 해소). */
	bool Requery();

	/** 초안을 원본에 명시 적용한다. 성공하면 기준을 새 값으로 갱신한다. */
	FWorkbenchApplyResult Apply();

private:
	TMap<FName, FWorkbenchRowValues> BuildEdits() const;
	void LoadBaseline(const FWorkbenchSyntheticItemRow& Row);

	TWeakObjectPtr<UDataTable> TargetTable;
	FName RowName;
	FWorkbenchSyntheticItemRow BaselineRow;
	uint32 BaselineHash = 0;
	bool bIsValid = false;
	bool bStale = false;

	TStrongObjectPtr<UWorkbenchItemDraftObject> DraftObject;
};
