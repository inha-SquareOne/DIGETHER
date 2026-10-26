#include "DigetherWorkbenchEditorModule.h"
#include "WorkbenchModule.h"
#include "SDigetherConnectionStatusPanel.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SWidget.h"

#define LOCTEXT_NAMESPACE "DigetherWorkbenchEditor"

DEFINE_LOG_CATEGORY_STATIC(LogDigetherWorkbenchEditor, Log, All);

namespace
{
	const TCHAR* const WorkbenchCoreModuleName = TEXT("WorkbenchEditor");
}

FName GetDigetherConnectionStatusToolId()
{
	return FName(TEXT("Digether.ConnectionStatus"));
}

FWorkbenchToolDescriptor MakeDigetherConnectionStatusDescriptor()
{
	return FWorkbenchToolDescriptor(
		GetDigetherConnectionStatusToolId(),
		LOCTEXT("ToolTitle", "DIGETHER 연결 상태"),
		LOCTEXT("ToolDescription",
			"아이템/경제/스폰/룸/런타임 준비 신호와 기획 승인 개정의 연결 여부를 보여주는 합성 예제 패널입니다. "
			"실게임 provider는 연결되지 않았으며, 합성 도구의 결과는 실게임 결과와 다릅니다."),
		[]() -> TSharedRef<SWidget>
		{
			return SNew(SDigetherConnectionStatusPanel);
		});
}

void FDigetherWorkbenchEditorModule::StartupModule()
{
	IWorkbenchModule* WorkbenchModule = FModuleManager::Get().LoadModulePtr<IWorkbenchModule>(WorkbenchCoreModuleName);
	if (!WorkbenchModule)
	{
		UE_LOG(LogDigetherWorkbenchEditor, Error,
			TEXT("Core 모듈 '%s'을 로드하지 못해 'DIGETHER 연결 상태' 패널을 등록하지 못했습니다."),
			WorkbenchCoreModuleName);
		return;
	}

	bConnectionStatusToolRegistered = WorkbenchModule->RegisterTool(MakeDigetherConnectionStatusDescriptor());
	if (!bConnectionStatusToolRegistered)
	{
		UE_LOG(LogDigetherWorkbenchEditor, Warning,
			TEXT("'DIGETHER 연결 상태' 도구 등록이 거부되었습니다. 같은 ID(%s)가 이미 등록되어 있을 수 있습니다."),
			*GetDigetherConnectionStatusToolId().ToString());
	}
}

void FDigetherWorkbenchEditorModule::ShutdownModule()
{
	if (!bConnectionStatusToolRegistered)
	{
		return;
	}
	bConnectionStatusToolRegistered = false;

	if (IWorkbenchModule* WorkbenchModule = FModuleManager::Get().GetModulePtr<IWorkbenchModule>(WorkbenchCoreModuleName))
	{
		WorkbenchModule->UnregisterTool(GetDigetherConnectionStatusToolId());
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDigetherWorkbenchEditorModule, DigetherWorkbenchEditor)
