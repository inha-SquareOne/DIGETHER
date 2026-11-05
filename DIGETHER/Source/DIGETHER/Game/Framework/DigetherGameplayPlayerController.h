#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Game/Framework/DigetherDepositResponse.h"
#include "Game/Run/DigetherRunResult.h"
#include "Game/Hub/DigetherHubTypes.h"
#include "DigetherGameplayPlayerController.generated.h"

class ADigetherElevatorLeverActor;
class ADigetherDepositStationActor;
class ADigetherFloorCoinActor;
class ADigetherGameplayGameMode;
class ADigetherGameplayPlayerState;
class UDigetherDepositPanelWidget;
class UDigetherGameOverCreditsWidget;
class UDigetherRunHudWidget;
class UDigetherRunResultWidget;
class UInputAction;
class UInputMappingContext;
class ADigetherHubFacilityActor;
class UDigetherHubFacilityWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDigetherDepositResult,
	EDigetherDepositOperation, Operation, FDigetherDepositResponse, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDigetherChainSummoned, float, SummonSeconds);
DECLARE_MULTICAST_DELEGATE(FOnDigetherPlayerStateChanged);

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
	virtual void SetupInputComponent() override;
	virtual void OnRep_PlayerState() override;
	virtual void OnPossess(APawn* InPawn) override;

	void RequestManualSave();
	void RequestLoadGame();
	void RequestNewGame();

	// 폐기된 금액 입력 납부의 진입점이다. BP 참조가 남아 있을 수 있어 선언만 두며 아무 동작도 하지 않는다.
	// 납부는 우클릭 유지 붓기(StartPour/StopPour)로 바뀌었다(기획 S24).
	UFUNCTION(BlueprintCallable, Category="Digether|Deposit", meta=(DeprecatedFunction, DeprecationMessage="금액 입력 납부는 폐기됐습니다. StartPour/StopPour를 쓰세요."))
	void RequestOpenDeposit(FName StationId);

	UFUNCTION(BlueprintCallable, Category="Digether|Deposit", meta=(DeprecatedFunction, DeprecationMessage="금액 입력 납부는 폐기됐습니다. StartPour/StopPour를 쓰세요."))
	void RequestSubmitDeposit(FGuid SessionId, FName StationId, int32 PeriodGeneration, int32 Amount);

	UFUNCTION(BlueprintCallable, Category="Digether|Deposit", meta=(DeprecatedFunction, DeprecationMessage="금액 입력 납부는 폐기됐습니다. StartPour/StopPour를 쓰세요."))
	void RequestCancelDeposit(FGuid SessionId);

	UFUNCTION(BlueprintCallable, Category="Digether|Deposit", meta=(DeprecatedFunction, DeprecationMessage="금액 입력 납부는 폐기됐습니다. StartPour/StopPour를 쓰세요."))
	void StartDepositInteraction(ADigetherDepositStationActor* Station);

	// 사용 중단된 금액 입력 위젯(UDigetherDepositPanelWidget)이 부르는 함수다. 아무 동작도 하지 않는다.
	void SubmitDepositInput(const FString& AmountInput);
	void CloseDepositInteraction();

	// 로컬 붓기 입력 진입점이다. 정식 캐릭터의 돈자루 사용 입력(우클릭 유지)이 부른다. 서버가 조건·차감을 판정한다.
	UFUNCTION(BlueprintCallable, Category="Digether|Deposit")
	void StartPour(ADigetherDepositStationActor* Station);

	UFUNCTION(BlueprintCallable, Category="Digether|Deposit")
	void StopPour();

	UFUNCTION(BlueprintPure, Category="Digether|Deposit")
	bool IsPouring() const { return HeldPourStation.IsValid(); }

	UFUNCTION(BlueprintCallable, Category="Digether|Run Result")
	bool ShowLastRunResult();

	UFUNCTION(BlueprintCallable, Category="Digether|Run Result")
	void CloseRunResult();

	UFUNCTION(BlueprintCallable, Category="Digether|Hub")
	void StartHubInteraction(ADigetherHubFacilityActor* Facility);
	void SubmitHubOption(int32 OptionIndex);
	void CloseHubInteraction();
	FText GetInteractionHint() const;

	// 클라이언트 PlayerState 복제·교체(재접속 포함) 알림, HUD 재구독용
	FOnDigetherPlayerStateChanged OnGameplayPlayerStateChanged;

	UFUNCTION(Server, Reliable)
	void Server_HubFacilityRequest(ADigetherHubFacilityActor* Facility, EDigetherHubAction Action,
		FName OptionId, FGuid RequestId);

	UFUNCTION(Client, Reliable)
	void Client_HubFacilityResult(const FDigetherHubView& View);

	UFUNCTION(Client, Reliable)
	void Client_CloseHubForRun();

	// 서버가 거점 문 닫힘 완료 순간 이 참가자를 사슬 소집으로 엘리베이터 안에 옮겼다. 위치 이동은 서버가 이미
	// 끝냈고, 소유 클라이언트는 SummonSeconds(LOBBY_CHAIN_SUMMON_TIME) 길이의 암전 연출만 한다.
	UFUNCTION(Client, Reliable)
	void Client_ChainSummoned(float SummonSeconds);

	// 사슬 소집 암전 연출 진입점이다. 실제 페이드·카메라 연출은 BP가 구현한다.
	UFUNCTION(BlueprintImplementableEvent, Category="Digether|Departure", meta=(DisplayName="On Chain Summoned"))
	void ReceiveChainSummoned(float SummonSeconds);

	UPROPERTY(BlueprintAssignable, Category="Digether|Departure")
	FOnDigetherChainSummoned OnChainSummoned;

	// Client_DepositResult가 소유자에게 도착한 뒤 UI로 전달한다.
	UPROPERTY(BlueprintAssignable, Category="Digether|Deposit")
	FOnDigetherDepositResult OnDepositResult;

	UFUNCTION(Server, Reliable)
	void Server_ManualSave();

	UFUNCTION(Server, Reliable)
	void Server_LoadGame();

	UFUNCTION(Server, Reliable)
	void Server_NewGame();

	UFUNCTION(Client, Reliable)
	void Client_SaveResult(bool bSucceeded, const FText& Message);

	// 붓기 시작·끝이다. 서버가 플레이어별 다음 주기 시각을 기록하고 GameMode Tick에서 차감한다(모델 A).
	UFUNCTION(Server, Reliable)
	void Server_BeginPour(ADigetherDepositStationActor* Station);

	UFUNCTION(Server, Reliable)
	void Server_EndPour();

	UFUNCTION(Server, Reliable)
	void Server_PickupFloorCoin(ADigetherFloorCoinActor* Coin);

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

	// 기획 S33 239행: 결과 크레딧 빨리감기 입력이다. 호스트만 반영되며 모두에게 같은 속도로 적용된다.
	UFUNCTION(BlueprintCallable, Category="Digether|Game Over")
	void RequestCreditFastForward(bool bFastForward);

	UFUNCTION(Server, Reliable)
	void Server_SetCreditFastForward(bool bFastForward);

	// 서버가 이 참가자를 실패 사건에서 흡입해 우물 아래 대기 지점으로 옮겼다(true) 또는 새 게임에서 풀었다(false).
	// 대기 중에는 이동 입력만 막고 시점 조작은 유지한다.
	UFUNCTION(Client, Reliable)
	void Client_SetGameOverAbsorbed(bool bAbsorbed);

	// 흡입·대기 연출 진입점이다. 실제 화면 연출은 BP가 구현한다.
	UFUNCTION(BlueprintImplementableEvent, Category="Digether|Game Over", meta=(DisplayName="On Game Over Absorbed"))
	void ReceiveGameOverAbsorbed(bool bAbsorbed);

	// UHT 선언은 모든 빌드에 유지하고 Shipping 구현은 상태를 변경하지 않는다.
	UFUNCTION(Server, Reliable)
	void Server_RunSmoke();

	UFUNCTION(Server, Reliable)
	void Server_GrantCredits(int32 Amount);

	// 다인 시험용: 자기 Pawn을 PlayerStart로 옮겨 엘리베이터 밖에 남긴다. Shipping 구현은 아무것도 하지 않는다.
	UFUNCTION(Server, Reliable)
	void Server_DevTeleportOut();

	// 다인 시험용: 자기 Pawn을 지정 납부대 앞으로 옮긴다. Shipping 구현은 아무것도 하지 않는다.
	UFUNCTION(Server, Reliable)
	void Server_DevTeleportToDeposit(FName StationId);

	// 시설 UI·다인 검증용 이동이며 Shipping에서는 상태를 바꾸지 않는다.
	UFUNCTION(Server, Reliable)
	void Server_DevTeleportToHubFacility(FName FacilityId);

	// 신호기 경계 시험용: 현재 목표의 공동 인정액을 정한다. Shipping 구현은 아무것도 하지 않는다.
	UFUNCTION(Server, Reliable)
	void Server_DevSetQuotaProgress(int32 DepositedCredits);

