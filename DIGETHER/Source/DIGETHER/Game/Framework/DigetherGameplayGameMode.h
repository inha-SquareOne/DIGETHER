#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Game/Data/DigetherTuningProfile.h"
#include "Game/Economy/DigetherPourSessionRegistry.h"
#include "Game/Economy/DigetherEconomyTuning.h"
#include "Game/Framework/DigetherDepositResponse.h"
#include "Game/Elevator/DigetherElevatorTypes.h"
#include "Game/Online/DigetherJoinRules.h"
#include "Game/Online/DigetherLoginReplacementQueue.h"
#include "Game/Run/DigetherGameOverTypes.h"
#include "Game/Run/DigetherRunTypes.h"
#include "Game/Save/DigetherSaveTypes.h"
#include "DigetherGameplayGameMode.generated.h"

class ADigetherGameplayPlayerController;
class ADigetherGameplayPlayerState;
class ADigetherElevatorActor;
class ADigetherElevatorLeverActor;
class ADigetherDepositStationActor;
class ADigetherFloorCoinActor;
class APawn;
class UDigetherElevatorComponent;
class UDigetherGameOverFlowComponent;
class UDigetherRunCoordinatorComponent;
class UDigetherQuotaStateComponent;
class UDigetherWalletComponent;

// 서버 연결층이다. 경제 판정은 P1 값 함수와 레지스트리에 위임한다.
UCLASS()
class DIGETHER_API ADigetherGameplayGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ADigetherGameplayGameMode();

	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// 기다릴 수 없는 동기 경로다. 같은 계정 교체 대상도 ②로 거절하며 이전 연결을 끊지 않는다.
	virtual void PreLogin(const FString& Options, const FString& Address,
		const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	// 엔진 접속(NMT_Login)이 부르는 승인 경로. 기획 C2: 같은 계정이면 이전 연결을 끊고 정리를 기다린 뒤 다시 판정한다.
	virtual void PreLoginAsync(const FString& Options, const FString& Address,
		const FUniqueNetIdRepl& UniqueId, const FOnPreLoginCompleteDelegate& OnComplete) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual APlayerController* Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal,
		const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void Logout(AController* Exiting) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;
	virtual bool ReadyToStartMatch_Implementation() override;
	virtual void AddInactivePlayer(APlayerState* PlayerState, APlayerController* PC) override;
	virtual bool FindInactivePlayer(APlayerController* PC) override;

	void NotifyPlayerDisconnecting(ADigetherGameplayPlayerController* Controller);
	bool SaveProgress(ADigetherGameplayPlayerController* Requester);
	bool SaveHubFacilityProgress();
	bool LoadProgress(ADigetherGameplayPlayerController* Requester, bool bStartup = false);
	bool StartNewGame(ADigetherGameplayPlayerController* Requester);
	// 기획 S02·S30 공통(호스트·공통 맵) 출발 로딩 실패: 출발 로딩 계층이 실패를 확정하면 부른다.
	// 디스크를 읽지 않고, 문 닫힘 완료 순간의 메모리 기준점으로 같은 일차 거점에 돌아간다.
	// 구매·납부·해금·배치와 게이지 완료 때 기록한 4일차 마감을 유지하고 일차를 소비하지 않는다.
	// 갱도 도착(제한 시간 시작) 뒤에는 로딩 단계가 아니므로 거절한다.
	bool RecoverFromCommonLoadingFailure();

	// 월드 없이도 같은 거절 경로를 검사한다. 실패하면 출력 스냅샷은 보존한다.
	static bool TryMakeRunSnapshot(const UDigetherTuningProfile* Profile,
		FDigetherRunTuning& OutSnapshot, TArray<FText>& OutProblems);

	bool IsRunReady() const { return bRunReady; }
	const FDigetherRunTuning* GetRunTuning() const { return bRunReady ? &RunTuning : nullptr; }
	UDigetherRunCoordinatorComponent* GetRunCoordinatorComponent() const { return RunCoordinatorComponent.Get(); }
	UDigetherGameOverFlowComponent* GetGameOverFlowComponent() const { return GameOverFlowComponent.Get(); }

	// 기획 S33: 게임 오버 확정(5일차 미달 귀환)부터 크레딧 뒤 새 게임까지다. 이 동안 구매·납부·출발·신규 참가·
	// 불러오기·새 게임 요청·개발 지급을 받지 않는다.
	bool IsGameOverActive() const;
	// 기획 S33 239행: 크레딧 빨리감기는 호스트만 조작하며 같은 속도를 모두에게 적용한다. 참가자 요청은 거절한다.
	bool SetCreditFastForward(ADigetherGameplayPlayerController* Requester, bool bFastForward);

	bool BeginLeverPull(ADigetherGameplayPlayerController* Requester, ADigetherElevatorLeverActor* Lever);
	bool ReleaseLeverPull(ADigetherGameplayPlayerController* Requester, ADigetherElevatorLeverActor* Lever);
	void CycleSpectateTarget(ADigetherGameplayPlayerController* Requester, bool bNext);
	bool StartRunSmoke(ADigetherGameplayPlayerController* Requester);
	bool GrantCredits(ADigetherGameplayPlayerController* Requester, int32 Amount);

	// 기획 S24 동시 붓기: 우클릭을 누르면 시작, 떼면 끝낸다. 실제 차감은 Tick의 ProcessPourTicks가 주기마다 한다.
	// 시작 조건(접수 열림·생존·거리·입구 방향·가림·잔고)을 먼저 확인하고, 첫 차감은 PAY_TICK_INTERVAL 뒤다.
	FDigetherDepositResponse BeginPour(ADigetherGameplayPlayerController* Requester, ADigetherDepositStationActor* Station);
	FDigetherDepositResponse EndPour(ADigetherGameplayPlayerController* Requester);
	// 바닥 금화를 서버에서 줍는다. 같은 처리 시점의 두 번째 요청은 소비 표시 때문에 거절된다. 줍기 통계에는 넣지 않는다.
	FDigetherDepositResponse PickupFloorCoin(ADigetherGameplayPlayerController* Requester, ADigetherFloorCoinActor* Coin);
	// 개발 시험용: 현재 목표의 공동 인정액을 정한다(목표 미만만). Shipping에서는 false다.
	bool SetQuotaProgressForDev(ADigetherGameplayPlayerController* Requester, int32 DepositedCredits);

	// 바닥 금화 Actor 클래스다. 비우면 C++ 기본 클래스를 쓴다. 금화 메시 교체용 BP를 지정한다.
	UPROPERTY(EditDefaultsOnly, Category="Digether|Deposit")
	TSubclassOf<ADigetherFloorCoinActor> FloorCoinClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning")
	TObjectPtr<UDigetherTuningProfile> TuningProfile;

	// 새 거점처럼 실제 지형이 있는 맵만 켠다. 바닥 없는 시험 월드는 기존 복원 규칙을 유지한다.
	UPROPERTY(EditDefaultsOnly, Category="Digether|Saved Spawn")
	bool bValidateSavedHubSpawn = false;

	// 저장된 갱도 좌표나 이전 거점의 좌표가 새 거점의 유효한 바닥으로 오인되지 않게 한다.
	UPROPERTY(EditDefaultsOnly, Category="Digether|Saved Spawn", meta=(EditCondition="bValidateSavedHubSpawn"))
	FBox HubSpawnAllowedBounds = FBox(FVector(-900.0, -1510.0, 0.0), FVector(900.0, 700.0, 250.0));

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FDigetherFrameworkSaveIntegrationAccess;
	friend class FDigetherHubSpawnTestAccess;
	friend class FDigetherHubWorldTestAccess;
	friend class FDigetherPourIntegrationAccess;
#endif
	// bAtLogin은 참가 확정(Login) 재검사다. 그때만 PreLogin 수락 기록으로 관전 합류를 판정한다.
	// 교체 대기(ReplacePrevious)면 OutPreviousToReplace에 끊을 이전 원격 컨트롤러를 넣는다.
	bool CheckPlayerAdmission(const FUniqueNetIdRepl& UniqueId, bool bAllowMissingId, bool bAtLogin, FString& ErrorMessage,
		DigetherJoinRules::EDigetherJoinDecision* OutDecision = nullptr,
		APlayerController** OutPreviousToReplace = nullptr) const;
	bool IsNewJoinLockedNow() const;
	bool HasParticipantRecord(const FUniqueNetIdRepl& UniqueId) const;
	// PQ-25 판단: 수락 시점 = 그 연결의 PreLogin 판정 시점. 잠금 전 신규 수락을 기록해 Login 확정 때 비교한다.
	void RecordAdmission(const FUniqueNetIdRepl& UniqueId, DigetherJoinRules::EDigetherJoinDecision Decision);
	// 기획 C2: KickPlayer 대신 연결만 닫아 평소 연결 끊김과 같은 정리(PawnLeavingGame → Logout → AddInactivePlayer)를 거친다.
	void DisconnectPreviousConnection(APlayerController* Previous, const FUniqueNetIdRepl& UniqueId) const;
	void BeginLoginReplacement(const FUniqueNetIdRepl& UniqueId, APlayerController* Previous,
		const FOnPreLoginCompleteDelegate& OnComplete);
	void PollLoginReplacements();
	void PollLoginReplacementsAt(double NowSeconds);
	// 이전 컨트롤러가 파괴(Logout 완료)됐고 같은 ID의 활성 PlayerState가 없으면 정리 완료다.
	// 탐사 중에는 AddInactivePlayer가 PS를 활성 명단에서 빼 비활성으로 보관한 뒤다.
	bool IsPreviousConnectionReleased(const FUniqueNetIdRepl& UniqueId, const APlayerController* Previous) const;
	void CompletePendingLoginReplacementsOnEnd();
	// S01 거점 퇴장: 몸을 제거하고 자리를 반환한다. 최신 장부는 저장 명단에 남겨 돌아올 때 초기 지급 없이 복원한다.
	void ReleaseHubSeat(ADigetherGameplayPlayerState* Player);
	void RememberDepartedPlayer(const ADigetherGameplayPlayerState* Player);
	// S01 관전 합류: 잠금 구간에 확정된 참가자는 몸·출발 인원·정산 없이 관전하다 잠금이 풀리면 일반 참가자가 된다.
	bool IsJoinSpectator(const APlayerController* Controller) const;
	void RemoveJoinSpectator(const APlayerController* Controller);
	void EnterJoinSpectate(APlayerController* Controller);
	void CycleJoinSpectateTarget(ADigetherGameplayPlayerController* Requester, bool bNext);
	void ReleaseJoinSpectators();
	bool CaptureProgress(FDigetherSaveSnapshot& OutSnapshot) const;
	bool RestoreProgress(const FDigetherSaveSnapshot& Snapshot, bool bValidateOnly = false);
	// 게임 맵 시작 때 호스트가 한 번 부른다. 방 생성 때 고른 진행 슬롯(저장 서브시스템)과 개발용 URL 옵션으로
	// 새 게임(StartNewGame) 또는 이어하기(LoadProgress 시작 모드)를 고른다. 선택한 슬롯 하나만 대상이다.
	// 반환값은 새 게임을 시도했는지다(시험용).
	bool RunStartupSaveAction(ADigetherGameplayPlayerController* Host);
	// 현재 진행을 한 번 쓴다. 재시도하지 않는다(개발 명령·시설 거래 저장용).
	bool WriteProgress();
	// 인증된 호스트 슬롯에 한 번 쓴다. 성공하면 더 오래된 실패 건의 재시도를 버린다.
	EDigetherSaveResult WriteSnapshot(const FDigetherSaveSnapshot& Snapshot, bool bExplicitNewGame);
	// 기획 S30 자동 저장 시점(탐사 정상 완료·할당량 달성·5일차 실패 귀환). 출발을 막지 않으며 실패하면
	// 호스트에게 알리고 같은 시점의 스냅샷을 일정 간격으로 다시 쓴다. 다음 저장 시점이 오면 최신 값으로 바꾼다.
	void RequestAutosave(const TCHAR* Reason);
	void RequestAutosaveSnapshot(const FDigetherSaveSnapshot& Snapshot, const TCHAR* Reason);
	void RetryPendingAutosave();
	void ClearPendingAutosave();
	// 기획 S30 ③: 호스트가 정상 거점에서 종료하면 한 번 저장한다. 실패해도 재시도로 종료를 붙잡지 않는다.
	// 탐사 중·게임 오버 처리 중에는 현재 상태를 쓰지 않는다.
	bool SaveOnHostExit();
	// 넷 드라이버 종료와 호스트 컨트롤러 파괴보다 먼저 오는 월드 정리 시작 시점에 종료 저장을 한다.
	void HandleWorldBeginTearDown(UWorld* World);
	bool IsHostRequester(const ADigetherGameplayPlayerController* Requester) const;
	void NotifySaveResult(ADigetherGameplayPlayerController* Requester, bool bSucceeded, const FText& Message) const;
	bool RestorePlayerRecord(ADigetherGameplayPlayerState* Player);
	void RestorePlayerPawnTransform(ADigetherGameplayPlayerState* Player) const;
	bool ResolveHubSpawnTransform(const APawn* Pawn, const FTransform* SavedTransform,
		const TArray<FTransform>& ReservedTransforms, FTransform& OutTransform, bool& bOutUsedFallback) const;
	bool IsUsableHubSpawnTransform(const APawn* Pawn, const FTransform& Transform,
		const TArray<FTransform>& ReservedTransforms) const;
	// 새 게임 초기 진행(새 GameId·RunId, 초기 할당량·기본 거점, 현재 접속자만 새 기록·통계 0, 떠난 사람 기록 버림)을 만든다.
	// 이전 게임 결과 보관 목록(ArchivedGameResults)은 그대로 넘긴다. 검증에 실패하면 false다.
	bool BuildNewGameSnapshot(const ADigetherGameplayPlayerController* Host, FDigetherSaveSnapshot& OutSnapshot) const;
	// 크레딧 뒤·시작 시 게임 오버 저장용 새 게임 적용이다. 메모리를 먼저 새 게임으로 바꾸고 현재 슬롯에 명시 새 게임으로 쓴다.
	// 쓰기에 실패하면 알리고 같은 새 게임 스냅샷을 재시도한다(첫 성공까지 명시 새 게임으로 쓴다).
	bool ApplyNewGame(ADigetherGameplayPlayerController* Host, const FDigetherSaveSnapshot& Initial, const TCHAR* Reason);
	ADigetherGameplayPlayerController* FindHostController() const;
	// 게임 오버 단계 시각(GameStateBase::GetServerWorldTimeSeconds)이다. 클라이언트 크레딧 진행과 같은 시계다.
	double GetGameOverNowSeconds() const;
	// 귀환 확정 뒤 부른다. 바닥 금화·붓기를 닫고 결과 기록을 확정해 정산 화면 단계를 시작하고 확정 저장을 요청한다.
	void BeginGameOverAfterReturn(double NowSeconds);
	void TickGameOverAt(double NowSeconds);
	bool SetCreditFastForwardAt(ADigetherGameplayPlayerController* Requester, bool bFastForward, double NowSeconds);
	void PublishGameOverState();
	void CloseHubTradingForGameOver();
	// 저장 명단(떠난 사람 포함)과 현재 참가자로 이번 게임 결과를 만든다. 현재 참가자가 최신 장부다.
	bool BuildGameResult(const FGuid& ResultId, FDigetherGameResult& OutResult) const;
	void AbsorbPlayers(const TArray<int32>& PlayerIds);
	void ReleaseAbsorbedPawns();
	// 크레딧이 끝나면 한 번 부른다. 현재 슬롯만 새 게임으로 초기화하고 같은 방의 새 거점으로 잇는다.
	bool CreateNextGameAfterCredits();
	// 시작 시 불러온 저장이 게임 오버 확정이면 결과를 보관하고 바로 같은 슬롯 새 게임을 만든다(S33 240행).
	bool StartNewGameAfterSavedGameOver(ADigetherGameplayPlayerController* Host, const FDigetherSaveSnapshot& Snapshot);
	int32 AllocateJoinOrder();
	bool InitializeRunActors();
	bool IsDepositClosed() const;
	bool IsValidRequester(const ADigetherGameplayPlayerController* Requester) const;
	bool IsConfiguredLever(const ADigetherElevatorLeverActor* Lever) const;
	bool IsWithinLeverRange(const ADigetherGameplayPlayerController* Requester,
		const ADigetherElevatorLeverActor* Lever) const;
	void CollectPlayers(TArray<ADigetherGameplayPlayerState*>& OutPlayers) const;
	void CollectDeparturePlayers(TArray<ADigetherGameplayPlayerState*>& OutPlayers) const;
	void CollectRunParticipants(TArray<ADigetherGameplayPlayerState*>& OutPlayers) const;
	bool TryMakePullContext(FDigetherElevatorPullContext& OutContext) const;
	// 기획 A3: 레버 게이지 완료는 문 닫힘을 시작하고, 출발·상승 확정은 문이 완전히 닫힌 순간에 한다.
	void BeginDoorClose(EDigetherElevatorLeverKind Kind);
	// 거점 출발 게이지 완료 순간 진행 중인 납부 입력을 끝내고 모든 참가자의 거점 화면을 닫는다.
	void CloseHubTradingForDeparture();
	// 시간 종료를 처리하기 전에 같은 처리 시점의 갱도 문 닫힘 완료를 먼저 판정한다.
	void AdvanceMineDoorBeforeTimeExpiry(float DeltaSeconds);
	void ProcessDoorClosed();
	void ProcessDoorReopenedByOverweight();
	void ResetLeverPull(EDigetherElevatorLeverKind Kind);
	void ProcessDeparture();
	void ProcessAscent();
	void ProcessPendingRunEnd();
	void FinishRunAndReturn(EDigetherRunTerminationTrigger Trigger);
	void LogRunStep(const TCHAR* Step) const;
	void HandleDepartureConfirmed(const FDigetherElevatorDepartureConfirmed& Confirmation);
	void HandleAscentLeverConfirmed(const FDigetherElevatorAscentConfirmed& Confirmation);
	void HandleElevatorAscentConfirmed(const FDigetherElevatorAscentConfirmed& Confirmation);
	void HandleExplorationFinished(const FDigetherElevatorAscentConfirmed& Confirmation);
	void HandleDoorClosed(EDigetherElevatorPhase OriginPhase);
	void HandleDoorReopenedByOverweight(EDigetherElevatorPhase OriginPhase);
	void HandleTimeExpired();
#if !UE_BUILD_SHIPPING
	void AdvanceSmoke();
	TWeakObjectPtr<ADigetherGameplayPlayerController> SmokeController;
	bool bSmokeActive = false;
#endif

	bool ResolveDepositContext(ADigetherGameplayPlayerController* Requester, FString& OutPlayerKey,
		UDigetherWalletComponent*& OutWallet, UDigetherQuotaStateComponent*& OutQuota) const;
	// 중복 ID는 어느 정산기가 유효한지 모호하므로 nullptr다.
	ADigetherDepositStationActor* FindDepositStation(FName StationId) const;
	// 요청자의 몸이 정산기 투입구 거리 안·입구 앞쪽에 있고, 시선이 입구를 향하며 가리는 것이 없는지 본다.
	bool IsPourInletReachable(const ADigetherGameplayPlayerController* Requester,
		const ADigetherDepositStationActor* Station) const;
	// 붓기 요청자의 공통 조건(요청자·PlayerArray·생존 Active·몸·관전 합류 아님)이다.
	bool IsActivePourer(ADigetherGameplayPlayerController* Requester) const;
	ADigetherGameplayPlayerController* FindControllerByDepositKey(const FString& PlayerKey) const;
	// 한 붓기 세션의 매 처리 시점 조건이다. 어긋나면 멈출 이유를 돌려준다.
	bool ValidatePourSession(const FDigetherPourSession& Session, EDigetherPourStopReason& OutReason,
		ADigetherGameplayPlayerController*& OutController, ADigetherDepositStationActor*& OutStation) const;
	// 이번 처리 시점까지 예정된 붓기 주기를 순서대로 처리한다. 레버 처리보다 먼저 불러 같은 시점이면 납부를 먼저 판정한다.
	void ProcessPourTicks(float DeltaSeconds);
	// 한 주기를 처리하고 목표 달성이면 true다. 달성하면 같은 처리 시점의 남은 주기는 처리하지 않는다.
	bool ProcessPourTick(const FDigetherPourDueTick& Tick);
	void StopPour(const FString& PlayerKey, EDigetherPourStopReason Reason);
	void StopAllPours(EDigetherPourStopReason Reason);
	void StopPourForController(ADigetherGameplayPlayerController* Controller, EDigetherPourStopReason Reason);
	void NotifyPourStopped(const FDigetherPourSession& Session, EDigetherPourStopReason Reason);
	ADigetherFloorCoinActor* FindFloorCoin(FName StationId) const;
	// 정산기 앞 금화 더미에 합치거나 새로 만든다. 실패하면 false이며 호출자는 차감을 N으로 줄인다.
	// 성공하면 되돌리기용으로 대상과 새로 만든 여부를 돌려준다.
	bool AddFloorCoins(ADigetherDepositStationActor* Station, int32 Credits, ADigetherFloorCoinActor*& OutCoin, bool& bOutCreated);
	void UndoFloorCoins(ADigetherFloorCoinActor* Coin, int32 Credits, bool bCreated);
	ADigetherFloorCoinActor* SpawnFloorCoin(const FGuid& CoinId, FName StationId, int32 Credits, const FTransform& Transform);
	void CaptureFloorCoins(TArray<FDigetherSaveFloorCoin>& OutCoins) const;
	// 기존 금화 Actor를 모두 지우고 저장 목록대로 다시 만든다. 하나라도 실패하면 false(이미 만든 것은 유지)다.
	bool RestoreFloorCoins(const TArray<FDigetherSaveFloorCoin>& Coins);
	void ClearFloorCoins();

	UPROPERTY(VisibleAnywhere, Category="Digether|Run")
	TObjectPtr<UDigetherRunCoordinatorComponent> RunCoordinatorComponent;

	UPROPERTY(VisibleAnywhere, Category="Digether|Game Over")
	TObjectPtr<UDigetherGameOverFlowComponent> GameOverFlowComponent;

	// 기획 15 GO_* 연출 튜닝(임시값)이다. 저장 형식에 넣지 않으며 단계 시작 때 공개 상태에 복사한다.
	FDigetherGameOverTuning GameOverTuning;
	// 이번 게임의 확정 결과다. 게임 오버 확정 저장에 함께 쓰며 크레딧 뒤 보관 목록으로 옮긴다.
	bool bHasGameResult = false;
	FDigetherGameResult GameResult;
	// 이 슬롯에서 끝난 이전 게임 결과다. 새 게임으로 바뀌어도 저장에 계속 남긴다.
	TArray<FDigetherGameResult> ArchivedGameResults;
	int32 LastJoinOrder = 0;
	// 명시 새 게임 쓰기가 아직 한 번도 성공하지 못한 게임 ID다. 그 게임의 저장은 성공할 때까지 명시 새 게임으로 쓴다.
	FGuid ExplicitNewGamePendingGameId;
	// 흡입 때 이동·충돌을 끈 몸이다. 새 게임 직전에 되돌린다.
	TArray<TWeakObjectPtr<APawn>> AbsorbedPawns;
	bool bNewGameFailureReported = false;

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
	bool bDoorClosedPending = false;
	bool bDoorReopenedPending = false;
	EDigetherElevatorPhase PendingDoorOriginPhase = EDigetherElevatorPhase::HubWaiting;
	bool bApplyingRunEnd = false;

	UPROPERTY(Transient)
	FDigetherRunTuning RunTuning;

	// 중앙 P1 기본값을 보관하며 클라이언트 입력으로 받지 않는다.
	FDigetherEconomyTuning EconomyTuning;
	FDigetherPourSessionRegistry PourSessions;
	// 붓기 주기 시계(초)다. Tick의 DeltaSeconds만 더해 월드 시각 변화·시험 월드와 무관하게 같은 간격을 쓴다.
	double PourClockSeconds = 0.0;
	bool bRunReady = false;
	FGuid GameId;
	FGuid CurrentRunId;
	TArray<FGuid> CompletedSettlementRunIds;
	// 디스크에서 읽은 미접속 참가자도 다음 저장에서 보존한다.
	TArray<FDigetherSavePlayerRecord> SavedPlayerRecords;
	bool bRestoreRejected = false;
	bool bHasSaveStartupAttempted = false;
	// 실패한 자동 저장 시점의 스냅샷이다. 성공한 더 최신 저장이 있으면 버려 오래된 값이 덮어쓰지 않게 한다.
	TOptional<FDigetherSaveSnapshot> PendingAutosave;
	// 저장 시점에 스냅샷을 만들지 못했으면 재시도 때 거점 상태에서 다시 캡처한다.
	bool bAutosaveRecapturePending = false;
	FTimerHandle AutosaveRetryTimer;
	FDelegateHandle WorldTearDownHandle;
	bool bHostExitSaveHandled = false;
	// 출발 확정(문 닫힘 완료) 직전의 메모리 진행이다. 공통 로딩 실패 복귀에만 쓰며 디스크에 쓰지 않는다.
	TOptional<FDigetherSaveSnapshot> DepartureBaseline;
	FDigetherRunRuleTuning FixedRunRuleTuning;
	TWeakObjectPtr<ADigetherGameplayPlayerController> DepartureRequester;
	TArray<TWeakObjectPtr<ADigetherGameplayPlayerState>> ReconnectedPlayers;
	TArray<TWeakObjectPtr<APlayerController>> LoggedInControllers;

	FDigetherLoginReplacementQueue LoginReplacements;
	FTimerHandle LoginReplacementTimer;
	// 잠금 전 PreLogin에서 신규 수락된 ID. 같은 연결의 Login 확정 판정 한 번에만 쓰고 지운다.
	TArray<FUniqueNetIdRepl> AcceptedBeforeLockIds;

	struct FDigetherJoinSpectator
	{
		TWeakObjectPtr<APlayerController> Controller;
		// 관전 대상 공개 키. 몸이 없어 생존 컴포넌트의 관전 상태(사망·탈출 전용)를 쓰지 않고 여기에 둔다.
		FString TargetPlayerKey;
	};
	TArray<FDigetherJoinSpectator> JoinSpectators;
};
