#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Game/Data/DigetherTuningProfile.h"
#include "Game/Economy/DigetherDepositSessionRegistry.h"
#include "Game/Economy/DigetherEconomyTuning.h"
#include "Game/Framework/DigetherDepositResponse.h"
#include "Game/Elevator/DigetherElevatorTypes.h"
#include "Game/Run/DigetherRunTypes.h"
#include "DigetherGameplayGameMode.generated.h"

class ADigetherGameplayPlayerController;
class ADigetherGameplayPlayerState;
class ADigetherElevatorActor;
class ADigetherElevatorLeverActor;
class UDigetherElevatorComponent;
class UDigetherRunCoordinatorComponent;
class UDigetherQuotaStateComponent;
class UDigetherWalletComponent;

// 서버 연결층이다. 경제 판정은 P1 값 함수와 레지스트리에 위임한다.
UCLASS()
class DIGETHER_API ADigetherGameplayGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADigetherGameplayGameMode();

	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PreLogin(const FString& Options, const FString& Address,
		const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;

	// 월드 없이도 같은 거절 경로를 검사한다. 실패하면 출력 스냅샷은 보존한다.
	static bool TryMakeRunSnapshot(const UDigetherTuningProfile* Profile,
		FDigetherRunTuning& OutSnapshot, TArray<FText>& OutProblems);

	bool IsRunReady() const { return bRunReady; }
	const FDigetherRunTuning* GetRunTuning() const { return bRunReady ? &RunTuning : nullptr; }
	UDigetherRunCoordinatorComponent* GetRunCoordinatorComponent() const { return RunCoordinatorComponent.Get(); }

	bool BeginLeverPull(ADigetherGameplayPlayerController* Requester, ADigetherElevatorLeverActor* Lever);
	bool ReleaseLeverPull(ADigetherGameplayPlayerController* Requester, ADigetherElevatorLeverActor* Lever);
	void CycleSpectateTarget(ADigetherGameplayPlayerController* Requester, bool bNext);
	bool StartRunSmoke(ADigetherGameplayPlayerController* Requester);
	bool GrantCredits(ADigetherGameplayPlayerController* Requester, int32 Amount);

	FDigetherDepositResponse OpenDeposit(ADigetherGameplayPlayerController* Requester, FName StationId);
	FDigetherDepositResponse SubmitDeposit(ADigetherGameplayPlayerController* Requester, FGuid SessionId,
		FName StationId, int32 PeriodGeneration, int32 Amount);
	FDigetherDepositResponse CancelDeposit(ADigetherGameplayPlayerController* Requester, FGuid SessionId);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning")
	TObjectPtr<UDigetherTuningProfile> TuningProfile;

private:
	bool InitializeRunActors();
	bool IsDepositClosed() const;
	bool IsValidRequester(const ADigetherGameplayPlayerController* Requester) const;
	bool IsConfiguredLever(const ADigetherElevatorLeverActor* Lever) const;
	void CollectPlayers(TArray<ADigetherGameplayPlayerState*>& OutPlayers) const;
	bool TryMakePullContext(FDigetherElevatorPullContext& OutContext) const;
	void ProcessDeparture();
	void ProcessAscent();
	void ProcessPendingRunEnd();
	void FinishRunAndReturn(EDigetherRunTerminationTrigger Trigger);
	void LogRunStep(const TCHAR* Step) const;
	void HandleDepartureConfirmed(const FDigetherElevatorDepartureConfirmed& Confirmation);
	void HandleAscentLeverConfirmed(const FDigetherElevatorAscentConfirmed& Confirmation);
	void HandleElevatorAscentConfirmed(const FDigetherElevatorAscentConfirmed& Confirmation);
	void HandleExplorationFinished(const FDigetherElevatorAscentConfirmed& Confirmation);
	void HandleTimeExpired();
#if !UE_BUILD_SHIPPING
	void AdvanceSmoke();
	TWeakObjectPtr<ADigetherGameplayPlayerController> SmokeController;
	bool bSmokeActive = false;
#endif

	bool ResolveDepositContext(ADigetherGameplayPlayerController* Requester, FString& OutPlayerKey,
		UDigetherWalletComponent*& OutWallet, UDigetherQuotaStateComponent*& OutQuota) const;
	void NotifyClosedSessions(const FDigetherDepositSessionCloseResult& Closed,
		EDigetherEconomyResult Result);

	UPROPERTY(VisibleAnywhere, Category="Digether|Run")
	TObjectPtr<UDigetherRunCoordinatorComponent> RunCoordinatorComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<ADigetherElevatorActor> ElevatorActor;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ADigetherElevatorLeverActor>> LeverActors;

	TArray<TWeakObjectPtr<ADigetherGameplayPlayerState>> RunParticipants;
	int32 DeparturePlayerCount = 0;
	int32 LastRecordedAscent = 0;
	int32 PendingRecordedAscent = 0;
	bool bDeparturePending = false;
	bool bAscentPending = false;
	bool bTimeExpiredPending = false;
	bool bExplorationFinishedPending = false;
	bool bApplyingRunEnd = false;

	UPROPERTY(Transient)
	FDigetherRunTuning RunTuning;

	// 중앙 P1 기본값을 보관하며 클라이언트 입력으로 받지 않는다.
	FDigetherEconomyTuning EconomyTuning;
	FDigetherDepositSessionRegistry DepositSessions;
	bool bRunReady = false;
};