#if !UE_BUILD_SHIPPING
	virtual bool ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor) override;
#endif

private:
	UPROPERTY(EditDefaultsOnly, Category="Digether|Interaction")
	TObjectPtr<UInputMappingContext> InteractMappingContext;
	UPROPERTY(EditDefaultsOnly, Category="Digether|Interaction")
	TObjectPtr<UInputAction> InteractAction;
	// 붓기(우클릭 유지) 입력이다. 에셋은 나중에 지정한다. 비어 있으면 개발 명령 DigetherPour로 시험한다.
	UPROPERTY(EditDefaultsOnly, Category="Digether|Deposit|Input")
	TObjectPtr<UInputMappingContext> PourMappingContext;
	UPROPERTY(EditDefaultsOnly, Category="Digether|Deposit|Input")
	TObjectPtr<UInputAction> PourAction;
	// 로컬 표시용이다. 서버가 멈춤을 알리면 비운다.
	TWeakObjectPtr<ADigetherDepositStationActor> HeldPourStation;
	UPROPERTY(EditDefaultsOnly, Category="Digether|Hub|UI")
	TSubclassOf<UDigetherHubFacilityWidget> HubFacilityWidgetClass;
	UPROPERTY(Transient)
	TObjectPtr<UDigetherHubFacilityWidget> HubFacilityWidget;
	TWeakObjectPtr<ADigetherHubFacilityActor> ActiveHubFacility;
	TWeakObjectPtr<ADigetherElevatorLeverActor> HeldInteractionLever;
	FDigetherHubView CurrentHubView;
	FGuid ActiveHubRequestId;
	double LastHubRequestSeconds = -1.0;
	bool bHubInputModeApplied = false;
	bool bHubPreviousCursorVisible = false;
	bool bHubRequestPending = false;
	EDigetherHubAction LastHubAction = EDigetherHubAction::Inspect;
	FTimerHandle HubViewRefreshTimer;
	void HandleInteractStarted();
	void HandleInteractReleased();
	void HandlePourStarted();
	void HandlePourReleased();
	AActor* FindInteractionTarget() const;
	void SendHubRequest(EDigetherHubAction Action, FName OptionId = NAME_None);

	UPROPERTY(EditDefaultsOnly, Category="Digether|UI")
	TSubclassOf<UDigetherRunHudWidget> HudWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UDigetherRunHudWidget> HudWidget;

	UPROPERTY(EditDefaultsOnly, Category="Digether|Run Result|UI")
	TSubclassOf<UDigetherRunResultWidget> RunResultWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UDigetherRunResultWidget> RunResultWidget;

	// 결과 크레딧 화면이다. 비우면 만들지 않는다. 위젯이 공개 상태를 보고 크레딧 단계에만 스스로 보인다.
	UPROPERTY(EditDefaultsOnly, Category="Digether|Game Over|UI")
	TSubclassOf<UDigetherGameOverCreditsWidget> GameOverCreditsWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UDigetherGameOverCreditsWidget> GameOverCreditsWidget;

	// 이 Controller가 흡입 대기 때문에 더한 이동 입력 잠금만 정확히 한 번 해제한다.
	bool bGameOverInputLocked = false;

	TWeakObjectPtr<ADigetherGameplayPlayerState> RunResultPlayerState;
	FGuid LastPresentedRunResultId;
	bool bRunResultPreviousCursorVisible = false;
	bool bRunResultInputModeApplied = false;
	void RefreshRunResultBinding();
	void HandleRunResultChanged(const FDigetherRunResult& Result);

	// 사용 중단: 금액 입력 납부 화면 클래스다. BP 기본값 호환을 위해 남기며 코드에서 만들지 않는다.
	UPROPERTY(EditDefaultsOnly, Category="Digether|Deposit|UI")
	TSubclassOf<UDigetherDepositPanelWidget> DepositPanelWidgetClass;

	// 사용 중단: 폐기된 금액 입력 화면이다. BP 기본값에 지정돼 있을 수 있어 프로퍼티만 남긴다.
	UPROPERTY(Transient)
	TObjectPtr<UDigetherDepositPanelWidget> DepositPanelWidget;

	// 사용 중단: 폐기된 금액 입력 시험 입력이다. BP 기본값 호환을 위해 프로퍼티만 남긴다.
	UPROPERTY(EditDefaultsOnly, Category="Digether|Deposit|Test Input")
	TObjectPtr<UInputMappingContext> TestDepositMappingContext;

	UPROPERTY(EditDefaultsOnly, Category="Digether|Deposit|Test Input")
	TObjectPtr<UInputAction> TestDepositInteractAction;

	ADigetherGameplayGameMode* ResolveGameplayGameMode() const;
	// 요청 종류별 최소 간격을 적용한다. 붓기 끝(EndPour)에는 쓰지 않는다(놓친 끝 요청이 붓기를 남기지 않게).
	ADigetherGameplayGameMode* ResolveDepositGameMode(EDigetherDepositOperation Operation,
		FDigetherDepositResponse& OutResponse);

	TMap<EDigetherDepositOperation, double> LastDepositRequestSeconds;
	// 이 Controller가 관전 때문에 더한 입력 잠금만 정확히 한 번 해제한다.
	bool bLifeInputLocked = false;

