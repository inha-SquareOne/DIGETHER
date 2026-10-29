// DIGETHER 전용 Editor 어댑터 모듈 진입점. Core(WorkbenchEditor 모듈)의 IWorkbenchModule에
// 'DIGETHER 연결 상태'와 'DIGETHER 개발 도구'를 등록·해제한다.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Templates/Function.h"

struct FWorkbenchToolDescriptor;

/** 'DIGETHER 연결 상태' 도구의 Core 등록 ID. */
FName GetDigetherConnectionStatusToolId();

/** 'DIGETHER 연결 상태' 도구 설명자. MakeWidget은 탭을 열 때만 호출된다. */
FWorkbenchToolDescriptor MakeDigetherConnectionStatusDescriptor();

/** 'DIGETHER 개발 도구' 도구의 Core 등록 ID. */
FName GetDigetherDeveloperToolsToolId();

/** 'DIGETHER 개발 도구' 도구 설명자. MakeWidget은 탭을 열 때만 호출된다. */
FWorkbenchToolDescriptor MakeDigetherDeveloperToolsDescriptor();

/** 이 모듈이 실제로 등록에 성공한 도구만 기록한다. 해제는 여기 기록된 도구에 한정한다. */
struct FDigetherRegisteredTools
{
	bool bConnectionStatus = false;
	bool bDeveloperTools = false;
};

/** 각 도구를 독립적으로 등록한다. 한 도구의 거부가 다른 도구 등록을 막지 않는다. */
FDigetherRegisteredTools RegisterDigetherTools(TFunctionRef<bool(const FWorkbenchToolDescriptor&)> RegisterTool);

/** 기록된 도구만 해제하고 기록을 비운다. 해제할 것이 없으면 Unregister를 호출하지 않는다. */
void UnregisterDigetherTools(FDigetherRegisteredTools& Registered, TFunctionRef<void(FName)> UnregisterTool);

class FDigetherWorkbenchEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	FDigetherRegisteredTools RegisteredTools;
};
