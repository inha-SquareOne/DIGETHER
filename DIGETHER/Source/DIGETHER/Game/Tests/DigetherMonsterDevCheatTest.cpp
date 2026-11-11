#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "BehaviorTree/BehaviorTree.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/Monster/DigetherMonsterCharacter.h"
#include "Game/Monster/DigetherMonsterDefinition.h"
#include "Game/Monster/DigetherMonsterDirectorSubsystem.h"
#include "Game/Monster/DigetherMonsterSenseSubsystem.h"
#include "Game/Monster/DigetherMonsterSettings.h"
#include "Game/Monster/DigetherMonsterSpawnPoint.h"
#include "Game/Monster/DigetherMonsterSpawnTable.h"
#include "Misc/AutomationTest.h"
#include "TimerManager.h"

// 개발 치트 진입점(F-01 파워 제외·위치 강제, F-02 자동 생성 정지·전부 제거)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherMonsterDevCheatTest,
	"DIGETHER.Game.Monster.DevCheat", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	// 유니티 빌드에서 Director 테스트의 익명 이름과 겹치지 않게 별도 이름
	class FMonsterDevCheatTestWorld
	{
	public:
		FMonsterDevCheatTestWorld()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(true);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (World && GEngine)
			{
				GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			}
		}
		~FMonsterDevCheatTestWorld()
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
		FMonsterDevCheatTestWorld(const FMonsterDevCheatTestWorld&) = delete;
		FMonsterDevCheatTestWorld& operator=(const FMonsterDevCheatTestWorld&) = delete;

		UWorld* World = nullptr;
	};

	// 시험용 정의, 수치는 픽스처이며 기획 값 아님
	UDigetherMonsterDefinition* MakeDevCheatDefinition(const TCHAR* Name, const EDigetherMonsterSpawnKind Kind, const EDigetherMonsterZone Zone,
		UBehaviorTree* Tree)
	{
		UDigetherMonsterDefinition* Definition = NewObject<UDigetherMonsterDefinition>(GetTransientPackage(), FName(Name), RF_Transient);
		Definition->SpawnKind = Kind;
		Definition->Zone = Zone;
		Definition->PowerLevel = Kind == EDigetherMonsterSpawnKind::PeriodicPower ? 3 : 0;
		Definition->MonsterClass = ADigetherMonsterCharacter::StaticClass();
		Definition->BehaviorTree = Tree;
		Definition->Movement.CapsuleRadius = 55.f;
		Definition->Movement.CapsuleHalfHeight = 80.f;
		Definition->Movement.MaxStepHeight = 100.f;
		Definition->Movement.WalkSpeed = 250.f;
		return Definition;
	}

	ADigetherMonsterSpawnPoint* AddDevCheatPoint(UWorld* World, const EDigetherMonsterSpawnPointKind Kind, const FVector& Location)
	{
		ADigetherMonsterSpawnPoint* Point = World->SpawnActor<ADigetherMonsterSpawnPoint>(Location, FRotator::ZeroRotator);
		if (Point)
		{
			Point->PointKind = Kind;
		}
		return Point;
	}
}

