#pragma once

#include "CoreMinimal.h"
#include "Game/Frontend/DigetherFrontendPlayerController.h"
#include "DigetherLobbyPlayerController.generated.h"

class UDigetherVoiceComponent;

// 대기실 입력: 준비 전환(참가자), 게임 시작(호스트), 나가기
UCLASS()
class DIGETHER_API ADigetherLobbyPlayerController : public ADigetherFrontendPlayerController
{
	GENERATED_BODY()

public:
	ADigetherLobbyPlayerController();

	// 서버 기본 음성 허용 뒤 개인 마이크 모드 적용
	virtual void ClientEnableNetworkVoice_Implementation(bool bEnable) override;

	void RequestSetReady(bool bReady);
	// 호스트 화면 전용, 서버 GameMode가 준비 상태 재확인
	void RequestStartGame();
	void RequestLeave();

	// 개발·시험용 콘솔 명령, 화면 버튼과 같은 함수 호출, Shipping에서는 동작 없음
	UFUNCTION(Exec) void LobbyReady(bool bReady);
	UFUNCTION(Exec) void LobbyStart();
	UFUNCTION(Exec) void LobbyLeave();

protected:
	virtual const TCHAR* GetAutoCommandsParam() const override { return TEXT("DigetherLobbyAutoCmds"); }
	virtual TSoftClassPtr<UCommonActivatableWidget> GetScreenClass() const override;
	// 클라이언트는 PlayerState가 목록 복제보다 늦게 연결될 수 있어 연결 시 대기실 화면 재갱신 통지
	virtual void OnRep_PlayerState() override;

private:
	UFUNCTION(Server, Reliable)
	void Server_SetReady(bool bReady);

	// 대기실은 몸이 없어 모든 참가자 목소리를 감쇠 없이 재생
	UPROPERTY(VisibleAnywhere, Category="Digether|Voice")
	TObjectPtr<UDigetherVoiceComponent> VoiceComponent;
};
