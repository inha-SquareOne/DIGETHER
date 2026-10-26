// DIGETHER 전용 Editor 어댑터 모듈 진입점. Core(WorkbenchEditor 모듈)의 IWorkbenchModule에
// 'DIGETHER 연결 상태' 도구를 등록·해제한다.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

struct FWorkbenchToolDescriptor;

/** 'DIGETHER 연결 상태' 도구의 Core 등록 ID. */
FName GetDigetherConnectionStatusToolId();

/** 'DIGETHER 연결 상태' 도구 설명자. MakeWidget은 탭을 열 때만 호출된다. */
FWorkbenchToolDescriptor MakeDigetherConnectionStatusDescriptor();

class FDigetherWorkbenchEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	bool bConnectionStatusToolRegistered = false;
};
