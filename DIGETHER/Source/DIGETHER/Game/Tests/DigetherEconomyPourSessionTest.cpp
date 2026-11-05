#if WITH_DEV_AUTOMATION_TESTS

// 아래 키·시각·간격은 테스트 픽스처이며 기획 값이 아니다.

#include "Game/Economy/DigetherPourSessionRegistry.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherEconomyPourSessionTest,
	"DIGETHER.Game.Economy.PourSession", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherEconomyPourSessionTest::RunTest(const FString& Parameters)
{
	constexpr double Interval = 0.5;
	const FName StationA(TEXT("StationA"));
	const FName StationB(TEXT("StationB"));
	FDigetherPourSessionRegistry Registry;

	// 시작 거절은 세션을 만들지 않는다.
	TestEqual(TEXT("빈 키 거절"), Registry.BeginPour(FString(), StationA, 0, 0.0, Interval), EDigetherEconomyResult::InvalidInput);
	TestEqual(TEXT("정산기 없음 거절"), Registry.BeginPour(TEXT("A"), NAME_None, 0, 0.0, Interval), EDigetherEconomyResult::InvalidInput);
	TestEqual(TEXT("음수 세대 거절"), Registry.BeginPour(TEXT("A"), StationA, -1, 0.0, Interval), EDigetherEconomyResult::InvalidInput);
	TestEqual(TEXT("간격 0 거절"), Registry.BeginPour(TEXT("A"), StationA, 0, 0.0, 0.0), EDigetherEconomyResult::InvalidInput);
	TestEqual(TEXT("무한 시각 거절"), Registry.BeginPour(TEXT("A"), StationA, 0, TNumericLimits<double>::Max(), Interval),
		EDigetherEconomyResult::InvalidInput);
	TestTrue(TEXT("거절은 세션 없음"), Registry.IsEmpty());

	// 첫 차감은 누른 뒤 한 간격이 지난 시각이다.
	FDigetherPourSession SessionA;
	TestEqual(TEXT("A 시작"), Registry.BeginPour(TEXT("A"), StationA, 3, 1.0, Interval, &SessionA), EDigetherEconomyResult::Success);
	TestEqual(TEXT("첫 주기 = 시작 + 간격"), SessionA.NextTickServerSeconds, 1.5);
	TestEqual(TEXT("시작 세대 기억"), SessionA.PeriodGeneration, 3);
	TestTrue(TEXT("간격 전에는 처리할 주기 없음"), Registry.CollectDueTicks(1.49, Interval, 10).IsEmpty());

	// 같은 정산기·같은 세대의 중복 시작(연타)은 예정 시각·순번을 앞당기지 않는다.
	FDigetherPourSession Repeated;
	TestEqual(TEXT("중복 시작 성공"), Registry.BeginPour(TEXT("A"), StationA, 3, 1.4, Interval, &Repeated), EDigetherEconomyResult::Success);
	TestEqual(TEXT("중복 시작은 예정 시각 유지"), Repeated.NextTickServerSeconds, 1.5);
	TestEqual(TEXT("중복 시작은 순번 유지"), Repeated.StartSequence, SessionA.StartSequence);

	// 같은 예정 시각이면 먼저 시작한 사람이 먼저 반영된다.
	FDigetherPourSession SessionB;
	TestEqual(TEXT("B 같은 정산기 동시 시작"), Registry.BeginPour(TEXT("B"), StationA, 3, 1.0, Interval, &SessionB),
		EDigetherEconomyResult::Success);
	TestTrue(TEXT("B 순번이 더 큼"), SessionB.StartSequence > SessionA.StartSequence);
	FDigetherPourSession SessionC;
	TestEqual(TEXT("C 다른 정산기 더 이른 시작"), Registry.BeginPour(TEXT("C"), StationB, 3, 0.8, Interval, &SessionC),
		EDigetherEconomyResult::Success);
	TArray<FDigetherPourDueTick> Due = Registry.CollectDueTicks(1.5, Interval, 10);
	if (TestEqual(TEXT("세 사람의 첫 주기"), Due.Num(), 3))
	{
		TestEqual(TEXT("예정 시각이 가장 이른 C 먼저"), Due[0].PlayerKey, FString(TEXT("C")));
		TestEqual(TEXT("같은 시각은 먼저 시작한 A"), Due[1].PlayerKey, FString(TEXT("A")));
		TestEqual(TEXT("같은 시각의 나중 시작 B"), Due[2].PlayerKey, FString(TEXT("B")));
	}

	// 처리한 주기만 다음 예정 시각으로 넘긴다.
	TestTrue(TEXT("A 주기 처리 후 예약"), Registry.AdvanceAfterTick(Due[1], Interval));
	TestEqual(TEXT("A 다음 예정"), Registry.Find(TEXT("A"))->NextTickServerSeconds, 2.0);
	FDigetherPourDueTick Stale = Due[2];
	Stale.StartSequence = SessionB.StartSequence + 100;
	TestFalse(TEXT("다른 순번의 오래된 주기는 예약 거절"), Registry.AdvanceAfterTick(Stale, Interval));

	// 긴 프레임에서 놓친 주기는 각각 펼치되 상한까지만 돌려준다.
	FDigetherPourSessionRegistry CatchUp;
	CatchUp.BeginPour(TEXT("A"), StationA, 0, 0.0, Interval);
	const TArray<FDigetherPourDueTick> Many = CatchUp.CollectDueTicks(100.0, Interval, 10);
	TestEqual(TEXT("프레임당 처리 상한 10"), Many.Num(), 10);
	TestEqual(TEXT("첫 주기 시각"), Many[0].ScheduledServerSeconds, 0.5);
	TestEqual(TEXT("열 번째 주기 시각"), Many[9].ScheduledServerSeconds, 5.0);
	TestTrue(TEXT("상한 0은 빈 목록"), CatchUp.CollectDueTicks(100.0, Interval, 0).IsEmpty());

	// 다른 정산기·새 세대로 다시 시작하면 새 세션이다(새 목표에 내려면 다시 눌러야 한다).
	FDigetherPourSession Moved;
	TestEqual(TEXT("A 다른 정산기 시작"), Registry.BeginPour(TEXT("A"), StationB, 3, 3.0, Interval, &Moved), EDigetherEconomyResult::Success);
	TestTrue(TEXT("새 세션 순번"), Moved.StartSequence > SessionC.StartSequence);
	TestEqual(TEXT("새 세션 첫 주기"), Moved.NextTickServerSeconds, 3.5);
	TestFalse(TEXT("이전 세션 주기는 예약 거절"), Registry.AdvanceAfterTick(Due[1], Interval));
	FDigetherPourSession NewPeriod;
	TestEqual(TEXT("새 세대 시작"), Registry.BeginPour(TEXT("A"), StationB, 4, 3.2, Interval, &NewPeriod), EDigetherEconomyResult::Success);
	TestEqual(TEXT("새 세대 기억"), NewPeriod.PeriodGeneration, 4);
	TestEqual(TEXT("새 세대는 다시 한 간격 뒤"), NewPeriod.NextTickServerSeconds, 3.7);
	TestEqual(TEXT("사람마다 세션 하나"), Registry.CaptureSessions().Num(), 3);

	// 멈춤은 본인 세션만 지운다.
	FDigetherPourSession Stopped;
	TestTrue(TEXT("B 멈춤"), Registry.StopPour(TEXT("B"), &Stopped));
	TestEqual(TEXT("멈춘 세션 반환"), Stopped.PlayerKey, FString(TEXT("B")));
	TestFalse(TEXT("없는 세션 멈춤"), Registry.StopPour(TEXT("B")));
	TestTrue(TEXT("A는 유지"), Registry.Find(TEXT("A")) != nullptr);
	const TArray<FDigetherPourSession> All = Registry.StopAll();
	TestEqual(TEXT("전체 멈춤은 남은 세션 반환"), All.Num(), 2);
	TestTrue(TEXT("전체 멈춤 후 비움"), Registry.IsEmpty());
	return true;
}

#endif
