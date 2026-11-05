#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/Function.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Game/Online/DigetherRoomTypes.h"
#include "Game/Online/DigetherSessionText.h"
#include "Game/Save/DigetherSaveSlotTypes.h"
#include "DigetherSessionSubsystem.generated.h"

class FTimerManager;
class UNetDriver;

DECLARE_MULTICAST_DELEGATE(FOnDigetherSessionEvent);

// Steam(또는 -nosteam의 Null LAN) 세션의 생성·검색·참가·초대·종료 흐름을 관리한다.
// 방 정보를 세션 설정에 쓰고 읽는 규약은 DigetherRoomProtocol이 맡는다.
// 화면과 콘솔 명령은 이 서브시스템의 같은 함수를 호출한다. 상태 문구는 ST_Session(String Table)에서 가져온다.
UCLASS()
class DIGETHER_API UDigetherSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 위젯·컨트롤러·GameMode 같은 월드 문맥 객체에서 이 서브시스템을 찾는다. 없으면 nullptr.
	static UDigetherSessionSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 방을 만들고 성공하면 대기실 맵을 listen으로 연다. RoomCode가 비면 새 코드를 만든다.
	// SaveSelection은 호스트가 고른 진행 슬롯·시작 방식(새 게임/이어하기)이다(기획 S01 2행). 저장 서브시스템에 보관해
	// 게임 맵 GameMode 시작이 사용한다. 기본값은 슬롯 0 이어하기(저장이 없으면 새 진행)로 슬롯 도입 전 동작과 같다.
	void HostRoom(const FString& RoomName, bool bPublic, const FString& RoomCode = FString(),
		const FDigetherSaveSlotSelection& SaveSelection = FDigetherSaveSlotSelection());
	// RoomCode가 비면 이 게임의 모든 공개 방을 찾는다.
	void FindRooms(const FString& RoomCode = FString());
	void JoinRoom(int32 Index);
	// 코드가 일치하는 첫 방에 참가한다. TimeoutSeconds가 0보다 크면 방이 열릴 때까지 주기적으로 다시 찾는다.
	void JoinByCode(const FString& RoomCode, float TimeoutSeconds = 0.f);
	// 세션을 끝내고 메인 메뉴로 이동한다. 코드 참가 대기 중이면 대기만 취소한다.
	void LeaveRoom();
	// 호스트가 대기실에서 게임 맵으로 모든 참가자를 이동시킨다. 호출 전 준비 상태 검사는 GameMode가 한다.
	bool StartGame();
	bool ShowInviteUI();

	bool IsSteamMode() const;
	bool IsOnlineAvailable() const;
	bool IsInSession() const;
	bool IsBusy() const { return Operation != EDigetherSessionOperation::Idle; }
	// 메뉴 화면이 방 만들기·참가·검색 버튼을 켤지 정하는 공통 판정: 진행 중 작업이 없고 온라인을 쓸 수 있다.
	// 로그인·기존 방 같은 세부 조건은 요청 시 CheckCanRequest가 다시 확인하고 문구로 알린다.
	bool CanStartRoomRequest() const;
	EDigetherSessionOperation GetOperation() const { return Operation; }
	const FText& GetStatus() const { return Status; }
	const TArray<FDigetherRoomInfo>& GetRooms() const { return RoomInfos; }
	const FString& GetActiveRoomCode() const { return ActiveRoomCode; }
	const FString& GetActiveRoomName() const { return ActiveRoomName; }
	FString GetLocalPlayerName() const;
	FString MakeDefaultRoomName() const;
	// 메인 메뉴로 돌아온 뒤 보여 줄 마지막 안내(연결 끊김 등)를 꺼낸다.
	FText ConsumeNotice();

	FOnDigetherSessionEvent OnStatusChanged;
	FOnDigetherSessionEvent OnRoomsUpdated;

