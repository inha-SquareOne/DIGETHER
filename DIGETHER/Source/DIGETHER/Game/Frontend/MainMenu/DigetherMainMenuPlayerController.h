#pragma once

#include "CoreMinimal.h"
#include "Game/Frontend/DigetherFrontendPlayerController.h"
#include "DigetherMainMenuPlayerController.generated.h"

UCLASS()
class DIGETHER_API ADigetherMainMenuPlayerController : public ADigetherFrontendPlayerController
{
	GENERATED_BODY()

public:
	// 개발·시험용 콘솔 명령, 화면의 방 만들기·코드 참가와 같은 함수 호출, Shipping에서는 동작 없음
	// RoomHost [방코드] [저장옵션], 저장옵션: 생략=슬롯 0 이어하기, new·newN=슬롯 N 새 게임, load·loadN=슬롯 N 이어하기
	// 코드 자동 생성 + 옵션만 줄 때는 방 코드 자리에 ""(예: RoomHost "" new1)
	UFUNCTION(Exec)
	void RoomHost(const FString& RoomCode, const FString& SaveOption);
	UFUNCTION(Exec)
	void RoomJoin(const FString& RoomCode, float TimeoutSeconds);

protected:
	virtual TSoftClassPtr<UCommonActivatableWidget> GetScreenClass() const override;
	virtual const TCHAR* GetAutoCommandsParam() const override { return TEXT("DigetherMenuAutoCmds"); }
};
