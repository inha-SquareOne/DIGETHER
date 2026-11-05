#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Game/Economy/DigetherQuotaStateComponent.h"
#include "Game/Elevator/DigetherElevatorActor.h"
#include "Game/Elevator/DigetherElevatorComponent.h"
#include "Game/Elevator/DigetherElevatorLeverActor.h"
#include "Game/Elevator/DigetherElevatorLeverComponent.h"
#include "Game/Elevator/DigetherElevatorRules.h"
#include "Game/Framework/DigetherFrameworkRules.h"
#include "Game/Framework/DigetherGameplayGameMode.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Run/DigetherRunCoordinatorComponent.h"
#include "Game/Run/DigetherRunPublicStateComponent.h"
#include "Game/Tests/DigetherTestValues.h"
#include "Misc/AutomationTest.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	// 숫자·식별자는 테스트 픽스처, 기획 수치 아님
	// Framework.Tuning 검사와 같은 유효 프로필 값으로 연결층 검사
	FDigetherRunTuning MakeIntegrationTuningFixture()
	{
		FDigetherRunTuning Tuning;
		Tuning.ProfileVersion = 17;
		Tuning.InitialQuotaCredits = 175;
		Tuning.RunSeconds = 540.f;
		Tuning.InteractionDistanceCm = 245.f;
		Tuning.ElevatorCapacityKg = 35.f;
		Tuning.ElevatorTripLimit = 2;
		Tuning.AscentPenaltySeconds = 5.f;
		Tuning.ElevatorTravelSeconds = 3.f;
		Tuning.DepositIntervalSeconds = 0.2f;
		Tuning.EmergencyWeightLimitKg = 4.f;
		Tuning.DeathLossPercent = 80;
		Tuning.CorpseRefundPercent = 30;
		return Tuning;
	}

	FDigetherFrameworkPlayerSnapshot MakeIntegrationPlayerFixture(const TCHAR* PlayerKey, const int32 BalanceCredits)
	{
		FDigetherFrameworkPlayerSnapshot Player;
		Player.PlayerKey = PlayerKey;
		Player.Wallet.BalanceCredits = BalanceCredits;
		Player.Wallet.LastRunPayoutCredits = 9;
		Player.Wallet.HiddenLifetimeCredits = 13;
		Player.Wallet.TotalDeathLossCredits = 7;
		return Player;
	}

	FDigetherQuotaState MakeIntegrationQuotaFixture()
	{
		FDigetherQuotaState Quota;
		Quota.Day = 1;
		Quota.TargetCredits = 175;
		Quota.DepositedCredits = 35;
		Quota.PeriodGeneration = 3;
		// 전체 일차는 기간 일차와 다른 픽스처 값(이전 기간들을 지난 뒤 1일차)
		Quota.TotalDay = 8;
		return Quota;
	}

	FDigetherRunTerminationInput MakeIntegrationTerminationFixture(
		const TArray<FDigetherFrameworkPlayerSnapshot>& Players,
		const EDigetherRunTerminationTrigger Trigger = EDigetherRunTerminationTrigger::FinalAscentCompleted,
		const EDigetherRunElevatorState ElevatorState = EDigetherRunElevatorState::AtBase)
	{
		FDigetherRunTerminationInput Input;
		Input.Trigger = Trigger;
		Input.ElevatorState = ElevatorState;
		for (const FDigetherFrameworkPlayerSnapshot& Player : Players)
		{
			FDigetherRunPlayerInput Entry;
			Entry.PlayerKey = Player.PlayerKey;
			Entry.Life = Player.Life.PublicState.Life;
			Entry.Location = EDigetherRunPlayerLocation::Base;
			Input.Players.Add(Entry);
		}
		return Input;
	}

	template <typename TState>
	bool AreIntegrationStatesEqual(const TState& Left, const TState& Right)
	{
		return TState::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
	}

	bool AreIntegrationReturnPlansEqual(const FDigetherFrameworkReturnPlan& Left,
		const FDigetherFrameworkReturnPlan& Right)
	{
		if (!AreIntegrationStatesEqual(Left.Termination, Right.Termination)
			|| !AreIntegrationStatesEqual(Left.ReturnedQuota, Right.ReturnedQuota)
			|| Left.bGameOver != Right.bGameOver || Left.Players.Num() != Right.Players.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Players.Num(); ++Index)
		{
			const FDigetherFrameworkPlayerReturn& LeftPlayer = Left.Players[Index];
			const FDigetherFrameworkPlayerReturn& RightPlayer = Right.Players[Index];
			if (!AreIntegrationStatesEqual(LeftPlayer.LifeAtEnd, RightPlayer.LifeAtEnd)
				|| !AreIntegrationStatesEqual(LeftPlayer.WalletAtEnd, RightPlayer.WalletAtEnd)
				|| !AreIntegrationStatesEqual(LeftPlayer.SettledWallet, RightPlayer.SettledWallet)
				|| !AreIntegrationStatesEqual(LeftPlayer.RevivedLife, RightPlayer.RevivedLife)
				|| !AreIntegrationStatesEqual(LeftPlayer.Settlement, RightPlayer.Settlement)
				|| LeftPlayer.DeathLossCredits != RightPlayer.DeathLossCredits
				|| LeftPlayer.ReturnOutcome != RightPlayer.ReturnOutcome)
			{
				return false;
			}
		}
		return true;
	}

	bool IsIntegrationDefaultSubobjectOf(const UActorComponent* Component, const AActor* Actor)
	{
		return IsValid(Component) && Component->HasAnyFlags(RF_DefaultSubObject) && Component->GetOuter() == Actor;
	}

	template <typename TObject>
	bool HasIntegrationReplicationCondition(const TObject* Object, const FName PropertyName,
		const ELifetimeCondition Condition)
	{
		UClass* ObjectClass = Object->GetClass();
		ObjectClass->SetUpRuntimeReplicationData();
		const FProperty* Property = FindFProperty<FProperty>(ObjectClass, PropertyName);
		if (Property == nullptr || !Property->HasAnyPropertyFlags(CPF_Net))
		{
			return false;
		}
		TArray<FLifetimeProperty> Properties;
		Object->GetLifetimeReplicatedProps(Properties);
		for (const FLifetimeProperty& Entry : Properties)
		{
			if (Entry.RepIndex == Property->RepIndex)
			{
				return Entry.Condition == Condition;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkIntegrationDefaultsTest,
	"DIGETHER.Game.Framework.Integration.DefaultSubobjects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkIntegrationDefaultsTest::RunTest(const FString& Parameters)
{
	// CDO의 배치·복제 등록만 검사, 실제 월드 겹침·원격 전송은 M2 범위
	const ADigetherGameplayGameMode* GameMode = GetDefault<ADigetherGameplayGameMode>();
	const ADigetherGameplayGameState* GameState = GetDefault<ADigetherGameplayGameState>();
	const ADigetherGameplayPlayerState* PlayerState = GetDefault<ADigetherGameplayPlayerState>();
	const ADigetherElevatorActor* Elevator = GetDefault<ADigetherElevatorActor>();
	const ADigetherElevatorLeverActor* Lever = GetDefault<ADigetherElevatorLeverActor>();
	if (!TestNotNull(TEXT("GameMode 기본 객체"), GameMode)
		|| !TestNotNull(TEXT("GameState 기본 객체"), GameState)
		|| !TestNotNull(TEXT("PlayerState 기본 객체"), PlayerState)
		|| !TestNotNull(TEXT("엘리베이터 기본 객체"), Elevator)
		|| !TestNotNull(TEXT("레버 기본 객체"), Lever))
	{
		return false;
	}
	const UDigetherRunCoordinatorComponent* Coordinator = GameMode->GetRunCoordinatorComponent();
	const UDigetherRunPublicStateComponent* RunPublic = GameState->GetRunPublicStateComponent();
	const UDigetherLifeStateComponent* Life = PlayerState->GetLifeComponent();
	const UDigetherElevatorComponent* ElevatorComponent = Elevator->GetElevatorComponent();
	const UDigetherElevatorLeverComponent* LeverComponent = Lever->GetLeverComponent();
	const UBoxComponent* BoardingVolume = Elevator->GetBoardingVolume();
	if (!TestNotNull(TEXT("서버 런 조율자 부착"), Coordinator)
		|| !TestNotNull(TEXT("공개 런 상태 부착"), RunPublic)
		|| !TestNotNull(TEXT("플레이어 생존 상태 부착"), Life)
		|| !TestNotNull(TEXT("엘리베이터 규칙 부착"), ElevatorComponent)
		|| !TestNotNull(TEXT("레버 규칙 부착"), LeverComponent)
		|| !TestNotNull(TEXT("탑승 볼륨 부착"), BoardingVolume))
	{
		return false;
	}

	TestTrue(TEXT("조율자는 GameMode 기본 하위객체"), IsIntegrationDefaultSubobjectOf(Coordinator, GameMode));
	TestTrue(TEXT("공개 런 상태는 GameState 기본 하위객체"), IsIntegrationDefaultSubobjectOf(RunPublic, GameState));
	TestTrue(TEXT("생존 상태는 PlayerState 기본 하위객체"), IsIntegrationDefaultSubobjectOf(Life, PlayerState));
	TestTrue(TEXT("엘리베이터 규칙은 액터 기본 하위객체"), IsIntegrationDefaultSubobjectOf(ElevatorComponent, Elevator));
	TestTrue(TEXT("레버 규칙은 액터 기본 하위객체"), IsIntegrationDefaultSubobjectOf(LeverComponent, Lever));
	TestTrue(TEXT("탑승 볼륨은 액터 기본 하위객체"), IsIntegrationDefaultSubobjectOf(BoardingVolume, Elevator));
	TestFalse(TEXT("GameMode는 서버 전용"), GameMode->GetIsReplicated());
	TestFalse(TEXT("런 조율자 내부 상태 비복제"), Coordinator->GetIsReplicated());
	TestTrue(TEXT("공개 런 상태의 소유 액터 복제"), GameState->GetIsReplicated());
	TestTrue(TEXT("생존 상태의 소유 액터 복제"), PlayerState->GetIsReplicated());
	TestTrue(TEXT("공개 런 상태 컴포넌트 복제"), RunPublic->GetIsReplicated());
	TestTrue(TEXT("생존 상태 컴포넌트 복제"), Life->GetIsReplicated());
	TestTrue(TEXT("런 공개 상태는 전체 복제"), HasIntegrationReplicationCondition(RunPublic, TEXT("PublicState"), COND_None));
	TestTrue(TEXT("생존 공개 상태는 전체 복제"), HasIntegrationReplicationCondition(Life, TEXT("PublicState"), COND_None));
	TestTrue(TEXT("생존 개인 상태는 소유자 전용"), HasIntegrationReplicationCondition(Life, TEXT("PrivateState"), COND_OwnerOnly));
	TestTrue(TEXT("엘리베이터 액터 복제"), Elevator->GetIsReplicated());
	TestTrue(TEXT("레버 액터 복제"), Lever->GetIsReplicated());
	TestTrue(TEXT("엘리베이터 규칙 컴포넌트 복제"), ElevatorComponent->GetIsReplicated());
	TestTrue(TEXT("레버 규칙 컴포넌트 복제"), LeverComponent->GetIsReplicated());
	TestEqual(TEXT("탑승 볼륨은 QueryOnly"), BoardingVolume->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
	FCollisionResponseContainer ExpectedResponses(ECR_Ignore);
	ExpectedResponses.SetResponse(ECC_Pawn, ECR_Overlap);
	TestTrue(TEXT("Pawn만 겹침이며 어떤 채널도 차단하지 않음"),
		BoardingVolume->GetCollisionResponseToChannels() == ExpectedResponses);
	TestTrue(TEXT("탑승 겹침 생성 활성"), BoardingVolume->GetGenerateOverlapEvents());
	const FVector Extent = BoardingVolume->GetUnscaledBoxExtent();
	TestTrue(TEXT("탑승 볼륨의 세 축 크기는 양수"), Extent.X > 0.0 && Extent.Y > 0.0 && Extent.Z > 0.0);
	// 기획: 문 닫히는 중에도 걸어서 탑승 가능, 문턱 밀어내기 볼륨 없이 탑승 볼륨 박스 하나
	TArray<UBoxComponent*> ElevatorBoxes;
	Elevator->GetComponents<UBoxComponent>(ElevatorBoxes);
	TestEqual(TEXT("엘리베이터 박스는 탑승 볼륨 하나"), ElevatorBoxes.Num(), 1);
	TestNull(TEXT("문 바깥 방향 설정 프로퍼티 없음"),
		FindFProperty<FProperty>(Elevator->GetClass(), TEXT("DoorOutwardDirection")));
	// 기획: 공동 인정액은 신호기 색으로만 공개, 누적 인정액 서버 원본은 비복제
	const UDigetherQuotaStateComponent* Quota = GameState->GetQuotaComponent();
	if (TestNotNull(TEXT("할당량 상태 부착"), Quota))
	{
		TestTrue(TEXT("할당량 복제 값은 전체 복제"), HasIntegrationReplicationCondition(Quota, TEXT("ReplicatedState"), COND_None));
		const FProperty* ServerQuota = FindFProperty<FProperty>(Quota->GetClass(), TEXT("PublicState"));
		TestTrue(TEXT("누적 인정액을 담은 서버 원본은 비복제"),
			ServerQuota != nullptr && !ServerQuota->HasAnyPropertyFlags(CPF_Net));
		TestNull(TEXT("복제 값에는 누적 인정액 없음"),
			FindFProperty<FProperty>(FDigetherQuotaReplicatedState::StaticStruct(), TEXT("DepositedCredits")));
	}
	TestFalse(TEXT("엘리베이터 액터 자체 Tick 없음"), Elevator->PrimaryActorTick.bCanEverTick);
	TestFalse(TEXT("레버 액터 자체 Tick 없음"), Lever->PrimaryActorTick.bCanEverTick);
	TestFalse(TEXT("엘리베이터 이동 복제 없음"), Elevator->IsReplicatingMovement());
	TestFalse(TEXT("레버 이동 복제 없음"), Lever->IsReplicatingMovement());

	const UStaticMeshComponent* FloorMesh = Elevator->FindComponentByClass<UStaticMeshComponent>();
	const UStaticMeshComponent* LeverMesh = Lever->FindComponentByClass<UStaticMeshComponent>();
	if (TestNotNull(TEXT("엘리베이터 표시 메시"), FloorMesh))
	{
		TestEqual(TEXT("엘리베이터 표시는 비충돌"), FloorMesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	}
	if (TestNotNull(TEXT("레버 표시 메시"), LeverMesh))
	{
		TestEqual(TEXT("레버 표시는 비충돌"), LeverMesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	}
	const FProperty* KindProperty = FindFProperty<FProperty>(Lever->GetClass(), TEXT("LeverKind"));
	const FProperty* ElevatorProperty = FindFProperty<FProperty>(Lever->GetClass(), TEXT("ElevatorActor"));
	const FProperty* PlacementProperty = FindFProperty<FProperty>(Lever->GetClass(), TEXT("Placement"));
	const FProperty* InstanceProperties[] = { KindProperty, ElevatorProperty, PlacementProperty };
	for (const FProperty* Property : InstanceProperties)
	{
		if (TestNotNull(TEXT("레버 인스턴스 설정 프로퍼티 존재"), Property))
		{
			TestTrue(TEXT("레버 종류·연결은 인스턴스 편집과 복제 대상"),
				Property->HasAllPropertyFlags(CPF_Edit | CPF_DisableEditOnTemplate | CPF_Net));
			TestFalse(TEXT("레버 종류·연결의 인스턴스 편집 허용"), Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance));
		}
	}
	TestTrue(TEXT("레버 종류는 전체 복제 등록"), HasIntegrationReplicationCondition(Lever, TEXT("LeverKind"), COND_None));
	TestTrue(TEXT("레버 연결은 전체 복제 등록"), HasIntegrationReplicationCondition(Lever, TEXT("ElevatorActor"), COND_None));
	TestTrue(TEXT("레버 안·밖 위치는 전체 복제 등록"), HasIntegrationReplicationCondition(Lever, TEXT("Placement"), COND_None));
	TestEqual(TEXT("레버 위치 기본값은 안쪽"), Lever->GetPlacement(), EDigetherElevatorLeverPlacement::Inside);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkPlayerLoadTest,
	"DIGETHER.Game.Framework.Integration.PlayerLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkPlayerLoadTest::RunTest(const FString& Parameters)
{
	// 숫자·식별자는 테스트 픽스처, 기획 값 아님
	const TArray<FGuid> PassengerIds = { FGuid(1, 2, 3, 4), FGuid(5, 6, 7, 8) };
	TArray<FDigetherElevatorLoadEntry> Entries;
	if (!TestTrue(TEXT("유효한 탑승자 목록 변환"), DigetherFrameworkRules::TryMakePlayerLoad(PassengerIds, Entries))
		|| !TestEqual(TEXT("탑승자마다 적재 항목 하나"), Entries.Num(), PassengerIds.Num()))
	{
		return false;
	}
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		TestEqual(TEXT("탑승자 순서와 ID 보존"), Entries[Index].EntryId, PassengerIds[Index]);
		TestEqual(TEXT("적재 소유자도 같은 플레이어"), Entries[Index].OwnerId, PassengerIds[Index]);
		TestEqual(TEXT("플레이어 적재 종류"), Entries[Index].Kind, EDigetherElevatorLoadKind::Player);
		TestEqual(TEXT("플레이어 신체 무게는 0kg"), Entries[Index].WeightKg, 0.f);
	}
	const TArray<FDigetherElevatorLoadEntry> PreviousEntries = Entries;
	const TArray<TArray<FGuid>> InvalidLists =
	{
		{ PassengerIds[0], FGuid() },
		{ PassengerIds[0], PassengerIds[0] }
	};
	for (const TArray<FGuid>& InvalidIds : InvalidLists)
	{
		TestFalse(TEXT("무효·중복 탑승자 거절"), DigetherFrameworkRules::TryMakePlayerLoad(InvalidIds, Entries));
		if (TestEqual(TEXT("거절은 출력 항목 수 보존"), Entries.Num(), PreviousEntries.Num()))
		{
			for (int32 Index = 0; Index < Entries.Num(); ++Index)
			{
				TestTrue(TEXT("부분 변환 없이 출력 항목 전체 보존"),
					AreIntegrationStatesEqual(Entries[Index], PreviousEntries[Index]));
			}
		}
	}
	TestTrue(TEXT("빈 탑승자 목록 허용"), DigetherFrameworkRules::TryMakePlayerLoad({}, Entries));
	TestTrue(TEXT("빈 목록은 이전 적재 항목 제거"), Entries.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkDepositBoundaryTest,
	"DIGETHER.Game.Framework.Integration.DepositBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkDepositBoundaryTest::RunTest(const FString& Parameters)
{
	FDigetherRunPublicState Run;
	TestFalse(TEXT("시작 전 거점 대기는 납부 가능"),
		DigetherFrameworkRules::IsDepositClosed(Run, EDigetherElevatorPhase::HubWaiting));
	Run.Phase = EDigetherRunPhase::InProgress;
	TestTrue(TEXT("탐사 중에는 납부 거절"),
		DigetherFrameworkRules::IsDepositClosed(Run, EDigetherElevatorPhase::HubWaiting));
	Run.Phase = EDigetherRunPhase::Ended;
	TestTrue(TEXT("귀환 정리 전 Finished는 납부 거절"),
		DigetherFrameworkRules::IsDepositClosed(Run, EDigetherElevatorPhase::Finished));
	TestFalse(TEXT("귀환 정리가 끝난 거점 대기는 납부 가능"),
		DigetherFrameworkRules::IsDepositClosed(Run, EDigetherElevatorPhase::HubWaiting));
	Run.bPaymentDeadline = true;
	TestTrue(TEXT("납부 마감이면 거점에서도 거절"),
		DigetherFrameworkRules::IsDepositClosed(Run, EDigetherElevatorPhase::HubWaiting));
	Run.bPaymentDeadline = false;
	Run.bGameOver = true;
	TestTrue(TEXT("게임 오버이면 거점에서도 거절"),
		DigetherFrameworkRules::IsDepositClosed(Run, EDigetherElevatorPhase::HubWaiting));
	TestEqual(TEXT("광산 바닥은 런 광산 정지로 변환"),
		DigetherFrameworkRules::MakeRunElevatorState(EDigetherElevatorPhase::BottomWaiting),
		EDigetherRunElevatorState::StoppedInMine);
	// 기획 A3: 문 닫히는 중 시간 종료는 갱도 대기로 처리
	TestEqual(TEXT("문 닫힘은 런 광산 정지로 변환"),
		DigetherFrameworkRules::MakeRunElevatorState(EDigetherElevatorPhase::DoorClosing),
		EDigetherRunElevatorState::StoppedInMine);
	Run = FDigetherRunPublicState();
	TestTrue(TEXT("거점 문 닫힘은 납부 거절"),
		DigetherFrameworkRules::IsDepositClosed(Run, EDigetherElevatorPhase::DoorClosing));
	// 기획 C3: 출발 게이지 완료(거점 문 닫힘)부터 새 참가 차단, 문 재개방 시 해제
	TestFalse(TEXT("거점 대기는 새 참가 허용"),
		DigetherFrameworkRules::IsNewJoinLocked(Run, EDigetherElevatorPhase::HubWaiting));
	TestTrue(TEXT("거점 문 닫힘은 새 참가 잠금"),
		DigetherFrameworkRules::IsNewJoinLocked(Run, EDigetherElevatorPhase::DoorClosing));
	Run.Phase = EDigetherRunPhase::InProgress;
	TestTrue(TEXT("탐사 중은 새 참가 잠금"),
		DigetherFrameworkRules::IsNewJoinLocked(Run, EDigetherElevatorPhase::BottomWaiting));
	Run.Phase = EDigetherRunPhase::Ended;
	TestFalse(TEXT("귀환 정리 뒤 거점 대기는 새 참가 허용"),
		DigetherFrameworkRules::IsNewJoinLocked(Run, EDigetherElevatorPhase::HubWaiting));
	// 기획 S33: 실패 사건·크레딧(게임 오버) 중에는 거점 대기여도 새 참가 차단
	Run.bGameOver = true;
	TestTrue(TEXT("게임 오버 중 거점 대기는 새 참가 잠금"),
		DigetherFrameworkRules::IsNewJoinLocked(Run, EDigetherElevatorPhase::HubWaiting));
	Run.bGameOver = false;
	TestEqual(TEXT("상승은 런 이동 중으로 변환"),
		DigetherFrameworkRules::MakeRunElevatorState(EDigetherElevatorPhase::FinalAscent), EDigetherRunElevatorState::InTransit);
	TestEqual(TEXT("최종 도착은 런 거점으로 변환"),
		DigetherFrameworkRules::MakeRunElevatorState(EDigetherElevatorPhase::Finished), EDigetherRunElevatorState::AtBase);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkFinalReturnTest,
	"DIGETHER.Game.Framework.Integration.FinalReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkFinalReturnTest::RunTest(const FString& Parameters)
{
	const FDigetherRunTuning Tuning = MakeIntegrationTuningFixture();
	const FDigetherQuotaState Quota = MakeIntegrationQuotaFixture();
	const TArray<FDigetherFrameworkPlayerSnapshot> Players =
	{
		MakeIntegrationPlayerFixture(TEXT("fixture-returned"), 100),
		MakeIntegrationPlayerFixture(TEXT("fixture-missing"), 200)
	};
	FDigetherRunTerminationInput Input = MakeIntegrationTerminationFixture(Players);
	Input.Players[0].Location = EDigetherRunPlayerLocation::Elevator;
	Input.Players[1].Location = EDigetherRunPlayerLocation::MineOrDungeon;
	FDigetherFrameworkReturnPlan Plan;
	if (!TestTrue(TEXT("최종 상승 귀환 계획 수락"),
		DigetherFrameworkRules::TryPlanReturn(Input, Players, Quota, Tuning, Players.Num(), Plan))
		|| !TestEqual(TEXT("귀환 처리 인원 보존"), Plan.Players.Num(), Players.Num())
		|| !TestEqual(TEXT("종료 판정 인원 보존"), Plan.Termination.Players.Num(), Players.Num()))
	{
		return false;
	}
	TestEqual(TEXT("탑승자는 귀환 판정"), Plan.Termination.Players[0].Outcome, EDigetherRunPlayerOutcome::Returned);
	TestEqual(TEXT("귀환자는 정산 전 탈출 확정"), Plan.Players[0].LifeAtEnd.PublicState.Life, EDigetherLifeState::Escaped);
	TestEqual(TEXT("귀환 탈출 경로는 엘리베이터"), Plan.Players[0].LifeAtEnd.PrivateState.EscapeKind, EDigetherEscapeKind::Elevator);
	TestEqual(TEXT("귀환자 잔고 손실 없음"), Plan.Players[0].SettledWallet.BalanceCredits, 100);
	TestEqual(TEXT("화물 없는 귀환 지급액 0"), Plan.Players[0].SettledWallet.LastRunPayoutCredits, 0);
	TestEqual(TEXT("잔류자는 사망 판정"), Plan.Players[1].LifeAtEnd.PublicState.Life, EDigetherLifeState::Dead);
	TestEqual(TEXT("잔류 사인은 최종 상승 잔류"),
		Plan.Players[1].LifeAtEnd.PrivateState.DeathCause, EDigetherDeathCause::CollapseRemainedAfterFinalAscent);
	TestEqual(TEXT("잔류자 80% 손실 뒤 잔고"), Plan.Players[1].WalletAtEnd.BalanceCredits, 40);
	TestEqual(TEXT("기존 사망 손실에 이번 손실 합산"), Plan.Players[1].WalletAtEnd.TotalDeathLossCredits, 167);
	TestEqual(TEXT("손실 전 잔고 보관"), Plan.Players[1].WalletAtEnd.LastPreDeathBalanceCredits, 200);
	for (const FDigetherFrameworkPlayerReturn& Returned : Plan.Players)
	{
		TestTrue(TEXT("귀환 정리 후 생존 초기값 복원"),
			AreIntegrationStatesEqual(Returned.RevivedLife, FDigetherLifeComponentState()));
		TestEqual(TEXT("화물 없는 정산은 히든 누적 보존"), Returned.SettledWallet.HiddenLifetimeCredits, 13);
	}
	TestEqual(TEXT("귀환은 일차 한 번 증가"), Plan.ReturnedQuota.Day, 2);
	TestEqual(TEXT("귀환은 전체 일차도 한 번 증가"), Plan.ReturnedQuota.TotalDay, 9);
	TestEqual(TEXT("계획 계산은 입력 전체 일차 보존"), Quota.TotalDay, 8);
	TestEqual(TEXT("귀환은 납부액 보존"), Plan.ReturnedQuota.DepositedCredits, Quota.DepositedCredits);
	TestEqual(TEXT("귀환은 기간 세대 보존"), Plan.ReturnedQuota.PeriodGeneration, Quota.PeriodGeneration);
	TestFalse(TEXT("마감 전 귀환은 게임 오버 아님"), Plan.bGameOver);
	TestEqual(TEXT("계획 계산은 입력 생존 상태 보존"), Players[1].Life.PublicState.Life, EDigetherLifeState::Active);
	TestEqual(TEXT("계획 계산은 입력 잔고 보존"), Players[1].Wallet.BalanceCredits, 200);
	TestEqual(TEXT("계획 계산은 입력 일차 보존"), Quota.Day, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkTimeoutReturnTest,
	"DIGETHER.Game.Framework.Integration.TimeoutReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkTimeoutReturnTest::RunTest(const FString& Parameters)
{
	const FDigetherRunTuning Tuning = MakeIntegrationTuningFixture();
	FDigetherQuotaState Quota = MakeIntegrationQuotaFixture();
	Quota.Day = 4;
	const TArray<FDigetherFrameworkPlayerSnapshot> Players =
	{
		MakeIntegrationPlayerFixture(TEXT("fixture-transit"), 100),
		MakeIntegrationPlayerFixture(TEXT("fixture-timeout"), 100)
	};
	FDigetherRunTerminationInput Input = MakeIntegrationTerminationFixture(Players,
		EDigetherRunTerminationTrigger::TimeExpired, EDigetherRunElevatorState::InTransit);
	Input.bElevatorOverweight = true;
	Input.Players[0].Location = EDigetherRunPlayerLocation::Elevator;
	Input.Players[1].Location = EDigetherRunPlayerLocation::MineOrDungeon;
	FDigetherFrameworkReturnPlan Plan;
	if (!TestTrue(TEXT("시간 만료 귀환 계획 수락"),
		DigetherFrameworkRules::TryPlanReturn(Input, Players, Quota, Tuning, Players.Num(), Plan))
		|| !TestEqual(TEXT("시간 만료 처리 인원 보존"), Plan.Players.Num(), Players.Num())
		|| !TestEqual(TEXT("시간 만료 판정 인원 보존"), Plan.Termination.Players.Num(), Players.Num()))
	{
		return false;
	}
	TestEqual(TEXT("이동 중 탑승자는 초과 적재에도 생존"),
		Plan.Termination.Players[0].Outcome, EDigetherRunPlayerOutcome::SurvivedInTransit);
	TestEqual(TEXT("이동 중 생존자의 잔고 보존"), Plan.Players[0].SettledWallet.BalanceCredits, 100);
	TestEqual(TEXT("미탑승 활동자는 시간 만료 사망"),
		Plan.Players[1].LifeAtEnd.PrivateState.DeathCause, EDigetherDeathCause::CollapseTimeExpired);
	TestEqual(TEXT("시간 만료 사망에 P1 손실 적용"), Plan.Players[1].SettledWallet.BalanceCredits, 20);
	TestEqual(TEXT("손실은 정산 과정에서 한 번만 적용"), Plan.Players[1].SettledWallet.TotalDeathLossCredits, 87);
	TestTrue(TEXT("사망자도 거점 정리 후 활동 상태 복원"),
		AreIntegrationStatesEqual(Plan.Players[1].RevivedLife, FDigetherLifeComponentState()));
	TestEqual(TEXT("4일차 귀환은 5일차"), Plan.ReturnedQuota.Day, 5);
	TestEqual(TEXT("4일차 귀환에 전체 일차도 하루 증가"), Plan.ReturnedQuota.TotalDay, 9);
	TestTrue(TEXT("마감 이후 미달 귀환은 게임 오버"), Plan.bGameOver);

	// 전원 사망(전멸)으로 일찍 끝나도 탐사 종료라 두 일차 동일 증가(기획 02 개요 5행)
	FDigetherRunTerminationInput Wipe = Input;
	Wipe.ElevatorState = EDigetherRunElevatorState::StoppedInMine;
	Wipe.bElevatorOverweight = false;
	Wipe.Players[0].Location = EDigetherRunPlayerLocation::MineOrDungeon;
	Wipe.Players[1].Location = EDigetherRunPlayerLocation::MineOrDungeon;
	FDigetherQuotaState WipeQuota = MakeIntegrationQuotaFixture();
	WipeQuota.Day = 2;
	FDigetherFrameworkReturnPlan WipePlan;
	if (TestTrue(TEXT("전멸 귀환 계획 수락"),
		DigetherFrameworkRules::TryPlanReturn(Wipe, Players, WipeQuota, Tuning, Players.Num(), WipePlan)))
	{
		bool bAllDied = WipePlan.Players.Num() == Players.Num();
		for (const FDigetherFrameworkPlayerReturn& Returned : WipePlan.Players)
		{
			bAllDied &= Returned.LifeAtEnd.PublicState.Life == EDigetherLifeState::Dead;
		}
		TestTrue(TEXT("갱도에 남은 전원은 시간 만료 사망"), bAllDied);
		TestEqual(TEXT("전멸도 기간 일차 하루 증가"), WipePlan.ReturnedQuota.Day, 3);
		TestEqual(TEXT("전멸도 전체 일차 하루 증가"), WipePlan.ReturnedQuota.TotalDay, 9);
	}

	Quota.DepositedCredits = Quota.TargetCredits;
	TestTrue(TEXT("동일 귀환의 목표 달성 상태도 수락"),
		DigetherFrameworkRules::TryPlanReturn(Input, Players, Quota, Tuning, Players.Num(), Plan));
	TestFalse(TEXT("납부 완료 상태는 게임 오버 아님"), Plan.bGameOver);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkRejectInvalidReturnTest,
	"DIGETHER.Game.Framework.Integration.RejectInvalidReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkRejectInvalidReturnTest::RunTest(const FString& Parameters)
{
	const FDigetherRunTuning Tuning = MakeIntegrationTuningFixture();
	const FDigetherQuotaState Quota = MakeIntegrationQuotaFixture();
	const TArray<FDigetherFrameworkPlayerSnapshot> Players =
	{
		MakeIntegrationPlayerFixture(TEXT("fixture-first"), 100),
		MakeIntegrationPlayerFixture(TEXT("fixture-second"), 200)
	};
	const FDigetherRunTerminationInput Input = MakeIntegrationTerminationFixture(Players);
	FDigetherFrameworkReturnPlan PreviousPlan;
	if (!TestTrue(TEXT("보존 검사 기준 계획 생성"),
		DigetherFrameworkRules::TryPlanReturn(Input, Players, Quota, Tuning, Players.Num(), PreviousPlan))
		|| !TestEqual(TEXT("기준 계획의 플레이어 수"), PreviousPlan.Players.Num(), Players.Num())
		|| !TestEqual(TEXT("기준 계획의 종료 판정 수"), PreviousPlan.Termination.Players.Num(), Players.Num()))
	{
		return false;
	}
	// 실패 도중 새 계획 일부가 출력에 쓰였는지 잡으려 출력 묶음 모두 구별
	PreviousPlan.Termination.Result = EDigetherRunResult::AlreadyEnded;
	PreviousPlan.Termination.bEjectCargo = true;
	PreviousPlan.Termination.ElevatorAction = EDigetherRunElevatorAction::EmptyAscentAfterEjection;
	PreviousPlan.Termination.Players[0].PlayerKey = TEXT("fixture-preserved-output");
	PreviousPlan.ReturnedQuota.Day = 9;
	PreviousPlan.ReturnedQuota.DepositedCredits = 1;
	PreviousPlan.bGameOver = true;
	for (FDigetherFrameworkPlayerReturn& Returned : PreviousPlan.Players)
	{
		Returned.LifeAtEnd.PrivateState.SpectateTargetPlayerKey = TEXT("fixture-preserved-target");
		Returned.WalletAtEnd.BalanceCredits = 301;
		Returned.SettledWallet.BalanceCredits = 302;
		Returned.RevivedLife.PrivateState.SpectateTargetPlayerKey = TEXT("fixture-preserved-revived-target");
		Returned.Settlement.GeneralCredits = 303;
		Returned.Settlement.PayoutCredits = 303;
		Returned.DeathLossCredits = 304;
		Returned.ReturnOutcome = EDigetherRunReturnOutcome::Died;
	}
	const auto ExpectRejected = [this, &Tuning, &PreviousPlan](const FDigetherRunTerminationInput& CandidateInput,
		const TArray<FDigetherFrameworkPlayerSnapshot>& CandidatePlayers, const FDigetherQuotaState& CandidateQuota,
		const int32 DepartureCount)
	{
		FDigetherFrameworkReturnPlan Output = PreviousPlan;
		TestFalse(TEXT("불일치·손상 귀환 입력 거절"), DigetherFrameworkRules::TryPlanReturn(
			CandidateInput, CandidatePlayers, CandidateQuota, Tuning, DepartureCount, Output));
		TestTrue(TEXT("귀환 거절은 기존 계획의 모든 필드 보존"), AreIntegrationReturnPlansEqual(Output, PreviousPlan));
	};

	TArray<FDigetherFrameworkPlayerSnapshot> InvalidPlayers = Players;
	InvalidPlayers.Swap(0, 1);
	ExpectRejected(Input, InvalidPlayers, Quota, Players.Num());
	InvalidPlayers = Players;
	InvalidPlayers[1].PlayerKey = TEXT("fixture-unrelated");
	ExpectRejected(Input, InvalidPlayers, Quota, Players.Num());
	InvalidPlayers = Players;
	InvalidPlayers[1].Wallet.BalanceCredits = -1;
	ExpectRejected(Input, InvalidPlayers, Quota, Players.Num());
	InvalidPlayers = Players;
	InvalidPlayers[1].Life.PublicState.bSpectating = true;
	ExpectRejected(Input, InvalidPlayers, Quota, Players.Num());
	InvalidPlayers = Players;
	InvalidPlayers.RemoveAt(1);
	ExpectRejected(Input, InvalidPlayers, Quota, Players.Num());
	FDigetherRunTerminationInput InvalidInput = Input;
	InvalidInput.Players[1].Life = EDigetherLifeState::Dead;
	ExpectRejected(InvalidInput, Players, Quota, Players.Num());
	InvalidInput = Input;
	InvalidInput.Players[1].PlayerKey = InvalidInput.Players[0].PlayerKey;
	ExpectRejected(InvalidInput, Players, Quota, Players.Num());
	FDigetherQuotaState InvalidQuota = Quota;
	InvalidQuota.Day = 0;
	ExpectRejected(Input, Players, InvalidQuota, Players.Num());
	ExpectRejected(Input, Players, Quota, 0);
	ExpectRejected(Input, Players, Quota, Players.Num() - 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkChainSummonPlanTest,
	"DIGETHER.Game.Framework.Integration.ChainSummonPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkChainSummonPlanTest::RunTest(const FString& Parameters)
{
	// 탑승 볼륨 크기·간격은 테스트 픽스처, 기획 탑승 경계 아님
	const FVector Extent(250.0, 250.0, 100.0);
	constexpr double Spacing = 100.0;
	const auto IsSpaced = [Spacing](const TArray<FVector>& Points)
	{
		for (int32 Left = 0; Left < Points.Num(); ++Left)
		{
			for (int32 Right = Left + 1; Right < Points.Num(); ++Right)
			{
				if (FVector::Dist2D(Points[Left], Points[Right]) < Spacing - KINDA_SMALL_NUMBER)
				{
					return false;
				}
			}
		}
		return true;
	};

	TArray<FVector> Planned;
	TestTrue(TEXT("소집 0명은 성공"), DigetherFrameworkRules::TryPlanChainSummon(Extent, Spacing, {}, 0, Planned));
	TestEqual(TEXT("소집 0명은 위치 없음"), Planned.Num(), 0);

	// 탑승자 없으면 4명 모두 바닥 안쪽 서로 떨어진 위치로 이동, 첫 소집자는 중심
	TestTrue(TEXT("빈 엘리베이터 4명 소집"), DigetherFrameworkRules::TryPlanChainSummon(Extent, Spacing, {}, 4, Planned));
	TestEqual(TEXT("소집 인원만큼 위치"), Planned.Num(), 4);
	TestTrue(TEXT("첫 소집 위치는 중심"), Planned.Num() > 0 && Planned[0].Equals(FVector::ZeroVector));
	TestTrue(TEXT("소집 위치끼리 간격 유지"), IsSpaced(Planned));
	for (const FVector& Point : Planned)
	{
		TestTrue(TEXT("소집 위치는 볼륨 안쪽"), FMath::Abs(Point.X) < Extent.X && FMath::Abs(Point.Y) < Extent.Y);
		TestEqual(TEXT("소집 위치 높이는 볼륨 중심"), Point.Z, 0.0);
	}

	// 직접 탑승자 자리 회피
	const TArray<FVector> Boarded = { FVector(0.0, 0.0, 12.0), FVector(100.0, 0.0, 0.0) };
	TestTrue(TEXT("직접 탑승자와 함께 2명 소집"), DigetherFrameworkRules::TryPlanChainSummon(Extent, Spacing, Boarded, 2, Planned));
	TArray<FVector> All = Boarded;
	All.Append(Planned);
	TestTrue(TEXT("직접 탑승자와 소집자 간격 유지"), IsSpaced(All));

	// 같은 입력은 같은 배치(서버 재시도·로그 재현)
	TArray<FVector> Again;
	DigetherFrameworkRules::TryPlanChainSummon(Extent, Spacing, Boarded, 2, Again);
	TestTrue(TEXT("같은 입력의 같은 배치"), Again == Planned);

	// 자리 부족 시 실패, 출력 보존(임시: 기획의 겹침 허용 대신 출발 거절)
	const FVector Narrow(60.0, 60.0, 100.0);
	TArray<FVector> Preserved = { FVector(1.0, 2.0, 3.0) };
	TestFalse(TEXT("좁은 볼륨 2명 소집 실패"), DigetherFrameworkRules::TryPlanChainSummon(Narrow, Spacing, {}, 2, Preserved));
	TestTrue(TEXT("실패 시 출력 보존"), Preserved.Num() == 1 && Preserved[0].Equals(FVector(1.0, 2.0, 3.0)));
	TestTrue(TEXT("좁은 볼륨도 1명은 중심 소집"), DigetherFrameworkRules::TryPlanChainSummon(Narrow, Spacing, {}, 1, Planned));
	TestFalse(TEXT("잘못된 간격 거절"), DigetherFrameworkRules::TryPlanChainSummon(Extent, 0.0, {}, 1, Planned));
	TestFalse(TEXT("음수 인원 거절"), DigetherFrameworkRules::TryPlanChainSummon(Extent, Spacing, {}, -1, Planned));
	TestFalse(TEXT("NaN 탑승 위치 거절"),
		DigetherFrameworkRules::TryPlanChainSummon(Extent, Spacing, { FVector(DigetherTestValues::NaN<double>(), 0.0, 0.0) }, 1, Planned));
	return true;
}

#endif