private:
	enum class ETravelIntent : uint8 { None, Host, Join, Leave, Start };

	// 코드 참가(JoinByCode)의 진행 단계. 목록 검색(FindRooms)은 None이다.
	enum class ECodeJoinState : uint8
	{
		None,
		// 코드가 맞는 방을 찾으면 바로 참가한다.
		Pending,
		// 검색 도중 제한 시간이 지났다. Steam 로비 검색은 취소되지 않으므로 늦게 온 결과를 버린다.
		ExpiredDuringSearch
	};

	IOnlineSessionPtr AcquireSessions(bool bRequireLogin);
	FUniqueNetIdPtr GetLocalUserId() const;
	FTimerManager& GetTimerManager() const;
	// 방 요청 공통 사전 조건(진행 중 작업, 방 코드, 온라인 로그인, 기존 방). 실패하면 상태 문구를 남기고 false.
	// bRequireCode가 거짓이면 RoomCode가 빈 경우 코드 검사를 생략한다.
	bool CheckCanRequest(const FString& RoomCode, bool bRequireCode);
	// 비동기 요청이 바로 거절돼 완료 알림이 오지 않으면 Idle로 되돌리고 true를 돌려준다.
	// 요청 안에서 완료 알림이 먼저 와 Operation이 이미 바뀌었으면 건드리지 않는다.
	bool RevertRejectedRequest(bool bRequested, EDigetherSessionOperation RequestOperation);
	// 비동기 세션 요청의 공통 순서: Operation 설정 → 완료 알림 구독 → 진행 문구(있으면) → 요청.
	// 요청이 바로 거절돼 Idle로 되돌렸으면 구독을 풀고 true를 돌려준다. 실패 문구는 호출자가 정한다.
	bool SendRequestOrRevert(EDigetherSessionOperation RequestOperation, const TOptional<FText>& PendingStatus,
		TFunctionRef<void()> Subscribe, TFunctionRef<bool()> Request, TFunctionRef<void()> Unsubscribe);
	// 방 나가기와 실패 정리가 함께 쓰는 세션 종료 요청. 반환값은 SendRequestOrRevert와 같다.
	bool SendDestroyRequestOrRevert(const TOptional<FText>& PendingStatus);
	bool CheckSteamSocketsAvailable();

	void SetStatus(const FText& Message);
	void SetStatus(EDigetherSessionText Key) { SetStatus(DigetherSessionText::Get(Key)); }
	FText MakeIdleStatus() const;
	// 실패 안내를 메인 메뉴용 공지로 남기고, 남은 세션을 정리한 뒤 상태로 보여 준다.
	void ReportFailure(const FText& Message);

	// 코드 참가 대기를 끝내고 재검색·제한 시간 타이머를 지운다. 남은 타이머가 다음 요청에 섞이지 않게 한다.
	void EndCodeJoin();
	void StartSearch();
	void RetrySearch();
	void HandleJoinDeadline();
	void JoinResult(const FOnlineSessionSearchResult& Result);
	void DestroyAfterFailure();
	void OpenMap(const FString& Map, const TCHAR* Options = nullptr);

	void HandleCreateComplete(FName SessionName, bool bSuccess);
	void HandleFindComplete(bool bSuccess);
	void HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroyComplete(FName SessionName, bool bSuccess);
	void HandleInviteAccepted(bool bSuccess, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& Result);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type Failure, const FString& Error);
	void HandleNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Failure, const FString& Error);
	// World 없이 온 대기 중 연결 실패가 이 인스턴스의 참가 이동에서 난 것인지.
	bool IsOwnPendingJoinFailure(const UNetDriver* Driver, ENetworkFailure::Type Failure) const;
	void HandlePostLoadMap(UWorld* World);

	EDigetherSessionOperation Operation = EDigetherSessionOperation::Idle;
	ETravelIntent TravelIntent = ETravelIntent::None;
	ECodeJoinState CodeJoin = ECodeJoinState::None;
	// 코드 참가 제한 시각(FPlatformTime::Seconds 기준). 0이면 제한 없음.
	double JoinDeadline = 0.0;
	bool bTravelAfterDestroy = true;
	FText Status;
	FText Notice;
	FString ActiveRoomCode;
	FString ActiveRoomName;
	FString SearchRoomCode;

	TSharedPtr<FOnlineSessionSearch> Search;
	TArray<FOnlineSessionSearchResult> Results;
	TArray<FDigetherRoomInfo> RoomInfos;
	IOnlineSessionPtr Sessions;
	// 초대 수락 알림을 등록한 세션 인터페이스. AcquireSessions가 Sessions를 다시 받아도 해제는 이 인스턴스에서 한다.
	TWeakPtr<IOnlineSession, ESPMode::ThreadSafe> InviteSessions;

	FTimerHandle RetryTimer;
	FTimerHandle DeadlineTimer;
	FDelegateHandle CreateHandle, FindHandle, JoinHandle, DestroyHandle, InviteHandle;
	FDelegateHandle TravelFailureHandle, NetworkFailureHandle, PostLoadMapHandle;
};
