#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Online/DigetherJoinRules.h"
#include "Game/Online/DigetherLoginReplacementQueue.h"
#include "Game/Online/DigetherSessionText.h"
#include "Misc/AutomationTest.h"
#include "OnlineSubsystemTypes.h"

#include <limits>

namespace
{
	// 입력으로 주입하는 정원 경계 시험값
	constexpr int32 JoinRulesFixtureMaxPlayers = 4;

	DigetherJoinRules::FDigetherJoinCheck MakeValidJoinRulesCheck()
	{
		DigetherJoinRules::FDigetherJoinCheck Check;
		Check.bIdValid = true;
		Check.bRequireSteamId = true;
		Check.bIdIsSteam = true;
		Check.MaxPlayers = JoinRulesFixtureMaxPlayers;
		return Check;
	}

	DigetherJoinRules::FDigetherInviteCheck MakeValidInviteRulesCheck()
	{
		DigetherJoinRules::FDigetherInviteCheck Check;
		Check.bOnlineAvailable = true;
		Check.bLocalController = true;
		Check.bSuccess = true;
		Check.bUserIdValid = true;
		Check.bProtocolMatches = true;
		return Check;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSessionJoinCapacityTest,
	"DIGETHER.Game.Online.JoinRules.NewJoinCapacity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSessionJoinCapacityTest::RunTest(const FString& Parameters)
{
	using namespace DigetherJoinRules;

	FDigetherJoinCheck Check = MakeValidJoinRulesCheck();
	TestTrue(TEXT("빈 방의 첫 참가 허용"), Evaluate(Check) == EDigetherJoinDecision::AcceptNew);

	Check.ConnectedPlayers = 1;
	TestTrue(TEXT("호스트 한 명인 방의 신규 참가 허용"), Evaluate(Check) == EDigetherJoinDecision::AcceptNew);

	Check.ConnectedPlayers = JoinRulesFixtureMaxPlayers - 1;
	TestTrue(TEXT("3명 접속 중 마지막 자리의 신규 참가 허용"), Evaluate(Check) == EDigetherJoinDecision::AcceptNew);

	Check.ConnectedPlayers = JoinRulesFixtureMaxPlayers;
	TestTrue(TEXT("4명 정원에 도달하면 신규 참가 거절"), Evaluate(Check) == EDigetherJoinDecision::RejectFull);

	Check.MaxPlayers = 1;
	Check.ConnectedPlayers = 0;
	TestTrue(TEXT("정원 1명인 빈 방의 첫 참가 허용"), Evaluate(Check) == EDigetherJoinDecision::AcceptNew);
	Check.ConnectedPlayers = 1;
	TestTrue(TEXT("정원 1명인 방에 한 명이 있으면 신규 참가 거절"), Evaluate(Check) == EDigetherJoinDecision::RejectFull);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSessionJoinLockTest,
	"DIGETHER.Game.Online.JoinRules.NewJoinLock", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSessionJoinLockTest::RunTest(const FString& Parameters)
{
	using namespace DigetherJoinRules;

	FDigetherJoinCheck Check = MakeValidJoinRulesCheck();
	Check.bNewJoinLocked = true;
	TestTrue(TEXT("빈자리가 있어도 잠긴 방의 신규 참가 거절"), Evaluate(Check) == EDigetherJoinDecision::RejectInProgress);

	Check.ConnectedPlayers = JoinRulesFixtureMaxPlayers;
	TestTrue(TEXT("잠금 거절은 정원 거절보다 우선"), Evaluate(Check) == EDigetherJoinDecision::RejectInProgress);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSessionReconnectTest,
	"DIGETHER.Game.Online.JoinRules.Reconnect", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSessionReconnectTest::RunTest(const FString& Parameters)
{
	using namespace DigetherJoinRules;

	FDigetherJoinCheck Check = MakeValidJoinRulesCheck();
	Check.bIdInRoster = true;
	TestTrue(TEXT("참가 기록이 있는 ID는 재접속으로 분류"), Evaluate(Check) == EDigetherJoinDecision::AcceptReconnect);

	Check.bNewJoinLocked = true;
	TestTrue(TEXT("잠금 상태에도 재접속 허용"), Evaluate(Check) == EDigetherJoinDecision::AcceptReconnect);

	Check.bNewJoinLocked = false;
	Check.ConnectedPlayers = JoinRulesFixtureMaxPlayers;
	TestTrue(TEXT("정원 상태에도 재접속 허용"), Evaluate(Check) == EDigetherJoinDecision::AcceptReconnect);

	Check.bNewJoinLocked = true;
	TestTrue(TEXT("잠금과 정원이 동시에 적용되어도 재접속 허용"), Evaluate(Check) == EDigetherJoinDecision::AcceptReconnect);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSessionJoinIdentityPriorityTest,
	"DIGETHER.Game.Online.JoinRules.IdentityAndDuplicatePriority", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSessionJoinIdentityPriorityTest::RunTest(const FString& Parameters)
{
	using namespace DigetherJoinRules;

	FDigetherJoinCheck Check = MakeValidJoinRulesCheck();
	Check.bIdAlreadyConnected = true;
	Check.bIdInRoster = true;
	Check.bNewJoinLocked = true;
	Check.ConnectedPlayers = JoinRulesFixtureMaxPlayers;
	// 기획 C2: 같은 계정은 이전 접속을 끊고 수락, 첫 시도 결과는 거절이 아닌 교체 대기
	TestTrue(TEXT("중복 연결 교체 대기는 재접속·잠금·정원보다 우선"), Evaluate(Check) == EDigetherJoinDecision::ReplacePrevious);
	Check.bDuplicateIsLocal = true;
	TestTrue(TEXT("호스트 자신과 같은 ID는 끊지 않고 중복 거절"), Evaluate(Check) == EDigetherJoinDecision::RejectDuplicate);

	Check.bIdValid = false;
	TestTrue(TEXT("무효 ID 거절은 중복·재접속·잠금·정원보다 우선"), Evaluate(Check) == EDigetherJoinDecision::RejectInvalidId);

	Check = MakeValidJoinRulesCheck();
	Check.bIdIsSteam = false;
	TestTrue(TEXT("Steam ID가 필요한 방은 비 Steam ID를 거절"), Evaluate(Check) == EDigetherJoinDecision::RejectInvalidId);

	Check.bIdInRoster = true;
	TestTrue(TEXT("Steam ID 요구는 참가 기록보다 우선"), Evaluate(Check) == EDigetherJoinDecision::RejectInvalidId);

	Check.bIdInRoster = false;
	Check.bRequireSteamId = false;
	TestTrue(TEXT("Steam ID가 불필요하면 유효한 비 Steam ID 허용"), Evaluate(Check) == EDigetherJoinDecision::AcceptNew);

	Check.bIdValid = false;
	TestTrue(TEXT("Steam ID가 불필요해도 무효 ID는 거절"), Evaluate(Check) == EDigetherJoinDecision::RejectInvalidId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSessionJoinAcceptedResultsTest,
	"DIGETHER.Game.Online.JoinRules.AcceptedResults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSessionJoinAcceptedResultsTest::RunTest(const FString& Parameters)
{
	using namespace DigetherJoinRules;

	TestTrue(TEXT("신규 참가 결과는 수락"), IsAccepted(EDigetherJoinDecision::AcceptNew));
	TestTrue(TEXT("재접속 결과는 수락"), IsAccepted(EDigetherJoinDecision::AcceptReconnect));
	TestTrue(TEXT("관전 합류 결과는 수락"), IsAccepted(EDigetherJoinDecision::AcceptSpectator));
	TestFalse(TEXT("교체 대기는 그 자체로 수락이 아님"), IsAccepted(EDigetherJoinDecision::ReplacePrevious));
	TestFalse(TEXT("무효 ID 결과는 거절"), IsAccepted(EDigetherJoinDecision::RejectInvalidId));
	TestFalse(TEXT("중복 연결 결과는 거절"), IsAccepted(EDigetherJoinDecision::RejectDuplicate));
	TestFalse(TEXT("잠금 결과는 거절"), IsAccepted(EDigetherJoinDecision::RejectInProgress));
	TestFalse(TEXT("정원 결과는 거절"), IsAccepted(EDigetherJoinDecision::RejectFull));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSessionJoinRejectionTextTest,
	"DIGETHER.Game.Online.JoinRules.RejectionText", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSessionJoinRejectionTextTest::RunTest(const FString& Parameters)
{
	using namespace DigetherJoinRules;

	FDigetherJoinCheck Check = MakeValidJoinRulesCheck();
	TestTrue(TEXT("정원 거절은 RoomFull 키"), GetRejectionText(EDigetherJoinDecision::RejectFull, Check) == EDigetherSessionText::RoomFull);
	TestTrue(TEXT("중복 연결 거절은 AccountAlreadyConnected 키"), GetRejectionText(EDigetherJoinDecision::RejectDuplicate, Check) == EDigetherSessionText::AccountAlreadyConnected);
	TestTrue(TEXT("교체를 끝내지 못한 거절도 AccountAlreadyConnected 키"), GetRejectionText(EDigetherJoinDecision::ReplacePrevious, Check) == EDigetherSessionText::AccountAlreadyConnected);
	TestTrue(TEXT("잠금 거절은 RunInProgress 키"), GetRejectionText(EDigetherJoinDecision::RejectInProgress, Check) == EDigetherSessionText::RunInProgress);
	TestTrue(TEXT("수락 결과를 잘못 넘기면 JoinFailed 키"), GetRejectionText(EDigetherJoinDecision::AcceptNew, Check) == EDigetherSessionText::JoinFailed);

	Check.bIdIsSteam = false;
	TestTrue(TEXT("Steam 방의 비 Steam ID 거절은 SteamIdRequired 키"), Evaluate(Check) == EDigetherJoinDecision::RejectInvalidId
		&& GetRejectionText(EDigetherJoinDecision::RejectInvalidId, Check) == EDigetherSessionText::SteamIdRequired);

	Check.bIdValid = false;
	TestTrue(TEXT("Steam 방의 ID 없음 거절은 AccountCheckFailed 키"), Evaluate(Check) == EDigetherJoinDecision::RejectInvalidId
		&& GetRejectionText(EDigetherJoinDecision::RejectInvalidId, Check) == EDigetherSessionText::AccountCheckFailed);

	Check.bRequireSteamId = false;
	TestTrue(TEXT("LAN 방의 ID 없음 거절은 AccountCheckFailed 키"), Evaluate(Check) == EDigetherJoinDecision::RejectInvalidId
		&& GetRejectionText(EDigetherJoinDecision::RejectInvalidId, Check) == EDigetherSessionText::AccountCheckFailed);
	TestTrue(TEXT("Steam 요구와 무관하게 정원 거절은 RoomFull 키"), GetRejectionText(EDigetherJoinDecision::RejectFull, Check) == EDigetherSessionText::RoomFull);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSessionJoinRejectionMessageTest,
	"DIGETHER.Game.Online.JoinRules.RejectionMessage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSessionJoinRejectionMessageTest::RunTest(const FString& Parameters)
{
	using namespace DigetherJoinRules;

	static const EDigetherSessionText RejectionTexts[] = {
		EDigetherSessionText::AccountCheckFailed,
		EDigetherSessionText::SteamIdRequired,
		EDigetherSessionText::AccountAlreadyConnected,
		EDigetherSessionText::RunInProgress,
		EDigetherSessionText::RoomFull,
		EDigetherSessionText::JoinFailed,
	};
	for (const EDigetherSessionText Text : RejectionTexts)
	{
		const FString Message = MakeRejectionMessage(Text);
		EDigetherSessionText Parsed = EDigetherSessionText::Busy;
		TestTrue(FString::Printf(TEXT("%s 사유 키 왕복"), *DigetherSessionText::GetKey(Text)), TryParseRejectionMessage(Message, Parsed) && Parsed == Text);
	}

	EDigetherSessionText Parsed = EDigetherSessionText::Busy;
	TestFalse(TEXT("엔진 기본 거절 문구는 사유 키가 아님"), TryParseRejectionMessage(TEXT("Server full."), Parsed));
	TestFalse(TEXT("빈 문자열은 사유 키가 아님"), TryParseRejectionMessage(FString(), Parsed));
	TestFalse(TEXT("사유 키 이름만 있는 문자열은 사유 키가 아님"), TryParseRejectionMessage(DigetherSessionText::GetKey(EDigetherSessionText::RoomFull), Parsed));
	TestTrue(TEXT("판정을 못 한 문자열은 결과를 바꾸지 않음"), Parsed == EDigetherSessionText::Busy);

	// 인자 자리가 있거나 거절 사유가 아닌 키는 원격 키로 전송 금지
	TestTrue(TEXT("거절 사유가 아닌 키는 JoinFailed로 보냄"),
		MakeRejectionMessage(EDigetherSessionText::ConnectionFailed) == MakeRejectionMessage(EDigetherSessionText::JoinFailed));

	const FString Prefix = MakeRejectionMessage(EDigetherSessionText::RoomFull).LeftChop(DigetherSessionText::GetKey(EDigetherSessionText::RoomFull).Len());
	TestTrue(TEXT("표지는 같지만 모르는 키는 JoinFailed"), TryParseRejectionMessage(Prefix + TEXT("UnknownFutureKey"), Parsed) && Parsed == EDigetherSessionText::JoinFailed);
	TestTrue(TEXT("표지는 같지만 원격 허용 목록 밖의 키는 JoinFailed"),
		TryParseRejectionMessage(Prefix + DigetherSessionText::GetKey(EDigetherSessionText::ConnectionFailed), Parsed) && Parsed == EDigetherSessionText::JoinFailed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSessionLobbyDuplicateReplaceTest,
	"DIGETHER.Game.Online.JoinRules.LobbyDuplicateReplace", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSessionLobbyDuplicateReplaceTest::RunTest(const FString& Parameters)
{
	using namespace DigetherJoinRules;

	TestTrue(TEXT("교체 대기는 이전 연결을 끊음"), ShouldDisconnectPreviousConnection(EDigetherJoinDecision::ReplacePrevious));

	static const EDigetherJoinDecision OtherDecisions[] = {
		EDigetherJoinDecision::AcceptNew,
		EDigetherJoinDecision::AcceptReconnect,
		EDigetherJoinDecision::AcceptSpectator,
		EDigetherJoinDecision::RejectInvalidId,
		EDigetherJoinDecision::RejectDuplicate,
		EDigetherJoinDecision::RejectInProgress,
		EDigetherJoinDecision::RejectFull,
	};
	for (const EDigetherJoinDecision Decision : OtherDecisions)
	{
		TestFalse(FString::Printf(TEXT("교체 대기가 아닌 결과(%d)는 이전 연결을 끊지 않음"), static_cast<int32>(Decision)), ShouldDisconnectPreviousConnection(Decision));
	}

	// 중복 판정이 정원·잠금보다 먼저라 정원이 찬 대기실에서도 같은 계정 이전 연결을 끊고 교체
	FDigetherJoinCheck Check = MakeValidJoinRulesCheck();
	Check.bIdAlreadyConnected = true;
	Check.ConnectedPlayers = JoinRulesFixtureMaxPlayers;
	TestTrue(TEXT("정원이 찬 대기실의 중복 접속도 이전 연결을 끊음"), ShouldDisconnectPreviousConnection(Evaluate(Check)));

	// 호스트 자신과 같은 ID는 끊지 않고 ②로 거절
	Check.bDuplicateIsLocal = true;
	TestFalse(TEXT("이전 연결이 호스트 자신이면 끊지 않음"), ShouldDisconnectPreviousConnection(Evaluate(Check)));
	TestTrue(TEXT("호스트 자신과 같은 ID는 ② 사유"), GetRejectionText(Evaluate(Check), Check) == EDigetherSessionText::AccountAlreadyConnected);

	// 정리 뒤 재판정: 대기실은 PlayerState가 사라져 빈자리 하나인 신규 참가
	Check.bDuplicateIsLocal = false;
	Check.bIdAlreadyConnected = false;
	Check.ConnectedPlayers = JoinRulesFixtureMaxPlayers - 1;
	TestTrue(TEXT("이전 연결 정리 뒤 대기실은 새 PlayerState로 수락"), Evaluate(Check) == EDigetherJoinDecision::AcceptNew);

	// 무효 ID는 중복 판정 전 거절, 연결 유지
	Check.bIdAlreadyConnected = true;
	Check.bIdValid = false;
	TestFalse(TEXT("무효 ID 접속은 이전 연결을 끊지 않음"), ShouldDisconnectPreviousConnection(Evaluate(Check)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSessionGameplayDuplicateRetryTest,
	"DIGETHER.Game.Online.JoinRules.GameplayDuplicateReplaceThenRejoin", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSessionGameplayDuplicateRetryTest::RunTest(const FString& Parameters)
{
	using namespace DigetherJoinRules;

	for (const bool bNewJoinLocked : { false, true })
	{
		const TCHAR* const Phase = bNewJoinLocked ? TEXT("탐사 중") : TEXT("거점");
		// 첫 판정: 같은 ID 이전 연결이 아직 활성, 정원이 차고 잠겨도 교체 대기 우선(C2)
		FDigetherJoinCheck Check = MakeValidJoinRulesCheck();
		Check.bNewJoinLocked = bNewJoinLocked;
		Check.bIdAlreadyConnected = true;
		Check.bHasParticipantRecord = true;
		Check.ConnectedPlayers = JoinRulesFixtureMaxPlayers;
		const EDigetherJoinDecision First = Evaluate(Check);
		TestTrue(FString::Printf(TEXT("%s 활성 중복의 첫 판정은 교체 대기"), Phase), First == EDigetherJoinDecision::ReplacePrevious);
		TestTrue(FString::Printf(TEXT("%s 교체 대기는 이전 연결을 끊음"), Phase), ShouldDisconnectPreviousConnection(First));
		TestTrue(FString::Printf(TEXT("%s 교체를 끝내지 못하면 ② 사유"), Phase),
			GetRejectionText(First, Check) == EDigetherSessionText::AccountAlreadyConnected);

		// 같은 시도 안 재판정: 탐사 중이면 이전 연결이 비활성 PlayerState(예약 자리)가 되어 재접속,
		// 거점이면 자리를 반환한 기존 참가자라 빈자리 검사 뒤 참가(S01)
		Check.bIdAlreadyConnected = false;
		Check.bIdInRoster = bNewJoinLocked;
		Check.ConnectedPlayers = JoinRulesFixtureMaxPlayers - 1;
		Check.ReservedPlayers = bNewJoinLocked ? 1 : 0;
		const EDigetherJoinDecision Retry = Evaluate(Check);
		TestTrue(FString::Printf(TEXT("%s 정리 뒤 재판정은 수락"), Phase), IsAccepted(Retry));
		TestTrue(FString::Printf(TEXT("%s 정리 뒤 재판정 종류"), Phase),
			Retry == (bNewJoinLocked ? EDigetherJoinDecision::AcceptReconnect : EDigetherJoinDecision::AcceptNew));
		TestFalse(FString::Printf(TEXT("%s 재판정 수락은 다른 연결을 끊지 않음"), Phase), ShouldDisconnectPreviousConnection(Retry));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSessionSpectatorJoinTest,
	"DIGETHER.Game.Online.JoinRules.SpectatorJoinDuringLock", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSessionSpectatorJoinTest::RunTest(const FString& Parameters)
{
	using namespace DigetherJoinRules;

	// 수락은 게이지 완료 전, 참가 확정(Login)은 잠금 중: 빈자리가 있으면 관전 합류(T76 첫 경우)
	FDigetherJoinCheck Check = MakeValidJoinRulesCheck();
	Check.bNewJoinLocked = true;
	Check.ConnectedPlayers = 2;
	Check.bAcceptedBeforeLock = true;
	TestTrue(TEXT("수락 후 게이지 완료 뒤 확정은 관전 합류"), Evaluate(Check) == EDigetherJoinDecision::AcceptSpectator);

	// 관전 합류 빈자리는 탐사 출발 후 끊긴 참가자의 예약 자리 제외 계산
	Check.ReservedPlayers = 2;
	TestTrue(TEXT("예약 자리까지 차면 관전 합류도 정원 거절"), Evaluate(Check) == EDigetherJoinDecision::RejectFull);
	Check.ReservedPlayers = 1;
	TestTrue(TEXT("예약 자리를 빼고 남은 자리가 있으면 관전 합류"), Evaluate(Check) == EDigetherJoinDecision::AcceptSpectator);

	// 거점에서 떠났던 기존 참가자가 탐사 중 돌아오면 관전(저장 명단 기록)
	Check = MakeValidJoinRulesCheck();
	Check.bNewJoinLocked = true;
	Check.ConnectedPlayers = 2;
	Check.bHasParticipantRecord = true;
	TestTrue(TEXT("로비 이탈자의 탐사 중 복귀는 관전 합류"), Evaluate(Check) == EDigetherJoinDecision::AcceptSpectator);
	Check.ConnectedPlayers = JoinRulesFixtureMaxPlayers;
	TestTrue(TEXT("반환한 자리를 다른 사람이 채웠으면 기존 참가자도 정원 거절(T77)"), Evaluate(Check) == EDigetherJoinDecision::RejectFull);

	// 완전 신규 참가와 게이지 완료 후 수락(PreLogin 자체가 잠금 중)은 여전히 ③
	Check = MakeValidJoinRulesCheck();
	Check.bNewJoinLocked = true;
	TestTrue(TEXT("잠금 중 신규 참가는 ③ 거절"), Evaluate(Check) == EDigetherJoinDecision::RejectInProgress);
	TestTrue(TEXT("③ 사유 키"), GetRejectionText(Evaluate(Check), Check) == EDigetherSessionText::RunInProgress);

	// 잠금이 없으면 기록과 무관하게 일반 참가(빈자리 검사)
	Check.bNewJoinLocked = false;
	Check.bHasParticipantRecord = true;
	Check.bAcceptedBeforeLock = true;
	TestTrue(TEXT("잠금이 없으면 일반 참가"), Evaluate(Check) == EDigetherJoinDecision::AcceptNew);

	// 이번 탐사 예약 참가자는 관전이 아닌 같은 자리 재접속
	Check.bNewJoinLocked = true;
	Check.bIdInRoster = true;
	Check.ConnectedPlayers = JoinRulesFixtureMaxPlayers;
	TestTrue(TEXT("탐사 출발 후 끊긴 참가자는 관전이 아닌 재접속"), Evaluate(Check) == EDigetherJoinDecision::AcceptReconnect);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSessionReplacementWaitTest,
	"DIGETHER.Game.Online.JoinRules.ReplacementWait", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSessionReplacementWaitTest::RunTest(const FString& Parameters)
{
	using namespace DigetherJoinRules;

	TestTrue(TEXT("정리 전·제한 시간 전은 대기"), EvaluateReplacementWait(false, 0.0, LoginReplacementTimeoutSeconds) == EDigetherReplacementWait::Waiting);
	TestTrue(TEXT("정리되면 승인 판정 준비"), EvaluateReplacementWait(true, 0.0, LoginReplacementTimeoutSeconds) == EDigetherReplacementWait::Ready);
	TestTrue(TEXT("제한 시간 도달은 시간 초과"), EvaluateReplacementWait(false, LoginReplacementTimeoutSeconds, LoginReplacementTimeoutSeconds) == EDigetherReplacementWait::TimedOut);
	TestTrue(TEXT("정리 완료는 시간 초과보다 우선"), EvaluateReplacementWait(true, LoginReplacementTimeoutSeconds * 2.0, LoginReplacementTimeoutSeconds) == EDigetherReplacementWait::Ready);
	TestTrue(TEXT("비교할 수 없는 시간은 무한 대기 대신 시간 초과"), EvaluateReplacementWait(false, std::numeric_limits<double>::quiet_NaN(), LoginReplacementTimeoutSeconds) == EDigetherReplacementWait::TimedOut);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSessionReplacementQueueTest,
	"DIGETHER.Game.Online.JoinRules.ReplacementQueueKeepsEveryCallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSessionReplacementQueueTest::RunTest(const FString& Parameters)
{
	const FUniqueNetIdRepl IdA(FUniqueNetIdRef(FUniqueNetIdString::Create(TEXT("fixture-replace-a"), FName(TEXT("DIGETHER_TEST")))));
	const FUniqueNetIdRepl IdB(FUniqueNetIdRef(FUniqueNetIdString::Create(TEXT("fixture-replace-b"), FName(TEXT("DIGETHER_TEST")))));
	TArray<FString> Completed;
	auto MakeEntry = [&Completed](const FUniqueNetIdRepl& Id, const TCHAR* Tag, const double StartSeconds)
	{
		FDigetherLoginReplacementQueue::FEntry Entry;
		Entry.UniqueId = Id;
		Entry.StartSeconds = StartSeconds;
		const FString Name(Tag);
		Entry.OnComplete = AGameModeBase::FOnPreLoginCompleteDelegate::CreateLambda([&Completed, Name](const FString&)
		{
			Completed.Add(Name);
		});
		return Entry;
	};
	FDigetherLoginReplacementQueue Queue;
	TArray<FDigetherLoginReplacementQueue::FEntry> Superseded;
	Queue.Add(MakeEntry(IdA, TEXT("A1"), 0.0), Superseded);
	Queue.Add(MakeEntry(IdB, TEXT("B1"), 0.0), Superseded);
	TestEqual(TEXT("다른 ID는 서로 밀어내지 않음"), Superseded.Num(), 0);
	Queue.Add(MakeEntry(IdA, TEXT("A2"), 1.0), Superseded);
	if (!TestEqual(TEXT("같은 ID의 앞선 대기는 밀려남"), Superseded.Num(), 1))
	{
		return false;
	}
	Superseded[0].OnComplete.ExecuteIfBound(FString());
	TestTrue(TEXT("밀려난 대기의 콜백 보존"), Completed.Contains(TEXT("A1")));
	TestEqual(TEXT("대기열에는 ID마다 최신 하나"), Queue.Num(), 2);

	TArray<FDigetherLoginReplacementQueue::FEntry> Ready;
	TArray<FDigetherLoginReplacementQueue::FEntry> TimedOut;
	Queue.Collect(2.0, 5.0, [&IdB](const FDigetherLoginReplacementQueue::FEntry& Entry) { return Entry.UniqueId == IdB; },
		Ready, TimedOut);
	TestTrue(TEXT("정리된 ID만 준비"), Ready.Num() == 1 && Ready[0].UniqueId == IdB);
	TestEqual(TEXT("제한 시간 전에는 시간 초과 없음"), TimedOut.Num(), 0);
	Ready.Reset();
	Queue.Collect(6.0, 5.0, [](const FDigetherLoginReplacementQueue::FEntry&) { return false; }, Ready, TimedOut);
	TestTrue(TEXT("남은 대기는 제한 시간 뒤 시간 초과로 꺼냄"), Ready.IsEmpty() && TimedOut.Num() == 1 && TimedOut[0].UniqueId == IdA);
	TestTrue(TEXT("모두 꺼낸 뒤 빈 대기열"), Queue.IsEmpty());

	Queue.Add(MakeEntry(IdB, TEXT("B2"), 0.0), Superseded);
	const TArray<FDigetherLoginReplacementQueue::FEntry> Remaining = Queue.TakeAll();
	TestTrue(TEXT("종료 정리는 남은 대기를 모두 돌려줌"), Remaining.Num() == 1 && Queue.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSessionInviteDecisionsTest,
	"DIGETHER.Game.Online.JoinRules.InviteDecisions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSessionInviteDecisionsTest::RunTest(const FString& Parameters)
{
	using namespace DigetherJoinRules;

	const FDigetherInviteCheck ValidCheck = MakeValidInviteRulesCheck();
	TestTrue(TEXT("모든 초대 조건을 충족하면 수락"), EvaluateInvite(ValidCheck) == EDigetherInviteDecision::Accept);

	FDigetherInviteCheck Check = ValidCheck;
	Check.bBusy = true;
	TestTrue(TEXT("다른 요청 처리 중인 초대 거절"), EvaluateInvite(Check) == EDigetherInviteDecision::RejectBusy);

	Check = ValidCheck;
	Check.bOnlineAvailable = false;
	TestTrue(TEXT("온라인 사용 불가 시 초대 거절"), EvaluateInvite(Check) == EDigetherInviteDecision::RejectOnlineUnavailable);

	Check = ValidCheck;
	Check.bLocalController = false;
	TestTrue(TEXT("로컬 컨트롤러가 아닌 초대 거절"), EvaluateInvite(Check) == EDigetherInviteDecision::RejectInvalidInvite);

	Check = ValidCheck;
	Check.bSuccess = false;
	TestTrue(TEXT("실패한 초대 결과 거절"), EvaluateInvite(Check) == EDigetherInviteDecision::RejectInvalidInvite);

	Check = ValidCheck;
	Check.bUserIdValid = false;
	TestTrue(TEXT("무효 사용자 ID인 초대 거절"), EvaluateInvite(Check) == EDigetherInviteDecision::RejectInvalidInvite);

	Check = ValidCheck;
	Check.bProtocolMatches = false;
	TestTrue(TEXT("프로토콜이 다른 초대 거절"), EvaluateInvite(Check) == EDigetherInviteDecision::RejectInvalidInvite);

	Check = ValidCheck;
	Check.bAlreadyInSession = true;
	TestTrue(TEXT("이미 세션에 있으면 초대 거절"), EvaluateInvite(Check) == EDigetherInviteDecision::RejectAlreadyInSession);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSessionInvitePriorityTest,
	"DIGETHER.Game.Online.JoinRules.InvitePriority", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSessionInvitePriorityTest::RunTest(const FString& Parameters)
{
	using namespace DigetherJoinRules;

	FDigetherInviteCheck Check;
	Check.bBusy = true;
	Check.bAlreadyInSession = true;
	TestTrue(TEXT("Busy 거절은 온라인·초대 유효성·기존 세션보다 우선"), EvaluateInvite(Check) == EDigetherInviteDecision::RejectBusy);

	Check.bBusy = false;
	TestTrue(TEXT("온라인 불가 거절은 초대 유효성·기존 세션보다 우선"), EvaluateInvite(Check) == EDigetherInviteDecision::RejectOnlineUnavailable);

	const FDigetherInviteCheck ValidCheck = MakeValidInviteRulesCheck();
	Check = ValidCheck;
	Check.bAlreadyInSession = true;
	Check.bLocalController = false;
	TestTrue(TEXT("로컬 컨트롤러 검사는 기존 세션보다 우선"), EvaluateInvite(Check) == EDigetherInviteDecision::RejectInvalidInvite);

	Check = ValidCheck;
	Check.bAlreadyInSession = true;
	Check.bSuccess = false;
	TestTrue(TEXT("초대 성공 검사는 기존 세션보다 우선"), EvaluateInvite(Check) == EDigetherInviteDecision::RejectInvalidInvite);

	Check = ValidCheck;
	Check.bAlreadyInSession = true;
	Check.bUserIdValid = false;
	TestTrue(TEXT("사용자 ID 검사는 기존 세션보다 우선"), EvaluateInvite(Check) == EDigetherInviteDecision::RejectInvalidInvite);

	Check = ValidCheck;
	Check.bAlreadyInSession = true;
	Check.bProtocolMatches = false;
	TestTrue(TEXT("프로토콜 검사는 기존 세션보다 우선"), EvaluateInvite(Check) == EDigetherInviteDecision::RejectInvalidInvite);
	return true;
}

#endif
