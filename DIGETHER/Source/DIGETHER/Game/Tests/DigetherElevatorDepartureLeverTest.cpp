#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Elevator/DigetherElevatorRules.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#include <cmath>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherElevatorDepartureLeverTest,
	"DIGETHER.Game.Elevator.DepartureLever", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherElevatorDepartureLeverTest::RunTest(const FString& Parameters)
{
	using namespace DigetherElevatorRules;
	// 테스트 픽스처이며 최종 기획 출발 홀드 시간이 아니다. 4일차·2배는 보드 확정 계약이다.
	FDigetherElevatorTuning Tuning;
	Tuning.DepartureHoldSeconds = 2.f;
	FDigetherElevatorPullContext Context;
	Context.ElevatorPhase = EDigetherElevatorPhase::HubWaiting;
	Context.DayNumber = 1;
	const FGuid Puller(11, 0, 0, 11);
	const FGuid OtherPuller(12, 0, 0, 12);
	FDigetherElevatorGaugeState State;
	const auto SameState = [](const FDigetherElevatorGaugeState& Left, const FDigetherElevatorGaugeState& Right)
	{
		return FDigetherElevatorGaugeState::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
	};
	TestEqual(TEXT("거점 출발 조건 허용"), CanUseDeparture(Context), EDigetherElevatorLeverResult::Started);
	for (const EDigetherElevatorPhase Phase : { EDigetherElevatorPhase::InitialDescent,
		EDigetherElevatorPhase::BottomWaiting, EDigetherElevatorPhase::Ascending,
		EDigetherElevatorPhase::UpperStop, EDigetherElevatorPhase::ReturnDescent,
		EDigetherElevatorPhase::FinalAscent, EDigetherElevatorPhase::Finished })
	{
		FDigetherElevatorPullContext WrongPhase = Context;
		WrongPhase.ElevatorPhase = Phase;
		TestEqual(TEXT("출발 조건은 거점만 허용"), CanUseDeparture(WrongPhase), EDigetherElevatorLeverResult::InvalidPhase);
		const FDigetherElevatorGaugeUpdate Rejected = BeginPull(State, EDigetherElevatorLeverKind::Departure, Puller, WrongPhase, Tuning);
		TestEqual(TEXT("거점 밖에서 출발 홀드 거절"), Rejected.Feedback.Result, EDigetherElevatorLeverResult::InvalidPhase);
		TestTrue(TEXT("운행 단계 거절 시 게이지 불변"), SameState(State, Rejected.State));
	}
	Context.bAnyoneWatchingResultCredits = true;
	TestEqual(TEXT("결과 크레딧 관람 중 출발 조건 거절"), CanUseDeparture(Context), EDigetherElevatorLeverResult::WatchingResultCredits);
	FDigetherElevatorGaugeUpdate Update = BeginPull(State, EDigetherElevatorLeverKind::Departure, Puller, Context, Tuning);
	TestEqual(TEXT("결과 크레딧 관람 중 홀드 거절"), Update.Feedback.Result, EDigetherElevatorLeverResult::WatchingResultCredits);
	TestTrue(TEXT("관람 거절 시 게이지 불변"), SameState(State, Update.State));
	Context.bAnyoneWatchingResultCredits = false;
	TestEqual(TEXT("보통 일차 출발 시간"), CalculateDepartureDuration(1, Tuning), Tuning.DepartureHoldSeconds);
	Update = BeginPull(State, EDigetherElevatorLeverKind::Departure, Puller, Context, Tuning);
	TestEqual(TEXT("보통 일차 출발 레버 시작"), Update.Feedback.Result, EDigetherElevatorLeverResult::Started);
	State = Update.State;
	TestEqual(TEXT("보통 일차 홀드 시간"), State.DurationSeconds, Tuning.DepartureHoldSeconds);
	TestFalse(TEXT("보통 일차 경고 없음"), State.bWarningActive);
	TestFalse(TEXT("보통 일차 경고 피드백 없음"), Update.Feedback.bWarning);
	Update = BeginPull(State, EDigetherElevatorLeverKind::Departure, OtherPuller, Context, Tuning);
	TestEqual(TEXT("출발 레버 동시 점유 거절"), Update.Feedback.Result, EDigetherElevatorLeverResult::Busy);
	TestTrue(TEXT("동시 점유 거절 상태 불변"), SameState(State, Update.State));
	Update = AdvancePull(State, EDigetherElevatorLeverKind::Departure, 1.f, Context, Tuning);
	TestEqual(TEXT("보통 일차 절반 홀드"), Update.Feedback.Result, EDigetherElevatorLeverResult::InProgress);
	TestEqual(TEXT("보통 일차 절반 진행률"), CalculateGaugeProgress(Update.State), 0.5f);
	Update = AdvancePull(Update.State, EDigetherElevatorLeverKind::Departure, 1.f, Context, Tuning);
	TestEqual(TEXT("보통 일차 정확한 시간에 확정"), Update.Feedback.Result, EDigetherElevatorLeverResult::Confirmed);
	State = Update.State;
	TestEqual(TEXT("출발 확정자 값"), State.Departure.PullerId, Puller);
	TestEqual(TEXT("출발 확정 일차 값"), State.Departure.DayNumber, 1);
	TestEqual(TEXT("출발 확정 진행률"), CalculateGaugeProgress(State), 1.f);
	int32 CompletionCount = Update.Feedback.Result == EDigetherElevatorLeverResult::Confirmed ? 1 : 0;
	for (int32 Attempt = 0; Attempt < 3; ++Attempt)
	{
		Update = AdvancePull(State, EDigetherElevatorLeverKind::Departure, 2.f, Context, Tuning);
		if (Update.Feedback.Result == EDigetherElevatorLeverResult::Confirmed)
		{
			++CompletionCount;
		}
		TestEqual(TEXT("출발 확정 반복은 이미 완료 결과"), Update.Feedback.Result, EDigetherElevatorLeverResult::AlreadyConfirmed);
		TestTrue(TEXT("중복 완료는 확정 payload 보존"), SameState(State, Update.State));
	}
	TestEqual(TEXT("출발 완료 결과는 한 번만 발생"), CompletionCount, 1);

	Context.DayNumber = 4;
	TestEqual(TEXT("4일차 출발 시간은 정확히 2배"), CalculateDepartureDuration(Context.DayNumber, Tuning), 2.f * Tuning.DepartureHoldSeconds);
	Update = BeginPull(FDigetherElevatorGaugeState(), EDigetherElevatorLeverKind::Departure, Puller, Context, Tuning);
	TestEqual(TEXT("4일차 출발 시작"), Update.Feedback.Result, EDigetherElevatorLeverResult::Started);
	TestTrue(TEXT("4일차 경고 상태 활성"), Update.State.bWarningActive);
	TestTrue(TEXT("4일차 경고 피드백"), Update.Feedback.bWarning);
	TestFalse(TEXT("4일차 경고는 과적 덜컹 아님"), Update.Feedback.bRattle);
	Update = AdvancePull(Update.State, EDigetherElevatorLeverKind::Departure, Tuning.DepartureHoldSeconds, Context, Tuning);
	TestEqual(TEXT("4일차는 보통 홀드 시간만으로 완료되지 않음"), Update.Feedback.Result, EDigetherElevatorLeverResult::InProgress);
	TestEqual(TEXT("4일차 보통 시간에 절반 진행"), CalculateGaugeProgress(Update.State), 0.5f);
	TestTrue(TEXT("4일차 홀드 중 경고 유지"), Update.State.bWarningActive);
	State = Update.State;
	Update = ReleasePull(State, Puller);
	TestEqual(TEXT("출발 레버 놓기"), Update.Feedback.Result, EDigetherElevatorLeverResult::Released);
	TestEqual(TEXT("출발 레버 놓기 진행률 0"), CalculateGaugeProgress(Update.State), 0.f);
	TestFalse(TEXT("출발 레버 놓기 경고 해제"), Update.State.bWarningActive);
	Update = AdvancePull(State, EDigetherElevatorLeverKind::Departure, Tuning.DepartureHoldSeconds, Context, Tuning);
	TestEqual(TEXT("4일차 2배 시간에 출발 확정"), Update.Feedback.Result, EDigetherElevatorLeverResult::Confirmed);
	TestEqual(TEXT("4일차 확정 payload 일차"), Update.State.Departure.DayNumber, 4);
	TestEqual(TEXT("4일차 확정 payload 조작자"), Update.State.Departure.PullerId, Puller);

	State = BeginPull(FDigetherElevatorGaugeState(), EDigetherElevatorLeverKind::Departure, Puller, Context, Tuning).State;
	for (const float InvalidTime : { -1.f, NAN, INFINITY, -INFINITY })
	{
		Update = AdvancePull(State, EDigetherElevatorLeverKind::Departure, InvalidTime, Context, Tuning);
		TestEqual(TEXT("손상된 출발 홀드 시간 거절"), Update.Feedback.Result, EDigetherElevatorLeverResult::InvalidInput);
		TestTrue(TEXT("시간 오류는 출발 게이지 불변"), SameState(State, Update.State));
	}
	FDigetherElevatorPullContext InvalidContext = Context;
	InvalidContext.ElevatorPhase = static_cast<EDigetherElevatorPhase>(MAX_uint8);
	Update = BeginPull(FDigetherElevatorGaugeState(), EDigetherElevatorLeverKind::Departure, Puller, InvalidContext, Tuning);
	TestEqual(TEXT("정의되지 않은 출발 단계 거절"), Update.Feedback.Result, EDigetherElevatorLeverResult::InvalidInput);
	TestTrue(TEXT("손상된 단계는 시작 상태 불변"), SameState(FDigetherElevatorGaugeState(), Update.State));
	Context.bAnyoneWatchingResultCredits = true;
	Update = AdvancePull(State, EDigetherElevatorLeverKind::Departure, 1.f, Context, Tuning);
	TestEqual(TEXT("홀드 중 결과 관람 시작은 출발 취소"), Update.Feedback.Result, EDigetherElevatorLeverResult::InterruptedConditions);
	TestEqual(TEXT("조건 상실 후 진행률 0"), CalculateGaugeProgress(Update.State), 0.f);
	TestEqual(TEXT("조건 상실 후 대기"), Update.State.Phase, EDigetherElevatorGaugePhase::Idle);
	TestEqual(TEXT("조건 상실은 출발 payload 미확정"), Update.State.Departure.DayNumber, 0);
	return true;
}

#endif
