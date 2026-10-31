#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DigetherLobbyGameMode.generated.h"

// 대기실: 최대 인원·Steam ID를 확인해 입장을 받고, 모두 준비되면 호스트가 게임을 시작한다.
UCLASS()
class DIGETHER_API ADigetherLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADigetherLobbyGameMode();

	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

	// 요청자가 호스트이고 모든 참가자가 준비했으면 게임 맵으로 이동한다.
	void TryStartGame(APlayerController* Requester);

protected:
	virtual void BeginPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
};
