#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "DigetherLobbyPlayerState.generated.h"

// 대기실 참가자의 준비·호스트 상태. 서버만 바꾸고 모두에게 복제한다.
// 값이 바뀌면 대기실 GameState에 알려 화면이 갱신되게 한다.
UCLASS()
class DIGETHER_API ADigetherLobbyPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	bool IsReady() const { return bReady; }
	bool IsHost() const { return bHost; }
	void SetReady(bool bInReady);
	void SetHost(bool bInHost);

	virtual void OnRep_PlayerName() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_LobbyStatus();

	void NotifyLobbyChanged() const;

	UPROPERTY(ReplicatedUsing = OnRep_LobbyStatus) bool bReady = false;
	UPROPERTY(ReplicatedUsing = OnRep_LobbyStatus) bool bHost = false;
};
