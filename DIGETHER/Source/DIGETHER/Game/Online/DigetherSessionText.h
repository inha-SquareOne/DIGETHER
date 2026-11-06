#pragma once

#include "CoreMinimal.h"
#include "DigetherSessionText.generated.h"

// 세션·대기실 상태 문구 String Table 키, 값 이름이 ST_Session 키
// 문구는 에디터에서 /Game/Frontend/Localization/ST_Session을 열어 수정, 키 추가 시 에셋에도 같은 키 추가
// (DIGETHER.Game.Online.SessionText 테스트가 빠진 키 검출)
UENUM()
enum class EDigetherSessionText : uint8
{
	// 대기 상태
	SteamConnected,			// {PlayerName}
	LanMode,
	OnlineUnavailable,
	SteamLoginUnavailable,
	SteamSocketsUnavailable,
	// 요청 사전 조건
	Busy,
	InvalidRoomCode,		// {MaxLength}
	AlreadyInRoom,
	// 방 만들기
	Creating,
	CreateRequestFailed,
	CreateFailed,
	Created,				// {RoomCode}
	// 방 찾기·코드 참가
	Searching,
	SearchingCode,			// {RoomCode}
	SearchRequestFailed,
	SearchFailed,
	RoomsFound,				// {Count}
	WaitingForRoom,			// {RoomCode}
	CodeNotFound,			// {RoomCode}
	JoinTimeout,
	JoinTimeoutPending,
	StaleRoom,
	// 참가
	Joining,
	JoinRequestFailed,
	RoomFull,
	RoomClosed,
	// 호스트 입장 거절 사유(DigetherJoinRules::GetRejectionText), SteamIdRequired도 같은 경로
	AccountCheckFailed,
	AccountAlreadyConnected,
	RunInProgress,
	JoinFailed,
	Connecting,
	HostConnected,
	// 나가기
	WaitCancelled,
	Leaving,
	LeaveRequestFailed,
	LeaveFailed,
	Left,
	// 게임 시작
	HostOnlyStart,
	GameMapMissing,
	Starting,
	StartTravelFailed,
	// 초대
	InviteSteamOnly,
	InviteOverlayFailed,
	InviteBusy,
	InviteVersionMismatch,
	InviteLeaveFirst,
	// 이동·연결 결과
	RoomOpen,				// {RoomCode} {MaxPlayers}
	NoSession,
	SteamSocketsDriverMissing,
	NetModeMismatch,
	TravelFailed,			// {Error}
	ConnectionLost,
	ConnectionFailed,		// {Error}
	SteamIdRequired,
	// 기본 이름
	DefaultPlayerName,
	DefaultRoomName,		// {PlayerName}
};

namespace DigetherSessionText
{
	// ST_Session 에셋 경로(String Table ID)
	DIGETHER_API extern const TCHAR* const TableId;

	DIGETHER_API FText Get(EDigetherSessionText Key);
	DIGETHER_API FString GetKey(EDigetherSessionText Key);
}
