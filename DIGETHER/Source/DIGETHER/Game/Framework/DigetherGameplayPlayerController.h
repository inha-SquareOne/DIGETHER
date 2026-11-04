#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Game/Framework/DigetherDepositResponse.h"
#include "DigetherGameplayPlayerController.generated.h"

class ADigetherElevatorLeverActor;
class ADigetherGameplayGameMode;

// 소유 연결의 요청을 전달하며 게임 상태는 서버 GameMode가 확정한다.
UCLASS()
class DIGETHER_API ADigetherGameplayPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADigetherGameplayPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PawnLeavingGame() override;

	void RequestManualSave();
	void RequestLoadGame();
	void RequestNewGame();

	UFUNCTION(Server, Reliable)
	void Server_ManualSave();

	UFUNCTION(Server, Reliable)
	void Server_LoadGame();

	UFUNCTION(Server, Reliable)
	void Server_NewGame();

	UFUNCTION(Client, Reliable)
	void Client_SaveResult(bool bSucceeded, const FText& Message);

	UFUNCTION(Server, Reliable)
	void Server_OpenDeposit(FName StationId);

	UFUNCTION(Server, Reliable)
	void Server_SubmitDeposit(FGuid SessionId, FName StationId, int32 PeriodGeneration, int32 Amount);

	UFUNCTION(Server, Reliable)
	void Server_CancelDeposit(FGuid SessionId);

	UFUNCTION(Client, Reliable)
	void Client_DepositResult(EDigetherDepositOperation Operation, const FDigetherDepositResponse& Response);

	UFUNCTION(Server, Reliable)
	void Server_BeginLeverPull(ADigetherElevatorLeverActor* Lever);

	UFUNCTION(Server, Reliable)
	void Server_ReleaseLeverPull(ADigetherElevatorLeverActor* Lever);

	UFUNCTION(Server, Reliable)
	void Server_CycleSpectateTarget(bool bNext);

	UFUNCTION(Client, Reliable)
	void Client_SetLifeSpectating(bool bSpectating);

	// UHT 선언은 모든 빌드에 유지하고 Shipping 구현은 상태를 변경하지 않는다.
	UFUNCTION(Server, Reliable)
	void Server_RunSmoke();

	UFUNCTION(Server, Reliable)
	void Server_GrantCredits(int32 Amount);

	// 다인 시험용: 자기 Pawn을 PlayerStart로 옮겨 엘리베이터 밖에 남긴다. Shipping 구현은 아무것도 하지 않는다.
	UFUNCTION(Server, Reliable)
	void Server_DevTeleportOut();

#if !UE_BUILD_SHIPPING
	virtual bool ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor) override;
	void DigetherDepositTest(const FString& AmountInput);
#endif

private:
	ADigetherGameplayGameMode* ResolveGameplayGameMode() const;
	ADigetherGameplayGameMode* ResolveDepositGameMode(EDigetherDepositOperation Operation,
		FDigetherDepositResponse& OutResponse);

	// 종류별로 제한하여 납부대를 연 직후 같은 입력 세션을 제출할 수 있게 한다.
	TMap<EDigetherDepositOperation, double> LastDepositRequestSeconds;
	// 이 Controller가 관전 때문에 더한 입력 잠금만 정확히 한 번 해제한다.
	bool bLifeInputLocked = false;

#if !UE_BUILD_SHIPPING
	enum class EDigetherDepositTestPhase : uint8
	{
		Idle,
		AwaitingOpen,
		AwaitingSubmit
	};

	void ResetDepositTest();
	void DigetherLever(const FString& Arguments);
	void DigetherRunSmoke();
	void DigetherGrantCredits(const FString& AmountInput);
	// 다인 시험용: 이 프로세스가 받은 복제 상태(런·목표·엘리베이터·플레이어별 지갑·생존)를 로그로 남긴다.
	void DigetherDumpState();
	void DigetherTeleportOut();
	// 명령줄 -DigetherAutoCmds="초:명령|초:명령"을 이 Controller의 BeginPlay 기준으로 예약 실행한다.
	void ScheduleAutoCommands();
	// 개발 서버 RPC는 호스트 본인 또는 서버가 -DigetherDevRpc로 켠 경우에만 받는다.
	bool IsDevServerRpcAllowed() const;

	EDigetherDepositTestPhase DepositTestPhase = EDigetherDepositTestPhase::Idle;
	int32 PendingDepositTestAmount = 0;
	FGuid PendingDepositTestSessionId;
#endif
};
