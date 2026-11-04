#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Elevator/DigetherElevatorComponent.h"
#include "Game/Elevator/DigetherElevatorLeverComponent.h"
#include "Game/Elevator/DigetherElevatorRules.h"
#include "Game/Data/DigetherTuningProfile.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

namespace
{
	// 디스크 맵과 기존 에디터 월드에 영향을 주지 않고 조기 반환 때도 정리한다.
	class FDigetherElevatorTestWorld
	{
	public:
		FDigetherElevatorTestWorld()
		{
			UWorld::InitializationValues Values;
			Values.InitializeScenes(false).AllowAudioPlayback(false).CreatePhysicsScene(false)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false)
				.CreateFXSystem(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &Values);
		}

		~FDigetherElevatorTestWorld()
		{
			if (World != nullptr)
			{
				World->DestroyWorld(false);
			}
		}

		FDigetherElevatorTestWorld(const FDigetherElevatorTestWorld&) = delete;
		FDigetherElevatorTestWorld& operator=(const FDigetherElevatorTestWorld&) = delete;

		UWorld* World = nullptr;
	};

	bool HasPublicReplication(const UActorComponent* Component)
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
		for (TFieldIterator<FProperty> It(ComponentClass); It; ++It)
		{
			if (It->GetOwnerStruct() != ComponentClass || !It->HasAnyPropertyFlags(CPF_Net))
			{
				continue;
			}
			if (It->GetFName() != TEXT("PublicState"))
			{
				return false;
			}
		}
		for (const FLifetimeProperty& Replicated : Properties)
		{
			if (Replicated.RepIndex == Property->RepIndex)
			{
				return Replicated.Condition == COND_None;
			}
		}
		return false;
	}

	bool HasOnlyExpectedPublicFields(const UScriptStruct* Struct, const TArray<FName>& Allowed)
	{
		int32 Count = 0;
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (!Allowed.Contains(It->GetFName()))
			{
				return false;
			}
			++Count;
		}
		return Count == Allowed.Num();
	}

	bool AreServerFieldsUnreplicated(const UActorComponent* Component, const TArray<FName>& Names)
	{
		for (const FName Name : Names)
		{
			const FProperty* Property = FindFProperty<FProperty>(Component->GetClass(), Name);
			if (Property == nullptr || Property->HasAnyPropertyFlags(CPF_Net))
			{
				return false;
			}
			// 비복제 필드의 RepIndex는 미할당이므로 인덱스 비교에 쓰지 않는다.
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherElevatorComponentsTest,
	"DIGETHER.Game.Elevator.Components", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherElevatorComponentsTest::RunTest(const FString& Parameters)
{
	// 테스트 픽스처이며 최종 기획 값이 아니다.
	FDigetherRunTuning Run;
	Run.ElevatorCapacityKg = 100.f;
	Run.ElevatorTripLimit = 1;
	Run.ElevatorTravelSeconds = 2.f;
	FDigetherElevatorTuning Tuning;
	Tuning.AscentHoldSeconds = 1.f;
	Tuning.DepartureHoldSeconds = 1.f;
	Tuning.UpperStopSeconds = 1.f;
	const FGuid Puller(1, 2, 3, 4);
	const FGuid OtherPuller(5, 6, 7, 8);

	UDigetherElevatorComponent* Orphan = NewObject<UDigetherElevatorComponent>();
	UDigetherElevatorLeverComponent* OrphanLever = NewObject<UDigetherElevatorLeverComponent>();
	TestTrue(TEXT("엘리베이터 기본 복제 활성"), Orphan->GetIsReplicated());
	TestTrue(TEXT("레버 기본 복제 활성"), OrphanLever->GetIsReplicated());
	TestTrue(TEXT("진행 공개 값은 조건 없는 복제"), HasPublicReplication(Orphan));
	TestTrue(TEXT("레버 공개 값은 조건 없는 복제"), HasPublicReplication(OrphanLever));
	TestTrue(TEXT("엘리베이터 공개 구조체는 허용 값만 포함"),
		HasOnlyExpectedPublicFields(FDigetherElevatorPublicState::StaticStruct(),
			{ TEXT("Phase"), TEXT("CompletedAscents"), TEXT("TravelProgress"), TEXT("WeightStage") }));
	TestTrue(TEXT("레버 공개 구조체에는 진행률과 경고만 포함"),
		HasOnlyExpectedPublicFields(FDigetherElevatorLeverPublicState::StaticStruct(),
			{ TEXT("Progress"), TEXT("bWarningActive") }));
	TestTrue(TEXT("정확 무게와 매니페스트 보관은 비복제"),
		AreServerFieldsUnreplicated(Orphan, { TEXT("LastAscent"), TEXT("State") }));
	TestTrue(TEXT("매니페스트와 당기는 사람을 가진 게이지는 비복제"),
		AreServerFieldsUnreplicated(OrphanLever, { TEXT("GaugeState") }));

	FDigetherElevatorPullContext Context;
	Context.CapacityKg = Run.ElevatorCapacityKg;
	Context.TripLimit = Run.ElevatorTripLimit;
	Context.DayNumber = 1;
	FDigetherElevatorAscentConfirmed Snapshot;
	TestEqual(TEXT("소유자 없는 초기화 거부"), Orphan->InitializeForRun(Run, Tuning), EDigetherElevatorResult::NotAuthority);
	TestEqual(TEXT("소유자 없는 하강 거부"), Orphan->DepartHub(), EDigetherElevatorResult::NotAuthority);
	TestEqual(TEXT("소유자 없는 시간 진행 거부"), Orphan->AdvanceTravel(Run.ElevatorTravelSeconds), EDigetherElevatorResult::NotAuthority);
	TestEqual(TEXT("소유자 없는 적재 갱신 거부"), Orphan->UpdateLoad(Context.LoadEntries), EDigetherElevatorResult::NotAuthority);
	TestEqual(TEXT("소유자 없는 상승 확정 거부"), Orphan->ConfirmAscent(Snapshot), EDigetherElevatorResult::NotAuthority);
	TestFalse(TEXT("소유자 없는 서버 스냅샷 조회 거부"), Orphan->TryGetLastAscent(Snapshot));
	FDigetherElevatorState ServerState;
	TestFalse(TEXT("소유자 없는 서버 상태 조회 거부"), Orphan->TryGetServerState(ServerState));
	TestEqual(TEXT("소유자 없는 레버 설정 거부"), OrphanLever->Configure(EDigetherElevatorLeverKind::Ascent, Tuning), EDigetherElevatorLeverResult::NotAuthority);
	TestEqual(TEXT("소유자 없는 레버 시작 거부"), OrphanLever->TryBeginPull(Puller, Context), EDigetherElevatorLeverResult::NotAuthority);
	TestEqual(TEXT("소유자 없는 레버 진행 거부"), OrphanLever->AdvancePull(Tuning.AscentHoldSeconds, Context), EDigetherElevatorLeverResult::NotAuthority);
	TestEqual(TEXT("소유자 없는 레버 해제 거부"), OrphanLever->ReleasePull(Puller), EDigetherElevatorLeverResult::NotAuthority);
	TestEqual(TEXT("소유자 없는 레버 초기화 거부"), OrphanLever->ResetPull(), EDigetherElevatorLeverResult::NotAuthority);
	TestFalse(TEXT("소유자 없는 레버 스냅샷 조회 거부"), OrphanLever->TryGetConfirmedAscent(Snapshot));
	TestEqual(TEXT("권한 거절은 공개 진행 상태 불변"), Orphan->GetPublicState().Phase, EDigetherElevatorPhase::HubWaiting);
	TestEqual(TEXT("권한 거절은 공개 게이지 불변"), OrphanLever->GetPublicState().Progress, 0.f);

	FDigetherElevatorTestWorld Fixture;
	if (!TestNotNull(TEXT("디스크 맵 없는 임시 월드"), Fixture.World))
	{
		return false;
	}
	AActor* Owner = Fixture.World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("서버 소유 액터"), Owner))
	{
		return false;
	}
	Owner->SetReplicates(true);
	TestTrue(TEXT("임시 월드는 서버 권위 경로"), Owner->HasAuthority());
	UDigetherElevatorComponent* Elevator = NewObject<UDigetherElevatorComponent>(Owner);
	UDigetherElevatorLeverComponent* Lever = NewObject<UDigetherElevatorLeverComponent>(Owner);
	Owner->AddInstanceComponent(Elevator);
	Owner->AddInstanceComponent(Lever);
	Elevator->RegisterComponent();
	Lever->RegisterComponent();

	int32 StateNotifications = 0;
	int32 AscentNotifications = 0;
	int32 FinishNotifications = 0;
	int32 LeverAscentNotifications = 0;
	int32 DepartureNotifications = 0;
	int32 RattleNotifications = 0;
	FDigetherElevatorAscentConfirmed NotifiedAscent;
	const FDelegateHandle StateHandle = Elevator->OnStateChanged.AddLambda(
		[&StateNotifications](const FDigetherElevatorPublicState& Value)
		{
			++StateNotifications;
		});
	const FDelegateHandle AscentHandle = Elevator->OnAscentConfirmed.AddLambda(
		[&AscentNotifications, &NotifiedAscent](const FDigetherElevatorAscentConfirmed& Value)
		{
			++AscentNotifications;
			NotifiedAscent = Value;
		});
	const FDelegateHandle FinishHandle = Elevator->OnExplorationFinished.AddLambda(
		[&FinishNotifications](const FDigetherElevatorAscentConfirmed& Value)
		{
			++FinishNotifications;
		});
	const FDelegateHandle LeverAscentHandle = Lever->OnAscentConfirmed.AddLambda(
		[&LeverAscentNotifications](const FDigetherElevatorAscentConfirmed& Value)
		{
			++LeverAscentNotifications;
		});
	const FDelegateHandle DepartureHandle = Lever->OnDepartureConfirmed.AddLambda(
		[&DepartureNotifications](const FDigetherElevatorDepartureConfirmed& Value)
		{
			++DepartureNotifications;
		});
	const FDelegateHandle FeedbackHandle = Lever->OnFeedback.AddLambda(
		[&RattleNotifications](const FDigetherElevatorLeverFeedback& Value)
		{
			if (Value.bWarning && Value.bRattle)
			{
				++RattleNotifications;
			}
		});

	TestEqual(TEXT("초기화 전 서버 운행 거부"), Elevator->DepartHub(), EDigetherElevatorResult::NotInitialized);
	TestEqual(TEXT("새 런 초기화"), Elevator->InitializeForRun(Run, Tuning), EDigetherElevatorResult::Success);
	TestEqual(TEXT("거점 출발 레버 설정"), Lever->Configure(EDigetherElevatorLeverKind::Departure, Tuning), EDigetherElevatorLeverResult::Released);
	Context.DayNumber = Tuning.WarningDayNumber;
	Context.bAnyoneWatchingResultCredits = true;
	TestEqual(TEXT("결과 크레딧 시청 중 시작 거부"), Lever->TryBeginPull(Puller, Context), EDigetherElevatorLeverResult::WatchingResultCredits);
	Context.bAnyoneWatchingResultCredits = false;
	TestEqual(TEXT("4일차 출발 홀드 시작"), Lever->TryBeginPull(Puller, Context), EDigetherElevatorLeverResult::Started);
	TestTrue(TEXT("4일차 진행 경고 공개"), Lever->GetPublicState().bWarningActive);
	TestEqual(TEXT("다른 플레이어 점유 거부"), Lever->TryBeginPull(OtherPuller, Context), EDigetherElevatorLeverResult::Busy);
	TestEqual(TEXT("4일차 기본 시간만으로 미확정"), Lever->AdvancePull(Tuning.DepartureHoldSeconds, Context), EDigetherElevatorLeverResult::InProgress);
	TestEqual(TEXT("4일차 두 배 시간에 확정"), Lever->AdvancePull(Tuning.DepartureHoldSeconds, Context), EDigetherElevatorLeverResult::Confirmed);
	TestEqual(TEXT("출발 확정 통지 한 번"), DepartureNotifications, 1);
	TestEqual(TEXT("레버는 하강을 직접 명령하지 않음"), Elevator->GetPublicState().Phase, EDigetherElevatorPhase::HubWaiting);
	TestEqual(TEXT("출발 중복 진행은 확정 상태"), Lever->AdvancePull(1.f, Context), EDigetherElevatorLeverResult::AlreadyConfirmed);
	TestEqual(TEXT("출발 확정 중복 통지 없음"), DepartureNotifications, 1);
	TestEqual(TEXT("조율자의 첫 하강 명령"), Elevator->DepartHub(), EDigetherElevatorResult::Success);
	TestEqual(TEXT("첫 하강 완료"), Elevator->AdvanceTravel(Run.ElevatorTravelSeconds), EDigetherElevatorResult::Success);
	TestEqual(TEXT("바닥 도착"), Elevator->GetPublicState().Phase, EDigetherElevatorPhase::BottomWaiting);
	TestEqual(TEXT("상승 레버 설정"), Lever->Configure(EDigetherElevatorLeverKind::Ascent, Tuning), EDigetherElevatorLeverResult::Released);
	Context.DayNumber = 1;
	Context.ElevatorPhase = EDigetherElevatorPhase::BottomWaiting;
	FDigetherElevatorLoadEntry Passenger;
	Passenger.EntryId = Puller;
	Passenger.Kind = EDigetherElevatorLoadKind::Player;
	Passenger.WeightKg = 999.f;
	FDigetherElevatorLoadEntry Cargo;
	Cargo.EntryId = FGuid(9, 10, 11, 12);
	Cargo.Kind = EDigetherElevatorLoadKind::Cargo;
	Cargo.WeightKg = 101.f;
	Context.LoadEntries = { Passenger, Cargo };
	TestEqual(TEXT("초과 적재를 공개 단계로 반영"), Elevator->UpdateLoad(Context.LoadEntries), EDigetherElevatorResult::Success);
	TestEqual(TEXT("정확 무게 대신 Over 공개"), Elevator->GetPublicState().WeightStage, EDigetherElevatorWeightStage::Over);
	TestEqual(TEXT("초과 시작 거부"), Lever->TryBeginPull(Puller, Context), EDigetherElevatorLeverResult::OverweightWarningAndRattle);
	TestEqual(TEXT("초과 경고와 덜컹거림 한 번"), RattleNotifications, 1);
	TestEqual(TEXT("초과 시작 후 게이지 0"), Lever->GetPublicState().Progress, 0.f);
	Context.LoadEntries[1].WeightKg = 40.f;
	TestEqual(TEXT("허용 적재 갱신"), Elevator->UpdateLoad(Context.LoadEntries), EDigetherElevatorResult::Success);
	TestEqual(TEXT("상승 홀드 시작"), Lever->TryBeginPull(Puller, Context), EDigetherElevatorLeverResult::Started);
	TestEqual(TEXT("정확한 홀드 시간에 매니페스트 고정"), Lever->AdvancePull(Tuning.AscentHoldSeconds, Context), EDigetherElevatorLeverResult::Confirmed);
	TestEqual(TEXT("상승 레버 확정 통지 한 번"), LeverAscentNotifications, 1);
	TestTrue(TEXT("서버에서 고정 매니페스트 조회"), Lever->TryGetConfirmedAscent(Snapshot));
	TestTrue(TEXT("1회 한도는 첫 상승이 최종"), Snapshot.bFinalAscent);
	TestEqual(TEXT("소지품 제외 플레이어 무게 0"), Snapshot.TotalWeightKg, 40.0);
	TestEqual(TEXT("레버는 엘리베이터를 직접 상승시키지 않음"), Elevator->GetPublicState().Phase, EDigetherElevatorPhase::BottomWaiting);
	FDigetherElevatorAscentConfirmed Tampered = Snapshot;
	Tampered.TotalWeightKg += 1.0;
	const int32 BeforeReject = StateNotifications;
	TestEqual(TEXT("조작된 매니페스트 합계 거부"), Elevator->ConfirmAscent(Tampered), EDigetherElevatorResult::InvalidInput);
	TestEqual(TEXT("거부는 상태 변경 통지 없음"), StateNotifications, BeforeReject);
	TArray<FDigetherElevatorLoadEntry> InvalidLoad = Context.LoadEntries;
	InvalidLoad[1].Kind = static_cast<EDigetherElevatorLoadKind>(MAX_uint8);
	TestEqual(TEXT("잘못된 적재 종류 갱신 거부"), Elevator->UpdateLoad(InvalidLoad), EDigetherElevatorResult::InvalidInput);
	TestEqual(TEXT("적재 거부는 상태 변경 통지 없음"), StateNotifications, BeforeReject);
	TestEqual(TEXT("조율자가 매니페스트를 전달해 상승 확정"), Elevator->ConfirmAscent(Snapshot), EDigetherElevatorResult::Success);
	TestEqual(TEXT("상승 확정 사건 한 번"), AscentNotifications, 1);
	TestEqual(TEXT("확정 편 번호"), NotifiedAscent.TripNumber, 1);
	TestEqual(TEXT("이동 중 같은 상승 재요청 거부"), Elevator->ConfirmAscent(Snapshot), EDigetherElevatorResult::InvalidPhase);
	TestEqual(TEXT("상승 확정 사건 중복 없음"), AscentNotifications, 1);
	Context.LoadEntries[1].WeightKg = 90.f;
	Context.LoadEntries.Reset();
	TestEqual(TEXT("확정 후 적재 목록 변경 가능"), Elevator->UpdateLoad(Context.LoadEntries), EDigetherElevatorResult::Success);
	FDigetherElevatorAscentConfirmed Frozen;
	TestTrue(TEXT("서버 엘리베이터 매니페스트 조회"), Elevator->TryGetLastAscent(Frozen));
	TestEqual(TEXT("외부 목록 변경 후 확정 화물 보존"), Frozen.LoadEntries.Num(), 2);
	TestEqual(TEXT("외부 목록 변경 후 확정 무게 보존"), Frozen.TotalWeightKg, 40.0);
	TestEqual(TEXT("확정 게이지 재진행 거부"), Lever->AdvancePull(1.f, Context), EDigetherElevatorLeverResult::AlreadyConfirmed);
	TestEqual(TEXT("레버 확정 사건 중복 없음"), LeverAscentNotifications, 1);
	TestEqual(TEXT("이동 절반 진행"), Elevator->AdvanceTravel(Run.ElevatorTravelSeconds / 2.f), EDigetherElevatorResult::Success);
	TestEqual(TEXT("공개 이동 진행률 절반"), Elevator->GetPublicState().TravelProgress, 0.5f);
	TestEqual(TEXT("완료 전 탐사 종료 없음"), FinishNotifications, 0);
	TestEqual(TEXT("최종 상승 완료는 탐사 종료 결과"), Elevator->AdvanceTravel(Run.ElevatorTravelSeconds / 2.f), EDigetherElevatorResult::ExplorationFinished);
	TestEqual(TEXT("종료 공개 단계"), Elevator->GetPublicState().Phase, EDigetherElevatorPhase::Finished);
	TestEqual(TEXT("성공 상승 횟수는 도착 때 증가"), Elevator->GetPublicState().CompletedAscents, 1);
	TestEqual(TEXT("탐사 종료 통지 한 번"), FinishNotifications, 1);
	TestEqual(TEXT("종료 후 시간 진행은 변화 없음"), Elevator->AdvanceTravel(100.f), EDigetherElevatorResult::NoChange);
	TestEqual(TEXT("탐사 종료 중복 통지 없음"), FinishNotifications, 1);
	TestEqual(TEXT("새 런 재초기화"), Elevator->InitializeForRun(Run, Tuning), EDigetherElevatorResult::Success);
	TestEqual(TEXT("새 런 거점 대기"), Elevator->GetPublicState().Phase, EDigetherElevatorPhase::HubWaiting);
	TestFalse(TEXT("새 런은 이전 편 매니페스트 없음"), Elevator->TryGetLastAscent(Frozen));

	// OnRep 호출은 표현 알림 검사다. 실제 클라이언트 권한·네트워크 전송은 M2에서 검사한다.
	UFunction* OnRep = Elevator->FindFunction(TEXT("OnRep_PublicState"));
	const int32 BeforeOnRep = StateNotifications;
	if (TestNotNull(TEXT("복제 알림 함수 등록"), OnRep))
	{
		Elevator->ProcessEvent(OnRep, nullptr);
	}
	TestEqual(TEXT("복제 알림은 표현 통지 한 번"), StateNotifications, BeforeOnRep + 1);
	TestEqual(TEXT("복제 표현 알림은 상승 사건 재발행 없음"), AscentNotifications, 1);
	TestEqual(TEXT("복제 표현 알림은 종료 사건 재발행 없음"), FinishNotifications, 1);

	// 실제 원격 접속 없이 소유 Actor의 비권위 역할에서 같은 API 경계를 검사한다.
	const ENetRole OriginalRole = Owner->GetLocalRole();
	Owner->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("모의 프록시 소유자는 비권위"), Owner->HasAuthority());
	const int32 BeforeProxyCall = StateNotifications;
	TestEqual(TEXT("비권위 소유자 초기화 거부"), Elevator->InitializeForRun(Run, Tuning), EDigetherElevatorResult::NotAuthority);
	TestEqual(TEXT("비권위 소유자 하강 거부"), Elevator->DepartHub(), EDigetherElevatorResult::NotAuthority);
	TestEqual(TEXT("비권위 소유자 시간 진행 거부"), Elevator->AdvanceTravel(1.f), EDigetherElevatorResult::NotAuthority);
	TestEqual(TEXT("비권위 소유자 적재 갱신 거부"), Elevator->UpdateLoad(Context.LoadEntries), EDigetherElevatorResult::NotAuthority);
	TestEqual(TEXT("비권위 소유자 상승 확정 거부"), Elevator->ConfirmAscent(Snapshot), EDigetherElevatorResult::NotAuthority);
	TestFalse(TEXT("비권위 소유자 정확 매니페스트 조회 거부"), Elevator->TryGetLastAscent(Frozen));
	TestFalse(TEXT("비권위 소유자 내부 상태 조회 거부"), Elevator->TryGetServerState(ServerState));
	TestEqual(TEXT("비권위 소유자 레버 설정 거부"), Lever->Configure(EDigetherElevatorLeverKind::Ascent, Tuning), EDigetherElevatorLeverResult::NotAuthority);
	TestEqual(TEXT("비권위 소유자 레버 시작 거부"), Lever->TryBeginPull(Puller, Context), EDigetherElevatorLeverResult::NotAuthority);
	TestEqual(TEXT("비권위 소유자 레버 진행 거부"), Lever->AdvancePull(1.f, Context), EDigetherElevatorLeverResult::NotAuthority);
	TestEqual(TEXT("비권위 소유자 레버 해제 거부"), Lever->ReleasePull(Puller), EDigetherElevatorLeverResult::NotAuthority);
	TestEqual(TEXT("비권위 소유자 레버 초기화 거부"), Lever->ResetPull(), EDigetherElevatorLeverResult::NotAuthority);
	TestFalse(TEXT("비권위 소유자 고정 명세 조회 거부"), Lever->TryGetConfirmedAscent(Frozen));
	TestEqual(TEXT("비권위 호출은 알림을 만들지 않음"), StateNotifications, BeforeProxyCall);
	Owner->SetRole(OriginalRole);

	Elevator->OnStateChanged.Remove(StateHandle);
	Elevator->OnAscentConfirmed.Remove(AscentHandle);
	Elevator->OnExplorationFinished.Remove(FinishHandle);
	Lever->OnAscentConfirmed.Remove(LeverAscentHandle);
	Lever->OnDepartureConfirmed.Remove(DepartureHandle);
	Lever->OnFeedback.Remove(FeedbackHandle);
	return true;
}

#endif
