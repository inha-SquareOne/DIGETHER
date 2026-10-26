#include "WorkbenchModule.h"

#include "Analytics/WorkbenchAnalytics.h"
#include "Core/WorkbenchCorePanels.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "Tools/WorkbenchTools.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WorkbenchEditorModule"

namespace
{
	const FName HubTabId(TEXT("Workbench.Hub"));
	const FName ToolMenuOwner(TEXT("DevelopmentWorkbench"));

	FName ToolTabId(FName ToolId)
	{
		return FName(*FString::Printf(TEXT("Workbench.Tool.%s"), *ToolId.ToString()));
	}
}

class FWorkbenchEditorModule final : public IWorkbenchModule
{
public:
	virtual void StartupModule() override
	{
		RegisterBuiltInTools();

		if (IsRunningCommandlet() || IsRunningDedicatedServer() || !FSlateApplication::IsInitialized())
		{
			return;
		}

		bUiActive = true;
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(HubTabId, FOnSpawnTab::CreateRaw(this, &FWorkbenchEditorModule::SpawnHubTab))
			.SetDisplayName(LOCTEXT("HubTabName", "개발 워크벤치"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);
		for (const FWorkbenchToolDescriptor& Tool : Tools)
		{
			RegisterTabSpawner(Tool);
		}
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FWorkbenchEditorModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		if (bUiActive)
		{
			UToolMenus::UnRegisterStartupCallback(this);
			UToolMenus::UnregisterOwner(ToolMenuOwner);
			if (FSlateApplication::IsInitialized())
			{
				CloseLiveTabs(NAME_None);
			}
			if (FSlateApplication::IsInitialized())
			{
				for (const FWorkbenchToolDescriptor& Tool : Tools)
				{
					FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ToolTabId(Tool.Id));
				}
				FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(HubTabId);
			}
			bUiActive = false;
		}
		Tools.Reset();
	}

	virtual bool RegisterTool(const FWorkbenchToolDescriptor& Descriptor) override
	{
		if (!Descriptor.IsValid() || FindIndex(Descriptor.Id) != INDEX_NONE)
		{
			return false;
		}
		Tools.Add(Descriptor);
		if (bUiActive)
		{
			RegisterTabSpawner(Descriptor);
		}
		return true;
	}

	virtual void UnregisterTool(FName ToolId) override
	{
		const int32 Index = FindIndex(ToolId);
		if (Index == INDEX_NONE)
		{
			return;
		}
		if (bUiActive && FSlateApplication::IsInitialized())
		{
			CloseLiveTabs(ToolId);
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ToolTabId(ToolId));
		}
		Tools.RemoveAt(Index);
	}

	virtual TArray<FWorkbenchToolDescriptor> GetRegisteredTools() const override
	{
		return Tools;
	}

