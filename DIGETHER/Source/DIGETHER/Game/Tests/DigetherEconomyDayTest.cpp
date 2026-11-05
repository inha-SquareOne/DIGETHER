#if WITH_DEV_AUTOMATION_TESTS

// 아래 숫자는 테스트 픽스처이며 최종 기획 값이 아니다.

#include "Game/Data/DigetherTuningProfile.h"
#include "Game/Economy/DigetherEconomyRules.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherEconomyTotalDayRulesTest,
	"DIGETHER.Game.Economy.Day.TotalAndPeriodDay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherEconomyTotalDayRulesTest::RunTest(const FString& Parameters)
{
	// 기획 1.1.0 02 개요 5행·공유 사양 "탐사·기간"·"일차 증가".
	FDigetherRunTuning Tuning;
	Tuning.InitialQuotaCredits = 100;
	const FDigetherQuotaState Initial = FDigetherEconomyRules::MakeInitialQuotaState(Tuning);
	TestEqual(TEXT("새 게임은 기간 1일차"), Initial.Day, 1);
	TestEqual(TEXT("새 게임은 전체 1일차"), Initial.TotalDay, 1);

	// 탐사 종료 확정 한 번에 두 일차가 각각 하루 오른다.
	const FDigetherDayAdvanceResult Second = FDigetherEconomyRules::AdvanceDay(Initial);
	TestEqual(TEXT("탐사 종료 확정 성공"), Second.Result, EDigetherEconomyResult::Success);
	TestEqual(TEXT("기간 일차 +1"), Second.State.Day, 2);
	TestEqual(TEXT("전체 일차 +1"), Second.State.TotalDay, 2);
	TestEqual(TEXT("입력 값은 바꾸지 않음"), Initial.TotalDay, 1);

	// 목표 달성으로 새 기간이 된 뒤(기간 1일차, 전체 4일차)의 탐사를 마치면 전체 5일차·기간 2일차다(02 개요 5행 예).
	FDigetherQuotaState NewPeriod = Initial;
	NewPeriod.Day = 1;
	NewPeriod.TotalDay = 4;
	NewPeriod.PeriodGeneration = 1;
	const FDigetherDayAdvanceResult AfterNewPeriod = FDigetherEconomyRules::AdvanceDay(NewPeriod);
	TestEqual(TEXT("새 기간 첫 탐사 뒤 기간 2일차"), AfterNewPeriod.State.Day, 2);
	TestEqual(TEXT("새 기간 첫 탐사 뒤 전체 5일차"), AfterNewPeriod.State.TotalDay, 5);
	TestEqual(TEXT("기간 세대는 일차 증가로 바뀌지 않음"), AfterNewPeriod.State.PeriodGeneration, 1);

	// 4일차 탐사가 끝나면 마감 다음 날(기간 5일차)로 넘어가고 전체 일차도 하루 오른다.
	FDigetherQuotaState Deadline = Initial;
	Deadline.Day = 4;
	Deadline.TotalDay = 7;
	const FDigetherDayAdvanceResult AfterDeadline = FDigetherEconomyRules::AdvanceDay(Deadline);
	TestEqual(TEXT("마감 다음 날 기간 5일차"), AfterDeadline.State.Day, 5);
	TestEqual(TEXT("마감 다음 날 전체 일차 +1"), AfterDeadline.State.TotalDay, 8);

	// 초기화 전(기간 일차 0)은 거절하고 입력을 그대로 돌려준다.
	FDigetherQuotaState Uninitialized;
	Uninitialized.TotalDay = 3;
	const FDigetherDayAdvanceResult Rejected = FDigetherEconomyRules::AdvanceDay(Uninitialized);
	TestEqual(TEXT("초기화 전 일차 증가 거절"), Rejected.Result, EDigetherEconomyResult::InvalidInput);
	TestEqual(TEXT("거절 시 전체 일차 유지"), Rejected.State.TotalDay, 3);

	// 표현 상한에서는 포화한다(게임 규칙 상한이 아니다).
	FDigetherQuotaState Saturated = Initial;
	Saturated.TotalDay = MAX_int32;
	TestEqual(TEXT("전체 일차 포화"), FDigetherEconomyRules::AdvanceDay(Saturated).State.TotalDay, MAX_int32);

	FDigetherQuotaState Negative = Initial;
	Negative.TotalDay = -1;
	TestFalse(TEXT("음수 전체 일차는 잘못된 값"), FDigetherEconomyRules::IsValidQuotaState(Negative));
	TestEqual(TEXT("음수 전체 일차는 일차 증가 거절"), FDigetherEconomyRules::AdvanceDay(Negative).Result,
		EDigetherEconomyResult::InvalidInput);
	return true;
}

#endif