bool FDigetherMonsterDevCheatTest::RunTest(const FString& Parameters)
{
	FMonsterDevCheatTestWorld TestWorld;
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
	TestTrue(TEXT("디렉터 활성"), Director->Activate());
	TestTrue(TEXT("감각 활성"), Sense->Activate());

	UBehaviorTree* Tree = NewObject<UBehaviorTree>(GetTransientPackage(), NAME_None, RF_Transient);
	UDigetherMonsterDefinition* Box = MakeDevCheatDefinition(TEXT("DA_DevCheatBox"), EDigetherMonsterSpawnKind::PeriodicPower, EDigetherMonsterZone::Dungeon, Tree);
	UDigetherMonsterDefinition* Spider = MakeDevCheatDefinition(TEXT("DA_DevCheatSpider"), EDigetherMonsterSpawnKind::Habitat, EDigetherMonsterZone::Mine, Tree);
	UDigetherMonsterDefinition* Mole = MakeDevCheatDefinition(TEXT("DA_DevCheatMole"), EDigetherMonsterSpawnKind::Periodic, EDigetherMonsterZone::Mine, Tree);

	UDigetherMonsterSpawnTable* Table = NewObject<UDigetherMonsterSpawnTable>(GetTransientPackage(), NAME_None, RF_Transient);
	Table->StageId = TEXT("DevCheatStage");
	Table->Curve.GraceSeconds = 0.f;
	Table->Curve.IntervalSeconds = 30.f;
	Table->Curve.StartChance = 1.f;
	Table->Curve.MaxChance = 1.f;
	Table->Curve.MaxChanceAtSeconds = 1.f;
	Table->DungeonPowerLimit.Base = 6;
	Table->DungeonPowerLimit.PerGoalAchieved = 0;
	Table->DungeonPowerLimit.Max = 6;
	FDigetherMonsterPeriodicEntry MoleEntry;
	MoleEntry.Definition = Mole;
	MoleEntry.Weight = 1.f;
	MoleEntry.MaxAlive = 1;
	Table->MinePeriodicEntries.Add(MoleEntry);
	FDigetherMonsterHabitatEntry Habitat;
	Habitat.Definition = Spider;
	Habitat.Chance = 1.f;
	Table->MineHabitatEntries.Add(Habitat);
	TArray<FText> TableProblems;
	TestTrue(TEXT("시험 테이블 유효"), Table->CollectProblems(TableProblems));

	ADigetherMonsterSpawnPoint* MolePoint = AddDevCheatPoint(World, EDigetherMonsterSpawnPointKind::MinePeriodic, FVector(0.f, 3000.f, 200.f));
	AddDevCheatPoint(World, EDigetherMonsterSpawnPointKind::HabitatNest, FVector(-8000.f, 0.f, 200.f));

	// 1. 파워 제외 소환 뒤 제거 시 장부 일치
	const FGuid PowerRun = FGuid::NewGuid();
	TestTrue(TEXT("런 시작"), Director->BeginRun(Table, PowerRun, 0, 21));
	UDigetherMonsterDirectorSubsystem::FDevSpawnOptions Charged;
	Charged.bForceTransform = true;
	Charged.DungeonId = FName(TEXT("D1"));
	UDigetherMonsterDirectorSubsystem::FDevSpawnOptions Free = Charged;
	Free.bChargePower = false;
	ADigetherMonsterCharacter* ChargedBox = nullptr;
	ADigetherMonsterCharacter* FreeBox = nullptr;
	TestEqual(TEXT("파워 부과 소환"), Director->DevSpawnWithOptions(Box, FTransform(FVector(500.f, 0.f, 200.f)), Charged, &ChargedBox),
		EDigetherMonsterSpawnResult::Spawned);
	TestEqual(TEXT("부과 소환 뒤 D1 파워 3"), Director->GetDungeonPowerUsed(TEXT("D1")), 3);
	TestEqual(TEXT("파워 제외 소환"), Director->DevSpawnWithOptions(Box, FTransform(FVector(900.f, 0.f, 200.f)), Free, &FreeBox),
		EDigetherMonsterSpawnResult::Spawned);
	TestEqual(TEXT("제외 소환은 파워 사용량 불변"), Director->GetDungeonPowerUsed(TEXT("D1")), 3);
	TestEqual(TEXT("제외 소환도 생존 수에는 포함"), Director->GetAliveCount(Box), 2);
	if (TestNotNull(TEXT("제외 소환 개체"), FreeBox))
	{
		FreeBox->RemoveFromRun(EDigetherMonsterRemovalReason::Despawned);
	}
	TestEqual(TEXT("제외 소환 개체 제거 때 다른 개체 몫 파워 반환 없음"), Director->GetDungeonPowerUsed(TEXT("D1")), 3);
	if (TestNotNull(TEXT("부과 소환 개체"), ChargedBox))
	{
		ChargedBox->RemoveFromRun(EDigetherMonsterRemovalReason::Killed);
	}
	TestEqual(TEXT("부과 개체 제거 때 부과한 파워만 반환"), Director->GetDungeonPowerUsed(TEXT("D1")), 0);
	TestEqual(TEXT("제거 뒤 생존 수 0"), Director->GetAliveCount(Box), 0);
	TestEqual(TEXT("탐사당 생성 수는 유지"), Director->GetRunSpawnedCount(Box), 2);

	// 2. 위치 강제 생성: 허용 스폰 지점이 있어도 지정 위치 사용
	const FVector ForcedLocation(1500.f, -1500.f, 200.f);
	ADigetherMonsterCharacter* ForcedMole = nullptr;
	UDigetherMonsterDirectorSubsystem::FDevSpawnOptions Forced;
	Forced.bForceTransform = true;
	TestEqual(TEXT("위치 강제 소환"), Director->DevSpawnWithOptions(Mole, FTransform(ForcedLocation), Forced, &ForcedMole),
		EDigetherMonsterSpawnResult::Spawned);
	if (TestNotNull(TEXT("위치 강제 개체"), ForcedMole))
	{
		TestTrue(TEXT("지정 위치에 생성"), ForcedMole->GetActorLocation().Equals(ForcedLocation, 100.f));
		ForcedMole->RemoveFromRun(EDigetherMonsterRemovalReason::Despawned);
	}
	ADigetherMonsterCharacter* PointMole = nullptr;
	UDigetherMonsterDirectorSubsystem::FDevSpawnOptions PreferPoint;
	TestEqual(TEXT("지점 우선 소환"), Director->DevSpawnWithOptions(Mole, FTransform(ForcedLocation), PreferPoint, &PointMole),
		EDigetherMonsterSpawnResult::Spawned);
	if (TestNotNull(TEXT("지점 우선 개체"), PointMole) && MolePoint)
	{
		TestTrue(TEXT("기본은 허용 스폰 지점 사용"), PointMole->GetActorLocation().Equals(MolePoint->GetActorLocation(), 100.f));
		PointMole->RemoveFromRun(EDigetherMonsterRemovalReason::Despawned);
	}
	Director->EndRun(PowerRun);

	// 3. 자동 생성 정지·전부 제거 뒤 예고 스폰 미발생
	UDigetherMonsterSettings* Settings = GetMutableDefault<UDigetherMonsterSettings>();
	const float SavedDelay = Settings->SpawnTimerStartDelaySeconds;
	const float SavedTelegraph = Mole->SpawnTelegraphSeconds;
	Mole->SpawnTelegraphSeconds = 2.f;
	auto StartTimerNow = [Director, Settings, SavedDelay](const FGuid& Run)
	{
		Settings->SpawnTimerStartDelaySeconds = 0.f;
		Director->NotifyMineArrival(Run);
		Settings->SpawnTimerStartDelaySeconds = SavedDelay;
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
	TArray<ADigetherMonsterCharacter*> Live;

	const FGuid PauseRun = FGuid::NewGuid();
	Director->BeginRun(Table, PauseRun, 0, 31);
	StartTimerNow(PauseRun);
	Director->DevRunEvaluationNow();
	TestEqual(TEXT("예고 예약"), Director->GetPendingSpawnCountForDev(), 1);
	TestEqual(TEXT("예고 중 생존 수 예약"), Director->GetAliveCount(Mole), 1);
	TestEqual(TEXT("정지 시 대기 예고 1건 취소"), Director->DevSetAutoSpawnPaused(true), 1);
	TestTrue(TEXT("정지 표시"), Director->IsAutoSpawnPausedForDev());
	TestEqual(TEXT("취소 뒤 예고 없음"), Director->GetPendingSpawnCountForDev(), 0);
	TestEqual(TEXT("취소 뒤 생존 수 반환"), Director->GetAliveCount(Mole), 0);
	TestEqual(TEXT("취소 뒤 탐사당 생성 수 반환"), Director->GetRunSpawnedCount(Mole), 0);
	TickTimers(2.5f);
	Director->GetLiveMonsters(Live);
	TestEqual(TEXT("정지 뒤 예고 스폰 미발생"), Live.Num(), 0);
	Director->DevRunEvaluationNow();
	TestEqual(TEXT("정지 중 주기 판정 생성 없음"), Director->GetRunSpawnedCount(Mole), 0);
	Director->NotifyMapReady(PauseRun, FVector::ZeroVector);
	Director->GetLiveMonsters(Live);
	TestEqual(TEXT("정지 중 서식지 배치 없음"), Live.Num(), 0);
	ADigetherMonsterCharacter* ManualSpider = nullptr;
	TestEqual(TEXT("정지 중에도 수동 소환"), Director->DevSpawnWithOptions(Spider, FTransform(FVector(0.f, -3000.f, 200.f)), Forced, &ManualSpider),
		EDigetherMonsterSpawnResult::Spawned);

	TestEqual(TEXT("재개 시 취소 없음"), Director->DevSetAutoSpawnPaused(false), 0);
	Director->DevRunEvaluationNow();
	TestEqual(TEXT("재개 뒤 다시 예고"), Director->GetPendingSpawnCountForDev(), 1);
	int32 Cancelled = 0;
	TestEqual(TEXT("전부 제거로 수동 소환 개체 제거"), Director->DevClearMonsters(&Cancelled), 1);
	TestEqual(TEXT("전부 제거로 대기 예고 취소"), Cancelled, 1);
	TestTrue(TEXT("제거된 개체"), !IsValid(ManualSpider) || ManualSpider->IsRemoved());
	TestEqual(TEXT("전부 제거 뒤 예약 반환"), Director->GetAliveCount(Mole), 0);
	TestEqual(TEXT("전부 제거 뒤 거미 생존 수 0"), Director->GetAliveCount(Spider), 0);
	TickTimers(2.5f);
	Director->GetLiveMonsters(Live);
	TestEqual(TEXT("전부 제거 뒤 예고 스폰 미발생"), Live.Num(), 0);
	Director->EndRun(PauseRun);

	Director->DevSetAutoSpawnPaused(false);
	Mole->SpawnTelegraphSeconds = SavedTelegraph;
	return true;
}

#endif
