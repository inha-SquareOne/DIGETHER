#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Game/Data/DigetherTuningProfile.h"
#include "Game/Elevator/DigetherElevatorComponent.h"
#include "Game/Elevator/DigetherElevatorRules.h"
#include "GameFramework/Actor.h"
#include "Game/Tests/DigetherTestValues.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#include <cmath>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherElevatorAscentLeverTest,
	"DIGETHER.Game.Elevator.AscentLever", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherElevatorAscentLeverTest::RunTest(const FString& Parameters)
{
	using namespace DigetherElevatorRules;
	// 테스트 픽스처, 기획 용량·레버 홀드 시간 아님
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
		EDigetherElevatorPhase::FinalAscent, EDigetherElevatorPhase::Finished, EDigetherElevatorPhase::DoorClosing })
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
	for (const float InvalidTime : { -1.f, DigetherTestValues::NaN(), DigetherTestValues::Infinity(), -DigetherTestValues::Infinity() })
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

	// 아래 명세는 게이지 완료 순간의 레버 스냅샷, 기획 A3 편 확정 목록은 문이 완전히 닫힌 순간
	// 조율자가 CaptureAscent로 재생성(Elevator.Components, Framework.SaveIntegration 문 닫힘 검사)
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
	const FDigetherElevatorAscentConfirmed FirstTrip = CaptureAscent(0, 2, Context.LoadEntries, Context.CapacityKg, Tuning);
	TestFalse(TEXT("2회 확정의 첫 편은 마지막 아님"), FirstTrip.bFinalAscent);
	// 1회 비교 설정은 폐기, 명세 미생성
	const FDigetherElevatorAscentConfirmed SingleTrip = CaptureAscent(0, 1, Context.LoadEntries, Context.CapacityKg, Tuning);
	TestEqual(TEXT("1회 설정은 명세 거절"), SingleTrip.TripNumber, 0);
	Context.LoadEntries[0].WeightKg = 999.f;
	Context.LoadEntries[1].EntryId = FirstPassenger;
	Context.LoadEntries.Reset();
	TestEqual(TEXT("입력 배열 변경 후 확정 명세 무게 보존"), State.Ascent.TotalWeightKg, 25.0);
	TestEqual(TEXT("입력 배열 제거 후 확정 명세 목록 보존"), State.Ascent.LoadEntries.Num(), 3);
	TestTrue(TEXT("입력 변경 후 확정 탑승자 보존"), State.Ascent.PassengerIds.Contains(FinalPassenger));
	Update = AdvancePull(State, EDigetherElevatorLeverKind::Ascent, 1.f, Context, Tuning);
	TestEqual(TEXT("확정 후 완료 결과를 다시 내지 않음"), Update.Feedback.Result, EDigetherElevatorLeverResult::AlreadyConfirmed);
	TestTrue(TEXT("중복 확정 요청은 명세와 상태 불변"), SameState(State, Update.State));
	// float 저장 반올림으로 완료 시각이 돼도 Pulling 고착 금지
	FDigetherElevatorGaugeState RoundingState = BeginPull(FDigetherElevatorGaugeState(),
		EDigetherElevatorLeverKind::Ascent, Puller, Context, Tuning).State;
	RoundingState.ElapsedSeconds = std::nextafter(Tuning.AscentHoldSeconds, 0.f);
	const float RoundingDelta = (Tuning.AscentHoldSeconds - RoundingState.ElapsedSeconds) / 2.f;
	Update = AdvancePull(RoundingState, EDigetherElevatorLeverKind::Ascent, RoundingDelta, Context, Tuning);
	TestEqual(TEXT("시각 반올림 경계에서도 게이지 확정"), Update.Feedback.Result, EDigetherElevatorLeverResult::Confirmed);
	TestEqual(TEXT("시각 반올림 경계에서 공개 진행률 1"), CalculateGaugeProgress(Update.State), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherElevatorAscentLeverPairTest,
	"DIGETHER.Game.Elevator.AscentLeverPair", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherElevatorAscentLeverPairTest::RunTest(const FString& Parameters)
{
	using namespace DigetherElevatorRules;
	// 테스트 픽스처, 기획 용량·레버 홀드 시간 아님
	FDigetherElevatorTuning Tuning;
	Tuning.AscentHoldSeconds = 2.f;
	FDigetherElevatorPullContext Context;
	Context.ElevatorPhase = EDigetherElevatorPhase::BottomWaiting;
	Context.CapacityKg = 100.f;
	Context.TripLimit = 2;
	Context.DayNumber = 1;
	const FGuid Puller(11, 0, 0, 11);
	const auto SameState = [](const FDigetherElevatorGaugeState& Left, const FDigetherElevatorGaugeState& Right)
	{
		return FDigetherElevatorGaugeState::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
	};
	FDigetherElevatorLoadEntry Cargo;
	Cargo.EntryId = FGuid(12, 0, 0, 12);
	Cargo.Kind = EDigetherElevatorLoadKind::Cargo;
	Cargo.WeightKg = 30.f;
	Context.LoadEntries.Add(Cargo);

	// S15: 안팎 레버 동시 당김은 먼저 시작한 한 건만 수락, 늦은 레버는 Idle 유지
	const FDigetherElevatorGaugeState Idle = FDigetherElevatorGaugeState();
	FDigetherElevatorPullContext Occupied = Context;
	Occupied.bOtherLeverOccupied = true;
	FDigetherElevatorGaugeUpdate Update = BeginPull(Idle, EDigetherElevatorLeverKind::Ascent, Puller, Occupied, Tuning);
	TestEqual(TEXT("다른 레버 점유 중 시작은 Busy"), Update.Feedback.Result, EDigetherElevatorLeverResult::Busy);
	TestTrue(TEXT("Busy 거절은 Idle 유지"), SameState(Idle, Update.State));
	TestEqual(TEXT("Busy 거절 진행률 0"), CalculateGaugeProgress(Update.State), 0.f);
	TestFalse(TEXT("Busy 거절은 경고 없음"), Update.Feedback.bWarning);
	TestFalse(TEXT("Busy 거절은 덜컹 없음"), Update.Feedback.bRattle);
	// 기획 확인 항목: 무게 초과여도 다른 레버 점유 먼저 통지
	Occupied.LoadEntries[0].WeightKg = 101.f;
	Update = BeginPull(Idle, EDigetherElevatorLeverKind::Ascent, Puller, Occupied, Tuning);
	TestEqual(TEXT("점유 중 무게 초과도 Busy 우선"), Update.Feedback.Result, EDigetherElevatorLeverResult::Busy);
	TestFalse(TEXT("점유 중 무게 초과는 덜컹 없음"), Update.Feedback.bRattle);
	TestTrue(TEXT("점유 중 무게 초과도 Idle 유지"), SameState(Idle, Update.State));

	// 점유 해제(먼저 당긴 사람이 놓음) 시 같은 레버 즉시 시작 가능
	Update = BeginPull(Idle, EDigetherElevatorLeverKind::Ascent, Puller, Context, Tuning);
	TestEqual(TEXT("점유 해제 후 시작"), Update.Feedback.Result, EDigetherElevatorLeverResult::Started);
	// 진행 중인 자기 레버는 점유 표시와 무관하게 진행 유지
	const FDigetherElevatorGaugeState Pulling = Update.State;
	Update = BeginPull(Pulling, EDigetherElevatorLeverKind::Ascent, Puller, Occupied, Tuning);
	TestEqual(TEXT("진행 중 자기 레버 재요청은 진행 유지"), Update.Feedback.Result, EDigetherElevatorLeverResult::InProgress);
	TestTrue(TEXT("진행 중 자기 레버 상태 보존"), SameState(Pulling, Update.State));

	// 맵 레버 구성: 출발 1, 상승 안 1, 밖 1만 허용
	TestTrue(TEXT("출발 1·안 1·밖 1 허용"), IsValidLeverLayout(1, 1, 1));
	TestFalse(TEXT("밖 레버 없음 거절"), IsValidLeverLayout(1, 1, 0));
	TestFalse(TEXT("안 레버 없음 거절"), IsValidLeverLayout(1, 0, 1));
	TestFalse(TEXT("출발 레버 없음 거절"), IsValidLeverLayout(0, 1, 1));
	TestFalse(TEXT("출발 레버 2개 거절"), IsValidLeverLayout(2, 1, 1));
	TestFalse(TEXT("안 레버 2개 거절"), IsValidLeverLayout(1, 2, 1));
	TestFalse(TEXT("밖 레버 2개 거절"), IsValidLeverLayout(1, 1, 2));
	TestFalse(TEXT("음수 개수 거절"), IsValidLeverLayout(1, -1, 1));

	// T93: 탑승자 없는 화물 단독 편도 같은 명세로 고정
	const FDigetherElevatorAscentConfirmed CargoOnly = CaptureAscent(0, Context.TripLimit, Context.LoadEntries,
		Context.CapacityKg, Tuning);
	TestEqual(TEXT("화물 단독 편 번호"), CargoOnly.TripNumber, 1);
	TestEqual(TEXT("화물 단독 편 탑승자 없음"), CargoOnly.PassengerIds.Num(), 0);
	TestEqual(TEXT("화물 단독 편 화물 목록"), CargoOnly.LoadEntries.Num(), 1);
	TestEqual(TEXT("화물 단독 편 무게"), CargoOnly.TotalWeightKg, 30.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherElevatorCargoOnlyAscentTest,
	"DIGETHER.Game.Elevator.CargoOnlyAscent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherElevatorCargoOnlyAscentTest::RunTest(const FString& Parameters)
{
	// T93 밖 레버 화물 단독 상승: 탑승자 없는 화물 명세의 운행 컴포넌트 상승 수락 확인
	// 화물 적재 경로가 없어 조율자 대신 문 닫힘 순간 화물 목록 직접 전달, 값은 테스트 픽스처
	FDigetherRunTuning Run;
	Run.ElevatorCapacityKg = 100.f;
	Run.ElevatorTripLimit = 2;
	Run.ElevatorTravelSeconds = 2.f;
	const FDigetherElevatorTuning Tuning;
	UWorld::InitializationValues Values;
	Values.InitializeScenes(false).AllowAudioPlayback(false).CreatePhysicsScene(false)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false)
		.CreateFXSystem(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("디스크 맵 없는 임시 월드"), World))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		World->DestroyWorld(false);
	};
	AActor* Owner = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("서버 소유 액터"), Owner))
	{
		return false;
	}
	Owner->SetReplicates(true);
	UDigetherElevatorComponent* Elevator = NewObject<UDigetherElevatorComponent>(Owner);
	Owner->AddInstanceComponent(Elevator);
	Elevator->RegisterComponent();

	FDigetherElevatorLoadEntry Cargo;
	Cargo.EntryId = FGuid(21, 0, 0, 21);
	Cargo.Kind = EDigetherElevatorLoadKind::Cargo;
	Cargo.WeightKg = 30.f;
	const TArray<FDigetherElevatorLoadEntry> Entries = { Cargo };
	TestEqual(TEXT("새 런 초기화"), Elevator->InitializeForRun(Run, Tuning), EDigetherElevatorResult::Success);
	TestEqual(TEXT("첫 하강"), Elevator->DepartHub(), EDigetherElevatorResult::Success);
	TestEqual(TEXT("첫 하강 완료"), Elevator->AdvanceTravel(Run.ElevatorTravelSeconds), EDigetherElevatorResult::Success);
	TestEqual(TEXT("하부 대기"), Elevator->GetPublicState().Phase, EDigetherElevatorPhase::BottomWaiting);
	TestEqual(TEXT("화물만 적재"), Elevator->UpdateLoad(Entries), EDigetherElevatorResult::Success);
	TestEqual(TEXT("탑승자 없이 문 닫힘 시작"), Elevator->BeginDoorClose(), EDigetherElevatorResult::Success);
	TestEqual(TEXT("갱도 문 닫힘 완료"), Elevator->AdvanceTravel(Tuning.MineDoorCloseSeconds), EDigetherElevatorResult::DoorClosed);
	const FDigetherElevatorAscentConfirmed Manifest = DigetherElevatorRules::CaptureAscent(
		Elevator->GetPublicState().CompletedAscents, Run.ElevatorTripLimit, Entries, Run.ElevatorCapacityKg, Tuning);
	TestEqual(TEXT("화물 단독 명세 탑승자 없음"), Manifest.PassengerIds.Num(), 0);
	TestEqual(TEXT("화물 단독 편 상승 확정"), Elevator->ConfirmAscent(Manifest), EDigetherElevatorResult::Success);
	TestEqual(TEXT("화물 단독 편 상승 시작"), Elevator->GetPublicState().Phase, EDigetherElevatorPhase::Ascending);
	FDigetherElevatorAscentConfirmed Stored;
	if (TestTrue(TEXT("확정 명세 보관"), Elevator->TryGetLastAscent(Stored)))
	{
		TestEqual(TEXT("보관 명세는 화물만"), Stored.LoadEntries.Num(), 1);
		TestEqual(TEXT("보관 명세 탑승자 없음"), Stored.PassengerIds.Num(), 0);
	}
	return true;
}

#endif
