#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Game/Online/DigetherJoinRules.h"
#include "Game/Online/DigetherLoginReplacementQueue.h"
#include "DigetherLobbyGameMode.generated.h"

// 대기실: 최대 인원·Steam ID를 확인해 입장을 받고, 모두 준비되면 호스트가 게임을 시작한다.
UCLASS()
class DIGETHER_API ADigetherLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADigetherLobbyGameMode();

	// 기다릴 수 없는 동기 경로다. 같은 계정 교체 대상도 ②로 거절하며 이전 연결을 끊지 않는다.
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	// 엔진 접속(NMT_Login)이 부르는 승인 경로. 기획 C2: 같은 계정이면 이전 연결을 내보내고 정리 뒤 새 접속을 받는다.
	virtual void PreLoginAsync(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId,
		const FOnPreLoginCompleteDelegate& OnComplete) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

	// 요청자가 호스트이고 모든 참가자가 준비했으면 게임 맵으로 이동한다.
	void TryStartGame(APlayerController* Requester);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

private:
	DigetherJoinRules::EDigetherJoinDecision EvaluateAdmission(const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage,
		APlayerController** OutPreviousToReplace);
	void PollLoginReplacements();
	bool IsPreviousConnectionReleased(const FUniqueNetIdRepl& UniqueId, const APlayerController* Previous) const;

	FDigetherLoginReplacementQueue LoginReplacements;
	FTimerHandle LoginReplacementTimer;
};
