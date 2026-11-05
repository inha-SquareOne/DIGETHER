#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Elevator/DigetherElevatorRules.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#include <cmath>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherElevatorDoorClosingTest,
	"DIGETHER.Game.Elevator.DoorClosing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherElevatorDoorClosingTest::RunTest(const FString& Parameters)
{
	using namespace DigetherElevatorRules;
	// 테스트 픽스처이며 최종 기획 문 닫힘 시간·운행 시간이 아니다.
	// 거점·갱도 닫힘 시간을 서로 다르게 두어 각 문이 자기 시간을 쓰는지 함께 검사한다.
	FDigetherElevatorTuning Tuning;
	Tuning.HubDoorCloseSeconds = 2.f;
	Tuning.MineDoorCloseSeconds = 5.f;
	constexpr float TravelSeconds = 4.f;
	constexpr int32 TripLimit = 2;
	const auto SameState = [](const FDigetherElevatorState& Left, const FDigetherElevatorState& Right)
	{
		return FDigetherElevatorState::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
	};
	FDigetherElevatorState State;
	const auto Apply = [&](const EDigetherElevatorCommand Command, const float DeltaSeconds,
		const EDigetherElevatorPhase ExpectedPhase, const EDigetherElevatorResult ExpectedResult)
	{
		const FDigetherElevatorTransition Update = Transition(State, Command, DeltaSeconds, TripLimit, TravelSeconds, Tuning);
		TestEqual(TEXT("문 전이 결과"), Update.Result, ExpectedResult);
		TestEqual(TEXT("문 전이 단계"), Update.State.Phase, ExpectedPhase);
		State = Update.State;
	};

	TestTrue(TEXT("기본 문 튜닝 유효"), IsValidTuning(FDigetherElevatorTuning()));
	TestEqual(TEXT("LOBBY_DOOR_CLOSE_TIME 임시 3초"), FDigetherElevatorTuning().HubDoorCloseSeconds, 3.f);
	TestEqual(TEXT("ELV_DOOR_CLOSE_TIME 임시 3초"), FDigetherElevatorTuning().MineDoorCloseSeconds, 3.f);
	TestEqual(TEXT("LOBBY_CHAIN_SUMMON_TIME MVP 0초"), FDigetherElevatorTuning().ChainSummonSeconds, 0.f);
	TestEqual(TEXT("거점 문은 거점 닫힘 시간"), GetDoorCloseSeconds(EDigetherElevatorPhase::HubWaiting, Tuning), 2.f);
	TestEqual(TEXT("갱도 문은 갱도 닫힘 시간"), GetDoorCloseSeconds(EDigetherElevatorPhase::BottomWaiting, Tuning), 5.f);
	TestEqual(TEXT("대기 단계가 아니면 닫힘 시간 0"), GetDoorCloseSeconds(EDigetherElevatorPhase::Ascending, Tuning), 0.f);

	// 거점: 출발 게이지 완료 → 문 닫힘 → 닫힘 완료 → 첫 하강.
	Apply(EDigetherElevatorCommand::BeginDoorClose, 0.f, EDigetherElevatorPhase::DoorClosing, EDigetherElevatorResult::Success);
	TestEqual(TEXT("거점 문 닫힘 시작 단계 기록"), State.DoorOriginPhase, EDigetherElevatorPhase::HubWaiting);
	TestEqual(TEXT("거점 문 닫힘 중 위치는 상부"), CalculateTravelProgress(State, TravelSeconds), 1.f);
	TestEqual(TEXT("문 닫힘 시작 진행률 0"), CalculateDoorCloseProgress(State, Tuning), 0.f);
	TestFalse(TEXT("문 닫힘은 이동 아님"), IsMoving(State.Phase));
	Apply(EDigetherElevatorCommand::AdvanceTime, Tuning.HubDoorCloseSeconds / 2.f, EDigetherElevatorPhase::DoorClosing,
		EDigetherElevatorResult::Success);
	TestEqual(TEXT("문 닫힘 절반 진행률"), CalculateDoorCloseProgress(State, Tuning), 0.5f);
	TestFalse(TEXT("절반은 닫힘 완료 아님"), IsDoorFullyClosed(State, Tuning));
	Apply(EDigetherElevatorCommand::AdvanceTime, 100.f, EDigetherElevatorPhase::DoorClosing, EDigetherElevatorResult::DoorClosed);
	TestTrue(TEXT("닫힘 시간 경과 후 완료"), IsDoorFullyClosed(State, Tuning));
	TestEqual(TEXT("큰 입력도 닫힘 시간에서 멈춤"), State.PhaseElapsedSeconds, Tuning.HubDoorCloseSeconds);
	TestEqual(TEXT("닫힘 완료 진행률 1"), CalculateDoorCloseProgress(State, Tuning), 1.f);
	const FDigetherElevatorState Closed = State;
	Apply(EDigetherElevatorCommand::AdvanceTime, 1.f, EDigetherElevatorPhase::DoorClosing, EDigetherElevatorResult::NoChange);
	TestTrue(TEXT("닫힘 완료 결과는 한 번만"), SameState(Closed, State));
	FDigetherElevatorTransition Rejected = Transition(State, EDigetherElevatorCommand::RequestAscent, 0.f, TripLimit, TravelSeconds, Tuning);
	TestEqual(TEXT("거점 문 닫힘에서 상승 거절"), Rejected.Result, EDigetherElevatorResult::InvalidPhase);
	TestTrue(TEXT("상승 거절 상태 불변"), SameState(State, Rejected.State));
	Apply(EDigetherElevatorCommand::DepartHub, 0.f, EDigetherElevatorPhase::InitialDescent, EDigetherElevatorResult::Success);
	TestEqual(TEXT("하강 시작 경과 0"), State.PhaseElapsedSeconds, 0.f);
	TestEqual(TEXT("하강 시작은 문 시작 단계 초기화"), State.DoorOriginPhase, EDigetherElevatorPhase::HubWaiting);
	TestEqual(TEXT("하강 중 문 진행률 0"), CalculateDoorCloseProgress(State, Tuning), 0.f);
	Rejected = Transition(State, EDigetherElevatorCommand::BeginDoorClose, 0.f, TripLimit, TravelSeconds, Tuning);
	TestEqual(TEXT("이동 중 문 닫힘 시작 거절"), Rejected.Result, EDigetherElevatorResult::InvalidPhase);
	Rejected = Transition(State, EDigetherElevatorCommand::ReopenDoor, 0.f, TripLimit, TravelSeconds, Tuning);
	TestEqual(TEXT("문 닫힘이 아니면 재개방 거절"), Rejected.Result, EDigetherElevatorResult::InvalidPhase);

	// 거점 출발 실패: 문을 다시 열면 출발 전 대기로 돌아간다.
	State = FDigetherElevatorState();
	Apply(EDigetherElevatorCommand::BeginDoorClose, 0.f, EDigetherElevatorPhase::DoorClosing, EDigetherElevatorResult::Success);
	Apply(EDigetherElevatorCommand::AdvanceTime, Tuning.HubDoorCloseSeconds, EDigetherElevatorPhase::DoorClosing, EDigetherElevatorResult::DoorClosed);
	Apply(EDigetherElevatorCommand::ReopenDoor, 0.f, EDigetherElevatorPhase::HubWaiting, EDigetherElevatorResult::Success);
	TestTrue(TEXT("거점 재개방은 기본 거점 상태"), SameState(FDigetherElevatorState(), State));

	// 갱도: 문 닫힘 중 무게 초과 재개방은 하부 대기로 돌아가고 상승 횟수에 넣지 않는다.
	State = FDigetherElevatorState();
	State.Phase = EDigetherElevatorPhase::BottomWaiting;
	State.CompletedAscents = 1;
	Apply(EDigetherElevatorCommand::BeginDoorClose, 0.f, EDigetherElevatorPhase::DoorClosing, EDigetherElevatorResult::Success);
	TestEqual(TEXT("갱도 문 닫힘 시작 단계 기록"), State.DoorOriginPhase, EDigetherElevatorPhase::BottomWaiting);
	TestEqual(TEXT("갱도 문 닫힘 중 위치는 바닥"), CalculateTravelProgress(State, TravelSeconds), 0.f);
	Rejected = Transition(State, EDigetherElevatorCommand::DepartHub, 0.f, TripLimit, TravelSeconds, Tuning);
	TestEqual(TEXT("갱도 문 닫힘에서 거점 출발 거절"), Rejected.Result, EDigetherElevatorResult::InvalidPhase);
	Rejected = Transition(State, EDigetherElevatorCommand::BeginDoorClose, 0.f, TripLimit, TravelSeconds, Tuning);
	TestEqual(TEXT("문 닫힘 중 중복 시작 거절"), Rejected.Result, EDigetherElevatorResult::InvalidPhase);
	Apply(EDigetherElevatorCommand::AdvanceTime, Tuning.HubDoorCloseSeconds, EDigetherElevatorPhase::DoorClosing,
		EDigetherElevatorResult::Success);
	TestFalse(TEXT("갱도 문은 거점 닫힘 시간이 지나도 아직 열림"), IsDoorFullyClosed(State, Tuning));
	TestEqual(TEXT("갱도 문 진행률은 갱도 닫힘 시간 기준"), CalculateDoorCloseProgress(State, Tuning),
		Tuning.HubDoorCloseSeconds / Tuning.MineDoorCloseSeconds);
	Apply(EDigetherElevatorCommand::ReopenDoor, 0.f, EDigetherElevatorPhase::BottomWaiting, EDigetherElevatorResult::Success);
	TestEqual(TEXT("재개방은 상승 횟수 불변"), State.CompletedAscents, 1);
	TestEqual(TEXT("재개방은 하부 대기 경과 0"), State.PhaseElapsedSeconds, 0.f);
	TestEqual(TEXT("재개방은 문 시작 단계 초기화"), State.DoorOriginPhase, EDigetherElevatorPhase::HubWaiting);

	// 갱도: 닫힘 완료 뒤 상승. 마지막 편이면 최종 상승이다.
	Apply(EDigetherElevatorCommand::BeginDoorClose, 0.f, EDigetherElevatorPhase::DoorClosing, EDigetherElevatorResult::Success);
	Apply(EDigetherElevatorCommand::AdvanceTime, Tuning.MineDoorCloseSeconds, EDigetherElevatorPhase::DoorClosing, EDigetherElevatorResult::DoorClosed);
	Apply(EDigetherElevatorCommand::RequestAscent, 0.f, EDigetherElevatorPhase::FinalAscent, EDigetherElevatorResult::Success);
	TestEqual(TEXT("상승 출발은 경과 0"), State.PhaseElapsedSeconds, 0.f);
	TestEqual(TEXT("상승 출발은 성공 횟수 증가 없음"), State.CompletedAscents, 1);

	// 시간 종료 귀환은 닫힘 도중에도 상승을 시작한다(갱도 대기와 같게 처리).
	State = FDigetherElevatorState();
	State.Phase = EDigetherElevatorPhase::BottomWaiting;
	Apply(EDigetherElevatorCommand::BeginDoorClose, 0.f, EDigetherElevatorPhase::DoorClosing, EDigetherElevatorResult::Success);
	Apply(EDigetherElevatorCommand::AdvanceTime, Tuning.MineDoorCloseSeconds / 4.f, EDigetherElevatorPhase::DoorClosing,
		EDigetherElevatorResult::Success);
	Apply(EDigetherElevatorCommand::RequestAscent, 0.f, EDigetherElevatorPhase::Ascending, EDigetherElevatorResult::Success);

	// 손상된 문 상태와 튜닝은 거절하고 상태를 바꾸지 않는다.
	FDigetherElevatorState Corrupted;
	Corrupted.Phase = EDigetherElevatorPhase::DoorClosing;
	Corrupted.DoorOriginPhase = EDigetherElevatorPhase::Ascending;
	FDigetherElevatorTransition Update = Transition(Corrupted, EDigetherElevatorCommand::AdvanceTime, 1.f, TripLimit, TravelSeconds, Tuning);
	TestEqual(TEXT("대기 단계가 아닌 문 시작 단계 거절"), Update.Result, EDigetherElevatorResult::InvalidInput);
	TestTrue(TEXT("문 시작 단계 오류 상태 불변"), SameState(Corrupted, Update.State));
	Corrupted.DoorOriginPhase = EDigetherElevatorPhase::HubWaiting;
	Corrupted.CompletedAscents = 1;
	Update = Transition(Corrupted, EDigetherElevatorCommand::AdvanceTime, 1.f, TripLimit, TravelSeconds, Tuning);
	TestEqual(TEXT("상승 기록이 있는 거점 문 닫힘 거절"), Update.Result, EDigetherElevatorResult::InvalidInput);
	Corrupted.CompletedAscents = 0;
	Corrupted.PhaseElapsedSeconds = std::nextafter(Tuning.HubDoorCloseSeconds, INFINITY);
	Update = Transition(Corrupted, EDigetherElevatorCommand::AdvanceTime, 1.f, TripLimit, TravelSeconds, Tuning);
	TestEqual(TEXT("닫힘 시간을 넘은 경과 거절"), Update.Result, EDigetherElevatorResult::InvalidInput);
	FDigetherElevatorState StrayOrigin;
	StrayOrigin.Phase = EDigetherElevatorPhase::BottomWaiting;
	StrayOrigin.DoorOriginPhase = EDigetherElevatorPhase::BottomWaiting;
	Update = Transition(StrayOrigin, EDigetherElevatorCommand::BeginDoorClose, 0.f, TripLimit, TravelSeconds, Tuning);
	TestEqual(TEXT("문 닫힘이 아닌 단계의 문 시작 값 거절"), Update.Result, EDigetherElevatorResult::InvalidInput);
	FDigetherElevatorState MineClosing;
	MineClosing.Phase = EDigetherElevatorPhase::DoorClosing;
	MineClosing.DoorOriginPhase = EDigetherElevatorPhase::BottomWaiting;
	MineClosing.PhaseElapsedSeconds = 1.f;
	for (const float InvalidSeconds : { 0.f, -1.f, NAN, INFINITY })
	{
		FDigetherElevatorTuning InvalidHub = Tuning;
		InvalidHub.HubDoorCloseSeconds = InvalidSeconds;
		TestFalse(TEXT("거점 문 닫힘 시간은 유한 양수"), IsValidTuning(InvalidHub));
		Update = Transition(FDigetherElevatorState(), EDigetherElevatorCommand::BeginDoorClose, 0.f, TripLimit, TravelSeconds, InvalidHub);
		TestEqual(TEXT("잘못된 거점 문 닫힘 시간 튜닝 거절"), Update.Result, EDigetherElevatorResult::InvalidInput);
		TestEqual(TEXT("잘못된 거점 튜닝의 거점 문 진행률 0"), CalculateDoorCloseProgress(Closed, InvalidHub), 0.f);

		FDigetherElevatorTuning InvalidMine = Tuning;
		InvalidMine.MineDoorCloseSeconds = InvalidSeconds;
		TestFalse(TEXT("갱도 문 닫힘 시간은 유한 양수"), IsValidTuning(InvalidMine));
		Update = Transition(StrayOrigin, EDigetherElevatorCommand::AdvanceTime, 1.f, TripLimit, TravelSeconds, InvalidMine);
		TestEqual(TEXT("잘못된 갱도 문 닫힘 시간 튜닝 거절"), Update.Result, EDigetherElevatorResult::InvalidInput);
		TestEqual(TEXT("잘못된 갱도 튜닝의 갱도 문 진행률 0"), CalculateDoorCloseProgress(MineClosing, InvalidMine), 0.f);
	}
	for (const float InvalidSummon : { -1.f, NAN, INFINITY })
	{
		FDigetherElevatorTuning InvalidTuning = Tuning;
		InvalidTuning.ChainSummonSeconds = InvalidSummon;
		TestFalse(TEXT("사슬 소집 연출 길이는 유한 비음수"), IsValidTuning(InvalidTuning));
	}
	FDigetherElevatorTuning DelayedSummon = Tuning;
	DelayedSummon.ChainSummonSeconds = 1.5f;
	TestTrue(TEXT("양수 소집 연출 길이 허용"), IsValidTuning(DelayedSummon));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherElevatorDoorOverweightPolicyTest,
	"DIGETHER.Game.Elevator.DoorOverweightPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherElevatorDoorOverweightPolicyTest::RunTest(const FString& Parameters)
{
	using namespace DigetherElevatorRules;
	// 테스트 픽스처이며 최종 기획 적재 한도가 아니다.
	constexpr float CapacityKg = 100.f;
	FDigetherElevatorState HubClosing;
	HubClosing.Phase = EDigetherElevatorPhase::DoorClosing;
	HubClosing.DoorOriginPhase = EDigetherElevatorPhase::HubWaiting;
	FDigetherElevatorState MineClosing = HubClosing;
	MineClosing.DoorOriginPhase = EDigetherElevatorPhase::BottomWaiting;
	FDigetherElevatorState MineWaiting;
	MineWaiting.Phase = EDigetherElevatorPhase::BottomWaiting;

	// 기획: 거점 게이지 완료 뒤에는 출발 취소가 없으므로 거점 문은 무게 초과로 다시 열지 않는다.
	TestFalse(TEXT("거점 문 닫힘 중 초과는 재개방하지 않음"), ShouldReopenDoorForOverweight(HubClosing, 101.0, CapacityKg));
	TestTrue(TEXT("갱도 문 닫힘 중 초과는 재개방"), ShouldReopenDoorForOverweight(MineClosing, 101.0, CapacityKg));
	TestFalse(TEXT("갱도 문 한도와 같으면 재개방 없음"), ShouldReopenDoorForOverweight(MineClosing, 100.0, CapacityKg));
	TestFalse(TEXT("문 닫힘이 아니면 재개방 없음"), ShouldReopenDoorForOverweight(MineWaiting, 101.0, CapacityKg));
	TestFalse(TEXT("비유한 무게는 재개방 근거 아님"), ShouldReopenDoorForOverweight(MineClosing, NAN, CapacityKg));
	return true;
}
#endif
