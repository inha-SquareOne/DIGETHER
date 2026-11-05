#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Elevator/DigetherElevatorLeverComponent.h"
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
		EDigetherElevatorPhase::FinalAscent, EDigetherElevatorPhase::Finished, EDigetherElevatorPhase::DoorClosing })
	{
		FDigetherElevatorPullContext WrongPhase = Context;
		WrongPhase.ElevatorPhase = Phase;
		TestEqual(TEXT("출발 조건은 거점만 허용"), CanUseDeparture(WrongPhase), EDigetherElevatorLeverResult::InvalidPhase);
		const FDigetherElevatorGaugeUpdate Rejected = BeginPull(State, EDigetherElevatorLeverKind::Departure, Puller, WrongPhase, Tuning);
		TestEqual(TEXT("거점 밖에서 출발 홀드 거절"), Rejected.Feedback.Result, EDigetherElevatorLeverResult::InvalidPhase);
		TestTrue(TEXT("운행 단계 거절 시 게이지 불변"), SameState(State, Rejected.State));
	}
	// 결과 크레딧 중 출발 차단은 게임 오버 단계가 연결층에서 막는다(S33). 순수 규칙의 출발 조건은 거점 대기만 본다.
	TestEqual(TEXT("거점 대기 출발 조건 허용"), CanUseDeparture(Context), EDigetherElevatorLeverResult::Started);
	TestEqual(TEXT("보통 일차 출발 시간"), CalculateDepartureDuration(1, Tuning), Tuning.DepartureHoldSeconds);
	FDigetherElevatorGaugeUpdate Update = BeginPull(State, EDigetherElevatorLeverKind::Departure, Puller, Context, Tuning);
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
	// 홀드 중 출발 조건(거점 대기)이 사라지면 취소한다. 조건 상실 사례로 거점 대기 이탈을 쓴다.
	Context.ElevatorPhase = EDigetherElevatorPhase::DoorClosing;
	Update = AdvancePull(State, EDigetherElevatorLeverKind::Departure, 1.f, Context, Tuning);
	TestEqual(TEXT("홀드 중 거점 대기 이탈은 출발 취소"), Update.Feedback.Result, EDigetherElevatorLeverResult::InterruptedConditions);
	TestEqual(TEXT("조건 상실 후 진행률 0"), CalculateGaugeProgress(Update.State), 0.f);
	TestEqual(TEXT("조건 상실 후 대기"), Update.State.Phase, EDigetherElevatorGaugePhase::Idle);
	TestEqual(TEXT("조건 상실은 출발 payload 미확정"), Update.State.Departure.DayNumber, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherElevatorDepartureWarningTest,
	"DIGETHER.Game.Elevator.DepartureWarning", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherElevatorDepartureWarningTest::RunTest(const FString& Parameters)
{
	using namespace DigetherElevatorRules;
	// 테스트 픽스처이며 최종 기획 출발 홀드 시간이 아니다.
	FDigetherElevatorTuning Tuning;
	Tuning.DepartureHoldSeconds = 2.f;
	FDigetherElevatorPullContext Context;
	Context.ElevatorPhase = EDigetherElevatorPhase::HubWaiting;
	const FGuid Puller(21, 0, 0, 21);

	TestEqual(TEXT("대기 게이지는 경고 없음"), CalculateDepartureWarning(FDigetherElevatorGaugeState()), EDigetherDepartureWarningKind::None);
	for (const int32 Day : { 1, 2, 3 })
	{
		Context.DayNumber = Day;
		const FDigetherElevatorGaugeUpdate Started = BeginPull(FDigetherElevatorGaugeState(), EDigetherElevatorLeverKind::Departure,
			Puller, Context, Tuning);
		TestEqual(TEXT("1~3일차 출발 시작"), Started.Feedback.Result, EDigetherElevatorLeverResult::Started);
		TestEqual(TEXT("1~3일차는 일반 출발 경고"), CalculateDepartureWarning(Started.State), EDigetherDepartureWarningKind::Standard);
		TestFalse(TEXT("1~3일차는 마감 경고 표식 없음"), Started.State.bWarningActive);
		const FDigetherElevatorGaugeUpdate Progress = AdvancePull(Started.State, EDigetherElevatorLeverKind::Departure, 1.f, Context, Tuning);
		TestEqual(TEXT("1~3일차 홀드 중 일반 경고 유지"), CalculateDepartureWarning(Progress.State), EDigetherDepartureWarningKind::Standard);
		const FDigetherElevatorGaugeUpdate Confirmed = AdvancePull(Progress.State, EDigetherElevatorLeverKind::Departure, 1.f, Context, Tuning);
		TestEqual(TEXT("게이지 완료 뒤 경고 해제"), CalculateDepartureWarning(Confirmed.State), EDigetherDepartureWarningKind::None);
		const FDigetherElevatorGaugeUpdate Released = ReleasePull(Progress.State, Puller);
		TestEqual(TEXT("놓으면 경고 해제"), CalculateDepartureWarning(Released.State), EDigetherDepartureWarningKind::None);
	}
	Context.DayNumber = DigetherElevatorBoardValues::WarningDayNumber;
	const FDigetherElevatorGaugeUpdate Deadline = BeginPull(FDigetherElevatorGaugeState(), EDigetherElevatorLeverKind::Departure,
		Puller, Context, Tuning);
	TestEqual(TEXT("4일차는 마감 경고"), CalculateDepartureWarning(Deadline.State), EDigetherDepartureWarningKind::Deadline);
	TestTrue(TEXT("4일차 마감 경고 표식 유지"), Deadline.State.bWarningActive);

	// 상승 레버는 출발 경고를 쓰지 않는다.
	FDigetherElevatorPullContext Ascent;
	Ascent.ElevatorPhase = EDigetherElevatorPhase::BottomWaiting;
	Ascent.TripLimit = 2;
	Ascent.CapacityKg = 100.f;
	Ascent.DayNumber = DigetherElevatorBoardValues::WarningDayNumber;
	const FDigetherElevatorGaugeUpdate AscentPull = BeginPull(FDigetherElevatorGaugeState(), EDigetherElevatorLeverKind::Ascent,
		Puller, Ascent, Tuning);
	TestEqual(TEXT("상승 레버 시작"), AscentPull.Feedback.Result, EDigetherElevatorLeverResult::Started);
	TestEqual(TEXT("상승 레버는 출발 경고 없음"), CalculateDepartureWarning(AscentPull.State), EDigetherDepartureWarningKind::None);

	TestFalse(TEXT("일반 경고 문구 있음"), UDigetherElevatorLeverComponent::GetDepartureWarningText(EDigetherDepartureWarningKind::Standard).IsEmpty());
	TestFalse(TEXT("마감 경고 문구 있음"), UDigetherElevatorLeverComponent::GetDepartureWarningText(EDigetherDepartureWarningKind::Deadline).IsEmpty());
	TestTrue(TEXT("경고 없음은 빈 문구"), UDigetherElevatorLeverComponent::GetDepartureWarningText(EDigetherDepartureWarningKind::None).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherElevatorDeparturePeriodTest,
	"DIGETHER.Game.Elevator.DeparturePeriodGeneration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherElevatorDeparturePeriodTest::RunTest(const FString& Parameters)
{
	using namespace DigetherElevatorRules;
	// 테스트 픽스처이며 최종 기획 출발 홀드 시간이 아니다.
	FDigetherElevatorTuning Tuning;
	Tuning.DepartureHoldSeconds = 2.f;
	FDigetherElevatorPullContext Context;
	Context.ElevatorPhase = EDigetherElevatorPhase::HubWaiting;
	Context.DayNumber = 3;
	Context.PeriodGeneration = 4;
	const FGuid Puller(31, 0, 0, 31);
	const FDigetherElevatorGaugeUpdate Started = BeginPull(FDigetherElevatorGaugeState(), EDigetherElevatorLeverKind::Departure,
		Puller, Context, Tuning);
	TestEqual(TEXT("출발 시작"), Started.Feedback.Result, EDigetherElevatorLeverResult::Started);
	TestEqual(TEXT("시작한 기간 세대 기록"), Started.State.DeparturePeriodGeneration, 4);

	// 게이지 완료 전에 목표를 달성하면(기간 세대 변화) 레버 입력을 취소한다.
	FDigetherElevatorPullContext Achieved = Context;
	Achieved.DayNumber = 1;
	Achieved.PeriodGeneration = 5;
	FDigetherElevatorGaugeUpdate Update = AdvancePull(Started.State, EDigetherElevatorLeverKind::Departure, 1.f, Achieved, Tuning);
	TestEqual(TEXT("완료 전 달성은 출발 레버 취소"), Update.Feedback.Result, EDigetherElevatorLeverResult::InterruptedConditions);
	TestEqual(TEXT("취소 뒤 대기 게이지"), Update.State.Phase, EDigetherElevatorGaugePhase::Idle);

	// 같은 일차 번호여도 기간이 바뀌면 달성으로 본다(일차 비교만으로는 놓친다).
	FDigetherElevatorPullContext SameDayNewPeriod = Context;
	SameDayNewPeriod.PeriodGeneration = 5;
	Update = AdvancePull(Started.State, EDigetherElevatorLeverKind::Departure, 1.f, SameDayNewPeriod, Tuning);
	TestEqual(TEXT("같은 일차 번호의 새 기간도 취소"), Update.Feedback.Result, EDigetherElevatorLeverResult::InterruptedConditions);

	// 게이지가 차는 같은 처리 시점의 납부 달성은 납부를 먼저 반영하고, 완료된 레버로 새 기간 1일차를 출발한다.
	const FDigetherElevatorGaugeUpdate Half = AdvancePull(Started.State, EDigetherElevatorLeverKind::Departure, 1.f, Context, Tuning);
	TestEqual(TEXT("같은 기간 절반 진행"), Half.Feedback.Result, EDigetherElevatorLeverResult::InProgress);
	Update = AdvancePull(Half.State, EDigetherElevatorLeverKind::Departure, 1.f, Achieved, Tuning);
	TestEqual(TEXT("같은 처리 시점 달성은 출발 확정"), Update.Feedback.Result, EDigetherElevatorLeverResult::Confirmed);
	TestEqual(TEXT("확정 일차는 새 기간 1일차"), Update.State.Departure.DayNumber, 1);
	TestEqual(TEXT("확정 조작자 유지"), Update.State.Departure.PullerId, Puller);

	// 같은 기간에서 일차만 달라지는 손상 입력은 취소한다.
	FDigetherElevatorPullContext DayOnly = Context;
	DayOnly.DayNumber = 4;
	Update = AdvancePull(Started.State, EDigetherElevatorLeverKind::Departure, 1.f, DayOnly, Tuning);
	TestEqual(TEXT("같은 기간 일차 변화는 취소"), Update.Feedback.Result, EDigetherElevatorLeverResult::InterruptedConditions);

	// 손상된 기간 세대 값은 받지 않는다.
	FDigetherElevatorPullContext Negative = Context;
	Negative.PeriodGeneration = -1;
	Update = AdvancePull(Started.State, EDigetherElevatorLeverKind::Departure, 1.f, Negative, Tuning);
	TestEqual(TEXT("음수 기간 세대 취소"), Update.Feedback.Result, EDigetherElevatorLeverResult::InterruptedConditions);
	FDigetherElevatorGaugeState CorruptIdle;
	CorruptIdle.DeparturePeriodGeneration = 2;
	TestEqual(TEXT("기간 세대가 남은 대기 게이지는 손상 입력"),
		AdvancePull(CorruptIdle, EDigetherElevatorLeverKind::Departure, 1.f, Context, Tuning).Feedback.Result,
		EDigetherElevatorLeverResult::InvalidInput);
	return true;
}

#endif
