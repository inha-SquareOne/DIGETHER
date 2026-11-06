#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"

enum class EDigetherSessionText : uint8;

namespace DigetherJoinRules
{
	struct FDigetherJoinCheck
	{
		bool bIdValid = false;
		bool bRequireSteamId = false;
		bool bIdIsSteam = false;
		bool bIdAlreadyConnected = false;
		// 이미 연결된 같은 ID가 호스트 자신(로컬 컨트롤러)이면 끊지 않고 거절
		bool bDuplicateIsLocal = false;
		// 이번 세션 비활성 PS에 예약된 ID만 재접속, 디스크 저장 명단 제외
		bool bIdInRoster = false;
		// 이 진행의 저장 명단(거점에서 떠난 참가자 포함)에 있는 ID, 잠금 중 관전 합류 자격 전용
		bool bHasParticipantRecord = false;
		// Login 재검사 전용: 같은 연결의 PreLogin 수락이 잠금 전(수락은 게이지 완료 전, 확정은 이후)
		bool bAcceptedBeforeLock = false;
		int32 ConnectedPlayers = 0;
		int32 ReservedPlayers = 0;
		int32 MaxPlayers = 0;
		// 런 단계 신호는 호출자 제공
		bool bNewJoinLocked = false;
	};

	enum class EDigetherJoinDecision : uint8
	{
		AcceptNew,
		AcceptReconnect,
		// 잠금 구간(문 닫힘·탐사)의 관전 합류, 자리는 차지하나 그날 출발 인원·정산 제외(S01)
		AcceptSpectator,
		// 기획 C2: 같은 ID의 원격 이전 연결을 끊고 정리 뒤 재판정, 그 자체로는 수락 아님
		ReplacePrevious,
		RejectInvalidId,
		RejectDuplicate,
		RejectInProgress,
		RejectFull
	};

	DIGETHER_API EDigetherJoinDecision Evaluate(const FDigetherJoinCheck& Check);
	DIGETHER_API bool IsAccepted(EDigetherJoinDecision Decision);
	// 거절 결과 전용, ID는 있으나 Steam ID가 아닌 거절(SteamIdRequired)과 ID 자체가 없는 거절(AccountCheckFailed) 구분
	DIGETHER_API EDigetherSessionText GetRejectionText(EDigetherJoinDecision Decision, const FDigetherJoinCheck& Check);

	// 이전 연결을 끊을지, 교체 대기(ReplacePrevious)만 해당, 호스트 자신은 Evaluate가 RejectDuplicate로 거름
	// 대기실은 KickPlayer, 게임 맵은 비활성 PlayerState·남긴 Pawn 재접속 계약 때문에 연결 종료(UNetConnection::Close)
	DIGETHER_API bool ShouldDisconnectPreviousConnection(EDigetherJoinDecision Decision);

	// 기획 C2 교체 대기 제한 시간, 임시값(2026-11-05, 기획 수치 아님)
	// 이전 연결 정리는 보통 다음 NetDriver 처리 몇 프레임 안에 끝남, 시간 안에 안 끝나면 ②(같은 계정 접속 중)로 거절
	inline constexpr double LoginReplacementTimeoutSeconds = 5.0;

	enum class EDigetherReplacementWait : uint8
	{
		Waiting,
		Ready,
		TimedOut
	};

	// 이전 연결 정리 여부·경과 시간으로 교체 대기 상태 결정, 정리 완료가 제한 시간보다 우선
	DIGETHER_API EDigetherReplacementWait EvaluateReplacementWait(bool bPreviousReleased, double ElapsedSeconds,
		double TimeoutSeconds);

	// PreLogin 거절 사유를 언어 무관 키로 전송, 엔진은 ErrorMessage를 NMT_Failure로 그대로 전달
	// 참가자가 키를 자기 언어 문구로 변환, 거절 사유 키가 아닌 값은 만들지 않음
	DIGETHER_API FString MakeRejectionMessage(EDigetherSessionText Text);
	// 거절 사유 키 형식이면 OutText에 키를 넣고 true, 그 밖의 문자열(엔진 기본 거절, 이전 형식 문구)은 false
	DIGETHER_API bool TryParseRejectionMessage(const FString& Message, EDigetherSessionText& OutText);

	struct FDigetherInviteCheck
	{
		bool bBusy = false;
		// 세션 인터페이스·필요한 로그인 확인 결과
		bool bOnlineAvailable = false;
		bool bLocalController = false;
		bool bSuccess = false;
		bool bUserIdValid = false;
		bool bProtocolMatches = false;
		bool bAlreadyInSession = false;
	};

	enum class EDigetherInviteDecision : uint8
	{
		Accept,
		RejectBusy,
		RejectOnlineUnavailable,
		RejectInvalidInvite,
		RejectAlreadyInSession
	};

	DIGETHER_API EDigetherInviteDecision EvaluateInvite(const FDigetherInviteCheck& Check);
}
