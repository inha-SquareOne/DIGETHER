#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "DigetherPlayerCameraManager.generated.h"

// 설정 화면 시야각 적용, 카메라 컴포넌트 유무와 무관하게 플레이 시점 기본 FOV 지정
// 카메라 흔들림·모디파이어는 그 뒤에 적용되어 유지, 카메라 액터(연출) 시점은 원래 값
UCLASS()
class DIGETHER_API ADigetherPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

protected:
	virtual void PostInitializeComponents() override;
	virtual void UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime) override;

private:
	// 엔진 화면 페이드의 기본 음량 페이드는 끝날 때 전체 음량을 1로 덮어 설정값이 사라짐
	// 이벤트를 받으면 엔진 기본 페이드가 꺼지고 설정 음량 유지, 화면 페이드에 소리 페이드는 없음
	UFUNCTION()
	void HandleAudioFadeChange(bool bAudioFadeOut, float AudioFadeTime);
};
