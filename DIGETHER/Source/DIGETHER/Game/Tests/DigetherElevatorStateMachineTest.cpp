#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Elevator/DigetherElevatorRules.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#include <cmath>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherElevatorStateMachineTest,
	"DIGETHER.Game.Elevator.StateMachine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherElevatorStateMachineTest::RunTest(const FString& Parameters)
{
	using namespace DigetherElevatorRules;
	// 테스트 픽스처이며 최종 기획 운행 시간·횟수가 아니다.
	FDigetherElevatorTuning Tuning;
	Tuning.UpperStopSeconds = 2.f;
	constexpr float TravelSeconds = 4.f;
	constexpr int32 TripLimit = 2;
	FDigetherElevatorState State;
	const auto SameState = [](const FDigetherElevatorState& Left, const FDigetherElevatorState& Right)
	{
		return FDigetherElevatorState::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
	};
	const auto Apply = [&](const EDigetherElevatorCommand Command, const float DeltaSeconds,
		const EDigetherElevatorPhase ExpectedPhase, const EDigetherElevatorResult ExpectedResult)
	{
		const FDigetherElevatorTransition Update = Transition(State, Command, DeltaSeconds, TripLimit, TravelSeconds, Tuning);
		TestEqual(TEXT("전이 결과"), Update.Result, ExpectedResult);
		TestEqual(TEXT("전이 단계"), Update.State.Phase, ExpectedPhase);
		State = Update.State;
	};
	Apply(EDigetherElevatorCommand::DepartHub, 0.f, EDigetherElevatorPhase::InitialDescent, EDigetherElevatorResult::Success);
	TestTrue(TEXT("최초 하강은 이동 중"), IsMoving(State.Phase));
	Apply(EDigetherElevatorCommand::AdvanceTime, TravelSeconds / 2.f, EDigetherElevatorPhase::InitialDescent, EDigetherElevatorResult::Success);
	TestEqual(TEXT("최초 하강 절반 위치"), CalculateTravelProgress(State, TravelSeconds), 0.5f);
	Apply(EDigetherElevatorCommand::AdvanceTime, TravelSeconds / 2.f, EDigetherElevatorPhase::BottomWaiting, EDigetherElevatorResult::Success);
	TestEqual(TEXT("하부 대기 위치"), CalculateTravelProgress(State, TravelSeconds), 0.f);
	TestEqual(TEXT("최초 하강은 상승 횟수 제외"), State.CompletedAscents, 0);
	Apply(EDigetherElevatorCommand::RequestAscent, 0.f, EDigetherElevatorPhase::Ascending, EDigetherElevatorResult::Success);
	TestEqual(TEXT("출발만으로 성공 횟수 증가 없음"), State.CompletedAscents, 0);
	for (const EDigetherElevatorCommand Command : { EDigetherElevatorCommand::RequestAscent, EDigetherElevatorCommand::DepartHub })
	{
		const FDigetherElevatorState Before = State;
		const FDigetherElevatorTransition Rejected = Transition(State, Command, 0.f, TripLimit, TravelSeconds, Tuning);
		TestEqual(TEXT("이동 중 재출발 거절"), Rejected.Result, EDigetherElevatorResult::InvalidPhase);
		TestTrue(TEXT("이동 중 거절 상태 불변"), SameState(Before, Rejected.State));
	}
	Apply(EDigetherElevatorCommand::AdvanceTime, TravelSeconds, EDigetherElevatorPhase::UpperStop, EDigetherElevatorResult::Success);
	TestEqual(TEXT("첫 상승 도착 후 성공 1회"), State.CompletedAscents, 1);
	TestEqual(TEXT("상부 정지 위치"), CalculateTravelProgress(State, TravelSeconds), 1.f);
	TestFalse(TEXT("상부 정지는 이동 아님"), IsMoving(State.Phase));
	Apply(EDigetherElevatorCommand::AdvanceTime, Tuning.UpperStopSeconds, EDigetherElevatorPhase::ReturnDescent, EDigetherElevatorResult::Success);
	Apply(EDigetherElevatorCommand::AdvanceTime, TravelSeconds, EDigetherElevatorPhase::BottomWaiting, EDigetherElevatorResult::Success);
	Apply(EDigetherElevatorCommand::RequestAscent, 0.f, EDigetherElevatorPhase::FinalAscent, EDigetherElevatorResult::Success);
	Apply(EDigetherElevatorCommand::AdvanceTime, TravelSeconds, EDigetherElevatorPhase::Finished, EDigetherElevatorResult::ExplorationFinished);
	TestEqual(TEXT("마지막 도착 후 성공 2회"), State.CompletedAscents, 2);
	const FDigetherElevatorState Finished = State;
	Apply(EDigetherElevatorCommand::AdvanceTime, TravelSeconds, EDigetherElevatorPhase::Finished, EDigetherElevatorResult::NoChange);
	TestTrue(TEXT("종료 후 중복 시간 입력 상태 불변"), SameState(Finished, State));
	Apply(EDigetherElevatorCommand::Reset, 0.f, EDigetherElevatorPhase::HubWaiting, EDigetherElevatorResult::Success);
	TestEqual(TEXT("거점 리셋 횟수 0"), State.CompletedAscents, 0);
	TestEqual(TEXT("거점 리셋 경과 0"), State.PhaseElapsedSeconds, 0.f);
	TestEqual(TEXT("거점 리셋 위치 상부"), CalculateTravelProgress(State, TravelSeconds), 1.f);

	State.Phase = EDigetherElevatorPhase::BottomWaiting;
	// ELV_ASCENT_LIMIT는 2회 확정이다. 1회 비교 설정은 운행 전이를 거절하고 상태를 바꾸지 않는다.
	FDigetherElevatorTransition Update = Transition(State, EDigetherElevatorCommand::RequestAscent, 0.f, 1, TravelSeconds, Tuning);
	TestEqual(TEXT("1회 설정 상승 요청 거절"), Update.Result, EDigetherElevatorResult::InvalidInput);
	TestTrue(TEXT("1회 설정 거절은 상태 불변"), SameState(State, Update.State));
	Update = Transition(State, EDigetherElevatorCommand::RequestAscent, 0.f, TripLimit, TravelSeconds, Tuning);
	TestEqual(TEXT("2회 확정의 첫 상승은 최종 아님"), Update.State.Phase, EDigetherElevatorPhase::Ascending);

	State = FDigetherElevatorState();
	Apply(EDigetherElevatorCommand::DepartHub, 0.f, EDigetherElevatorPhase::InitialDescent, EDigetherElevatorResult::Success);
	Apply(EDigetherElevatorCommand::AdvanceTime, 100.f, EDigetherElevatorPhase::BottomWaiting, EDigetherElevatorResult::Success);
	TestEqual(TEXT("큰 시간 입력의 나머지는 하부 대기에 보관하지 않음"), State.PhaseElapsedSeconds, 0.f);
	Apply(EDigetherElevatorCommand::RequestAscent, 0.f, EDigetherElevatorPhase::Ascending, EDigetherElevatorResult::Success);
	Apply(EDigetherElevatorCommand::AdvanceTime, 100.f, EDigetherElevatorPhase::UpperStop, EDigetherElevatorResult::Success);
	TestEqual(TEXT("큰 입력으로 상승 뒤 정지까지 넘기지 않음"), State.PhaseElapsedSeconds, 0.f);
	Apply(EDigetherElevatorCommand::AdvanceTime, 100.f, EDigetherElevatorPhase::ReturnDescent, EDigetherElevatorResult::Success);
	TestEqual(TEXT("큰 입력으로 정지 뒤 하강까지 넘기지 않음"), State.PhaseElapsedSeconds, 0.f);
	Apply(EDigetherElevatorCommand::AdvanceTime, 1.f, EDigetherElevatorPhase::ReturnDescent, EDigetherElevatorResult::Success);
	TestEqual(TEXT("다음 호출은 새 시간만 소비"), State.PhaseElapsedSeconds, 1.f);
	TestEqual(TEXT("하강 1초 위치"), CalculateTravelProgress(State, TravelSeconds), 0.75f);
	for (const float InvalidTime : { -1.f, NAN, INFINITY, -INFINITY })
	{
		const FDigetherElevatorTransition Rejected = Transition(State, EDigetherElevatorCommand::AdvanceTime, InvalidTime, TripLimit, TravelSeconds, Tuning);
		TestEqual(TEXT("손상된 시간 입력 거절"), Rejected.Result, EDigetherElevatorResult::InvalidInput);
		TestTrue(TEXT("시간 입력 오류 상태 불변"), SameState(State, Rejected.State));
	}
	Update = Transition(State, static_cast<EDigetherElevatorCommand>(MAX_uint8), 0.f, TripLimit, TravelSeconds, Tuning);
	TestEqual(TEXT("정의되지 않은 명령 거절"), Update.Result, EDigetherElevatorResult::InvalidInput);
	TestTrue(TEXT("명령 오류 상태 불변"), SameState(State, Update.State));
	State.Phase = static_cast<EDigetherElevatorPhase>(MAX_uint8);
	Update = Transition(State, EDigetherElevatorCommand::AdvanceTime, 1.f, TripLimit, TravelSeconds, Tuning);
	TestEqual(TEXT("정의되지 않은 단계 거절"), Update.Result, EDigetherElevatorResult::InvalidInput);
	TestTrue(TEXT("단계 오류 상태 불변"), SameState(State, Update.State));
	State.CompletedAscents = -1;
	State.PhaseElapsedSeconds = NAN;
	Update = Transition(State, EDigetherElevatorCommand::Reset, 0.f, TripLimit, TravelSeconds, Tuning);
	TestEqual(TEXT("리셋은 손상된 현재 상태도 복구"), Update.Result, EDigetherElevatorResult::Success);
	TestTrue(TEXT("손상된 상태 리셋은 거점 기본 상태"), SameState(FDigetherElevatorState(), Update.State));
	State = Update.State;
	Update = Transition(State, EDigetherElevatorCommand::DepartHub, 0.f, 0, TravelSeconds, Tuning);
	TestEqual(TEXT("잘못된 운행 횟수 거절"), Update.Result, EDigetherElevatorResult::InvalidInput);
	TestTrue(TEXT("운행 매개변수 오류 상태 불변"), SameState(State, Update.State));
	FDigetherElevatorTuning InvalidTuning = Tuning;
	InvalidTuning.UpperStopSeconds = 0.f;
	Update = Transition(State, EDigetherElevatorCommand::DepartHub, 0.f, TripLimit, TravelSeconds, InvalidTuning);
	TestEqual(TEXT("잘못된 튜닝 거절"), Update.Result, EDigetherElevatorResult::InvalidInput);
	TestTrue(TEXT("튜닝 오류 상태 불변"), SameState(State, Update.State));
	// float 저장 시 반올림으로 완료 시각에 도달해도 이동 상태에 갇히면 안 된다.
	FDigetherElevatorState RoundingState;
	RoundingState.Phase = EDigetherElevatorPhase::InitialDescent;
	RoundingState.PhaseElapsedSeconds = std::nextafter(TravelSeconds, 0.f);
	const float RoundingDelta = (TravelSeconds - RoundingState.PhaseElapsedSeconds) / 2.f;
	Update = Transition(RoundingState, EDigetherElevatorCommand::AdvanceTime, RoundingDelta, TripLimit, TravelSeconds, Tuning);
	TestEqual(TEXT("시각 반올림 경계에서도 하강 완료"), Update.State.Phase, EDigetherElevatorPhase::BottomWaiting);
	TestEqual(TEXT("시각 반올림 경계에서 경과 시각 초기화"), Update.State.PhaseElapsedSeconds, 0.f);
	return true;
}

#endif
