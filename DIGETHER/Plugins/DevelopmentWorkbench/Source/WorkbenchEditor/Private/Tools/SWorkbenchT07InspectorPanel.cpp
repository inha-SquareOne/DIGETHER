#include "Tools/SWorkbenchT07InspectorPanel.h"

#include "Tools/WorkbenchT07FixtureObject.h"
#include "ContentBrowserModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "IContentBrowserSingleton.h"
#include "IDetailCustomization.h"
#include "IDetailPropertyRow.h"
#include "IDetailsView.h"
#include "Internationalization/Text.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Selection.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WorkbenchT07"

using namespace WorkbenchToolsAlgo;

namespace
{
	class FProfileCustomization : public IDetailCustomization
	{
	public:
		explicit FProfileCustomization(const FInspectorProfile& InProfile) : Profile(InProfile) {}

		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
		{
			for (const FInspectorRule& Rule : Profile.Rules)
			{
				const TSharedRef<IPropertyHandle> Handle = DetailBuilder.GetProperty(FName(*Rule.Property));
				if (!Handle->IsValidHandle())
				{
					continue;
				}

				IDetailPropertyRow* Row = nullptr;
				if (Rule.Group.IsEmpty())
				{
					Row = DetailBuilder.EditDefaultProperty(Handle);
				}
				else
				{
					Row = &DetailBuilder.EditCategory(FName(*Rule.Group)).AddProperty(Handle);
				}
				if (!Row || Rule.ConditionProperty.IsEmpty())
				{
					continue;
				}

				const TSharedRef<IPropertyHandle> Condition = DetailBuilder.GetProperty(FName(*Rule.ConditionProperty));
				if (!Condition->IsValidHandle())
				{
					continue;
				}

				const bool bNegate = Rule.bNegateCondition;
				Row->Visibility(TAttribute<EVisibility>::CreateLambda([Condition, bNegate]()
				{
					bool bValue = false;
					if (Condition->GetValue(bValue) != FPropertyAccess::Success)
					{
						return EVisibility::Visible; // 다중 값이거나 읽을 수 없으면 숨기지 않는다.
					}
					return (bValue != bNegate) ? EVisibility::Visible : EVisibility::Collapsed;
				}));
			}
		}

	private:
		FInspectorProfile Profile;
	};

	bool IsEditableWorld(const UWorld* World)
	{
		return World && World->WorldType == EWorldType::Editor;
	}
}

