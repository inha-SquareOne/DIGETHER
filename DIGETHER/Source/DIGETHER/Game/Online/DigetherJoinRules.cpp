#include "Game/Online/DigetherJoinRules.h"

#include "Game/Online/DigetherSessionText.h"

namespace DigetherJoinRules
{
	EDigetherJoinDecision Evaluate(const FDigetherJoinCheck& Check)
	{
		if (!Check.bIdValid || (Check.bRequireSteamId && !Check.bIdIsSteam))
		{
			return EDigetherJoinDecision::RejectInvalidId;
		}
		if (Check.bIdAlreadyConnected)
		{
			return EDigetherJoinDecision::RejectDuplicate;
		}
		if (Check.bIdInRoster)
		{
			return EDigetherJoinDecision::AcceptReconnect;
		}
		if (Check.bNewJoinLocked)
		{
			return EDigetherJoinDecision::RejectInProgress;
		}
		// 끊긴 참가자의 자리는 세션 종료까지 예약한다. 재접속은 같은 자리를 다시 쓰므로 위에서 수락한다.
		if (static_cast<int64>(Check.ConnectedPlayers) + Check.ReservedPlayers >= Check.MaxPlayers)
		{
			return EDigetherJoinDecision::RejectFull;
		}
		return EDigetherJoinDecision::AcceptNew;
	}

	bool IsAccepted(EDigetherJoinDecision Decision)
	{
		return Decision == EDigetherJoinDecision::AcceptNew || Decision == EDigetherJoinDecision::AcceptReconnect;
	}

	EDigetherSessionText GetRejectionText(EDigetherJoinDecision Decision, const FDigetherJoinCheck& Check)
	{
		if (Decision == EDigetherJoinDecision::RejectFull)
		{
			return EDigetherSessionText::RoomFull;
		}
		if (Decision == EDigetherJoinDecision::RejectInvalidId && Check.bRequireSteamId)
		{
			return EDigetherSessionText::SteamIdRequired;
		}
		// ST_Session 키 추가 필요: 무효 ID·중복 연결·탐사 중 신규 참가의 전용 거절 문구.
		return EDigetherSessionText::JoinFailed;
	}

	EDigetherInviteDecision EvaluateInvite(const FDigetherInviteCheck& Check)
	{
		if (Check.bBusy)
		{
			return EDigetherInviteDecision::RejectBusy;
		}
		if (!Check.bOnlineAvailable)
		{
			return EDigetherInviteDecision::RejectOnlineUnavailable;
		}
		if (!Check.bLocalController || !Check.bSuccess || !Check.bUserIdValid || !Check.bProtocolMatches)
		{
			return EDigetherInviteDecision::RejectInvalidInvite;
		}
		if (Check.bAlreadyInSession)
		{
			return EDigetherInviteDecision::RejectAlreadyInSession;
		}
		return EDigetherInviteDecision::Accept;
	}
}
