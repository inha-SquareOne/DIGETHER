#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "DigetherCheatManager.generated.h"

// 게임 PlayerController의 CheatClass, 엔진 기본 치트(god·slomo·ToggleDebugCamera 등)만 제공
// Digether 개발 명령은 DIGETHERDevTools 모듈이 콘솔·웹 명령으로 등록(치트 매니저 생성 여부와 무관)
// 기존 에셋의 CheatClass 참조 호환으로 클래스 유지, Shipping에서는 엔진이 생성하지 않음
UCLASS()
class DIGETHER_API UDigetherCheatManager : public UCheatManager
{
	GENERATED_BODY()
};