private:
	int32 FindIndex(FName ToolId) const
	{
		return Tools.IndexOfByPredicate([ToolId](const FWorkbenchToolDescriptor& Tool) { return Tool.Id == ToolId; });
	}

	void RegisterTabSpawner(const FWorkbenchToolDescriptor& Tool)
	{
		const FName Id = Tool.Id;
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ToolTabId(Id), FOnSpawnTab::CreateRaw(this, &FWorkbenchEditorModule::SpawnToolTab, Id))
			.SetDisplayName(Tool.Title)
			.SetTooltipText(Tool.Description)
			.SetMenuType(ETabSpawnerMenuType::Hidden);
	}

	TSharedRef<SDockTab> SpawnToolTab(const FSpawnTabArgs& Args, FName ToolId)
	{
		const int32 Index = FindIndex(ToolId);
		TSharedRef<SWidget> Content = SNew(SBox).Padding(12.f)
		[
			SNew(STextBlock).Text(FText::Format(LOCTEXT("ToolGone", "'{0}' 도구는 더 이상 등록되어 있지 않습니다."), FText::FromName(ToolId)))
		];
		FText Title = FText::FromName(ToolId);
		if (Index != INDEX_NONE)
		{
			Title = Tools[Index].Title;
			Content = Tools[Index].MakeWidget();
		}
		TSharedRef<SDockTab> Tab = SNew(SDockTab).TabRole(ETabRole::NomadTab).Label(Title)
		[
			Content
		];
		LiveTabs.RemoveAll([](const TPair<FName, TWeakPtr<SDockTab>>& Entry) { return !Entry.Value.IsValid(); });
		LiveTabs.Emplace(ToolId, Tab);
		return Tab;
	}

	TSharedRef<SDockTab> SpawnHubTab(const FSpawnTabArgs& Args)
	{
		TSharedRef<SScrollBox> List = SNew(SScrollBox);
		List->AddSlot().Padding(8.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HubIntro", "개발 워크벤치 도구 목록입니다. 버튼으로 각 도구를 별도 탭에서 엽니다. 게임 규칙·수치 정책은 정해지지 않아 도구는 승인된 정책이 없는 항목을 '결정 대기'로 표시합니다."))
			.AutoWrapText(true)
		];
		for (const FWorkbenchToolDescriptor& Tool : Tools)
		{
			const FName Id = Tool.Id;
			List->AddSlot().Padding(8.f, 4.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(SButton)
					.Text(Tool.Title)
					.OnClicked_Lambda([Id]()
					{
						FGlobalTabmanager::Get()->TryInvokeTab(FTabId(ToolTabId(Id)));
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(Tool.Description).AutoWrapText(true)
				]
			];
		}
		TSharedRef<SDockTab> Tab = SNew(SDockTab).TabRole(ETabRole::NomadTab).Label(LOCTEXT("HubTabLabel", "개발 워크벤치"))
		[
			List
		];
		LiveTabs.RemoveAll([](const TPair<FName, TWeakPtr<SDockTab>>& Entry) { return !Entry.Value.IsValid(); });
		LiveTabs.Emplace(HubTabId, Tab);
		return Tab;
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(ToolMenuOwner);
		UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		FToolMenuSection& Section = ToolsMenu->FindOrAddSection(TEXT("DevelopmentWorkbench"), LOCTEXT("MenuSection", "개발 워크벤치"));
		Section.AddMenuEntry(
			TEXT("OpenHub"),
			LOCTEXT("OpenHub", "개발 워크벤치 열기"),
			LOCTEXT("OpenHubTip", "도구 목록 탭을 엽니다."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(FTabId(HubTabId)); })));
		Section.AddSubMenu(
			TEXT("Tools"),
			LOCTEXT("ToolsSubMenu", "도구"),
			LOCTEXT("ToolsSubMenuTip", "등록된 개발 도구를 엽니다."),
			FNewToolMenuDelegate::CreateRaw(this, &FWorkbenchEditorModule::FillToolsSubMenu));
	}

	void FillToolsSubMenu(UToolMenu* Menu)
	{
		FToolMenuSection& Section = Menu->AddSection(TEXT("RegisteredTools"));
		for (const FWorkbenchToolDescriptor& Tool : Tools)
		{
			const FName Id = Tool.Id;
			Section.AddMenuEntry(
				ToolTabId(Id),
				Tool.Title,
				Tool.Description,
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([Id]() { FGlobalTabmanager::Get()->TryInvokeTab(FTabId(ToolTabId(Id))); })));
		}
	}

	void RegisterBuiltInTools()
	{
		// 메뉴·허브 순서는 T01..T11이다. 담당: Core(T01·T05·T08), Analytics(T02~T04), Tools(T06·T07·T09~T11).
		RegisterTool(FWorkbenchToolDescriptor(
			TEXT("T01.ItemEditor"),
			LOCTEXT("T01Title", "T01 아이템 편집"),
			LOCTEXT("T01Desc", "DataTable 행의 허용된 스칼라 필드를 초안으로 편집하고 검증한 뒤 적용·저장합니다. 행 추가·삭제, 배열·맵·객체 참조는 지원하지 않습니다."),
			[]() { return WorkbenchCore::MakePanel(TEXT("T01.ItemEditor")); }));
		RegisterTool(FWorkbenchToolDescriptor(
			TEXT("T02"),
			LOCTEXT("T02Title", "T02 스폰 확률"),
			LOCTEXT("T02Desc", "단일 풀 복원 추출의 정확 확률·기대값을 계산하고 결정적 Monte Carlo 결과와 대조합니다. 비복원·pity·조건부 가중치는 지원하지 않습니다."),
			[]() { return WorkbenchAnalytics::MakePanel(TEXT("T02")); }));
		RegisterTool(FWorkbenchToolDescriptor(
			TEXT("T03"),
			LOCTEXT("T03Title", "T03 합성 잠재가치 분석"),
			LOCTEXT("T03Desc", "입력한 풀에서 가중 복원 추출한 값의 합을 에피소드 단위로 반복하는 합성 시뮬레이션입니다. 실게임 수익이 아닙니다."),
			[]() { return WorkbenchAnalytics::MakePanel(TEXT("T03")); }));
		RegisterTool(FWorkbenchToolDescriptor(
			TEXT("T04"),
			LOCTEXT("T04Title", "T04 룸 연결성 검사"),
			LOCTEXT("T04Desc", "입력한 룸·문·연결 매니페스트를 스키마·허용 오차·그래프 도달성 기준으로 검사합니다. 월드 충돌·NavMesh·PCG와는 연결되어 있지 않습니다."),
			[]() { return WorkbenchAnalytics::MakePanel(TEXT("T04")); }));
		RegisterTool(FWorkbenchToolDescriptor(
			TEXT("T05.DataValidation"),
			LOCTEXT("T05Title", "T05 데이터 검증"),
			LOCTEXT("T05Desc", "UI·자동화 테스트·엔진 DataValidation이 공유하는 구조 검증 규칙을 실행합니다. 승인되지 않은 정책은 결정 대기로 표시합니다."),
			[]() { return WorkbenchCore::MakePanel(TEXT("T05.DataValidation")); }));
		RegisterTool(FWorkbenchToolDescriptor(
			TEXT("T06"),
			LOCTEXT("T06Title", "T06 멀티플레이 실행"),
			LOCTEXT("T06Desc", "에디터 PIE와 분리된 별도 프로세스로 N명 멀티플레이 실행을 띄우고, 이 패널이 만든 프로세스만 추적·종료합니다."),
			[]() { return WorkbenchTools::MakePanel(TEXT("T06")); }));
		RegisterTool(FWorkbenchToolDescriptor(
			TEXT("T07"),
			LOCTEXT("T07Title", "T07 인스펙터 프로필"),
			LOCTEXT("T07Desc", "고른 액터 또는 블루프린트 CDO를 엔진 Details 뷰로 직접 편집하고, bool 조건 기반 프로필로 보이는 속성을 좁힙니다."),
			[]() { return WorkbenchTools::MakePanel(TEXT("T07")); }));
		RegisterTool(FWorkbenchToolDescriptor(
			TEXT("T08.CsvBulkEdit"),
			LOCTEXT("T08Title", "T08 CSV 일괄 편집"),
			LOCTEXT("T08Desc", "이 도구가 내보낸 CSV를 편집해 미리보기로 검토한 뒤 적용합니다. 기준 해시가 없는 CSV는 거부하고, 빈 셀은 원본을 유지하며, 중복 ID·알 수 없는 열은 거부합니다."),
			[]() { return WorkbenchCore::MakePanel(TEXT("T08.CsvBulkEdit")); }));
		RegisterTool(FWorkbenchToolDescriptor(
			TEXT("T09"),
			LOCTEXT("T09Title", "T09 에셋 정리"),
			LOCTEXT("T09Desc", "경로 기준으로 에셋을 조사하고 접두사 규칙 이름 변경 계획과 참조 없음 후보를 보여줍니다. 삭제 기능은 없습니다."),
			[]() { return WorkbenchTools::MakePanel(TEXT("T09")); }));
		RegisterTool(FWorkbenchToolDescriptor(
			TEXT("T10"),
			LOCTEXT("T10Title", "T10 검색·참조 보기"),
			LOCTEXT("T10Desc", "엔진 기본 검색(Find in Blueprints)과 Reference Viewer를 여는 진입점과 콘텐츠 브라우저 선택 에셋 정보를 제공합니다."),
			[]() { return WorkbenchTools::MakePanel(TEXT("T10")); }));
		RegisterTool(FWorkbenchToolDescriptor(
			TEXT("T11"),
			LOCTEXT("T11Title", "T11 배치 도우미"),
			LOCTEXT("T11Desc", "선택 액터 중 첫 번째를 기준으로 나머지를 간격·회전·스케일 단계·시드 지터로 재배치합니다."),
			[]() { return WorkbenchTools::MakePanel(TEXT("T11")); }));
	}

	void CloseLiveTabs(FName ToolId)
	{
		for (int32 i = LiveTabs.Num() - 1; i >= 0; --i)
		{
			if (!ToolId.IsNone() && LiveTabs[i].Key != ToolId)
			{
				continue;
			}
			if (TSharedPtr<SDockTab> Tab = LiveTabs[i].Value.Pin())
			{
				Tab->RequestCloseTab();
			}
			LiveTabs.RemoveAt(i);
		}
	}

	TArray<FWorkbenchToolDescriptor> Tools;
	TArray<TPair<FName, TWeakPtr<SDockTab>>> LiveTabs;
	bool bUiActive = false;
};

IWorkbenchModule& IWorkbenchModule::Get()
{
	return FModuleManager::LoadModuleChecked<IWorkbenchModule>(TEXT("WorkbenchEditor"));
}

bool IWorkbenchModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded(TEXT("WorkbenchEditor"));
}

IMPLEMENT_MODULE(FWorkbenchEditorModule, WorkbenchEditor)

#undef LOCTEXT_NAMESPACE
