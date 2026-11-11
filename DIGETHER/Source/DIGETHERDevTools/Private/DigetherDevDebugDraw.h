#pragma once

#include "CoreMinimal.h"

class UWorld;

// 화면 디버그 표시 틀: 항목마다 콘솔 변수 digether.Debug.<Id>(0/1)
// 글자 패널은 UDebugDrawService 캔버스, 3D 표시는 월드 틱 뒤 DrawDebug*
// 값은 판정 코드가 쓰는 컴포넌트·서브시스템에서 그대로 읽어야 함(표시용 재계산 금지)
namespace DigetherDev::Debug
{
	using FTextProvider = TFunction<void(UWorld* World, TArray<FString>& OutLines)>;
	using FWorldDrawer = TFunction<void(UWorld* World)>;

	struct FPanelDesc
	{
		FName Id;
		FString Title;
		FString RequestIds;
		// 호스트만 아는 값이면 참가자 화면에서 안내 한 줄만 표시
		bool bHostOnly = false;
		FTextProvider Text;
		FWorldDrawer Draw3D;
	};

	void RegisterPanel(FPanelDesc Desc);
	void Startup();
	void Shutdown();

	bool SetEnabled(FName Id, bool bEnabled);
	bool IsEnabled(FName Id);
	// 등록 순서대로 {Id, 켜짐}
	TArray<TPair<FString, bool>> GetPanelStates();
	TArray<FString> GetPanelIds();
}
