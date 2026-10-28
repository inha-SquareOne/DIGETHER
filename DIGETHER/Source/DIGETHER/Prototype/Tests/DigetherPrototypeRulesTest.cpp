#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Prototype/DigetherPrototypeRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherPrototypeSettlementTest,
	"DIGETHER.Prototype.Settlement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherPrototypeSettlementTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Q01: 4인 중 3인 생존"), DigetherPrototypeRules::GeneralShare(900, 4, 3), 225);
	TestEqual(TEXT("Q02: 내려놓은 물품을 다시 회수"), DigetherPrototypeRules::GeneralShare(1020, 4, 3), 255);
	TestEqual(TEXT("Q03: 긴급 탈출 순서는 몫에 영향 없음"), DigetherPrototypeRules::GeneralShare(900, 4, 3), 225);
	TestEqual(TEXT("Q04: 1인 생존 시 다른 사람 몫 소멸"), DigetherPrototypeRules::GeneralShare(600, 4, 1), 150);
	TestEqual(TEXT("Q05: 전멸 시 유효 지급 없음"), DigetherPrototypeRules::GeneralShare(600, 4, 0), 0);
	TestEqual(TEXT("Q06: 전원 생존"), DigetherPrototypeRules::GeneralShare(900, 4, 4), 225);
	TestEqual(TEXT("Q07: 마지막 편 생존자"), DigetherPrototypeRules::GeneralShare(300, 4, 2), 75);
	TestEqual(TEXT("Q08: 먼저 긴급 탈출한 사람만 생존"), DigetherPrototypeRules::GeneralShare(600, 4, 1), 150);
	TestEqual(TEXT("Q09: 정수 나머지는 소멸"), DigetherPrototypeRules::GeneralShare(100, 3, 3), 33);
	TestEqual(TEXT("출발 인원 0은 지급 없음"), DigetherPrototypeRules::GeneralShare(100, 0, 1), 0);
	TestEqual(TEXT("시체 복구는 내림"), DigetherPrototypeRules::CorpseRefund(101), 50);
	return true;
}

#endif
