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
class UDigetherHubInteractionComponent;
class UDigetherPauseMenuWidget;
class UDigetherPlayerScreenComponent;
class UDigetherVoiceComponent;
class UDigetherMonsterCameraSampleComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDigetherDepositResult,
	EDigetherDepositOperation, Operation, FDigetherDepositResponse, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDigetherChainSummoned, float, SummonSeconds);
DECLARE_MULTICAST_DELEGATE(FOnDigetherPlayerStateChanged);

// 소유 연결 요청 전달, 게임 상태 확정은 서버 GameMode
// 구현 파일: 본체·입력·런 결과(.cpp), 거점 상호작용(HubInteraction), 붓기·납부(Deposit), 저장 요청(Save), 개발 명령(Dev)
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
	// 서버 기본 음성 허용 뒤 개인 마이크 모드(항상 켜짐·누르고 말하기) 적용
	virtual void ClientEnableNetworkVoice_Implementation(bool bEnable) override;

	void RequestManualSave();
	void RequestLoadGame();
	void RequestNewGame();

	// 폐기된 금액 입력 납부 진입점, BP 참조 호환용 빈 구현
	// 납부는 우클릭 유지 붓기(StartPour/StopPour)로 대체(기획 S24)
	UFUNCTION(BlueprintCallable, Category="Digether|Deposit", meta=(DeprecatedFunction, DeprecationMessage="금액 입력 납부는 폐기됐습니다. StartPour/StopPour를 쓰세요."))
	void RequestOpenDeposit(FName StationId);

	UFUNCTION(BlueprintCallable, Category="Digether|Deposit", meta=(DeprecatedFunction, DeprecationMessage="금액 입력 납부는 폐기됐습니다. StartPour/StopPour를 쓰세요."))
	void RequestSubmitDeposit(FGuid SessionId, FName StationId, int32 PeriodGeneration, int32 Amount);

	UFUNCTION(BlueprintCallable, Category="Digether|Deposit", meta=(DeprecatedFunction, DeprecationMessage="금액 입력 납부는 폐기됐습니다. StartPour/StopPour를 쓰세요."))
	void RequestCancelDeposit(FGuid SessionId);

	UFUNCTION(BlueprintCallable, Category="Digether|Deposit", meta=(DeprecatedFunction, DeprecationMessage="금액 입력 납부는 폐기됐습니다. StartPour/StopPour를 쓰세요."))
	void StartDepositInteraction(ADigetherDepositStationActor* Station);

	// 사용 중단 금액 입력 위젯(UDigetherDepositPanelWidget) 호출용
	void SubmitDepositInput(const FString& AmountInput);
	void CloseDepositInteraction();

	// 로컬 붓기 입력 진입점, 정식 캐릭터의 돈자루 사용 입력(우클릭 유지)이 호출, 조건·차감은 서버 판정
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

	// ESC 메뉴: 로컬 화면만 열고 닫음, 게임 진행은 멈추지 않음
	UFUNCTION(BlueprintCallable, Category="Digether|Pause")
	void OpenPauseMenu();
	UFUNCTION(BlueprintCallable, Category="Digether|Pause")
	void ClosePauseMenu();
	UFUNCTION(BlueprintPure, Category="Digether|Pause")
	bool IsPauseMenuOpen() const;

	UFUNCTION(BlueprintCallable, Category="Digether|Hub")
	void StartHubInteraction(ADigetherHubFacilityActor* Facility);
	void SubmitHubOption(int32 OptionIndex);
	void CloseHubInteraction();
	FText GetInteractionHint() const;

	UDigetherHubInteractionComponent* GetHubInteractionComponent() const { return HubInteractionComponent; }
	// 안내 문구의 키 이름 조회용
	const UInputAction* GetInteractAction() const { return InteractAction; }
	const UInputAction* GetPourAction() const { return PourAction; }
	TSubclassOf<UDigetherHubFacilityWidget> GetHubFacilityWidgetClass() const { return HubFacilityWidgetClass; }
	UDigetherPlayerScreenComponent* GetPlayerScreenComponent() const { return PlayerScreenComponent; }
	UDigetherVoiceComponent* GetVoiceComponent() const { return VoiceComponent; }
	TSubclassOf<UDigetherRunHudWidget> GetHudWidgetClass() const { return HudWidgetClass; }
	TSubclassOf<UDigetherRunResultWidget> GetRunResultWidgetClass() const { return RunResultWidgetClass; }
	TSubclassOf<UDigetherGameOverCreditsWidget> GetGameOverCreditsWidgetClass() const { return GameOverCreditsWidgetClass; }
	bool IsRunResultInputModeApplied() const;
	bool IsLifeInputLocked() const { return bLifeInputLocked; }
	// 화면 전환 전 공통 정리, 순서: 레버 해제 → 붓기 종료 → 시설 패널 닫기
	void ReleaseHeldInputsAndHubPanel();

	// 클라이언트 PlayerState 복제·교체(재접속 포함) 알림, HUD 재구독용
	FOnDigetherPlayerStateChanged OnGameplayPlayerStateChanged;

	UFUNCTION(Server, Reliable)
	void Server_HubFacilityRequest(ADigetherHubFacilityActor* Facility, EDigetherHubAction Action,
		FName OptionId, FGuid RequestId);

	UFUNCTION(Client, Reliable)
	void Client_HubFacilityResult(const FDigetherHubView& View);

	UFUNCTION(Client, Reliable)
	void Client_CloseHubForRun();

	// 거점 문 닫힘 완료 순간 서버가 사슬 소집으로 엘리베이터 안에 이동 완료
	// 소유 클라이언트는 SummonSeconds(LOBBY_CHAIN_SUMMON_TIME) 길이의 암전 연출만 담당
	UFUNCTION(Client, Reliable)
	void Client_ChainSummoned(float SummonSeconds);

	// 사슬 소집 암전 연출 진입점, 페이드·카메라 연출은 BP 구현
	UFUNCTION(BlueprintImplementableEvent, Category="Digether|Departure", meta=(DisplayName="On Chain Summoned"))
	void ReceiveChainSummoned(float SummonSeconds);

	UPROPERTY(BlueprintAssignable, Category="Digether|Departure")
	FOnDigetherChainSummoned OnChainSummoned;

	// Client_DepositResult 도착 뒤 UI 전달
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

	// 붓기 시작·끝, 서버가 플레이어별 다음 주기 시각 기록 후 GameMode Tick에서 차감(모델 A)
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

	// 기획 S33 239행: 결과 크레딧 빨리감기 입력, 호스트만 반영·모두에게 같은 속도
	UFUNCTION(BlueprintCallable, Category="Digether|Game Over")
	void RequestCreditFastForward(bool bFastForward);

	UFUNCTION(Server, Reliable)
	void Server_SetCreditFastForward(bool bFastForward);

	// 실패 사건 흡입으로 우물 아래 대기 지점 이동(true), 새 게임에서 해제(false)
	// 대기 중 이동 입력만 잠금, 시점 조작 유지
	UFUNCTION(Client, Reliable)
	void Client_SetGameOverAbsorbed(bool bAbsorbed);

	// 흡입·대기 연출 진입점, 화면 연출은 BP 구현
	UFUNCTION(BlueprintImplementableEvent, Category="Digether|Game Over", meta=(DisplayName="On Game Over Absorbed"))
	void ReceiveGameOverAbsorbed(bool bAbsorbed);

	// 개발용 RPC, UHT 선언은 모든 빌드 유지·Shipping 구현은 상태 불변
	UFUNCTION(Server, Reliable)
	void Server_RunSmoke();

	UFUNCTION(Server, Reliable)
	void Server_GrantCredits(int32 Amount);

	// 다인 시험용: 자기 Pawn을 PlayerStart로 옮겨 엘리베이터 밖에 남김
	UFUNCTION(Server, Reliable)
	void Server_DevTeleportOut();

	// 다인 시험용: 자기 Pawn을 지정 납부대 앞으로 이동
	UFUNCTION(Server, Reliable)
	void Server_DevTeleportToDeposit(FName StationId);

	// 시설 UI·다인 검증용 이동
	UFUNCTION(Server, Reliable)
	void Server_DevTeleportToHubFacility(FName FacilityId);

	// 신호기 경계 시험용: 현재 목표의 공동 인정액 지정
	UFUNCTION(Server, Reliable)
	void Server_DevSetQuotaProgress(int32 DepositedCredits);

