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

// 서버 연결층, 경제 판정은 P1 값 함수·레지스트리에 위임
// 구현 파일: 시작·참가자 생성(GameMode), 접속 승인(Login), 재접속·퇴장(Reconnect), 런 흐름·Tick(RunFlow),
// 관전(Spectate), 붓기·바닥 금화(Pour), 저장(Save), 거점 스폰(Spawn), 게임 오버(GameOver), 개발(Dev)
UCLASS()
class DIGETHER_API ADigetherGameplayGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ADigetherGameplayGameMode();

	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// 동기 경로, 정리 대기 불가로 같은 계정 교체 대상도 ②로 거절·이전 연결 유지
	virtual void PreLogin(const FString& Options, const FString& Address,
		const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	// 엔진 접속(NMT_Login) 승인 경로, 기획 C2: 같은 계정이면 이전 연결 종료·정리 대기 후 재판정
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
	// 기획 S02·S30 공통(호스트·공통 맵) 출발 로딩 실패 확정 시 호출
	// 디스크 미사용, 문 닫힘 완료 순간 메모리 기준점으로 같은 일차 거점 복귀
	// 구매·납부·해금·배치·게이지 완료 때 기록한 4일차 마감 유지, 일차 미소비
	// 갱도 도착(제한 시간 시작) 뒤에는 로딩 단계가 아니므로 거절
	bool RecoverFromCommonLoadingFailure();

	// 월드 없이도 같은 거절 경로 검사, 실패 시 출력 스냅샷 보존
	static bool TryMakeRunSnapshot(const UDigetherTuningProfile* Profile,
		FDigetherRunTuning& OutSnapshot, TArray<FText>& OutProblems);

	bool IsRunReady() const { return bRunReady; }
	const FDigetherRunTuning* GetRunTuning() const { return bRunReady ? &RunTuning : nullptr; }
	UDigetherRunCoordinatorComponent* GetRunCoordinatorComponent() const { return RunCoordinatorComponent.Get(); }
	UDigetherGameOverFlowComponent* GetGameOverFlowComponent() const { return GameOverFlowComponent.Get(); }

	// 기획 S33: 게임 오버 확정(5일차 미달 귀환)부터 크레딧 뒤 새 게임까지
	// 이 동안 구매·납부·출발·신규 참가·불러오기·새 게임 요청·개발 지급 거절
	bool IsGameOverActive() const;
	// 기획 S33 239행: 크레딧 빨리감기는 호스트만, 같은 속도를 모두에게 적용, 참가자 요청 거절
	bool SetCreditFastForward(ADigetherGameplayPlayerController* Requester, bool bFastForward);

	bool BeginLeverPull(ADigetherGameplayPlayerController* Requester, ADigetherElevatorLeverActor* Lever);
	bool ReleaseLeverPull(ADigetherGameplayPlayerController* Requester, ADigetherElevatorLeverActor* Lever);
	void CycleSpectateTarget(ADigetherGameplayPlayerController* Requester, bool bNext);
	bool StartRunSmoke(ADigetherGameplayPlayerController* Requester);
	bool GrantCredits(ADigetherGameplayPlayerController* Requester, int32 Amount);

	// 기획 S24 동시 붓기: 우클릭 누르면 시작, 떼면 종료, 실제 차감은 Tick의 ProcessPourTicks가 주기마다
	// 시작 조건(접수 열림·생존·거리·입구 방향·가림·잔고) 선확인, 첫 차감은 PAY_TICK_INTERVAL 뒤
	FDigetherDepositResponse BeginPour(ADigetherGameplayPlayerController* Requester, ADigetherDepositStationActor* Station);
	FDigetherDepositResponse EndPour(ADigetherGameplayPlayerController* Requester);
	// 서버 바닥 금화 줍기, 같은 처리 시점 두 번째 요청은 소비 표시로 거절, 줍기 통계 미포함
	FDigetherDepositResponse PickupFloorCoin(ADigetherGameplayPlayerController* Requester, ADigetherFloorCoinActor* Coin);
	// 개발 시험용: 현재 목표의 공동 인정액 지정(목표 미만만), Shipping은 false
	bool SetQuotaProgressForDev(ADigetherGameplayPlayerController* Requester, int32 DepositedCredits);

	// 바닥 금화 Actor 클래스, 비우면 C++ 기본 클래스, 금화 메시 교체용 BP 지정
	UPROPERTY(EditDefaultsOnly, Category="Digether|Deposit")
	TSubclassOf<ADigetherFloorCoinActor> FloorCoinClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning")
	TObjectPtr<UDigetherTuningProfile> TuningProfile;

	// 실제 지형이 있는 새 거점 맵만 켬, 바닥 없는 시험 월드는 기존 복원 규칙 유지
	UPROPERTY(EditDefaultsOnly, Category="Digether|Saved Spawn")
	bool bValidateSavedHubSpawn = false;

	// 저장된 갱도 좌표·이전 거점 좌표의 새 거점 유효 바닥 오인 방지
	UPROPERTY(EditDefaultsOnly, Category="Digether|Saved Spawn", meta=(EditCondition="bValidateSavedHubSpawn"))
	FBox HubSpawnAllowedBounds = FBox(FVector(-900.0, -1510.0, 0.0), FVector(900.0, 700.0, 250.0));

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FDigetherFrameworkSaveIntegrationAccess;
	friend class FDigetherHubSpawnTestAccess;
	friend class FDigetherHubWorldTestAccess;
	friend class FDigetherPourIntegrationAccess;
#endif

	// --- 접속 승인·같은 계정 교체 (Login.cpp) ---
	// bAtLogin은 참가 확정(Login) 재검사, 그때만 PreLogin 수락 기록으로 관전 합류 판정
	// 교체 대기(ReplacePrevious)면 OutPreviousToReplace에 끊을 이전 원격 컨트롤러
	bool CheckPlayerAdmission(const FUniqueNetIdRepl& UniqueId, bool bAllowMissingId, bool bAtLogin, FString& ErrorMessage,
		DigetherJoinRules::EDigetherJoinDecision* OutDecision = nullptr,
		APlayerController** OutPreviousToReplace = nullptr) const;
	bool IsNewJoinLockedNow() const;
	bool HasParticipantRecord(const FUniqueNetIdRepl& UniqueId) const;
	// PQ-25 판단: 수락 시점 = 그 연결의 PreLogin 판정 시점, 잠금 전 신규 수락을 기록해 Login 확정 때 비교
	void RecordAdmission(const FUniqueNetIdRepl& UniqueId, DigetherJoinRules::EDigetherJoinDecision Decision);
	// 기획 C2: KickPlayer 대신 연결만 닫아 평소 연결 끊김과 같은 정리(PawnLeavingGame → Logout → AddInactivePlayer)
	void DisconnectPreviousConnection(APlayerController* Previous, const FUniqueNetIdRepl& UniqueId) const;
	void BeginLoginReplacement(const FUniqueNetIdRepl& UniqueId, APlayerController* Previous,
		const FOnPreLoginCompleteDelegate& OnComplete);
	void PollLoginReplacements();
	void PollLoginReplacementsAt(double NowSeconds);
	// 이전 컨트롤러 파괴(Logout 완료)·같은 ID 활성 PlayerState 없음이면 정리 완료
	// 탐사 중에는 AddInactivePlayer가 PS를 활성 명단에서 빼 비활성 보관한 뒤
	bool IsPreviousConnectionReleased(const FUniqueNetIdRepl& UniqueId, const APlayerController* Previous) const;
	void CompletePendingLoginReplacementsOnEnd();
	int32 AllocateJoinOrder();

	// --- 거점 퇴장·관전 합류 (Reconnect.cpp, Spectate.cpp) ---
	// S01 거점 퇴장: 몸 제거·자리 반환, 최신 장부는 저장 명단에 남겨 복귀 때 초기 지급 없이 복원
	void ReleaseHubSeat(ADigetherGameplayPlayerState* Player);
	void RememberDepartedPlayer(const ADigetherGameplayPlayerState* Player);
	// S01 관전 합류: 잠금 구간 확정 참가자는 몸·출발 인원·정산 없이 관전, 잠금 해제 시 일반 참가자
	bool IsJoinSpectator(const APlayerController* Controller) const;
	void RemoveJoinSpectator(const APlayerController* Controller);
	void EnterJoinSpectate(APlayerController* Controller);
	void CycleJoinSpectateTarget(ADigetherGameplayPlayerController* Requester, bool bNext);
	void ReleaseJoinSpectators();

	// --- 저장·불러오기 (Save.cpp, Spawn.cpp) ---
	bool CaptureProgress(FDigetherSaveSnapshot& OutSnapshot) const;
	bool RestoreProgress(const FDigetherSaveSnapshot& Snapshot, bool bValidateOnly = false);
	// 게임 맵 시작 때 호스트가 한 번 호출, 방 생성 때 고른 진행 슬롯(저장 서브시스템)·개발용 URL 옵션으로
	// 새 게임(StartNewGame) 또는 이어하기(LoadProgress 시작 모드) 선택, 선택 슬롯 하나만 대상
	// 반환값은 새 게임 시도 여부(시험용)
	bool RunStartupSaveAction(ADigetherGameplayPlayerController* Host);
	// 현재 진행 1회 쓰기, 재시도 없음(개발 명령·시설 거래 저장용)
	bool WriteProgress();
	// 인증된 호스트 슬롯에 1회 쓰기, 성공 시 더 오래된 실패 건 재시도 폐기
	EDigetherSaveResult WriteSnapshot(const FDigetherSaveSnapshot& Snapshot, bool bExplicitNewGame);
	// 기획 S30 자동 저장 시점(탐사 정상 완료·할당량 달성·5일차 실패 귀환), 출발 미차단
	// 실패 시 호스트 알림·같은 시점 스냅샷을 일정 간격 재시도, 다음 저장 시점이 오면 최신 값으로 교체
	void RequestAutosave(const TCHAR* Reason);
	void RequestAutosaveSnapshot(const FDigetherSaveSnapshot& Snapshot, const TCHAR* Reason);
	void RetryPendingAutosave();
	void ClearPendingAutosave();
	// 기획 S30 ③: 호스트가 정상 거점에서 종료하면 1회 저장, 실패해도 재시도로 종료를 막지 않음
	// 탐사 중·게임 오버 처리 중에는 현재 상태 미기록
	bool SaveOnHostExit();
	// 넷 드라이버 종료·호스트 컨트롤러 파괴보다 먼저 오는 월드 정리 시작 시점의 종료 저장
	void HandleWorldBeginTearDown(UWorld* World);
	bool IsHostRequester(const ADigetherGameplayPlayerController* Requester) const;
	void NotifySaveResult(ADigetherGameplayPlayerController* Requester, bool bSucceeded, const FText& Message) const;
	bool RestorePlayerRecord(ADigetherGameplayPlayerState* Player);
	void RestorePlayerPawnTransform(ADigetherGameplayPlayerState* Player) const;
	bool ResolveHubSpawnTransform(const APawn* Pawn, const FTransform* SavedTransform,
		const TArray<FTransform>& ReservedTransforms, FTransform& OutTransform, bool& bOutUsedFallback) const;
	bool IsUsableHubSpawnTransform(const APawn* Pawn, const FTransform& Transform,
		const TArray<FTransform>& ReservedTransforms) const;
	// 새 게임 초기 진행(새 GameId·RunId, 초기 할당량·기본 거점, 현재 접속자만 새 기록·통계 0, 떠난 사람 기록 폐기)
	// 이전 게임 결과 보관 목록(ArchivedGameResults)은 유지, 검증 실패 시 false
	bool BuildNewGameSnapshot(const ADigetherGameplayPlayerController* Host, FDigetherSaveSnapshot& OutSnapshot) const;

	// --- 게임 오버 (GameOver.cpp) ---
	// 크레딧 뒤·시작 시 게임 오버 저장용 새 게임 적용, 메모리를 먼저 새 게임으로 바꾸고 현재 슬롯에 명시 새 게임으로 쓰기
	// 쓰기 실패 시 알림·같은 새 게임 스냅샷 재시도(첫 성공까지 명시 새 게임)
	bool ApplyNewGame(ADigetherGameplayPlayerController* Host, const FDigetherSaveSnapshot& Initial, const TCHAR* Reason);
	ADigetherGameplayPlayerController* FindHostController() const;
	// 게임 오버 단계 시각(GameStateBase::GetServerWorldTimeSeconds), 클라이언트 크레딧 진행과 같은 시계
	double GetGameOverNowSeconds() const;
	// 귀환 확정 뒤 호출, 바닥 금화·붓기 종료·결과 기록 확정 후 정산 화면 단계 시작·확정 저장 요청
	void BeginGameOverAfterReturn(double NowSeconds);
	void TickGameOverAt(double NowSeconds);
	bool SetCreditFastForwardAt(ADigetherGameplayPlayerController* Requester, bool bFastForward, double NowSeconds);
	void PublishGameOverState();
	void CloseHubTradingForGameOver();
	// 저장 명단(떠난 사람 포함)·현재 참가자로 이번 게임 결과 생성, 현재 참가자가 최신 장부
	bool BuildGameResult(const FGuid& ResultId, FDigetherGameResult& OutResult) const;
	void AbsorbPlayers(const TArray<int32>& PlayerIds);
	void ReleaseAbsorbedPawns();
	// 크레딧 종료 시 1회, 현재 슬롯만 새 게임으로 초기화하고 같은 방의 새 거점으로 연결
	bool CreateNextGameAfterCredits();
	// 시작 시 불러온 저장이 게임 오버 확정이면 결과 보관 후 바로 같은 슬롯 새 게임(S33 240행)
	bool StartNewGameAfterSavedGameOver(ADigetherGameplayPlayerController* Host, const FDigetherSaveSnapshot& Snapshot);

	// --- 런 흐름·레버·문 (RunFlow.cpp) ---
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
	// 기획 A3: 레버 게이지 완료는 문 닫힘 시작, 출발·상승 확정은 문이 완전히 닫힌 순간
	void BeginDoorClose(EDigetherElevatorLeverKind Kind);
	// 거점 출발 게이지 완료 순간 진행 중 납부 입력 종료·모든 참가자 거점 화면 닫기
	void CloseHubTradingForDeparture();
	// 시간 종료 처리 전에 같은 처리 시점의 갱도 문 닫힘 완료 선판정
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

	// --- 붓기·바닥 금화 (Pour.cpp) ---
	bool ResolveDepositContext(ADigetherGameplayPlayerController* Requester, FString& OutPlayerKey,
		UDigetherWalletComponent*& OutWallet, UDigetherQuotaStateComponent*& OutQuota) const;
	// 중복 ID는 유효 정산기가 모호하므로 nullptr
	ADigetherDepositStationActor* FindDepositStation(FName StationId) const;
	// 요청자 몸이 정산기 투입구 거리 안·입구 앞쪽, 시선이 입구 방향, 가림 없음 확인
	bool IsPourInletReachable(const ADigetherGameplayPlayerController* Requester,
		const ADigetherDepositStationActor* Station) const;
	// 붓기 요청자 공통 조건(요청자·PlayerArray·생존 Active·몸·관전 합류 아님)
	bool IsActivePourer(ADigetherGameplayPlayerController* Requester) const;
	ADigetherGameplayPlayerController* FindControllerByDepositKey(const FString& PlayerKey) const;
	// 붓기 세션의 매 처리 시점 조건, 어긋나면 멈출 이유 반환
	bool ValidatePourSession(const FDigetherPourSession& Session, EDigetherPourStopReason& OutReason,
		ADigetherGameplayPlayerController*& OutController, ADigetherDepositStationActor*& OutStation) const;
	// 이번 처리 시점까지 예정된 붓기 주기를 순서대로 처리, 레버보다 먼저 호출해 같은 시점이면 납부 선판정
	void ProcessPourTicks(float DeltaSeconds);
	// 한 주기 처리, 목표 달성이면 true·같은 처리 시점 남은 주기 미처리
	bool ProcessPourTick(const FDigetherPourDueTick& Tick);
	void StopPour(const FString& PlayerKey, EDigetherPourStopReason Reason);
	void StopAllPours(EDigetherPourStopReason Reason);
	void StopPourForController(ADigetherGameplayPlayerController* Controller, EDigetherPourStopReason Reason);
	void NotifyPourStopped(const FDigetherPourSession& Session, EDigetherPourStopReason Reason);
	ADigetherFloorCoinActor* FindFloorCoin(FName StationId) const;
	// 정산기 앞 금화 더미에 합치거나 새로 생성, 실패 시 false·호출자는 차감을 N으로 축소
	// 성공 시 되돌리기용 대상·새로 만든 여부 반환
	bool AddFloorCoins(ADigetherDepositStationActor* Station, int32 Credits, ADigetherFloorCoinActor*& OutCoin, bool& bOutCreated);
	void UndoFloorCoins(ADigetherFloorCoinActor* Coin, int32 Credits, bool bCreated);
	ADigetherFloorCoinActor* SpawnFloorCoin(const FGuid& CoinId, FName StationId, int32 Credits, const FTransform& Transform);
	void CaptureFloorCoins(TArray<FDigetherSaveFloorCoin>& OutCoins) const;
	// 기존 금화 Actor 전부 제거 후 저장 목록대로 재생성, 하나라도 실패하면 false(이미 만든 것은 유지)
	bool RestoreFloorCoins(const TArray<FDigetherSaveFloorCoin>& Coins);
	void ClearFloorCoins();

	// --- 개발 스모크 (Dev.cpp) ---
#if !UE_BUILD_SHIPPING
	void AdvanceSmoke();
	TWeakObjectPtr<ADigetherGameplayPlayerController> SmokeController;
	bool bSmokeActive = false;
#endif

	// --- 상태 ---
	UPROPERTY(VisibleAnywhere, Category="Digether|Run")
	TObjectPtr<UDigetherRunCoordinatorComponent> RunCoordinatorComponent;

	UPROPERTY(VisibleAnywhere, Category="Digether|Game Over")
	TObjectPtr<UDigetherGameOverFlowComponent> GameOverFlowComponent;

	// 기획 15 GO_* 연출 튜닝(임시값), 저장 형식 미포함, 단계 시작 때 공개 상태에 복사
	FDigetherGameOverTuning GameOverTuning;
	// 이번 게임 확정 결과, 게임 오버 확정 저장에 포함·크레딧 뒤 보관 목록으로 이동
	bool bHasGameResult = false;
	FDigetherGameResult GameResult;
	// 이 슬롯에서 끝난 이전 게임 결과, 새 게임 뒤에도 저장 유지
	TArray<FDigetherGameResult> ArchivedGameResults;
	int32 LastJoinOrder = 0;
	// 명시 새 게임 쓰기가 아직 성공하지 못한 게임 ID, 성공 전까지 그 게임 저장은 명시 새 게임
	FGuid ExplicitNewGamePendingGameId;
	// 흡입 때 이동·충돌을 끈 몸, 새 게임 직전 복구
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

	// 중앙 P1 기본값 보관, 클라이언트 입력으로 받지 않음
	FDigetherEconomyTuning EconomyTuning;
	FDigetherPourSessionRegistry PourSessions;
	// 붓기 주기 시계(초), Tick DeltaSeconds만 누적해 월드 시각 변화·시험 월드와 무관한 같은 간격
	double PourClockSeconds = 0.0;
	bool bRunReady = false;
	FGuid GameId;
	FGuid CurrentRunId;
	TArray<FGuid> CompletedSettlementRunIds;
	// 디스크에서 읽은 미접속 참가자도 다음 저장에서 보존
	TArray<FDigetherSavePlayerRecord> SavedPlayerRecords;
	bool bRestoreRejected = false;
	bool bHasSaveStartupAttempted = false;
	// 실패한 자동 저장 시점 스냅샷, 더 최신 저장 성공 시 폐기해 오래된 값 덮어쓰기 방지
	TOptional<FDigetherSaveSnapshot> PendingAutosave;
	// 저장 시점에 스냅샷 생성 실패 시 재시도 때 거점 상태에서 재캡처
	bool bAutosaveRecapturePending = false;
	FTimerHandle AutosaveRetryTimer;
	FDelegateHandle WorldTearDownHandle;
	bool bHostExitSaveHandled = false;
	// 출발 확정(문 닫힘 완료) 직전 메모리 진행, 공통 로딩 실패 복귀 전용·디스크 미기록
	TOptional<FDigetherSaveSnapshot> DepartureBaseline;
	FDigetherRunRuleTuning FixedRunRuleTuning;
	TWeakObjectPtr<ADigetherGameplayPlayerController> DepartureRequester;
	TArray<TWeakObjectPtr<ADigetherGameplayPlayerState>> ReconnectedPlayers;
	TArray<TWeakObjectPtr<APlayerController>> LoggedInControllers;

	FDigetherLoginReplacementQueue LoginReplacements;
	FTimerHandle LoginReplacementTimer;
	// 잠금 전 PreLogin에서 신규 수락된 ID, 같은 연결의 Login 확정 판정 1회에만 쓰고 삭제
	TArray<FUniqueNetIdRepl> AcceptedBeforeLockIds;

	struct FDigetherJoinSpectator
	{
		TWeakObjectPtr<APlayerController> Controller;
		// 관전 대상 공개 키, 몸이 없어 생존 컴포넌트 관전 상태(사망·탈출 전용) 대신 여기 보관
		FString TargetPlayerKey;
	};
	TArray<FDigetherJoinSpectator> JoinSpectators;
};