void SWorkbenchT07InspectorPanel::Construct(const FArguments& InArgs)
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs ViewArgs;
	ViewArgs.bUpdatesFromSelection = false;
	ViewArgs.bLockable = false;
	ViewArgs.bAllowSearch = true;
	ViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = PropertyEditor.CreateDetailView(ViewArgs);

	SelectionHandle = USelection::SelectionChangedEvent.AddRaw(this, &SWorkbenchT07InspectorPanel::HandleSelectionChanged);
	ReplacedHandle = FCoreUObjectDelegates::OnObjectsReplaced.AddRaw(this, &SWorkbenchT07InspectorPanel::HandleObjectsReplaced);
	UndoHandle = FEditorDelegates::PostUndoRedo.AddRaw(this, &SWorkbenchT07InspectorPanel::HandleUndoRedo);
	PieHandle = FEditorDelegates::PreBeginPIE.AddRaw(this, &SWorkbenchT07InspectorPanel::HandleBeginPie);
	if (GEditor)
	{
		CompiledHandle = GEditor->OnBlueprintCompiled().AddRaw(this, &SWorkbenchT07InspectorPanel::HandleBlueprintCompiled);
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8)
		[
			SNew(STextBlock).AutoWrapText(true)
			.Text(LOCTEXT("Intro", "대상을 직접 지정해 엔진 기본 디테일 뷰로 편집한다. 초안 복사본이 아니라 원본이므로 바꾸면 원본이 변경 상태가 되고 저장은 직접 해야 한다. 블루프린트 CDO는 컴파일·저장된 상태에서만 연결한다."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
			[SNew(SButton).Text(LOCTEXT("BindActor", "선택 액터 원본")).OnClicked(this, &SWorkbenchT07InspectorPanel::OnBindSelectedActorClicked)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
			[SNew(SButton).Text(LOCTEXT("BindActorCdo", "선택 액터의 BP CDO")).OnClicked(this, &SWorkbenchT07InspectorPanel::OnBindSelectedActorCdoClicked)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
			[SNew(SButton).Text(LOCTEXT("BindCb", "콘텐츠 브라우저의 BP CDO")).OnClicked(this, &SWorkbenchT07InspectorPanel::OnBindContentBrowserBlueprintClicked)]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
				[SNew(SButton).Text(LOCTEXT("BindFixture", "예시 대상 연결(합성)")).ToolTipText(LOCTEXT("BindFixtureTip", "bShowAdvanced·FixtureValue·FixtureName을 가진 임시 예시 객체를 만든다. 저장되지 않으며 자동으로 적용되지 않는다.")).OnClicked(this, &SWorkbenchT07InspectorPanel::OnBindFixtureClicked)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bFollowSelection ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState S) { bFollowSelection = S == ECheckBoxState::Checked; })
				[SNew(STextBlock).Text(LOCTEXT("Follow", "선택 변경 시 액터 원본 대상 따라가기"))]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 4)
		[
			SNew(STextBlock).AutoWrapText(true).Text(this, &SWorkbenchT07InspectorPanel::GetStatusText)
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(8, 2)
		[
			DetailsView.ToSharedRef()
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 4)
		[
			SNew(STextBlock).AutoWrapText(true)
			.Text(LOCTEXT("ProfileHelp", "프로필(JSON, 선택): targetClass는 '/Script/모듈.클래스' 또는 '/Game/경로/BP_X.BP_X_C' 같은 전체 경로여야 하며 이름만·와일드카드는 거부한다. 최상위 프로퍼티의 그룹과 bool 조건 표시만 지원한다. 구조체 내부·배열 요소·중첩 조건·bool 이외 조건·다중 선택(복합 선택)은 지원하지 않는다. 블루프린트 컴파일·객체 재생성·대상 변경 뒤에는 프로필이 자동 해제되며 '프로필 적용'을 다시 눌러야 한다. 이미 엔진 커스터마이징이 있는 클래스와의 병합 동작과 그룹 이동 결과는 런타임 확인 전이므로 화면에서 직접 확인한다. 임의 코드 실행이나 전역 메타데이터 변경은 하지 않는다."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 2).MaxHeight(140.f)
		[
			SAssignNew(ProfileBox, SMultiLineEditableTextBox)
			.HintText(LOCTEXT("ProfileHint", "(합성 예시) {\"targetClass\":\"/Script/WorkbenchEditor.WorkbenchT07FixtureObject\",\"rules\":[{\"property\":\"FixtureValue\",\"group\":\"예시 그룹\",\"visibleWhen\":\"bShowAdvanced\",\"negate\":false}]}"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
			[SNew(SButton).Text(LOCTEXT("FillExample", "예시 프로필 입력(미적용)")).OnClicked(this, &SWorkbenchT07InspectorPanel::OnFillExampleProfileClicked)]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
				[SNew(SButton).Text(LOCTEXT("ApplyProfile", "프로필 적용")).OnClicked(this, &SWorkbenchT07InspectorPanel::OnApplyProfileClicked)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
			[SNew(SButton).Text(LOCTEXT("ClearProfile", "프로필 해제")).OnClicked(this, &SWorkbenchT07InspectorPanel::OnClearProfileClicked)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[SNew(STextBlock).AutoWrapText(true).Text(this, &SWorkbenchT07InspectorPanel::GetProfileText)]
		]
	];
}

SWorkbenchT07InspectorPanel::~SWorkbenchT07InspectorPanel()
{
	USelection::SelectionChangedEvent.Remove(SelectionHandle);
	FCoreUObjectDelegates::OnObjectsReplaced.Remove(ReplacedHandle);
	FEditorDelegates::PostUndoRedo.Remove(UndoHandle);
	FEditorDelegates::PreBeginPIE.Remove(PieHandle);
	if (GEditor)
	{
		GEditor->OnBlueprintCompiled().Remove(CompiledHandle);
	}
	if (DetailsView.IsValid())
	{
		UnregisterProfileCustomization();
		DetailsView->SetObject(nullptr);
	}
}

UObject* SWorkbenchT07InspectorPanel::ResolveTarget() const
{
	switch (Kind)
	{
	case ETargetKind::ActorOriginal:
		return TargetActor.Get();
	case ETargetKind::BlueprintCdo:
		if (const UBlueprint* Blueprint = TargetBlueprint.Get())
		{
			return Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;
		}
		return nullptr;
	case ETargetKind::FixtureExample:
		return FixtureObject.Get();
	default:
		return nullptr;
	}
}

void SWorkbenchT07InspectorPanel::ClearTarget(const FText& Reason)
{
	DisableProfile(LOCTEXT("ProfileOffTargetCleared", "대상 연결이 끊겨 프로필을 해제했다. 다시 연결한 뒤 '프로필 적용'을 눌러야 한다."));
	Kind = ETargetKind::None;
	TargetActor.Reset();
	TargetBlueprint.Reset();
	DetailsView->SetObject(nullptr);
	GuardMessage = Reason;
}

bool SWorkbenchT07InspectorPanel::BindActor(AActor* Actor)
{
	if (GEditor && GEditor->PlayWorld)
	{
		GuardMessage = LOCTEXT("GuardPie", "PIE 실행 중에는 연결하지 않는다. PIE를 종료한다.");
		return false;
	}
	if (!Actor || !IsEditableWorld(Actor->GetWorld()) || Actor->bIsEditorPreviewActor)
	{
		GuardMessage = LOCTEXT("GuardWorld", "편집 가능한 에디터 월드의 액터가 아니다(PIE·미리보기 월드·미리보기 액터 제외).");
		return false;
	}
	if (Actor->GetPackage() && Actor->GetPackage()->IsDirty())
	{
		GuardMessage = LOCTEXT("GuardActorDirty", "액터가 속한 패키지에 저장되지 않은 변경이 있어 연결하지 않는다(보수적 차단). 저장한 뒤 다시 연결한다.");
		return false;
	}

	if (Kind != ETargetKind::ActorOriginal || TargetActor.Get() != Actor)
	{
		DisableProfile(LOCTEXT("ProfileOffTargetChanged", "대상이 바뀌어 프로필을 해제했다. '프로필 적용'을 다시 눌러야 한다."));
	}
	Kind = ETargetKind::ActorOriginal;
	TargetActor = Actor;
	TargetBlueprint.Reset();
	GuardMessage = FText::GetEmpty();
	RefreshView(false);
	return true;
}

bool SWorkbenchT07InspectorPanel::BindBlueprint(UBlueprint* Blueprint)
{
	if (GEditor && GEditor->PlayWorld)
	{
		GuardMessage = LOCTEXT("GuardPie2", "PIE 실행 중에는 연결하지 않는다. PIE를 종료한다.");
		return false;
	}
	if (!Blueprint || !Blueprint->GeneratedClass)
	{
		GuardMessage = LOCTEXT("GuardNoClass", "생성된 클래스가 없다. 블루프린트를 컴파일한 뒤 다시 연결한다.");
		return false;
	}
	if (Blueprint->Status == BS_Dirty || Blueprint->GetOutermost()->IsDirty())
	{
		GuardMessage = LOCTEXT("GuardDirty", "블루프린트가 컴파일되지 않았거나 저장되지 않은 변경이 있어 연결하지 않는다. 컴파일·저장한 뒤 다시 연결한다.");
		return false;
	}

	if (Kind != ETargetKind::BlueprintCdo || TargetBlueprint.Get() != Blueprint)
	{
		DisableProfile(LOCTEXT("ProfileOffTargetChanged2", "대상이 바뀌어 프로필을 해제했다. '프로필 적용'을 다시 눌러야 한다."));
	}
	Kind = ETargetKind::BlueprintCdo;
	TargetBlueprint = Blueprint;
	TargetActor.Reset();
	GuardMessage = FText::GetEmpty();
	RefreshView(false);
	return true;
}

FReply SWorkbenchT07InspectorPanel::OnBindFixtureClicked()
{
	if (GEditor && GEditor->PlayWorld)
	{
		GuardMessage = LOCTEXT("GuardPie3", "PIE 실행 중에는 연결하지 않는다. PIE를 종료한다.");
		return FReply::Handled();
	}
	DisableProfile(LOCTEXT("ProfileOffFixture", "예시 대상으로 바꿔 프로필을 해제했다. '프로필 적용'을 눌러야 적용된다."));
	FixtureObject.Reset(NewObject<UWorkbenchT07FixtureObject>(GetTransientPackage()));
	Kind = ETargetKind::FixtureExample;
	TargetActor.Reset();
	TargetBlueprint.Reset();
	GuardMessage = FText::GetEmpty();
	RefreshView(false);
	return FReply::Handled();
}

FReply SWorkbenchT07InspectorPanel::OnFillExampleProfileClicked()
{
	ProfileBox->SetText(FText::FromString(TEXT("{\"targetClass\":\"/Script/WorkbenchEditor.WorkbenchT07FixtureObject\",\"rules\":[{\"property\":\"FixtureValue\",\"group\":\"예시 그룹\",\"visibleWhen\":\"bShowAdvanced\",\"negate\":false}]}")));
	ProfileMessage = LOCTEXT("ExampleFilled", "합성 예시를 입력만 했다. '프로필 적용'을 누르기 전에는 적용되지 않는다.");
	return FReply::Handled();
}

FReply SWorkbenchT07InspectorPanel::OnBindSelectedActorClicked()
{
	if (!GEditor || GEditor->GetSelectedActorCount() != 1)
	{
		GuardMessage = LOCTEXT("NeedOne", "액터를 정확히 1개 선택해야 한다.");
		return FReply::Handled();
	}
	BindActor(Cast<AActor>(GEditor->GetSelectedActors()->GetSelectedObject(0)));
	return FReply::Handled();
}

FReply SWorkbenchT07InspectorPanel::OnBindSelectedActorCdoClicked()
{
	if (!GEditor || GEditor->GetSelectedActorCount() != 1)
	{
		GuardMessage = LOCTEXT("NeedOne2", "액터를 정확히 1개 선택해야 한다.");
		return FReply::Handled();
	}
	const AActor* Actor = Cast<AActor>(GEditor->GetSelectedActors()->GetSelectedObject(0));
	UBlueprint* Blueprint = Actor ? UBlueprint::GetBlueprintFromClass(Actor->GetClass()) : nullptr;
	if (!Blueprint)
	{
		GuardMessage = LOCTEXT("NotBp", "선택 액터의 클래스가 블루프린트가 아니다. 네이티브 클래스 CDO는 이 도구가 다루지 않는다.");
		return FReply::Handled();
	}
	BindBlueprint(Blueprint);
	return FReply::Handled();
}

FReply SWorkbenchT07InspectorPanel::OnBindContentBrowserBlueprintClicked()
{
	TArray<FAssetData> Selected;
	FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get().GetSelectedAssets(Selected);
	UBlueprint* Blueprint = Selected.Num() == 1 ? Cast<UBlueprint>(Selected[0].GetAsset()) : nullptr;
	if (!Blueprint)
	{
		GuardMessage = LOCTEXT("NeedBp", "콘텐츠 브라우저에서 블루프린트 에셋을 정확히 1개 선택해야 한다.");
		return FReply::Handled();
	}
	BindBlueprint(Blueprint);
	return FReply::Handled();
}

void SWorkbenchT07InspectorPanel::DisableProfile(const FText& Reason)
{
	const bool bWasOn = bProfileLoaded || bProfileActive;
	UnregisterProfileCustomization();
	bProfileLoaded = false;
	ProfileAppliedTarget.Reset();
	if (bWasOn)
	{
		ProfileMessage = Reason;
	}
}

void SWorkbenchT07InspectorPanel::RefreshView(bool bApplyProfile)
{
	UObject* Target = ResolveTarget();
	if (!Target)
	{
		if (Kind != ETargetKind::None)
		{
			ClearTarget(LOCTEXT("TargetLost", "대상이 사라졌다(삭제·언로드·재생성). 다시 연결한다."));
		}
		return;
	}

	UnregisterProfileCustomization();
	if (bApplyProfile && bProfileLoaded)
	{
		RegisterProfileCustomization(Target->GetClass());
		if (bProfileActive)
		{
			ProfileAppliedTarget = Target;
		}
		else
		{
			bProfileLoaded = false;
		}
	}
	DetailsView->SetObject(Target, true);
}

void SWorkbenchT07InspectorPanel::UnregisterProfileCustomization()
{
	if (UClass* Class = RegisteredClass.Get())
	{
		DetailsView->UnregisterInstancedCustomPropertyLayout(Class);
	}
	RegisteredClass.Reset();
	bProfileActive = false;
}

void SWorkbenchT07InspectorPanel::RegisterProfileCustomization(UClass* TargetClass)
{
	TArray<FString> ClassChain;
	for (const UClass* Class = TargetClass; Class; Class = Class->GetSuperClass())
	{
		ClassChain.Add(Class->GetClassPathName().ToString());
	}

	if (!ProfileMatchesClassChain(Profile, ClassChain))
	{
		ProfileMessage = FText::Format(LOCTEXT("ProfileMismatch", "프로필 대상 클래스 '{0}'가 현재 대상 클래스 계층과 일치하지 않아 적용하지 않았다."), FText::FromString(Profile.TargetClass));
		return;
	}

	TArray<FString> Problems;
	for (const FInspectorRule& Rule : Profile.Rules)
	{
		if (!FindFProperty<FProperty>(TargetClass, *Rule.Property))
		{
			Problems.Add(FString::Printf(TEXT("프로퍼티 '%s'를 대상 클래스에서 찾지 못했다."), *Rule.Property));
		}
		if (!Rule.ConditionProperty.IsEmpty() && !CastField<FBoolProperty>(FindFProperty<FProperty>(TargetClass, *Rule.ConditionProperty)))
		{
			Problems.Add(FString::Printf(TEXT("조건 '%s'가 대상 클래스의 bool 프로퍼티가 아니다."), *Rule.ConditionProperty));
		}
	}
	if (Problems.Num() > 0)
	{
		ProfileMessage = FText::FromString(FString::Join(Problems, TEXT("\n")));
		return;
	}

	const FInspectorProfile Copy = Profile;
	DetailsView->RegisterInstancedCustomPropertyLayout(TargetClass,
		FOnGetDetailCustomizationInstance::CreateLambda([Copy]() { return MakeShared<FProfileCustomization>(Copy); }));
	RegisteredClass = TargetClass;
	bProfileActive = true;
	ProfileMessage = FText::Format(LOCTEXT("ProfileOn", "프로필 적용됨 (규칙 {0}개)"), FText::AsNumber(Profile.Rules.Num()));
}

FReply SWorkbenchT07InspectorPanel::OnApplyProfileClicked()
{
	TArray<FString> Errors;
	FInspectorProfile Parsed;
	if (!ParseInspectorProfile(ProfileBox->GetText().ToString(), Parsed, Errors))
	{
		ProfileMessage = FText::FromString(FString::Join(Errors, TEXT("\n")));
		return FReply::Handled();
	}

	Profile = Parsed;
	bProfileLoaded = true;
	if (ResolveTarget())
	{
		RefreshView(true);
	}
	else
	{
		ProfileMessage = LOCTEXT("ProfileWait", "프로필을 읽었다. 대상을 연결하면 검사 후 적용한다.");
	}
	return FReply::Handled();
}

FReply SWorkbenchT07InspectorPanel::OnClearProfileClicked()
{
	DisableProfile(FText::GetEmpty());
	Profile = FInspectorProfile();
	ProfileMessage = LOCTEXT("ProfileCleared", "프로필을 해제했다.");
	RefreshView(false);
	return FReply::Handled();
}

void SWorkbenchT07InspectorPanel::HandleSelectionChanged(UObject* NewSelection)
{
	if (!bFollowSelection || Kind != ETargetKind::ActorOriginal || !GEditor || GEditor->GetSelectedActorCount() != 1)
	{
		return;
	}
	AActor* Actor = Cast<AActor>(GEditor->GetSelectedActors()->GetSelectedObject(0));
	if (Actor && Actor != TargetActor.Get())
	{
		BindActor(Actor);
	}
}

void SWorkbenchT07InspectorPanel::HandleObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap)
{
	if (Kind == ETargetKind::None)
	{
		return;
	}
	const UObject* Current = ResolveTarget();
	bool bAffected = false;
	for (const TPair<UObject*, UObject*>& Pair : ReplacementMap)
	{
		if (Pair.Key == Current || (Current && Pair.Key == Current->GetClass()) || Pair.Key == RegisteredClass.Get() || Pair.Key == TargetActor.Get())
		{
			bAffected = true;
			break;
		}
	}
	if (!bAffected)
	{
		return;
	}
	if (Kind == ETargetKind::ActorOriginal)
	{
		if (UObject* const* NewObject = ReplacementMap.Find(TargetActor.Get()))
		{
			TargetActor = Cast<AActor>(*NewObject);
		}
	}
	DisableProfile(LOCTEXT("ProfileOffReplaced", "대상 또는 클래스가 재생성되어 프로필을 해제했다. 이름만으로 다시 붙이지 않으므로 '프로필 적용'을 다시 눌러야 한다."));
	RefreshView(false);
}

void SWorkbenchT07InspectorPanel::HandleBlueprintCompiled()
{
	if (Kind != ETargetKind::None)
	{
		DisableProfile(LOCTEXT("ProfileOffCompiled", "블루프린트가 컴파일되어 프로필을 해제했다. '프로필 적용'을 다시 눌러야 한다."));
		RefreshView(false);
	}
}

void SWorkbenchT07InspectorPanel::HandleUndoRedo()
{
	if (Kind == ETargetKind::None)
	{
		return;
	}
	if (ProfileAppliedTarget.IsValid() && ProfileAppliedTarget.Get() != ResolveTarget())
	{
		DisableProfile(LOCTEXT("ProfileOffUndo", "실행 취소·다시 실행으로 대상이 달라져 프로필을 해제했다. '프로필 적용'을 다시 눌러야 한다."));
		RefreshView(false);
	}
	else if (!ProfileAppliedTarget.IsValid() && bProfileLoaded)
	{
		DisableProfile(LOCTEXT("ProfileOffUndo2", "프로필을 적용했던 대상이 사라져 프로필을 해제했다."));
		RefreshView(false);
	}
}

void SWorkbenchT07InspectorPanel::HandleBeginPie(bool bIsSimulating)
{
	if (Kind != ETargetKind::None)
	{
		ClearTarget(LOCTEXT("PieCleared", "PIE가 시작되어 연결을 해제했다. PIE 종료 후 다시 연결한다."));
	}
}

FText SWorkbenchT07InspectorPanel::GetStatusText() const
{
	FTextBuilder Builder;
	const UObject* Target = ResolveTarget();
	if (!Target)
	{
		Builder.AppendLine(LOCTEXT("NoTarget", "연결된 대상이 없다."));
	}
	else
	{
		const FText KindText = Kind == ETargetKind::ActorOriginal ? LOCTEXT("KindActor", "선택 액터 원본")
			: Kind == ETargetKind::FixtureExample ? LOCTEXT("KindFixture", "합성 예시 객체(저장되지 않음)")
			: LOCTEXT("KindCdo", "블루프린트 CDO");
		Builder.AppendLine(FText::Format(LOCTEXT("Target", "대상: {0} - {1} ({2})"), KindText, FText::FromString(Target->GetName()), FText::FromString(Target->GetClass()->GetName())));
		Builder.AppendLine(Target->GetOutermost()->IsDirty()
			? LOCTEXT("Dirty", "원본 패키지가 변경 상태다. 이 도구는 자동 저장하지 않는다.")
			: LOCTEXT("Clean", "원본 패키지는 저장된 상태다. 값을 바꾸면 변경 상태가 된다."));

		if (Kind == ETargetKind::ActorOriginal && GEditor && !GEditor->GetSelectedActors()->IsSelected(Target))
		{
			Builder.AppendLine(LOCTEXT("Diverged", "현재 에디터 선택은 이 대상과 다르다. 명시 대상은 유지된다."));
		}
	}
	if (!GuardMessage.IsEmpty())
	{
		Builder.AppendLine(GuardMessage);
	}
	return Builder.ToText();
}

FText SWorkbenchT07InspectorPanel::GetProfileText() const
{
	return ProfileMessage;
}

#undef LOCTEXT_NAMESPACE