#if !UE_BUILD_SHIPPING
	// --- 개발 명령 진입점 (Dev.cpp) ---
	// 콘솔·웹 명령 등록은 DIGETHERDevTools 모듈, 여기는 이 컨트롤러가 보낼 요청만 담당
	// 인자는 공백으로 이은 문자열, 서버 판정은 각 Server_Dev* RPC와 GameMode
	void DigetherLever(const FString& Arguments);
	void DigetherRunSmoke();
	void DigetherGrantCredits(const FString& AmountInput);
	// 붓기 입력 에셋 없이 시험: 가장 가까운(또는 지정한) 정산기에 <초> 동안 붓기
	void DigetherPour(const FString& Arguments);
	void DigetherPourStop();
	void DigetherSetQuotaProgress(const FString& AmountInput);
	// 크레딧 빨리감기 시험용: 1|0, 호스트가 아니면 서버 거절
	void DigetherCreditFastForward(const FString& Arguments);
	// 다인 시험용: 이 프로세스가 받은 복제 상태(런·목표·엘리베이터·플레이어별 지갑·생존·바닥 금화) 로그
	void DigetherDumpState();
	void DigetherTeleportOut();
	void DigetherTeleportToDeposit(const FString& StationInput);
	void DigetherHubTeleport(const FString& FacilityInput);
	// <FacilityId> <EDigetherHubAction 이름> [OptionId]
	void DigetherHubAction(const FString& Arguments);
