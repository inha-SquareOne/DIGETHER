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

private:
	UPROPERTY(Transient)
	TObjectPtr<UDigetherFrontendLayoutWidget> Layout;
};