#if !UE_BUILD_SHIPPING
	void DigetherLever(const FString& Arguments);
	void DigetherRunSmoke();
	void DigetherGrantCredits(const FString& AmountInput);
	// 붓기 입력 에셋 없이 시험한다: 가장 가까운(또는 지정한) 정산기에 <초> 동안 붓는다.
	void DigetherPour(const FString& Arguments);
	void DigetherPourStop();
	void DigetherSetQuotaProgress(const FString& AmountInput);
	// 크레딧 빨리감기 시험용: DigetherCreditFastForward 1|0. 호스트가 아니면 서버가 거절한다.
	void DigetherCreditFastForward(const FString& Arguments);
	// 다인 시험용: 이 프로세스가 받은 복제 상태(런·목표·엘리베이터·플레이어별 지갑·생존·바닥 금화)를 로그로 남긴다.
	void DigetherDumpState();
	void DigetherTeleportOut();
	void DigetherTeleportToDeposit(const FString& StationInput);
	// 명령줄 -DigetherAutoCmds="초:명령|초:명령"을 이 Controller의 BeginPlay 기준으로 예약 실행한다.
	void ScheduleAutoCommands();
	// 개발 서버 RPC는 호스트 본인 또는 서버가 -DigetherDevRpc로 켠 경우에만 받는다.
	bool IsDevServerRpcAllowed() const;

	FTimerHandle DevPourTimer;
#endif
};