#endif

private:
	// --- 공통 ---
	ADigetherGameplayGameMode* ResolveGameplayGameMode() const;

	// --- 상호작용 입력·거점 시설 (HubInteraction.cpp, 로컬 상태는 컴포넌트) ---
	UPROPERTY(VisibleAnywhere, Category="Digether|Interaction")
	TObjectPtr<UDigetherHubInteractionComponent> HubInteractionComponent;
	UPROPERTY(EditDefaultsOnly, Category="Digether|Interaction")
	TObjectPtr<UInputMappingContext> InteractMappingContext;
	UPROPERTY(EditDefaultsOnly, Category="Digether|Interaction")
	TObjectPtr<UInputAction> InteractAction;
	UPROPERTY(EditDefaultsOnly, Category="Digether|Hub|UI")
	TSubclassOf<UDigetherHubFacilityWidget> HubFacilityWidgetClass;
	// 서버 측 시설 요청 간격 기록
	double LastHubRequestSeconds = -1.0;

	// --- 붓기·납부 (Deposit.cpp) ---
	// 붓기(우클릭 유지) 입력, 에셋 미지정 시 개발 명령 DigetherPour로 시험
	UPROPERTY(EditDefaultsOnly, Category="Digether|Deposit|Input")
	TObjectPtr<UInputMappingContext> PourMappingContext;
	UPROPERTY(EditDefaultsOnly, Category="Digether|Deposit|Input")
	TObjectPtr<UInputAction> PourAction;
	// 로컬 표시용, 서버 중단 알림 시 비움
	TWeakObjectPtr<ADigetherDepositStationActor> HeldPourStation;
	TMap<EDigetherDepositOperation, double> LastDepositRequestSeconds;
	void HandlePourStarted();
	void HandlePourReleased();
	// 요청 종류별 최소 간격 적용, EndPour에는 미적용(놓친 끝 요청이 붓기를 남기지 않게)
	ADigetherGameplayGameMode* ResolveDepositGameMode(EDigetherDepositOperation Operation,
		FDigetherDepositResponse& OutResponse);

	// 사용 중단: 금액 입력 납부 화면 클래스, BP 기본값 호환용으로 남기며 코드에서 미생성
	UPROPERTY(EditDefaultsOnly, Category="Digether|Deposit|UI")
	TSubclassOf<UDigetherDepositPanelWidget> DepositPanelWidgetClass;

	// 사용 중단: 폐기된 금액 입력 화면, BP 기본값 호환용 프로퍼티
	UPROPERTY(Transient)
	TObjectPtr<UDigetherDepositPanelWidget> DepositPanelWidget;

	// 사용 중단: 폐기된 금액 입력 시험 입력, BP 기본값 호환용 프로퍼티
	UPROPERTY(EditDefaultsOnly, Category="Digether|Deposit|Test Input")
	TObjectPtr<UInputMappingContext> TestDepositMappingContext;

	UPROPERTY(EditDefaultsOnly, Category="Digether|Deposit|Test Input")
	TObjectPtr<UInputAction> TestDepositInteractAction;

	// --- 화면: HUD·런 결과·크레딧 (위젯 수명·입력 모드는 컴포넌트) ---
	UPROPERTY(VisibleAnywhere, Category="Digether|UI")
	TObjectPtr<UDigetherPlayerScreenComponent> PlayerScreenComponent;

	// --- ESC 메뉴 (PauseMenu.cpp) ---
	UPROPERTY(EditDefaultsOnly, Category="Digether|Pause|Input")
	TObjectPtr<UInputMappingContext> PauseMappingContext;
	UPROPERTY(EditDefaultsOnly, Category="Digether|Pause|Input")
	TObjectPtr<UInputAction> PauseAction;
	UPROPERTY(EditDefaultsOnly, Category="Digether|Pause|UI")
	TSubclassOf<UDigetherPauseMenuWidget> PauseMenuWidgetClass;
	UPROPERTY(Transient)
	TObjectPtr<UDigetherPauseMenuWidget> PauseMenuWidget;
	bool bPauseMenuPreviousCursorVisible = false;
	void HandlePausePressed();
	void HandlePauseMenuDeactivated();

	// --- 음성: 마이크 모드·재생 설정·관전 청취 위치 (로컬 전용) ---
	UPROPERTY(VisibleAnywhere, Category="Digether|Voice")
	TObjectPtr<UDigetherVoiceComponent> VoiceComponent;

	// --- 몬스터 시선 판정: 로컬 카메라 표본을 서버에 전송 ---
	UPROPERTY(VisibleAnywhere, Category="Digether|Monster")
	TObjectPtr<UDigetherMonsterCameraSampleComponent> MonsterCameraSampleComponent;

	UPROPERTY(EditDefaultsOnly, Category="Digether|UI")
	TSubclassOf<UDigetherRunHudWidget> HudWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="Digether|Run Result|UI")
	TSubclassOf<UDigetherRunResultWidget> RunResultWidgetClass;

	// 결과 크레딧 화면, 비우면 미생성, 위젯이 공개 상태를 보고 크레딧 단계에만 표시
	UPROPERTY(EditDefaultsOnly, Category="Digether|Game Over|UI")
	TSubclassOf<UDigetherGameOverCreditsWidget> GameOverCreditsWidgetClass;

	void RefreshRunResultBinding();

	// --- 관전·게임 오버 입력 잠금 ---
	// 관전 때문에 더한 입력 잠금만 정확히 한 번 해제
	bool bLifeInputLocked = false;
	// 흡입 대기 때문에 더한 이동 입력 잠금만 정확히 한 번 해제
	bool bGameOverInputLocked = false;

	// --- 개발 명령 내부 (Dev.cpp) ---
#if !UE_BUILD_SHIPPING
	// 명령줄 -DigetherAutoCmds="초:명령|초:명령"을 이 Controller의 BeginPlay 기준으로 예약 실행
	void ScheduleAutoCommands();
	// 개발 서버 RPC는 호스트 본인 또는 서버가 -DigetherDevRpc로 켠 경우만 수락
	bool IsDevServerRpcAllowed() const;

	FTimerHandle DevPourTimer;
#endif
};
