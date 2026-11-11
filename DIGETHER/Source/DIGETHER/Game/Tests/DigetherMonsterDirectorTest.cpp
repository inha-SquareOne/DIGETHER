#if WITH_DEV_AUTOMATION_TESTS

#include "BehaviorTree/BehaviorTree.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/Monster/DigetherMonsterCharacter.h"
#include "Game/Monster/DigetherMonsterDefinition.h"
#include "Game/Monster/DigetherMonsterDirectorSubsystem.h"
#include "Game/Monster/DigetherMonsterHealthComponent.h"
#include "Game/Monster/DigetherMonsterSenseSubsystem.h"
#include "Game/Monster/DigetherMonsterSettings.h"
#include "Game/Monster/DigetherMonsterSpawnPoint.h"
#include "Game/Monster/DigetherMonsterSpawnTable.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherMonsterDirectorTest,
	"DIGETHER.Game.Monster.Director", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	// 독립 게임 월드, BeginPlay·타이머 틱 없이 디렉터 공개 함수만 호출
	class FMonsterTestWorld
	{
	public:
		FMonsterTestWorld()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(true);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (World && GEngine)
			{
				GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			}
		}
		~FMonsterTestWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine)
				{
					GEngine->DestroyWorldContext(World);
				}
			}
		}
		FMonsterTestWorld(const FMonsterTestWorld&) = delete;
		FMonsterTestWorld& operator=(const FMonsterTestWorld&) = delete;

		UWorld* World = nullptr;
	};

	// 시험용 정의, 수치는 픽스처이며 기획 값 아님
	UDigetherMonsterDefinition* MakeDefinition(const TCHAR* Name, const EDigetherMonsterSpawnKind Kind, const EDigetherMonsterZone Zone,
		UBehaviorTree* Tree)
	{
		UDigetherMonsterDefinition* Definition = NewObject<UDigetherMonsterDefinition>(GetTransientPackage(), FName(Name), RF_Transient);
		Definition->SpawnKind = Kind;
		Definition->Zone = Zone;
		Definition->PowerLevel = Kind == EDigetherMonsterSpawnKind::PeriodicPower ? 3 : 0;
		Definition->RunSpawnCap = Kind == EDigetherMonsterSpawnKind::PeriodicPower ? 1 : 0;
		Definition->MonsterClass = ADigetherMonsterCharacter::StaticClass();
		Definition->BehaviorTree = Tree;
		Definition->Movement.CapsuleRadius = 55.f;
		Definition->Movement.CapsuleHalfHeight = 80.f;
		Definition->Movement.MaxStepHeight = 100.f;
		Definition->Movement.WalkSpeed = 250.f;
		return Definition;
	}

	FDigetherMonsterPeriodicEntry MakeEntry(UDigetherMonsterDefinition* Definition, const int32 MaxAlive)
	{
		FDigetherMonsterPeriodicEntry Entry;
		Entry.Definition = Definition;
		Entry.Weight = 1.f;
		Entry.MaxAlive = MaxAlive;
		return Entry;
	}

	ADigetherMonsterSpawnPoint* AddPoint(UWorld* World, const EDigetherMonsterSpawnPointKind Kind, const FVector& Location, const bool bCollapse = false)
	{
		ADigetherMonsterSpawnPoint* Point = World->SpawnActor<ADigetherMonsterSpawnPoint>(Location, FRotator::ZeroRotator);
		if (Point)
		{
			Point->PointKind = Kind;
			Point->bInCollapseArea = bCollapse;
		}
		return Point;
	}
}

