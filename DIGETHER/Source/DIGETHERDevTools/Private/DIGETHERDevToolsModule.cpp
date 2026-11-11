// 개발 치트툴 모듈: 명령 등록, 화면 디버그 표시, 같은 PC 브라우저용 웹 서버
// Shipping에서는 이 모듈 자체가 빌드되지 않음(DIGETHER.uproject TargetConfigurationDenyList)
#include "Commands/DigetherDevCommands.h"
#include "DigetherDevDebugDraw.h"
#include "DigetherDevWebServer.h"
#include "Engine/Engine.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"

namespace
{
	FDigetherDevWebServer WebServer;
	FDelegateHandle PostEngineInitHandle;

	// 에디터(PIE)는 자동 실행, 패키지 개발 빌드는 -DigetherDevWeb, 자동 검사(-unattended)·커맨드렛은 끔
	bool ShouldAutoStartWeb()
	{
		const TCHAR* CommandLine = FCommandLine::Get();
		if (IsRunningCommandlet() || FApp::IsUnattended() || FParse::Param(CommandLine, TEXT("NoDigetherDevWeb")))
		{
			return false;
		}
		return GIsEditor || FParse::Param(CommandLine, TEXT("DigetherDevWeb"));
	}

	void HandlePostEngineInit()
	{
		if (ShouldAutoStartWeb())
		{
			DigetherDev::StartWebServer();
		}
	}
}

namespace DigetherDev
{
	FDigetherDevWebServer& GetWebServer()
	{
		return WebServer;
	}

	bool StartWebServer()
	{
		uint32 Port = FDigetherDevWebServer::DefaultPort;
		FParse::Value(FCommandLine::Get(), TEXT("DigetherDevWebPort="), Port);
		return WebServer.Start(Port);
	}
}

class FDigetherDevToolsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		DigetherDev::Debug::Startup();
		// 예약(사용 불가) 먼저, 같은 이름을 실제로 연결한 도메인 등록이 뒤에서 교체
		DigetherDev::RegisterReservedCommands();
		DigetherDev::RegisterCoreCommands();
		DigetherDev::RegisterLegacyCommands();
		DigetherDev::RegisterMonsterCommands();
		DigetherDev::RegisterRunCommands();
		DigetherDev::RegisterPlayerCommands();
		DigetherDev::RegisterDisplayPanels();
		if (GEngine && GEngine->IsInitialized())
		{
			HandlePostEngineInit();
		}
		else
		{
			PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddStatic(&HandlePostEngineInit);
		}
	}

	virtual void ShutdownModule() override
	{
		FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
		WebServer.Stop();
		DigetherDev::UnregisterAll();
		DigetherDev::Debug::Shutdown();
	}
};

IMPLEMENT_MODULE(FDigetherDevToolsModule, DIGETHERDevTools)
