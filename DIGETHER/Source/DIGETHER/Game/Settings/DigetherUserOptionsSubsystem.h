#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "DigetherUserOptionsSubsystem.generated.h"

// 로컬 플레이어 설정 유지: 키 재설정 컨텍스트 등록(저장된 키 적용), 컨트롤러 변경·맵 이동 후 볼륨·밝기·마이크 값 재적용
UCLASS()
class DIGETHER_API UDigetherUserOptionsSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

private:
	void HandlePostLoadMap(UWorld* World);

	FDelegateHandle PostLoadMapHandle;
};
