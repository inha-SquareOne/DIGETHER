#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "DigetherLobbyGameState.generated.h"

// 대기실 화면에 표시할 방 이름·코드. 호스트가 세션 정보로 채워 참가자에게 복제한다.
// 참가자 입장·퇴장, 준비·호스트 표시, 이름, 방 정보가 바뀌면 OnLobbyChanged를 알린다(서버·클라이언트 모두).
UCLASS()
class DIGETHER_API ADigetherLobbyGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	const FString& GetRoomName() const { return RoomName; }
	const FString& GetRoomCode() const { return RoomCode; }
	void SetRoomInfo(const FString& InRoomName, const FString& InRoomCode);
	// 호스트를 뺀 모든 참가자가 준비했는지. 혼자서도 시작할 수 있다(서버·클라이언트 모두 같은 판정).
	bool AreGuestsReady() const;

	// 대기실 표시 정보가 바뀌었을 때 화면이 다시 그리도록 알린다.
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
