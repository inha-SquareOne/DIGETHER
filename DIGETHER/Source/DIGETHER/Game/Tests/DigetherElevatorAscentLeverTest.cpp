#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Elevator/DigetherElevatorRules.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#include <cmath>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherElevatorAscentLeverTest,
	"DIGETHER.Game.Elevator.AscentLever", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherElevatorAscentLeverTest::RunTest(const FString& Parameters)
{
	using namespace DigetherElevatorRules;
	// 테스트 픽스처이며 최종 기획 용량·레버 홀드 시간이 아니다.
	FDigetherElevatorTuning Tuning;
	Tuning.AscentHoldSeconds = 2.f;
	FDigetherElevatorPullContext Context;
	Context.ElevatorPhase = EDigetherElevatorPhase::BottomWaiting;
	Context.CapacityKg = 100.f;
	Context.TripLimit = 2;
	Context.DayNumber = 1;
	const FGuid Puller(1, 0, 0, 1);
	const FGuid OtherPuller(2, 0, 0, 2);
	const FGuid FirstPassenger(3, 0, 0, 3);
	const FGuid FinalPassenger(4, 0, 0, 4);
	const auto SameState = [](const FDigetherElevatorGaugeState& Left, const FDigetherElevatorGaugeState& Right)
	{
		return FDigetherElevatorGaugeState::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
	};
	FDigetherElevatorLoadEntry Cargo;
	Cargo.EntryId = FGuid(5, 0, 0, 5);
	Cargo.Kind = EDigetherElevatorLoadKind::Cargo;
	Cargo.WeightKg = 10.f;
	Context.LoadEntries.Add(Cargo);
	FDigetherElevatorLoadEntry Passenger;
	Passenger.EntryId = FirstPassenger;
	Passenger.Kind = EDigetherElevatorLoadKind::Player;
	Passenger.WeightKg = 80.f;
	Context.LoadEntries.Add(Passenger);
	FDigetherElevatorGaugeState State;
	FDigetherElevatorGaugeUpdate InvalidPuller = BeginPull(State, EDigetherElevatorLeverKind::Ascent, FGuid(), Context, Tuning);
	TestEqual(TEXT("발급되지 않은 조작자 ID 거절"), InvalidPuller.Feedback.Result, EDigetherElevatorLeverResult::InvalidInput);
	TestTrue(TEXT("조작자 ID 오류 상태 불변"), SameState(State, InvalidPuller.State));
	for (const EDigetherElevatorPhase Phase : { EDigetherElevatorPhase::HubWaiting,
		EDigetherElevatorPhase::InitialDescent, EDigetherElevatorPhase::Ascending,
		EDigetherElevatorPhase::UpperStop, EDigetherElevatorPhase::ReturnDescent,
		EDigetherElevatorPhase::FinalAscent, EDigetherElevatorPhase::Finished })
	{
		FDigetherElevatorPullContext WrongPhase = Context;
		WrongPhase.ElevatorPhase = Phase;
		const FDigetherElevatorGaugeUpdate Rejected = BeginPull(State, EDigetherElevatorLeverKind::Ascent, Puller, WrongPhase, Tuning);
		TestEqual(TEXT("상승 레버는 하부 대기에서만 시작"), Rejected.Feedback.Result, EDigetherElevatorLeverResult::InvalidPhase);
		TestTrue(TEXT("잘못된 운행 단계에서 게이지 불변"), SameState(State, Rejected.State));
	}
	Context.LoadEntries[0].WeightKg = 101.f;
	FDigetherElevatorGaugeUpdate Update = BeginPull(State, EDigetherElevatorLeverKind::Ascent, Puller, Context, Tuning);
	TestEqual(TEXT("시작 시 과적은 경고와 덜컹 결과"), Update.Feedback.Result, EDigetherElevatorLeverResult::OverweightWarningAndRattle);
	TestTrue(TEXT("과적 시작 경고"), Update.Feedback.bWarning);
	TestTrue(TEXT("과적 시작 덜컹"), Update.Feedback.bRattle);
	TestEqual(TEXT("과적 시작 진행률 0"), CalculateGaugeProgress(Update.State), 0.f);
	TestTrue(TEXT("과적 시작은 게이지 상태 불변"), SameState(State, Update.State));

	Context.LoadEntries[0].WeightKg = 100.f;
	Update = BeginPull(State, EDigetherElevatorLeverKind::Ascent, Puller, Context, Tuning);
	TestEqual(TEXT("한도와 같은 적재는 레버 시작"), Update.Feedback.Result, EDigetherElevatorLeverResult::Started);
	State = Update.State;
	TestEqual(TEXT("상승 홀드 시간 적용"), State.DurationSeconds, Tuning.AscentHoldSeconds);
	Update = BeginPull(State, EDigetherElevatorLeverKind::Ascent, OtherPuller, Context, Tuning);
	TestEqual(TEXT("다른 플레이어의 중복 점유 거절"), Update.Feedback.Result, EDigetherElevatorLeverResult::Busy);
	TestTrue(TEXT("점유 거절은 기존 게이지 보존"), SameState(State, Update.State));
	Update = ReleasePull(State, OtherPuller);
	TestEqual(TEXT("점유자 아닌 해제 거절"), Update.Feedback.Result, EDigetherElevatorLeverResult::NotPuller);
	TestTrue(TEXT("잘못된 해제는 기존 게이지 보존"), SameState(State, Update.State));
	Update = AdvancePull(State, EDigetherElevatorLeverKind::Ascent, 1.f, Context, Tuning);
	TestEqual(TEXT("홀드 중간 진행"), Update.Feedback.Result, EDigetherElevatorLeverResult::InProgress);
	TestEqual(TEXT("홀드 절반 진행률"), CalculateGaugeProgress(Update.State), 0.5f);
	State = Update.State;
	Update = BeginPull(State, EDigetherElevatorLeverKind::Ascent, Puller, Context, Tuning);
	TestEqual(TEXT("같은 점유자의 중복 시작은 진행 유지"), Update.Feedback.Result, EDigetherElevatorLeverResult::InProgress);
	TestTrue(TEXT("중복 시작은 진행 게이지를 초기화하지 않음"), SameState(State, Update.State));
	Update = ReleasePull(State, Puller);
	TestEqual(TEXT("레버 놓기는 해제"), Update.Feedback.Result, EDigetherElevatorLeverResult::Released);
	TestEqual(TEXT("레버 놓기 진행률 0"), CalculateGaugeProgress(Update.State), 0.f);
	TestEqual(TEXT("레버 놓기는 대기 상태"), Update.State.Phase, EDigetherElevatorGaugePhase::Idle);
	TestFalse(TEXT("레버 놓기는 점유자 제거"), Update.State.PullerId.IsValid());
	Update = BeginPull(Update.State, EDigetherElevatorLeverKind::Ascent, Puller, Context, Tuning);
	Update = AdvancePull(Update.State, EDigetherElevatorLeverKind::Ascent, 1.f, Context, Tuning);
	Context.LoadEntries[0].WeightKg = 101.f;
	Update = AdvancePull(Update.State, EDigetherElevatorLeverKind::Ascent, 1.f, Context, Tuning);
	TestEqual(TEXT("홀드 중 과적은 취소"), Update.Feedback.Result, EDigetherElevatorLeverResult::InterruptedOverweight);
	TestTrue(TEXT("홀드 중 과적 경고"), Update.Feedback.bWarning);
	TestTrue(TEXT("홀드 중 과적 덜컹"), Update.Feedback.bRattle);
	TestEqual(TEXT("홀드 중 과적 진행률 0"), CalculateGaugeProgress(Update.State), 0.f);
	TestEqual(TEXT("홀드 중 과적은 대기 상태"), Update.State.Phase, EDigetherElevatorGaugePhase::Idle);
	TestEqual(TEXT("취소는 완료 탑승자 명세를 만들지 않음"), Update.State.Ascent.PassengerIds.Num(), 0);

	Context.LoadEntries[0].WeightKg = 10.f;
	State = BeginPull(FDigetherElevatorGaugeState(), EDigetherElevatorLeverKind::Ascent, Puller, Context, Tuning).State;
	for (const float InvalidTime : { -1.f, NAN, INFINITY, -INFINITY })
	{
		Update = AdvancePull(State, EDigetherElevatorLeverKind::Ascent, InvalidTime, Context, Tuning);
		TestEqual(TEXT("손상된 상승 홀드 시간 거절"), Update.Feedback.Result, EDigetherElevatorLeverResult::InvalidInput);
		TestTrue(TEXT("시간 오류는 홀드 상태 불변"), SameState(State, Update.State));
	}
	Update = AdvancePull(State, static_cast<EDigetherElevatorLeverKind>(MAX_uint8), 1.f, Context, Tuning);
	TestEqual(TEXT("정의되지 않은 레버 종류 거절"), Update.Feedback.Result, EDigetherElevatorLeverResult::InvalidInput);
	TestTrue(TEXT("레버 종류 오류 상태 불변"), SameState(State, Update.State));
	FDigetherElevatorPullContext InvalidContext = Context;
	InvalidContext.LoadEntries[0].Kind = static_cast<EDigetherElevatorLoadKind>(MAX_uint8);
	Update = AdvancePull(State, EDigetherElevatorLeverKind::Ascent, 1.f, InvalidContext, Tuning);
	TestEqual(TEXT("정의되지 않은 적재 종류 거절"), Update.Feedback.Result, EDigetherElevatorLeverResult::InvalidInput);
	TestTrue(TEXT("적재 종류 오류 상태 불변"), SameState(State, Update.State));

	State = AdvancePull(State, EDigetherElevatorLeverKind::Ascent, 1.f, Context, Tuning).State;
	TestEqual(TEXT("확정 전에는 명세를 동결하지 않음"), State.Ascent.LoadEntries.Num(), 0);
	Context.LoadEntries[0].WeightKg = 25.f;
	Context.LoadEntries[1].EntryId = FinalPassenger;
	FDigetherElevatorLoadEntry Corpse;
	Corpse.EntryId = FGuid(6, 0, 0, 6);
	Corpse.Kind = EDigetherElevatorLoadKind::Corpse;
	Corpse.WeightKg = 90.f;
	Context.LoadEntries.Add(Corpse);
	Update = AdvancePull(State, EDigetherElevatorLeverKind::Ascent, 1.f, Context, Tuning);
	TestEqual(TEXT("정확한 홀드 시간에 상승 확정"), Update.Feedback.Result, EDigetherElevatorLeverResult::Confirmed);
	State = Update.State;
	TestEqual(TEXT("확정 진행률 1"), CalculateGaugeProgress(State), 1.f);
	TestEqual(TEXT("첫 상승 편 번호"), State.Ascent.TripNumber, 1);
	TestFalse(TEXT("2회 설정 첫 편은 마지막 아님"), State.Ascent.bFinalAscent);
	TestEqual(TEXT("완료 순간 탑승자 수"), State.Ascent.PassengerIds.Num(), 1);
	TestTrue(TEXT("완료 순간의 탑승자를 포함"), State.Ascent.PassengerIds.Contains(FinalPassenger));
	TestFalse(TEXT("누르기 시작 때 떠난 탑승자는 제외"), State.Ascent.PassengerIds.Contains(FirstPassenger));
	TestEqual(TEXT("완료 순간 시체 포함 전체 명세"), State.Ascent.LoadEntries.Num(), 3);
	TestEqual(TEXT("완료 순간 총중량"), State.Ascent.TotalWeightKg, 25.0);
	if (State.Ascent.LoadEntries.Num() == 3)
	{
		TestEqual(TEXT("명세 플레이어 저장 무게 0"), State.Ascent.LoadEntries[1].WeightKg, 0.f);
		TestEqual(TEXT("명세 시체 저장 무게 0"), State.Ascent.LoadEntries[2].WeightKg, 0.f);
	}
	const FDigetherElevatorAscentConfirmed LastTrip = CaptureAscent(1, 2, Context.LoadEntries, Context.CapacityKg, Tuning);
	TestEqual(TEXT("두 번째 확정 명세 편 번호"), LastTrip.TripNumber, 2);
	TestTrue(TEXT("두 번째 확정 명세는 마지막 편"), LastTrip.bFinalAscent);
	const FDigetherElevatorAscentConfirmed SingleTrip = CaptureAscent(0, 1, Context.LoadEntries, Context.CapacityKg, Tuning);
	TestTrue(TEXT("1회 설정 확정 명세는 첫 편부터 마지막"), SingleTrip.bFinalAscent);
	Context.LoadEntries[0].WeightKg = 999.f;
	Context.LoadEntries[1].EntryId = FirstPassenger;
	Context.LoadEntries.Reset();
	TestEqual(TEXT("입력 배열 변경 후 확정 명세 무게 보존"), State.Ascent.TotalWeightKg, 25.0);
	TestEqual(TEXT("입력 배열 제거 후 확정 명세 목록 보존"), State.Ascent.LoadEntries.Num(), 3);
	TestTrue(TEXT("입력 변경 후 확정 탑승자 보존"), State.Ascent.PassengerIds.Contains(FinalPassenger));
	Update = AdvancePull(State, EDigetherElevatorLeverKind::Ascent, 1.f, Context, Tuning);
	TestEqual(TEXT("확정 후 완료 결과를 다시 내지 않음"), Update.Feedback.Result, EDigetherElevatorLeverResult::AlreadyConfirmed);
	TestTrue(TEXT("중복 확정 요청은 명세와 상태 불변"), SameState(State, Update.State));
	// float 저장 시 완료 시각으로 반올림돼도 Pulling에 갇히면 안 된다.
	FDigetherElevatorGaugeState RoundingState = BeginPull(FDigetherElevatorGaugeState(),
		EDigetherElevatorLeverKind::Ascent, Puller, Context, Tuning).State;
	RoundingState.ElapsedSeconds = std::nextafter(Tuning.AscentHoldSeconds, 0.f);
	const float RoundingDelta = (Tuning.AscentHoldSeconds - RoundingState.ElapsedSeconds) / 2.f;
	Update = AdvancePull(RoundingState, EDigetherElevatorLeverKind::Ascent, RoundingDelta, Context, Tuning);
	TestEqual(TEXT("시각 반올림 경계에서도 게이지 확정"), Update.Feedback.Result, EDigetherElevatorLeverResult::Confirmed);
	TestEqual(TEXT("시각 반올림 경계에서 공개 진행률 1"), CalculateGaugeProgress(Update.State), 1.f);
	return true;
}

#endif
