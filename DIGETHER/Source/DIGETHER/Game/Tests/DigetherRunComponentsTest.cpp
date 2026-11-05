#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Run/DigetherRunCoordinatorComponent.h"
#include "Game/Run/DigetherRunPublicStateComponent.h"
#include "Game/Economy/DigetherEconomyTypes.h"
#include "Misc/AutomationTest.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

#include <limits>

namespace
{
	bool HasRunPublicReplication(const UDigetherRunPublicStateComponent* Component)
	{
		UClass* ComponentClass = Component->GetClass();
		ComponentClass->SetUpRuntimeReplicationData();
		const FProperty* Property = FindFProperty<FProperty>(ComponentClass, TEXT("PublicState"));
		if (Property == nullptr || !Property->HasAllPropertyFlags(CPF_Net | CPF_RepNotify))
		{
			return false;
		}
		TArray<FLifetimeProperty> Properties;
		Component->GetLifetimeReplicatedProps(Properties);
		for (const FLifetimeProperty& Entry : Properties)
		{
			if (Entry.RepIndex == Property->RepIndex)
			{
				return Entry.Condition == COND_None;
			}
		}
		return false;
	}

	bool AreRunPublicStatesEqual(const FDigetherRunPublicState& Left, const FDigetherRunPublicState& Right)
	{
		return FDigetherRunPublicState::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherRunCoordinatorFlowTest,
	"DIGETHER.Game.Run.CoordinatorFlow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherRunCoordinatorFlowTest::RunTest(const FString& Parameters)
{
	// 테스트 픽스처이며 최종 기획 값이 아니다.
	FDigetherRunTuning Tuning;
	Tuning.RunSeconds = 100.f;
	Tuning.AscentPenaltySeconds = 20.f;
	FDigetherRunRuleTuning RuleTuning;
	FDigetherRunCoordinatorState State;
	TestEqual(TEXT("시작 전 갱신 거절"), FDigetherRunCoordinatorRules::UpdateTime(State, 10.0).Result,
		EDigetherRunResult::NotInProgress);
	TestEqual(TEXT("시작 전 상승 거절"), FDigetherRunCoordinatorRules::RecordConfirmedAscent(State).Result,
		EDigetherRunResult::NotInProgress);
	TestEqual(TEXT("시작"), FDigetherRunCoordinatorRules::StartRun(State, Tuning, RuleTuning, 10.0).Result,
		EDigetherRunResult::Success);
	TestEqual(TEXT("런 단계 반영"), State.Phase, EDigetherRunPhase::InProgress);
	TestEqual(TEXT("시작 시간"), State.RemainingSeconds, 100.0);
	TestEqual(TEXT("중복 시작 거절"), FDigetherRunCoordinatorRules::StartRun(State, Tuning, RuleTuning, 30.0).Result,
		EDigetherRunResult::AlreadyInProgress);
	TestEqual(TEXT("중복 시작은 시작 시각 보존"), State.StartedAtSeconds, 10.0);
	TestEqual(TEXT("상승 1회"), FDigetherRunCoordinatorRules::RecordConfirmedAscent(State).Result,
		EDigetherRunResult::Success);
	TestEqual(TEXT("상승 횟수"), State.ConfirmedAscentCount, 1);
	TestEqual(TEXT("상승 페널티"), State.RemainingSeconds, 80.0);
	TestEqual(TEXT("서버 시각 갱신"), FDigetherRunCoordinatorRules::UpdateTime(State, 40.0).Result,
		EDigetherRunResult::Success);
	TestEqual(TEXT("경과 시간과 상승 페널티 누적"), State.RemainingSeconds, 50.0);
	TestTrue(TEXT("여진 단계 상승"), State.AftershockStage > 0);
	TestEqual(TEXT("역행 시간 거절"), FDigetherRunCoordinatorRules::UpdateTime(State, 39.0).Result,
		EDigetherRunResult::InvalidInput);
	TestEqual(TEXT("거절 시 남은 시간 불변"), State.RemainingSeconds, 50.0);
	TestEqual(TEXT("NaN 시간 거절"), FDigetherRunCoordinatorRules::UpdateTime(State,
		std::numeric_limits<double>::quiet_NaN()).Result, EDigetherRunResult::InvalidInput);
	TestEqual(TEXT("NaN 거절 시 마지막 시각 보존"), State.LastUpdatedAtSeconds, 40.0);
	const FDigetherRunAdvanceResult Expired = FDigetherRunCoordinatorRules::UpdateTime(State, 90.0);
	TestEqual(TEXT("시간 종료 갱신"), Expired.Result, EDigetherRunResult::Success);
	TestTrue(TEXT("종료 감지 1회"), Expired.bTimeExpiredNow);
	TestEqual(TEXT("시간 0 하한"), State.RemainingSeconds, 0.0);
	TestFalse(TEXT("시간 종료 재통지 없음"), FDigetherRunCoordinatorRules::UpdateTime(State, 100.0).bTimeExpiredNow);

	FDigetherRunTerminationInput Input;
	Input.Trigger = EDigetherRunTerminationTrigger::TimeExpired;
	Input.ElevatorState = EDigetherRunElevatorState::StoppedInMine;
	FDigetherRunTerminationInput InvalidInput = Input;
	InvalidInput.Trigger = static_cast<EDigetherRunTerminationTrigger>(MAX_uint8);
	TestEqual(TEXT("잘못된 종료 입력 거절"), FDigetherRunCoordinatorRules::CompleteRun(State, InvalidInput).Result,
		EDigetherRunResult::InvalidInput);
	TestEqual(TEXT("종료 입력 거절 후 진행 유지"), State.Phase, EDigetherRunPhase::InProgress);
	TestEqual(TEXT("종료 확정"), FDigetherRunCoordinatorRules::CompleteRun(State, Input).Result,
		EDigetherRunResult::Success);
	TestEqual(TEXT("종료 단계 반영"), State.Phase, EDigetherRunPhase::Ended);
	const FDigetherRunPublicState Public = FDigetherRunCoordinatorRules::MakePublicState(State, true, true);
	TestEqual(TEXT("서버 단계가 공개 값에 반영"), Public.Phase, State.Phase);
	TestEqual(TEXT("서버 여진 단계가 공개 값에 반영"), Public.AftershockStage, State.AftershockStage);
	TestTrue(TEXT("확정 게임 오버만 공개 값에 반영"), Public.bGameOver);
	TestTrue(TEXT("확정 납부 마감만 공개 값에 반영"), Public.bPaymentDeadline);
	TestEqual(TEXT("종료 확정은 한 번만"), FDigetherRunCoordinatorRules::CompleteRun(State, Input).Result,
		EDigetherRunResult::AlreadyEnded);
	TestEqual(TEXT("종료 후 갱신 거절"), FDigetherRunCoordinatorRules::UpdateTime(State, 110.0).Result,
		EDigetherRunResult::AlreadyEnded);
	TestEqual(TEXT("종료 후 상승 거절"), FDigetherRunCoordinatorRules::RecordConfirmedAscent(State).Result,
		EDigetherRunResult::AlreadyEnded);
	TestEqual(TEXT("다음 런 시작"), FDigetherRunCoordinatorRules::StartRun(State, Tuning, RuleTuning, 200.0).Result,
		EDigetherRunResult::Success);
	TestEqual(TEXT("다음 런 상승 초기화"), State.ConfirmedAscentCount, 0);
	TestFalse(TEXT("다음 런 종료 통지 이력 초기화"), State.bTimeExpiredNotified);
	TestEqual(TEXT("다음 런 시간 초기화"), State.RemainingSeconds, 100.0);

	FDigetherRunCoordinatorState PenaltyState;
	Tuning.AscentPenaltySeconds = Tuning.RunSeconds;
	FDigetherRunCoordinatorRules::StartRun(PenaltyState, Tuning, RuleTuning, 0.0);
	TestTrue(TEXT("상승만으로 시간 종료 감지"),
		FDigetherRunCoordinatorRules::RecordConfirmedAscent(PenaltyState).bTimeExpiredNow);
	TestFalse(TEXT("상승 후 종료 재통지 없음"),
		FDigetherRunCoordinatorRules::RecordConfirmedAscent(PenaltyState).bTimeExpiredNow);
	FDigetherRunCoordinatorState InvalidStart;
	Tuning.RunSeconds = -1.f;
	TestEqual(TEXT("무효 튜닝 시작 거절"),
		FDigetherRunCoordinatorRules::StartRun(InvalidStart, Tuning, RuleTuning, 0.0).Result,
		EDigetherRunResult::InvalidInput);
	TestEqual(TEXT("무효 시작은 상태 불변"), InvalidStart.Phase, EDigetherRunPhase::NotStarted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherRunClockStartTest,
	"DIGETHER.Game.Run.ClockStartsOnMineArrival", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherRunClockStartTest::RunTest(const FString& Parameters)
{
	// 테스트 픽스처이며 최종 기획 값이 아니다.
	FDigetherRunTuning Tuning;
	Tuning.RunSeconds = 100.f;
	Tuning.AscentPenaltySeconds = 20.f;
	FDigetherRunRuleTuning RuleTuning;
	FDigetherRunCoordinatorState State;
	TestEqual(TEXT("시작 전 시계 시작 거절"), FDigetherRunCoordinatorRules::StartClock(State, 5.0).Result,
		EDigetherRunResult::NotInProgress);

	// 기획: 제한 시간은 최초 하강을 마치고 갱도에 도착한 순간부터 흐른다. 출발~도착 사이에는 줄지 않는다.
	TestEqual(TEXT("시계 정지 상태로 출발"), FDigetherRunCoordinatorRules::StartRun(State, Tuning, RuleTuning, 10.0, false).Result,
		EDigetherRunResult::Success);
	TestFalse(TEXT("출발 직후 시계 정지"), State.bClockStarted);
	TestEqual(TEXT("출발 직후 전체 제한 시간"), State.RemainingSeconds, 100.0);
	const FDigetherRunAdvanceResult Descending = FDigetherRunCoordinatorRules::UpdateTime(State, 500.0);
	TestEqual(TEXT("하강 중 시간 갱신 성공"), Descending.Result, EDigetherRunResult::Success);
	TestFalse(TEXT("하강 중에는 시간 종료 없음"), Descending.bTimeExpiredNow);
	TestEqual(TEXT("하강 중 남은 시간 유지"), State.RemainingSeconds, 100.0);
	TestEqual(TEXT("하강 중 여진 없음"), State.AftershockStage, 0);
	TestEqual(TEXT("과거 시각 시계 시작 거절"), FDigetherRunCoordinatorRules::StartClock(State, 499.0).Result,
		EDigetherRunResult::InvalidInput);
	TestFalse(TEXT("거절 뒤 시계 정지 유지"), State.bClockStarted);

	TestEqual(TEXT("갱도 도착 때 시계 시작"), FDigetherRunCoordinatorRules::StartClock(State, 520.0).Result,
		EDigetherRunResult::Success);
	TestTrue(TEXT("도착 뒤 시계 흐름"), State.bClockStarted);
	TestEqual(TEXT("도착 순간 전체 제한 시간"), State.RemainingSeconds, 100.0);
	TestEqual(TEXT("시계 중복 시작 거절"), FDigetherRunCoordinatorRules::StartClock(State, 530.0).Result,
		EDigetherRunResult::AlreadyInProgress);
	TestEqual(TEXT("중복 시작은 기준 시각 보존"), State.StartedAtSeconds, 520.0);
	FDigetherRunCoordinatorRules::UpdateTime(State, 550.0);
	TestEqual(TEXT("도착 기준 경과 시간 차감"), State.RemainingSeconds, 70.0);
	FDigetherRunCoordinatorRules::RecordConfirmedAscent(State);
	TestEqual(TEXT("상승 페널티는 도착 기준 시간에 누적"), State.RemainingSeconds, 50.0);
	const FDigetherRunAdvanceResult Expired = FDigetherRunCoordinatorRules::UpdateTime(State, 600.0);
	TestTrue(TEXT("도착 기준 제한 시간 종료"), Expired.bTimeExpiredNow);

	// 기본 시작은 기존처럼 즉시 시계를 흐르게 한다(다른 조율 경로·시험 호환).
	FDigetherRunCoordinatorState Immediate;
	FDigetherRunCoordinatorRules::StartRun(Immediate, Tuning, RuleTuning, 0.0);
	TestTrue(TEXT("기본 시작은 시계 즉시 흐름"), Immediate.bClockStarted);
	FDigetherRunCoordinatorRules::UpdateTime(Immediate, 30.0);
	TestEqual(TEXT("기본 시작 경과 차감"), Immediate.RemainingSeconds, 70.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherRunComponentsTest,
	"DIGETHER.Game.Run.Components", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherRunComponentsTest::RunTest(const FString& Parameters)
{
	// 테스트 픽스처이며 최종 기획 값이 아니다.
	UDigetherRunCoordinatorComponent* Coordinator = NewObject<UDigetherRunCoordinatorComponent>();
	UDigetherRunPublicStateComponent* Public = NewObject<UDigetherRunPublicStateComponent>();
	TestFalse(TEXT("조율자 복제 비활성"), Coordinator->GetIsReplicated());
	TestTrue(TEXT("공개 컴포넌트 복제 활성"), Public->GetIsReplicated());
	TestTrue(TEXT("공개 상태 COND_None"), HasRunPublicReplication(Public));
	TestEqual(TEXT("초기 단계"), Public->CaptureState().Phase, EDigetherRunPhase::NotStarted);
	TestTrue(TEXT("조율자 초기 공개 값 일치"),
		AreRunPublicStatesEqual(Coordinator->CapturePublicState(), Public->CaptureState()));
	TestEqual(TEXT("조율자 서버 시간 초기값"), Coordinator->GetRemainingSeconds(), 0.0);
	TestEqual(TEXT("조율자 상승 초기값"), Coordinator->GetConfirmedAscentCount(), 0);

	int32 FieldCount = 0;
	for (TFieldIterator<FProperty> It(FDigetherRunPublicState::StaticStruct()); It; ++It)
	{
		++FieldCount;
		TestFalse(TEXT("공개 값에 시간 숫자 필드 없음"),
			It->IsA<FFloatProperty>() || It->IsA<FDoubleProperty>());
	}
	TestEqual(TEXT("공개 필드는 단계·여진·게임오버·마감 네 개"), FieldCount, 4);
	TestNull(TEXT("남은 시간 필드는 공개 구조체에 없음"),
		FindFProperty<FProperty>(FDigetherRunPublicState::StaticStruct(), TEXT("RemainingSeconds")));

	FDigetherRunPublicState Candidate;
	Candidate.Phase = EDigetherRunPhase::Ended;
	Candidate.AftershockStage = 3;
	Candidate.bGameOver = true;
	Candidate.bPaymentDeadline = true;
	TestTrue(TEXT("확정 공개 값 형태"), FDigetherRunRules::IsValidPublicState(Candidate));
	FDigetherRunPublicState Copied;
	FDigetherRunPublicState::StaticStruct()->CopyScriptStruct(&Copied, &Candidate);
	TestTrue(TEXT("인계 값 전체 필드 복사 왕복"), AreRunPublicStatesEqual(Candidate, Copied));
	FDigetherRunPublicState Invalid = Candidate;
	Invalid.AftershockStage = -1;
	TestFalse(TEXT("잘못된 공개 값 거절"), FDigetherRunRules::IsValidPublicState(Invalid));
	Invalid = Candidate;
	Invalid.Phase = static_cast<EDigetherRunPhase>(MAX_uint8);
	TestFalse(TEXT("잘못된 단계 거절"), FDigetherRunRules::IsValidPublicState(Invalid));

	int32 Notifications = 0;
	FDigetherRunPublicState Notified;
	const FDelegateHandle Handle = Public->OnRunPublicStateChanged.AddLambda(
		[&Notifications, &Notified](const FDigetherRunPublicState& State)
		{
			++Notifications;
			Notified = State;
		});
	const FDigetherRunPublicState Before = Public->CaptureState();
	TestEqual(TEXT("소유자 없는 공개 복원 거절"), Public->RestoreState(Candidate), EDigetherRunResult::NotAuthority);
	TestEqual(TEXT("소유자 없는 지연 복원 거절"), Public->RestoreState(Candidate, false), EDigetherRunResult::NotAuthority);
	TestEqual(TEXT("소유자 없는 공개 통지 거절"), Public->NotifyStateChanged(), EDigetherRunResult::NotAuthority);
	TestTrue(TEXT("거절된 공개 값 불변"), AreRunPublicStatesEqual(Before, Public->CaptureState()));
	TestEqual(TEXT("거절된 복원 통지 없음"), Notifications, 0);
	UFunction* OnRep = Public->FindFunction(TEXT("OnRep_PublicState"));
	if (TestNotNull(TEXT("공개 OnRep 등록"), OnRep))
	{
		Public->ProcessEvent(OnRep, nullptr);
	}
	TestEqual(TEXT("OnRep 표현 통지 한 번"), Notifications, 1);
	TestTrue(TEXT("OnRep은 캡처한 현재 값 전달"), AreRunPublicStatesEqual(Before, Notified));
	TestTrue(TEXT("OnRep은 상태 불변"), AreRunPublicStatesEqual(Before, Public->CaptureState()));

	FDigetherRunTuning Tuning;
	Tuning.RunSeconds = 100.f;
	Tuning.AscentPenaltySeconds = 20.f;
	const FDigetherRunRuleTuning RuleTuning;
	TestEqual(TEXT("권한 없는 공개 연결 거절"), Coordinator->SetPublicStateComponent(Public), EDigetherRunResult::NotAuthority);
	TestEqual(TEXT("권한 없는 시작 거절"), Coordinator->StartRun(Tuning, RuleTuning, 0.0), EDigetherRunResult::NotAuthority);
	TestEqual(TEXT("권한 없는 상승 거절"), Coordinator->RecordConfirmedAscent(), EDigetherRunResult::NotAuthority);
	TestEqual(TEXT("권한 없는 시간 갱신 거절"), Coordinator->UpdateTime(100.0), EDigetherRunResult::NotAuthority);
	TestEqual(TEXT("권한 없는 종료 거절"), Coordinator->CompleteRun(FDigetherRunTerminationInput()).Result,
		EDigetherRunResult::NotAuthority);
	FDigetherQuotaState Quota;
	TestEqual(TEXT("권한 없는 레버 완료 거절"), Coordinator->HandleDepartureLeverCompleted(Quota).Result,
		EDigetherRunResult::NotAuthority);
	TestEqual(TEXT("권한 없는 거점 귀환 거절"), Coordinator->HandleBaseReturn(Quota).Result,
		EDigetherRunResult::NotAuthority);
	TestEqual(TEXT("거절 시 조율자 시간 불변"), Coordinator->GetRemainingSeconds(), 0.0);
	TestEqual(TEXT("거절 시 조율자 단계 불변"), Coordinator->CapturePublicState().Phase, EDigetherRunPhase::NotStarted);
	Public->OnRunPublicStateChanged.Remove(Handle);
	// 월드 없는 검사에서 권위 소유자를 흉내 내면 실제 권한 검사의 증거가 되지 않는다.
	// 실제 Capture/Restore 왕복·공개 반영은 호출자의 직렬 통합 검사에서 확인한다.
	return true;
}

#endif
