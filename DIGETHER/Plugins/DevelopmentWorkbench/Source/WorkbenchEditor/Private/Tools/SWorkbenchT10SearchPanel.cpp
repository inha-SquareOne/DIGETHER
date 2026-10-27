#include "Tools/SWorkbenchT10SearchPanel.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "FindInBlueprintManager.h"
#include "IContentBrowserSingleton.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "WorkbenchT10"

namespace
{
	constexpr int32 MaxRowsPerCategory = 500;

	void AppendPackages(TArray<TSharedPtr<FString>>& Rows, const TCHAR* Label, const TArray<FName>& Packages, const FName Self)
	{
		int32 Shown = 0;
		for (const FName& Package : Packages)
		{
			if (Package == Self)
			{
				continue;
			}
			if (Shown++ >= MaxRowsPerCategory)
			{
				Rows.Add(MakeShared<FString>(FString::Printf(TEXT("[%s] ... %d개 초과 생략"), Label, Packages.Num() - MaxRowsPerCategory)));
				break;
			}
			Rows.Add(MakeShared<FString>(FString::Printf(TEXT("[%s] %s"), Label, *Package.ToString())));
		}
	}
}

void SWorkbenchT10SearchPanel::Construct(const FArguments& InArgs)
{
	SubjectLabel = LOCTEXT("NoSubject", "선택된 에셋 없음");

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8)
		[
			SNew(STextBlock).AutoWrapText(true)
			.Text(LOCTEXT("Intro", "엔진 기본 검색 창을 여는 진입점과 선택 에셋의 패키지 의존·참조 목록이다. 목록은 에셋 레지스트리에 기록된 정적 관계이며, 문자열 이름·이벤트 바인딩·런타임 로드 같은 동적 사용은 확인하지 못한다."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
			[SNew(SButton).Text(LOCTEXT("Read", "콘텐츠 브라우저 선택 읽기")).OnClicked(this, &SWorkbenchT10SearchPanel::OnReadSelectionClicked)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
			[SNew(SButton).Text(LOCTEXT("List", "의존·참조 목록")).IsEnabled_Lambda([this]() { return HasSubject(); }).OnClicked(this, &SWorkbenchT10SearchPanel::OnListClicked)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
			[SNew(SButton).Text(LOCTEXT("Fib", "Find in Blueprints 열기")).OnClicked(this, &SWorkbenchT10SearchPanel::OnOpenFindInBlueprintsClicked)]
			+ SHorizontalBox::Slot().AutoWidth()
			[SNew(SButton).Text(LOCTEXT("Ref", "Reference Viewer 열기")).OnClicked(this, &SWorkbenchT10SearchPanel::OnOpenReferenceViewerClicked)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 4)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]()
			{
				return FText::Format(LOCTEXT("StatusFmt", "대상: {0}\n{1}\n{2}"), SubjectLabel, ListSummary, EngineMessage);
			})
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(8, 2)
		[
			SAssignNew(RowsView, SListView<FRow>).ListItemsSource(&Rows).OnGenerateRow(this, &SWorkbenchT10SearchPanel::OnGenerateRow)
		]
	];
}

FReply SWorkbenchT10SearchPanel::OnReadSelectionClicked()
{
	Rows.Reset();
	ListSummary = FText::GetEmpty();

	TArray<FAssetData> Selected;
	FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get().GetSelectedAssets(Selected);

	if (Selected.IsEmpty())
	{
		SubjectPackage = NAME_None;
		SubjectLabel = LOCTEXT("NoSelection", "콘텐츠 브라우저에서 선택된 에셋이 없다.");
	}
	else
	{
		SubjectPackage = Selected[0].PackageName;
		SubjectLabel = Selected.Num() > 1
			? FText::Format(LOCTEXT("MultiSel", "{0} (선택 {1}개 중 첫 번째만 사용)"), FText::FromString(Selected[0].GetObjectPathString()), FText::AsNumber(Selected.Num()))
			: FText::FromString(Selected[0].GetObjectPathString());
	}
	RowsView->RequestListRefresh();
	return FReply::Handled();
}

FReply SWorkbenchT10SearchPanel::OnListClicked()
{
	Rows.Reset();
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	if (Registry.IsLoadingAssets())
	{
		ListSummary = LOCTEXT("RegLoading", "에셋 레지스트리가 아직 읽는 중이라 목록이 불완전할 수 있다. 끝난 뒤 다시 조회한다.");
	}

	TArray<FName> HardDeps, SoftDeps, HardRefs, SoftRefs;
	using namespace UE::AssetRegistry;
	Registry.GetDependencies(SubjectPackage, HardDeps, EDependencyCategory::Package, FDependencyQuery(EDependencyQuery::Hard));
	Registry.GetDependencies(SubjectPackage, SoftDeps, EDependencyCategory::Package, FDependencyQuery(EDependencyQuery::Soft));
	Registry.GetReferencers(SubjectPackage, HardRefs, EDependencyCategory::Package, FDependencyQuery(EDependencyQuery::Hard));
	Registry.GetReferencers(SubjectPackage, SoftRefs, EDependencyCategory::Package, FDependencyQuery(EDependencyQuery::Soft));

	AppendPackages(Rows, TEXT("의존-Hard"), HardDeps, SubjectPackage);
	AppendPackages(Rows, TEXT("의존-Soft"), SoftDeps, SubjectPackage);
	AppendPackages(Rows, TEXT("참조-Hard"), HardRefs, SubjectPackage);
	AppendPackages(Rows, TEXT("참조-Soft"), SoftRefs, SubjectPackage);

	ListSummary = FText::Format(LOCTEXT("Summary", "의존 Hard {0} / Soft {1}, 참조 Hard {2} / Soft {3}. 동적 사용은 이 목록에 없으므로 참조 0이어도 미사용으로 단정하지 않는다."),
		FText::AsNumber(HardDeps.Num()), FText::AsNumber(SoftDeps.Num()), FText::AsNumber(HardRefs.Num()), FText::AsNumber(SoftRefs.Num()));
	RowsView->RequestListRefresh();
	return FReply::Handled();
}

FText SWorkbenchT10SearchPanel::BuildIndexStateText() const
{
	FFindInBlueprintSearchManager& Manager = FFindInBlueprintSearchManager::Get();
	const FText Unindexed = FText::AsNumber(Manager.GetNumberUnindexedAssets());
	const FText Discovery = Manager.IsAssetDiscoveryInProgress() ? LOCTEXT("DiscoveryOn", "에셋 탐색 중(색인 범위 불완전)") : LOCTEXT("DiscoveryOff", "에셋 탐색 끝남");
	const FText Caching = Manager.IsCacheInProgress()
		? FText::Format(LOCTEXT("CachingOn", "캐싱 중 {0}%"), FText::AsNumber(FMath::RoundToInt(Manager.GetCacheProgress() * 100.f)))
		: LOCTEXT("CachingOff", "캐싱 중 아님");
	const FText Failed = Manager.HasCachingFailed()
		? FText::Format(LOCTEXT("CachingFailed", "캐싱 실패 {0}개"), FText::AsNumber(Manager.GetFailedToCacheCount()))
		: LOCTEXT("CachingNoFail", "캐싱 실패 없음");
	return FText::Format(LOCTEXT("IndexState", "색인 상태: 미색인 에셋 {0}개 / {1} / {2} / {3}. 색인되지 않은 블루프린트는 검색 결과에 나오지 않는다. 색인 범위의 세부는 엔진 공개 API로 확인할 수 없어 '확인불가'다."),
		Unindexed, Discovery, Caching, Failed);
}

FReply SWorkbenchT10SearchPanel::OnOpenFindInBlueprintsClicked()
{
	const TSharedPtr<SFindInBlueprints> Results = FFindInBlueprintSearchManager::Get().GetGlobalFindResults();
	if (Results.IsValid())
	{
		EngineMessage = FText::Format(LOCTEXT("FibOpened", "Find in Blueprints 창을 열었다. 검색어는 창에서 직접 입력한다.\n{0}"), BuildIndexStateText());
	}
	else
	{
		EngineMessage = FText::Format(LOCTEXT("FibFailed", "Find in Blueprints 창을 열지 못했다(엔진이 결과 창을 반환하지 않음). 직접 열어야 한다.\n{0}"), BuildIndexStateText());
	}
	return FReply::Handled();
}

FReply SWorkbenchT10SearchPanel::OnOpenReferenceViewerClicked()
{
	if (!HasSubject())
	{
		EngineMessage = LOCTEXT("RefNoSubject", "Reference Viewer를 열 대상이 없다. 먼저 콘텐츠 브라우저에서 에셋을 선택하고 '선택 읽기'를 누른다.");
		return FReply::Handled();
	}
	if (!FEditorDelegates::OnOpenReferenceViewer.IsBound())
	{
		EngineMessage = LOCTEXT("RefUnbound", "이 에디터에서는 Reference Viewer 열기 요청을 받는 모듈이 연결되어 있지 않다(미지원). 콘텐츠 브라우저 우클릭 메뉴를 쓴다.");
		return FReply::Handled();
	}
	TArray<FAssetIdentifier> Identifiers;
	Identifiers.Add(FAssetIdentifier(SubjectPackage));
	FEditorDelegates::OnOpenReferenceViewer.Broadcast(Identifiers, FReferenceViewerParams());
	EngineMessage = LOCTEXT("RefRequested", "Reference Viewer 열기를 요청했다. 창이 실제로 떴는지는 이 패널이 확인하지 못한다.");
	return FReply::Handled();
}

TSharedRef<ITableRow> SWorkbenchT10SearchPanel::OnGenerateRow(FRow Row, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<FRow>, OwnerTable)[SNew(STextBlock).Text(FText::FromString(*Row))];
}

#undef LOCTEXT_NAMESPACE
