#include "DigetherWorkbenchEditorModule.h"
#include "WorkbenchModule.h"
#include "SDigetherConnectionStatusPanel.h"
#include "SDigetherDeveloperToolsPanel.h"
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

FName GetDigetherDeveloperToolsToolId()
{
	return FName(TEXT("Digether.DeveloperTools"));
}

FWorkbenchToolDescriptor MakeDigetherDeveloperToolsDescriptor()
{
	return FWorkbenchToolDescriptor(
		GetDigetherDeveloperToolsToolId(),
		LOCTEXT("DevToolsTitle", "DIGETHER 개발 도구"),
		LOCTEXT("DevToolsDescription",
			"엔진 버전, 유용한 플러그인 상태, 개인 자동 저장 설정값, GameFeatureData 에셋 매니저 규칙을 조회하고 "
			"관련 엔진 화면을 여는 버튼을 제공합니다. 표시된 값은 설정 조회 결과이며 기능 동작 검증이 아닙니다."),
		[]() -> TSharedRef<SWidget>
		{
			return SNew(SDigetherDeveloperToolsPanel);
		});
}

FDigetherRegisteredTools RegisterDigetherTools(TFunctionRef<bool(const FWorkbenchToolDescriptor&)> RegisterTool)
{
	FDigetherRegisteredTools Registered;

	Registered.bConnectionStatus = RegisterTool(MakeDigetherConnectionStatusDescriptor());
	if (!Registered.bConnectionStatus)
	{
		UE_LOG(LogDigetherWorkbenchEditor, Warning,
			TEXT("'DIGETHER 연결 상태' 도구 등록이 거부되었습니다. 같은 ID(%s)가 이미 등록되어 있을 수 있습니다."),
			*GetDigetherConnectionStatusToolId().ToString());
	}

	Registered.bDeveloperTools = RegisterTool(MakeDigetherDeveloperToolsDescriptor());
	if (!Registered.bDeveloperTools)
	{
		UE_LOG(LogDigetherWorkbenchEditor, Warning,
			TEXT("'DIGETHER 개발 도구' 도구 등록이 거부되었습니다. 같은 ID(%s)가 이미 등록되어 있을 수 있습니다."),
			*GetDigetherDeveloperToolsToolId().ToString());
	}

	return Registered;
}

void UnregisterDigetherTools(FDigetherRegisteredTools& Registered, TFunctionRef<void(FName)> UnregisterTool)
{
	if (Registered.bConnectionStatus)
	{
		Registered.bConnectionStatus = false;
		UnregisterTool(GetDigetherConnectionStatusToolId());
	}
	if (Registered.bDeveloperTools)
	{
		Registered.bDeveloperTools = false;
		UnregisterTool(GetDigetherDeveloperToolsToolId());
	}
}

void FDigetherWorkbenchEditorModule::StartupModule()
{
	IWorkbenchModule* WorkbenchModule = FModuleManager::Get().LoadModulePtr<IWorkbenchModule>(WorkbenchCoreModuleName);
	if (!WorkbenchModule)
	{
		UE_LOG(LogDigetherWorkbenchEditor, Error,
			TEXT("Core 모듈 '%s'을 로드하지 못해 DIGETHER 도구를 등록하지 못했습니다."),
			WorkbenchCoreModuleName);
		return;
	}

	RegisteredTools = RegisterDigetherTools(
		[WorkbenchModule](const FWorkbenchToolDescriptor& Descriptor)
		{
			return WorkbenchModule->RegisterTool(Descriptor);
		});
}

void FDigetherWorkbenchEditorModule::ShutdownModule()
{
	if (!RegisteredTools.bConnectionStatus && !RegisteredTools.bDeveloperTools)
	{
		return;
	}

	IWorkbenchModule* WorkbenchModule = FModuleManager::Get().GetModulePtr<IWorkbenchModule>(WorkbenchCoreModuleName);
	if (!WorkbenchModule)
	{
		RegisteredTools = FDigetherRegisteredTools();
		return;
	}

	UnregisterDigetherTools(RegisteredTools,
		[WorkbenchModule](FName ToolId)
		{
			WorkbenchModule->UnregisterTool(ToolId);
		});
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDigetherWorkbenchEditorModule, DigetherWorkbenchEditor)