bool FDigetherMonsterDirectorTest::RunTest(const FString& Parameters)
{
	FMonsterTestWorld TestWorld;
	UWorld* World = TestWorld.World;
	if (!TestNotNull(TEXT("테스트 월드"), World))
	{
		return false;
	}
	UDigetherMonsterDirectorSubsystem* Director = World->GetSubsystem<UDigetherMonsterDirectorSubsystem>();
	UDigetherMonsterSenseSubsystem* Sense = World->GetSubsystem<UDigetherMonsterSenseSubsystem>();
	if (!TestNotNull(TEXT("디렉터 생성"), Director) || !TestNotNull(TEXT("감각 서브시스템 생성"), Sense))
	{
		return false;
	}
	TestTrue(TEXT("단독 실행 월드에서 디렉터 활성"), Director->Activate());
	TestTrue(TEXT("단독 실행 월드에서 감각 활성"), Sense->Activate());

	UBehaviorTree* Tree = NewObject<UBehaviorTree>(GetTransientPackage(), NAME_None, RF_Transient);
	UDigetherMonsterDefinition* Box = MakeDefinition(TEXT("DA_TestBox"), EDigetherMonsterSpawnKind::PeriodicPower, EDigetherMonsterZone::Dungeon, Tree);
	UDigetherMonsterDefinition* Spider = MakeDefinition(TEXT("DA_TestSpider"), EDigetherMonsterSpawnKind::Habitat, EDigetherMonsterZone::Mine, Tree);
	Spider->bKillable = true;
	Spider->MaxHealth = 50.f;
	UDigetherMonsterDefinition* Mole = MakeDefinition(TEXT("DA_TestMole"), EDigetherMonsterSpawnKind::Periodic, EDigetherMonsterZone::Mine, Tree);

	UDigetherMonsterSpawnTable* Table = NewObject<UDigetherMonsterSpawnTable>(GetTransientPackage(), NAME_None, RF_Transient);
	Table->StageId = TEXT("TestStage");
	Table->Curve.GraceSeconds = 0.f;
	Table->Curve.IntervalSeconds = 30.f;
	Table->Curve.StartChance = 1.f;
	Table->Curve.MaxChance = 1.f;
	Table->Curve.MaxChanceAtSeconds = 1.f;
	Table->DungeonPowerLimit.Base = 3;
	Table->DungeonPowerLimit.PerGoalAchieved = 1;
	Table->DungeonPowerLimit.Max = 6;
	Table->DungeonEntries.Add(MakeEntry(Box, 0));
	Table->MinePeriodicEntries.Add(MakeEntry(Mole, 1));
	FDigetherMonsterHabitatEntry Habitat;
	Habitat.Definition = Spider;
	Habitat.Chance = 1.f;
	Table->MineHabitatEntries.Add(Habitat);
	TArray<FText> TableProblems;
	TestTrue(TEXT("시험 테이블 유효"), Table->CollectProblems(TableProblems));

	// 런 없이 생성 거절
	TestEqual(TEXT("런 없으면 생성 거절"), Director->SpawnMonsterAt(Box, FTransform::Identity, TEXT("D1")), EDigetherMonsterSpawnResult::NoActiveRun);

	const FGuid RunId = FGuid::NewGuid();
	TestTrue(TEXT("런 시작"), Director->BeginRun(Table, RunId, 1, 42));
	TestEqual(TEXT("목표 1개 달성 시 파워 한도 4"), Director->GetDungeonPowerLimit(), 4);

	// 정의 주입: 캡슐·이동·체력 정의가 생성자가 고른 정의로 일관 적용
	ADigetherMonsterCharacter* SpiderActor = nullptr;
	TestEqual(TEXT("직접 생성"), Director->SpawnMonsterAt(Spider, FTransform(FVector(0.f, 0.f, 200.f)), NAME_None, &SpiderActor),
		EDigetherMonsterSpawnResult::Spawned);
	if (TestNotNull(TEXT("생성 개체"), SpiderActor))
	{
		TestEqual(TEXT("정의 주입"), SpiderActor->GetDefinition(), Spider);
		TestEqual(TEXT("체력 컴포넌트에도 같은 정의"), SpiderActor->GetHealthComponent()->Definition.Get(), Spider);
		TestEqual(TEXT("캡슐 반지름 적용"), SpiderActor->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), 55.f, 1e-3f);
		TestEqual(TEXT("단차 100cm 적용"), SpiderActor->GetCharacterMovement()->MaxStepHeight, 100.f, 1e-3f);
		TestEqual(TEXT("내비 에이전트 단차 일치"), SpiderActor->GetCharacterMovement()->NavAgentProps.AgentStepHeight, 100.f, 1e-3f);
		TestEqual(TEXT("런 ID 기록"), SpiderActor->GetRunId(), RunId);
		TestFalse(TEXT("시작 후 재주입 거절"), SpiderActor->InitializeFromDefinition(Box, RunId, NAME_None));
	}
	TestEqual(TEXT("생존 수 1"), Director->GetAliveCount(Spider), 1);

	// 소멸 시 파워 반환, 탐사당 생성 수 유지
	ADigetherMonsterCharacter* BoxActor = nullptr;
	Director->SpawnMonsterAt(Box, FTransform(FVector(500.f, 0.f, 200.f)), TEXT("D1"), &BoxActor);
	TestEqual(TEXT("던전 D1 파워 사용 3"), Director->GetDungeonPowerUsed(TEXT("D1")), 3);
	TestEqual(TEXT("던전 D2는 0(던전별 장부)"), Director->GetDungeonPowerUsed(TEXT("D2")), 0);
	if (TestNotNull(TEXT("박스 생성"), BoxActor))
	{
		BoxActor->RemoveFromRun(EDigetherMonsterRemovalReason::Despawned);
	}
	TestEqual(TEXT("소멸 후 파워 반환"), Director->GetDungeonPowerUsed(TEXT("D1")), 0);
	TestEqual(TEXT("소멸 후 탐사당 생성 수 유지"), Director->GetRunSpawnedCount(Box), 1);

	// 잘못된 정의 거절
	UDigetherMonsterDefinition* Broken = MakeDefinition(TEXT("DA_TestBroken"), EDigetherMonsterSpawnKind::PeriodicPower, EDigetherMonsterZone::Dungeon, Tree);
	Broken->MonsterClass.Reset();
	TestEqual(TEXT("클래스 없는 정의 거절"), Director->SpawnMonsterAt(Broken, FTransform::Identity, TEXT("D1")), EDigetherMonsterSpawnResult::InvalidDefinition);

	// 이전 런 ID 종료 요청 무시, 현재 런 종료 시 정리
	Director->EndRun(FGuid::NewGuid());
	TestTrue(TEXT("다른 런 ID 종료 무시"), Director->HasActiveRun());
	Director->EndRun(RunId);
	TestFalse(TEXT("런 종료"), Director->HasActiveRun());
	TestTrue(TEXT("종료 시 몬스터 정리"), !IsValid(SpiderActor) || SpiderActor->IsRemoved());

	// 서식지: 붕괴 구간·엘리베이터 근처 후보 제외, 생존자 거리 필터 없음
	AddPoint(World, EDigetherMonsterSpawnPointKind::HabitatNest, FVector(10000.f, 0.f, 200.f), true);
	AddPoint(World, EDigetherMonsterSpawnPointKind::HabitatNest, FVector(500.f, 0.f, 200.f));
	ADigetherMonsterSpawnPoint* FarNest = AddPoint(World, EDigetherMonsterSpawnPointKind::HabitatNest, FVector(-8000.f, 0.f, 200.f));
	const FGuid HabitatRun = FGuid::NewGuid();
	Director->BeginRun(Table, HabitatRun, 0, 7);
	Director->NotifyMapReady(HabitatRun, FVector::ZeroVector);
	TArray<ADigetherMonsterCharacter*> Live;
	Director->GetLiveMonsters(Live);
	TestEqual(TEXT("둥지 1마리 배치"), Live.Num(), 1);
	if (Live.Num() == 1 && FarNest)
	{
		TestTrue(TEXT("허용된 둥지 후보에 배치"), Live[0]->GetActorLocation().Equals(FarNest->GetActorLocation(), 100.f));
	}
	Director->NotifyMapReady(HabitatRun, FVector::ZeroVector);
	Director->GetLiveMonsters(Live);
	TestEqual(TEXT("맵 준비 중복 통지에도 추가 배치 없음"), Live.Num(), 1);

	// 갱도 주기형: 스폰 타이머 시작 후 판정, 종별 최대 수 1
	AddPoint(World, EDigetherMonsterSpawnPointKind::MinePeriodic, FVector(0.f, 3000.f, 200.f));
	UDigetherMonsterSettings* Settings = GetMutableDefault<UDigetherMonsterSettings>();
	const float SavedDelay = Settings->SpawnTimerStartDelaySeconds;
	Settings->SpawnTimerStartDelaySeconds = 0.f;
	Director->NotifyMineArrival(FGuid::NewGuid());
	TestFalse(TEXT("다른 런 ID 도착 통지 무시"), Director->IsSpawnTimerRunning());
	Director->NotifyMineArrival(HabitatRun);
	Settings->SpawnTimerStartDelaySeconds = SavedDelay;
	TestTrue(TEXT("지연 0이면 즉시 스폰 타이머 시작"), Director->IsSpawnTimerRunning());
	Director->DevAdvanceSpawnTimer(10.f);
	TestEqual(TEXT("곡선 확률 1"), Director->GetCurrentChance(), 1.f, 1e-4f);
	Director->DevRunEvaluationNow();
	TestEqual(TEXT("갱도 주기형 1마리"), Director->GetAliveCount(Mole), 1);
	Director->DevRunEvaluationNow();
	TestEqual(TEXT("종별 최대 수에서 멈춤"), Director->GetAliveCount(Mole), 1);
	TestEqual(TEXT("던전 볼륨 없으면 던전 생성 없음"), Director->GetRunSpawnedCount(Box), 0);
	Director->EndRun(HabitatRun);
	TestFalse(TEXT("종료 후 스폰 타이머 정지"), Director->IsSpawnTimerRunning());

	// 예고: 예고 동안 상한을 예약, 시간이 지나면 생성
	Mole->SpawnTelegraphSeconds = 2.f;
	auto StartTimerNow = [Director, Settings](const FGuid& Run)
	{
		const float Saved = Settings->SpawnTimerStartDelaySeconds;
		Settings->SpawnTimerStartDelaySeconds = 0.f;
		Director->NotifyMineArrival(Run);
		Settings->SpawnTimerStartDelaySeconds = Saved;
		Director->DevAdvanceSpawnTimer(10.f);
	};
	// 엔진 타이머는 한 프레임에 한 번만 틱하고 새 타이머는 다음 틱에 활성화되므로 프레임을 넘겨 두 번 틱
	auto TickTimers = [World](const float Seconds)
	{
		++GFrameCounter;
		World->GetTimerManager().Tick(0.f);
		++GFrameCounter;
		World->GetTimerManager().Tick(Seconds);
	};
	const FGuid TelegraphRun = FGuid::NewGuid();
	Director->BeginRun(Table, TelegraphRun, 0, 9);
	StartTimerNow(TelegraphRun);
	Director->DevRunEvaluationNow();
	Director->GetLiveMonsters(Live);
	TestEqual(TEXT("예고 중에는 아직 생성 안 됨"), Live.Num(), 0);
	TestEqual(TEXT("예고 중 최대 수 예약"), Director->GetAliveCount(Mole), 1);
	Director->DevRunEvaluationNow();
	TestEqual(TEXT("예약으로 중복 예고 없음"), Director->GetRunSpawnedCount(Mole), 1);
	TickTimers(2.5f);
	Director->GetLiveMonsters(Live);
	TestEqual(TEXT("예고 후 생성"), Live.Num(), 1);
	Director->EndRun(TelegraphRun);

	// 예고 중 런 종료: 이전 런 예약은 다음 런에서 생성되지 않음
	const FGuid AbortedRun = FGuid::NewGuid();
	Director->BeginRun(Table, AbortedRun, 0, 11);
	StartTimerNow(AbortedRun);
	Director->DevRunEvaluationNow();
	TestEqual(TEXT("예고 예약"), Director->GetAliveCount(Mole), 1);
	Director->EndRun(AbortedRun);
	const FGuid NextRun = FGuid::NewGuid();
	Director->BeginRun(Table, NextRun, 0, 12);
	TickTimers(2.5f);
	Director->GetLiveMonsters(Live);
	TestEqual(TEXT("종료된 런의 예고는 폐기"), Live.Num(), 0);
	TestEqual(TEXT("새 런 장부는 비어 있음"), Director->GetAliveCount(Mole), 0);
	Director->EndRun(NextRun);
	return true;
}

#endif
