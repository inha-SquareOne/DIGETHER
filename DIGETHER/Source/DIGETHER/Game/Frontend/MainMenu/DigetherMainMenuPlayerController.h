#pragma once

#include "CoreMinimal.h"
#include "Game/Frontend/DigetherFrontendPlayerController.h"
#include "DigetherMainMenuPlayerController.generated.h"

UCLASS()
class DIGETHER_API ADigetherMainMenuPlayerController : public ADigetherFrontendPlayerController
{
	GENERATED_BODY()

public:
	// 개발·시험용 콘솔 명령. 화면의 방 만들기·코드 참가와 같은 함수를 호출한다.
	UFUNCTION(Exec)
	void RoomHost(const FString& RoomCode);
	UFUNCTION(Exec)
	void RoomJoin(const FString& RoomCode, float TimeoutSeconds);

protected:
	virtual TSoftClassPtr<UCommonActivatableWidget> GetScreenClass() const override;
};
