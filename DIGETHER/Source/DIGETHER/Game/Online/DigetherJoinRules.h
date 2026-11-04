#pragma once

#include "CoreTypes.h"

enum class EDigetherSessionText : uint8;

namespace DigetherJoinRules
{
	struct FDigetherJoinCheck
	{
		bool bIdValid = false;
		bool bRequireSteamId = false;
		bool bIdIsSteam = false;
		bool bIdAlreadyConnected = false;
		// 이번 세션의 비활성 PS에 예약된 ID만 재접속이다. 디스크 저장 명단은 포함하지 않는다.
		bool bIdInRoster = false;
		int32 ConnectedPlayers = 0;
		int32 ReservedPlayers = 0;
		int32 MaxPlayers = 0;
		// 런 단계 신호는 호출자가 제공한다.
		bool bNewJoinLocked = false;
	};

	enum class EDigetherJoinDecision : uint8
	{
		AcceptNew,
		AcceptReconnect,
		RejectInvalidId,
		RejectDuplicate,
		RejectInProgress,
		RejectFull
	};

	DIGETHER_API EDigetherJoinDecision Evaluate(const FDigetherJoinCheck& Check);
	DIGETHER_API bool IsAccepted(EDigetherJoinDecision Decision);
	// 거절 결과에만 사용한다. Steam 인증 거절은 일반 ID 거절과 문구를 구별한다.
	DIGETHER_API EDigetherSessionText GetRejectionText(EDigetherJoinDecision Decision, const FDigetherJoinCheck& Check);

	struct FDigetherInviteCheck
	{
		bool bBusy = false;
		// 세션 인터페이스와 필요한 로그인 확인을 마친 결과다.
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
