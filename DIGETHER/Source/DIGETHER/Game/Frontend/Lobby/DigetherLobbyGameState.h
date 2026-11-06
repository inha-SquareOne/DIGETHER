#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "DigetherLobbyGameState.generated.h"

// 대기실 화면용 방 이름·코드, 호스트가 세션 정보로 채워 참가자에게 복제
// 참가자 입장·퇴장, 준비·호스트 표시, 이름, 방 정보 변경 시 OnLobbyChanged 통지(서버·클라이언트 모두)
UCLASS()
class DIGETHER_API ADigetherLobbyGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	const FString& GetRoomName() const { return RoomName; }
	const FString& GetRoomCode() const { return RoomCode; }
	void SetRoomInfo(const FString& InRoomName, const FString& InRoomCode);
	// 호스트를 뺀 모든 참가자 준비 여부, 혼자서도 시작 가능(서버·클라이언트 같은 판정)
	bool AreGuestsReady() const;

	// 대기실 표시 정보 변경 시 화면 재갱신 통지
	void NotifyLobbyChanged();
	FSimpleMulticastDelegate OnLobbyChanged;

	virtual void AddPlayerState(APlayerState* PlayerState) override;
	virtual void RemovePlayerState(APlayerState* PlayerState) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_RoomInfo();

	UPROPERTY(ReplicatedUsing = OnRep_RoomInfo) FString RoomName;
	UPROPERTY(ReplicatedUsing = OnRep_RoomInfo) FString RoomCode;
};
