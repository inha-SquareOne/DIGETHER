#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DigetherFrontendPlayerController.generated.h"

class UCommonActivatableWidget;
class UDigetherFrontendLayoutWidget;

// 메인 메뉴·대기실 공통: 로컬 플레이어에게 프런트엔드 레이아웃을 띄우고 이 맵의 화면을 올림
// 입력 모드는 CommonUI 화면(UCommonActivatableWidget::GetDesiredInputConfig)이 결정
UCLASS(Abstract)
class DIGETHER_API ADigetherFrontendPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 이 맵에서 처음 올릴 화면, 프로젝트 설정(DIGETHER Frontend) 값
	virtual TSoftClassPtr<UCommonActivatableWidget> GetScreenClass() const PURE_VIRTUAL(ADigetherFrontendPlayerController::GetScreenClass, return nullptr;);

	// 다인 시험용 예약 명령의 명령줄 키(예: TEXT("DigetherLobbyAutoCmds")), 비면 미사용, Shipping 미동작
	virtual const TCHAR* GetAutoCommandsParam() const { return nullptr; }

private:
#if !UE_BUILD_SHIPPING
	// -<키>="초:명령|초:명령"을 이 맵 입장 기준으로 예약, 키별 프로세스당 한 번
	void ScheduleAutoCommands();
#endif

	UPROPERTY(Transient)
	TObjectPtr<UDigetherFrontendLayoutWidget> Layout;
};
