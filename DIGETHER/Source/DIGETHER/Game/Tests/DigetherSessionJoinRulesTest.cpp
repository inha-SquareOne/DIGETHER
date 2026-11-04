#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Online/DigetherJoinRules.h"
#include "Game/Online/DigetherSessionText.h"
#include "Misc/AutomationTest.h"

namespace
{
	// 입력으로 주입하는 정원 경계 시험값이다.
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
	TestTrue(TEXT("중복 연결 거절은 재접속·잠금·정원보다 우선"), Evaluate(Check) == EDigetherJoinDecision::RejectDuplicate);

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
	TestTrue(TEXT("Steam ID 요구 거절은 SteamIdRequired 키"), GetRejectionText(EDigetherJoinDecision::RejectInvalidId, Check) == EDigetherSessionText::SteamIdRequired);
	TestTrue(TEXT("중복 연결 거절은 JoinFailed 키"), GetRejectionText(EDigetherJoinDecision::RejectDuplicate, Check) == EDigetherSessionText::JoinFailed);
	TestTrue(TEXT("잠금 거절은 JoinFailed 키"), GetRejectionText(EDigetherJoinDecision::RejectInProgress, Check) == EDigetherSessionText::JoinFailed);

	Check.bRequireSteamId = false;
	TestTrue(TEXT("일반 무효 ID 거절은 JoinFailed 키"), GetRejectionText(EDigetherJoinDecision::RejectInvalidId, Check) == EDigetherSessionText::JoinFailed);
	TestTrue(TEXT("Steam 요구와 무관하게 정원 거절은 RoomFull 키"), GetRejectionText(EDigetherJoinDecision::RejectFull, Check) == EDigetherSessionText::RoomFull);
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
