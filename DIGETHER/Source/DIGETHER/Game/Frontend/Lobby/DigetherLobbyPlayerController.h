#pragma once

#include "CoreMinimal.h"
#include "Game/Frontend/DigetherFrontendPlayerController.h"
#include "DigetherLobbyPlayerController.generated.h"

// 대기실 입력: 준비 전환(참가자), 게임 시작(호스트), 나가기.
UCLASS()
class DIGETHER_API ADigetherLobbyPlayerController : public ADigetherFrontendPlayerController
{
	GENERATED_BODY()

public:
	void RequestSetReady(bool bReady);
	// 호스트 화면에서만 호출된다. 서버 쪽 GameMode가 준비 상태를 다시 확인한다.
	void RequestStartGame();
	void RequestLeave();

	// 개발·시험용 콘솔 명령. 화면 버튼과 같은 함수를 호출한다. 출시 빌드에서는 아무 일도 하지 않는다.
	UFUNCTION(Exec) void LobbyReady(bool bReady);
	UFUNCTION(Exec) void LobbyStart();
	UFUNCTION(Exec) void LobbyLeave();

protected:
	virtual TSoftClassPtr<UCommonActivatableWidget> GetScreenClass() const override;
	// 클라이언트는 PlayerState가 목록 복제보다 늦게 연결될 수 있어, 연결되면 대기실 화면을 다시 그리게 알린다.
	virtual void OnRep_PlayerState() override;

private:
	UFUNCTION(Server, Reliable)
	void Server_SetReady(bool bReady);
};
