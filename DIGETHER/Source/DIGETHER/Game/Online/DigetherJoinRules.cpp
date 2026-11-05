#include "Game/Online/DigetherJoinRules.h"

#include "Game/Online/DigetherSessionText.h"
#include "Math/UnrealMathUtility.h"

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
			// 기획 C2(2026-11-04 확정): 이전 접속을 끊고 새 접속 수락, 호스트 자신의 연결은 유지
			return Check.bDuplicateIsLocal ? EDigetherJoinDecision::RejectDuplicate : EDigetherJoinDecision::ReplacePrevious;
		}
		if (Check.bIdInRoster)
		{
			return EDigetherJoinDecision::AcceptReconnect;
		}
		// 탐사 출발 후 끊긴 참가자 자리는 예약, 재접속은 같은 자리를 쓰므로 위에서 수락
		const bool bSeatAvailable = static_cast<int64>(Check.ConnectedPlayers) + Check.ReservedPlayers < Check.MaxPlayers;
		if (Check.bNewJoinLocked)
		{
			// S01: 수락은 게이지 완료 전·확정은 완료 후인 참가자와 거점에서 떠난 기존 참가자는 빈자리가 있으면 관전 합류
			// 완전 신규 참가는 잠금 거절(③)이 정원 거절보다 우선
			if (Check.bAcceptedBeforeLock || Check.bHasParticipantRecord)
			{
				return bSeatAvailable ? EDigetherJoinDecision::AcceptSpectator : EDigetherJoinDecision::RejectFull;
			}
			return EDigetherJoinDecision::RejectInProgress;
		}
		if (!bSeatAvailable)
		{
			return EDigetherJoinDecision::RejectFull;
		}
		return EDigetherJoinDecision::AcceptNew;
	}

	bool IsAccepted(EDigetherJoinDecision Decision)
	{
		return Decision == EDigetherJoinDecision::AcceptNew || Decision == EDigetherJoinDecision::AcceptReconnect
			|| Decision == EDigetherJoinDecision::AcceptSpectator;
	}

	EDigetherSessionText GetRejectionText(EDigetherJoinDecision Decision, const FDigetherJoinCheck& Check)
	{
		switch (Decision)
		{
		case EDigetherJoinDecision::RejectInvalidId:
			// ID는 있으나 Steam ID가 아니면 Steam 실행 안내, ID 자체가 없으면 계정 확인 실패 안내
			return Check.bIdValid && Check.bRequireSteamId ? EDigetherSessionText::SteamIdRequired : EDigetherSessionText::AccountCheckFailed;
		case EDigetherJoinDecision::RejectDuplicate:
		case EDigetherJoinDecision::ReplacePrevious:
			// 호스트 자신과 같은 ID, 또는 교체 대기를 끝내지 못한 경우(제한 시간·기다릴 수 없는 동기 경로)
			return EDigetherSessionText::AccountAlreadyConnected;
		case EDigetherJoinDecision::RejectInProgress:
			return EDigetherSessionText::RunInProgress;
		case EDigetherJoinDecision::RejectFull:
			return EDigetherSessionText::RoomFull;
		default:
			// 수락 결과가 잘못 넘어온 경우, 호출자는 거절 결과에만 호출
			return EDigetherSessionText::JoinFailed;
		}
	}

	bool ShouldDisconnectPreviousConnection(EDigetherJoinDecision Decision)
	{
		return Decision == EDigetherJoinDecision::ReplacePrevious;
	}

	EDigetherReplacementWait EvaluateReplacementWait(const bool bPreviousReleased, const double ElapsedSeconds,
		const double TimeoutSeconds)
	{
		if (bPreviousReleased)
		{
			return EDigetherReplacementWait::Ready;
		}
		// NaN·무한대처럼 비교할 수 없는 시간도 무한 대기 대신 시간 초과(IsFinite는 비트 검사라 빠른 부동소수 설정에서도 유지)
		if (!FMath::IsFinite(ElapsedSeconds) || ElapsedSeconds >= TimeoutSeconds)
		{
			return EDigetherReplacementWait::TimedOut;
		}
		return EDigetherReplacementWait::Waiting;
	}

	namespace
	{
		// 거절 사유 키 앞의 고정 표지, 사람이 읽는 문구와 겹치지 않게
		const TCHAR* const RejectionMessagePrefix = TEXT("DigetherJoinRejected:");

		// 원격으로 받는 키는 GetRejectionText 반환값뿐, 인자 자리가 있는 문구 제외
		constexpr EDigetherSessionText RemoteRejectionTexts[] = {
			EDigetherSessionText::AccountCheckFailed,
			EDigetherSessionText::SteamIdRequired,
			EDigetherSessionText::AccountAlreadyConnected,
			EDigetherSessionText::RunInProgress,
			EDigetherSessionText::RoomFull,
			EDigetherSessionText::JoinFailed,
		};

		bool IsRemoteRejectionText(EDigetherSessionText Text)
		{
			for (const EDigetherSessionText Allowed : RemoteRejectionTexts)
			{
				if (Allowed == Text)
				{
					return true;
				}
			}
			return false;
		}
	}

	FString MakeRejectionMessage(EDigetherSessionText Text)
	{
		const EDigetherSessionText Sent = IsRemoteRejectionText(Text) ? Text : EDigetherSessionText::JoinFailed;
		return FString(RejectionMessagePrefix) + DigetherSessionText::GetKey(Sent);
	}

	bool TryParseRejectionMessage(const FString& Message, EDigetherSessionText& OutText)
	{
		if (!Message.StartsWith(RejectionMessagePrefix, ESearchCase::CaseSensitive))
		{
			return false;
		}
		const FString Key = Message.RightChop(FCString::Strlen(RejectionMessagePrefix));
		for (const EDigetherSessionText Allowed : RemoteRejectionTexts)
		{
			if (DigetherSessionText::GetKey(Allowed) == Key)
			{
				OutText = Allowed;
				return true;
			}
		}
		// 표지는 맞지만 이 빌드가 모르는 키(호스트가 더 새 빌드), 공통 실패 문구로 표시
		OutText = EDigetherSessionText::JoinFailed;
		return true;
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
