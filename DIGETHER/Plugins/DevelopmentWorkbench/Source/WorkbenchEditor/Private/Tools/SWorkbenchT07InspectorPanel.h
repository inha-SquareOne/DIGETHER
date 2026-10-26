#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"
#include "Tools/WorkbenchToolsAlgo.h"

class AActor;
class IDetailsView;
class SMultiLineEditableTextBox;
class UBlueprint;

// T07: 명시적으로 고른 대상(선택 액터 원본 또는 블루프린트 GeneratedClass의 CDO)을 엔진 기본 IDetailsView로 직접 편집한다.
// 초안 프록시가 아니므로 여기서 바꾸면 원본이 변경 상태가 된다. bool 스칼라 조건·그룹은 명시 JSON 프로필로만 적용한다.
class SWorkbenchT07InspectorPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorkbenchT07InspectorPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SWorkbenchT07InspectorPanel() override;

private:
	enum class ETargetKind : uint8
	{
		None,
		ActorOriginal,
		BlueprintCdo,
		FixtureExample, // 합성 예시 대상(UWorkbenchT07FixtureObject). 사용자가 버튼을 눌러야만 만든다.
	};

	FReply OnBindFixtureClicked();
	FReply OnFillExampleProfileClicked();
	FReply OnBindSelectedActorClicked();
	FReply OnBindSelectedActorCdoClicked();
	FReply OnBindContentBrowserBlueprintClicked();
	FReply OnApplyProfileClicked();
	FReply OnClearProfileClicked();

	bool BindActor(AActor* Actor);
	bool BindBlueprint(UBlueprint* Blueprint);
	void ClearTarget(const FText& Reason);
	UObject* ResolveTarget() const;
	// bApplyProfile가 true일 때만(=사용자가 '프로필 적용'을 누른 경우) 프로필을 등록한다. 그 외 갱신은 프로필 없이 연다.
	void RefreshView(bool bApplyProfile);
	void RegisterProfileCustomization(UClass* TargetClass);
	void UnregisterProfileCustomization();
	// 컴파일·재인스턴스·대상 변경 뒤에는 이름만으로 다시 붙이지 않고 프로필을 해제해 명시 재적용을 요구한다.
	void DisableProfile(const FText& Reason);

	void HandleSelectionChanged(UObject* NewSelection);
	void HandleObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap);
	void HandleBlueprintCompiled();
	void HandleUndoRedo();
	void HandleBeginPie(bool bIsSimulating);

	FText GetStatusText() const;
	FText GetProfileText() const;

	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<SMultiLineEditableTextBox> ProfileBox;

	ETargetKind Kind = ETargetKind::None;
	TWeakObjectPtr<AActor> TargetActor;
	TWeakObjectPtr<UBlueprint> TargetBlueprint;
	bool bFollowSelection = false;
	FText GuardMessage;

	WorkbenchToolsAlgo::FInspectorProfile Profile;
	bool bProfileLoaded = false;
	bool bProfileActive = false;
	FText ProfileMessage;
	TWeakObjectPtr<UClass> RegisteredClass;
	TWeakObjectPtr<UObject> ProfileAppliedTarget; // 프로필을 적용한 시점의 대상(소유자). 다른 대상이면 재적용을 요구한다.
	TStrongObjectPtr<UObject> FixtureObject;

	FDelegateHandle SelectionHandle;
	FDelegateHandle ReplacedHandle;
	FDelegateHandle CompiledHandle;
	FDelegateHandle UndoHandle;
	FDelegateHandle PieHandle;
};
