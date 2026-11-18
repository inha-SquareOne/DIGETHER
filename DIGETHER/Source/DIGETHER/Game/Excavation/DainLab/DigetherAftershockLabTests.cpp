#if WITH_DEV_AUTOMATION_TESTS
#include "DigetherAftershockLab.h"
#include "DigetherCeilingProbe.h"
#include "Game/Excavation/Dain/DigetherDiggableSoil.h"
#include "Components/BoxComponent.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherAftershockLabTest, "DIGETHER.Game.Aftershock.Lab",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherAftershockLabTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->AddToRoot();
	Instance->InitializeStandalone();
	UWorld* World = Instance->GetWorld();
	const FURL URL(nullptr, TEXT("/Engine/Maps/Entry?game=/Script/Engine.GameModeBase"), TRAVEL_Absolute);
	World->SetGameMode(URL);
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();
	const auto Step = [World](int32 Frames)
	{
		for (int32 Index = 0; Index < Frames; ++Index)
		{
			++GFrameCounter;
			World->Tick(LEVELTICK_All, .05f);
		}
	};
	// 저장 맵 없이 실제 TimerManager와 BeginPlay 등록 경로 실행
	ADigetherAftershockLab* Lab = World->SpawnActorDeferred<ADigetherAftershockLab>(
		ADigetherAftershockLab::StaticClass(), FTransform::Identity);
	Lab->IntervalSeconds = .5f;
	Lab->InitialSegments.Add(TEXT("A"), 50.f);
	Lab->InitialSegments.Add(TEXT("B"), 100.f);
	Lab->FinishSpawning(FTransform::Identity);
	const auto GetDamage = [Lab](FName Id)
	{
		const FDigetherAftershockLabSegment* Segment = Lab->Segments.FindByPredicate(
			[Id](const FDigetherAftershockLabSegment& Value) { return Value.SegmentId == Id; });
		return Segment ? Segment->Damage : -1.f;
	};
	TestEqual(TEXT("two explicitly registered segments"), Lab->Segments.Num(), 2);
	Step(8);
	TestEqual(TEXT("no early quake before 0.5 seconds"), Lab->AftershockCount, 0);
	Step(4);
	TestEqual(TEXT("first timed quake"), Lab->AftershockCount, 1);
	TestEqual(TEXT("first damage A"), GetDamage(TEXT("A")), 25.f);
	TestEqual(TEXT("first damage B"), GetDamage(TEXT("B")), 25.f);
	Step(10);
	TestEqual(TEXT("second timed quake"), Lab->AftershockCount, 2);
	TestEqual(TEXT("A threshold request"), Lab->CollapseRequestCount, 1);
	TestEqual(TEXT("B remains independent"), GetDamage(TEXT("B")), 50.f);
	Step(20);
	TestEqual(TEXT("four timed quakes"), Lab->AftershockCount, 4);
	TestEqual(TEXT("B threshold request"), Lab->CollapseRequestCount, 2);
	Step(20);
	TestEqual(TEXT("no duplicate requests"), Lab->CollapseRequestCount, 2);
	TestEqual(TEXT("A capped at threshold"), GetDamage(TEXT("A")), 50.f);
	TestEqual(TEXT("B capped at threshold"), GetDamage(TEXT("B")), 100.f);
	for (const FDigetherAftershockLabSegment& Segment : Lab->Segments)
	{
		TestTrue(TEXT("threshold state latched"), Segment.bCollapseRequested);
	}
	const int32 BeforeChange = Lab->AftershockCount;
	Lab->IntervalSeconds = .25f;
	Lab->RestartSchedule();
	Step(3);
	TestEqual(TEXT("new interval does not fire immediately"), Lab->AftershockCount, BeforeChange);
	Step(3);
	TestEqual(TEXT("new interval takes effect"), Lab->AftershockCount, BeforeChange + 1);
	TestTrue(TEXT("late segment registration"), Lab->RegisterSegment(TEXT("C"), 75.f));
	TestFalse(TEXT("duplicate ID cannot reset damage"), Lab->RegisterSegment(TEXT("A"), 500.f));
	TestFalse(TEXT("empty ID rejected"), Lab->RegisterSegment(NAME_None, 100.f));
	for (float Invalid : {0.f, -1.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
	{
		TestFalse(TEXT("invalid threshold rejected"), Lab->RegisterSegment(TEXT("Invalid"), Invalid));
		Lab->DamagePerAftershock = Invalid;
		const int32 BeforeInvalid = Lab->AftershockCount;
		Lab->TriggerAftershock();
		TestEqual(TEXT("invalid damage does not advance state"), Lab->AftershockCount, BeforeInvalid);
		Lab->IntervalSeconds = Invalid;
		Lab->RestartSchedule();
		Step(10);
		TestEqual(TEXT("invalid interval stops timer"), Lab->AftershockCount, BeforeInvalid);
	}
	Lab->DamagePerAftershock = 25.f;
	Lab->TriggerAftershock();
	TestEqual(TEXT("late segment starts at zero"), GetDamage(TEXT("C")), 25.f);
	Lab->IntervalSeconds = .25f;
	Lab->RestartSchedule();
	// Role 전환은 가드 시험이며 실제 멀티플레이/복제 검증이 아님
	Lab->SetRole(ROLE_SimulatedProxy);
	const int32 BeforeClient = Lab->AftershockCount;
	TestFalse(TEXT("non-authority cannot register"), Lab->RegisterSegment(TEXT("Client"), 100.f));
	Lab->TriggerAftershock();
	Step(10);
	TestEqual(TEXT("non-authority cannot accumulate"), Lab->AftershockCount, BeforeClient);
	Lab->SetRole(ROLE_Authority);
	Lab->RestartSchedule();
	Step(6);
	TestEqual(TEXT("authority timer resumes"), Lab->AftershockCount, BeforeClient + 1);
	TestEqual(TEXT("late segment second hit"), GetDamage(TEXT("C")), 50.f);
	Lab->DamagePerAftershock = std::numeric_limits<float>::max();
	Lab->TriggerAftershock();
	TestEqual(TEXT("large finite damage capped safely"), GetDamage(TEXT("C")), 75.f);
	TestEqual(TEXT("one request per each of three segments"), Lab->CollapseRequestCount, 3);
	const int32 BeforeDestroy = Lab->AftershockCount;
	Lab->Destroy();
	Step(10);
	TestEqual(TEXT("destroy clears callback"), Lab->AftershockCount, BeforeDestroy);
	ADigetherAftershockLab* Clock = World->SpawnActor<ADigetherAftershockLab>();
	Clock->bUseExpeditionClock = true;
	for (const TPair<float, float>& Row : {TPair<float,float>(600,300), {599,180}, {300,180}, {299,90}, {120,90}, {119,45}})
	{
		Clock->InitialRemainingSeconds = Row.Key;
		Clock->RestartSchedule();
		TestEqual(TEXT("stage boundary"), Clock->ReservedIntervalSeconds, Row.Value);
		const int32 Max = Row.Key >= 600 ? 1 : Row.Key >= 120 ? 2 : 3;
		TestTrue(TEXT("reserved count in stage range"), Clock->ReservedCollapseCount >= 0 && Clock->ReservedCollapseCount <= Max);
	}
	Clock->LabClockRate = 60;
	Clock->InitialRemainingSeconds = 950;
	Clock->RestartSchedule();
	Step(96);
	Clock->DeductExpeditionSeconds(400);
	TestEqual(TEXT("elevator changes stage"), Clock->ReservedIntervalSeconds, 90.f);
	Step(6);
	TestEqual(TEXT("elevator preserves earlier deadline"), Clock->AftershockCount, 1);
	TestEqual(TEXT("no ceiling produces no warning"), Clock->CollapseRequestCount, 0);
	Clock->DeductExpeditionSeconds(1000);
	Step(40);
	TestEqual(TEXT("expired clock stops"), Clock->AftershockCount, 1);
	Clock->Destroy();
	World->EndPlay(EEndPlayReason::Quit);
	Instance->Shutdown();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	Instance->RemoveFromRoot();
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherAftershockCeilingTest, "DIGETHER.Game.Aftershock.CeilingLink",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherAftershockCeilingTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->AddToRoot();
	Instance->InitializeStandalone();
	UWorld* World = Instance->GetWorld();
	const FURL URL(nullptr, TEXT("/Engine/Maps/Entry?game=/Script/Engine.GameModeBase"), TRAVEL_Absolute);
	World->SetGameMode(URL);
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();
	ADigetherDiggableSoil* SoilA = World->SpawnActor<ADigetherDiggableSoil>();
	ADigetherDiggableSoil* SoilB = World->SpawnActor<ADigetherDiggableSoil>(FVector(1000, 0, 0), FRotator::ZeroRotator);
	ADigetherCeilingProbe* ProbeA = World->SpawnActor<ADigetherCeilingProbe>();
	ADigetherCeilingProbe* ProbeB = World->SpawnActor<ADigetherCeilingProbe>();
	ProbeA->Soil = SoilA;
	ProbeB->Soil = SoilB;
	ProbeA->SetActorLocation(FVector(200));
	ProbeB->SetActorLocation(FVector(1200, 200, 200));
	TestTrue(TEXT("Initial baseline A"), ProbeA->CaptureBaseline());
	TestTrue(TEXT("Initial baseline B"), ProbeB->CaptureBaseline());
	ADigetherAftershockLab* Lab = World->SpawnActorDeferred<ADigetherAftershockLab>(
		ADigetherAftershockLab::StaticClass(), FTransform::Identity);
	Lab->bRequireCeiling = true;
	Lab->IntervalSeconds = .5f;
	Lab->InitialSegments.Add(TEXT("A"), 75.f);
	Lab->InitialSegments.Add(TEXT("B"), 50.f);
	Lab->InitialSegments.Add(TEXT("Missing"), 50.f);
	Lab->InitialSegments.Add(TEXT("Null"), 50.f);
	Lab->CeilingRegions.Add(TEXT("A"), ProbeA);
	Lab->CeilingRegions.Add(TEXT("B"), ProbeB);
	Lab->CeilingRegions.Add(TEXT("Null"), nullptr);
	Lab->FinishSpawning(FTransform::Identity);
	const auto Damage = [Lab](FName Id)
	{
		return Lab->Segments.FindByPredicate([Id](const FDigetherAftershockLabSegment& Segment)
		{
			return Segment.SegmentId == Id;
		})->Damage;
	};
	Lab->TriggerAftershock();
	TestEqual(TEXT("Uncarved original ceiling inactive"), Damage(TEXT("A")), 0.f);
	TestTrue(TEXT("Carve A"), SoilA->TryExcavate(FVector(200), 100) == EDigetherSoilEditResult::Applied);
	const uint32 Hash = SoilA->GetDensityHash();
	ProbeA->SetActorLocation(FVector(200, 200, 130));
	ProbeA->Region->SetBoxExtent(FVector(150, 150, 45));
	Lab->TriggerAftershock();
	TestEqual(TEXT("Floor never accumulates"), Damage(TEXT("A")), 0.f);
	ProbeA->SetActorLocation(FVector(290, 200, 200));
	ProbeA->Region->SetBoxExtent(FVector(60, 150, 20));
	Lab->TriggerAftershock();
	TestEqual(TEXT("Wall never accumulates"), Damage(TEXT("A")), 0.f);
	ProbeA->SetActorLocation(FVector(200));
	ProbeA->Region->SetBoxExtent(FVector(300));
	Lab->TriggerAftershock();
	TestEqual(TEXT("Carved ceiling accumulates immediately without display tick"), Damage(TEXT("A")), 25.f);
	for (int32 Frame = 0; Frame < 12; ++Frame)
	{
		++GFrameCounter;
		World->Tick(LEVELTICK_All, .05f);
	}
	TestEqual(TEXT("Timed aftershock uses same gate"), Damage(TEXT("A")), 50.f);
	Lab->TriggerAftershock();
	TestEqual(TEXT("Threshold reached once"), Lab->CollapseRequestCount, 1);
	TestEqual(TEXT("Uncarved B independent"), Damage(TEXT("B")), 0.f);
	TestEqual(TEXT("Missing binding inactive"), Damage(TEXT("Missing")), 0.f);
	TestEqual(TEXT("Null binding inactive"), Damage(TEXT("Null")), 0.f);
	TestEqual(TEXT("Aftershock never edits soil"), SoilA->GetDensityHash(), Hash);
	TestTrue(TEXT("Carve B"), SoilB->TryExcavate(FVector(1200, 200, 200), 100) == EDigetherSoilEditResult::Applied);
	Lab->TriggerAftershock();
	TestEqual(TEXT("B starts its own damage"), Damage(TEXT("B")), 25.f);
	ProbeB->SetActorLocation(FVector(5000));
	Lab->TriggerAftershock();
	TestEqual(TEXT("No current candidate freezes existing damage"), Damage(TEXT("B")), 25.f);
	ProbeB->SetActorLocation(FVector(1200, 200, 200));
	TestTrue(TEXT("Reset B before next display tick"), SoilB->ResetSoil());
	Lab->TriggerAftershock();
	TestEqual(TEXT("Reset soil does not reuse stale candidates"), Damage(TEXT("B")), 25.f);
	TestTrue(TEXT("New baseline B"), ProbeB->CaptureBaseline());
	SoilB->TryExcavate(FVector(1200, 200, 200), 100);
	Lab->TriggerAftershock();
	TestEqual(TEXT("Reappearing ceiling resumes accumulated damage"), Damage(TEXT("B")), 50.f);
	TestEqual(TEXT("Two independent requests"), Lab->CollapseRequestCount, 2);
	ProbeA->Destroy();
	SoilB->Destroy();
	Lab->TriggerAftershock();
	TestEqual(TEXT("Destroyed references do not repeat or cancel requests"), Lab->CollapseRequestCount, 2);
	for (const FDigetherAftershockLabSegment& Segment : Lab->Segments)
	{
		TestFalse(TEXT("Invalid or missing targets inactive"), Segment.bDamageActive);
	}
	World->EndPlay(EEndPlayReason::Quit);
	Instance->Shutdown();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	Instance->RemoveFromRoot();
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherCollapseWarningTest, "DIGETHER.Game.Aftershock.WarningDelay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherCollapseWarningTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->AddToRoot();
	Instance->InitializeStandalone();
	UWorld* World = Instance->GetWorld();
	const FURL URL(nullptr, TEXT("/Engine/Maps/Entry?game=/Script/Engine.GameModeBase"), TRAVEL_Absolute);
	World->SetGameMode(URL);
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();
	const auto Step = [World](int32 Frames)
	{
		for (int32 Index = 0; Index < Frames; ++Index)
		{
			++GFrameCounter;
			World->Tick(LEVELTICK_All, .05f);
		}
	};
	ADigetherDiggableSoil* Soil = World->SpawnActor<ADigetherDiggableSoil>();
	ADigetherCeilingProbe* Probe = World->SpawnActor<ADigetherCeilingProbe>();
	Probe->Soil = Soil;
	Probe->SetActorLocation(FVector(200));
	TestTrue(TEXT("Capture intact ceiling baseline"), Probe->CaptureBaseline());
	TestTrue(TEXT("Create real carved ceiling"), Soil->TryExcavate(FVector(200), 100) == EDigetherSoilEditResult::Applied);
	ADigetherAftershockLab* Lab = World->SpawnActorDeferred<ADigetherAftershockLab>(
		ADigetherAftershockLab::StaticClass(), FTransform::Identity);
	Lab->IntervalSeconds = 1000.f;
	Lab->bRequireCeiling = true;
	Lab->InitialSegments.Add(TEXT("A"), 50.f);
	Lab->InitialSegments.Add(TEXT("B"), 75.f);
	Lab->CeilingRegions.Add(TEXT("A"), Probe);
	Lab->CeilingRegions.Add(TEXT("B"), Probe);
	Lab->FinishSpawning(FTransform::Identity);
	const auto Find = [Lab](FName Id) -> const FDigetherAftershockLabSegment&
	{
		return *Lab->Segments.FindByPredicate([Id](const FDigetherAftershockLabSegment& Segment)
		{
			return Segment.SegmentId == Id;
		});
	};
	const uint32 CarvedHash = Soil->GetDensityHash();
	Lab->TriggerAftershock();
	TestEqual(TEXT("Below threshold no warning"), Lab->CollapseRequestCount, 0);
	Lab->TriggerAftershock();
	const float DelayA = Find(TEXT("A")).WarningDelaySeconds;
	const TArray<FVector> WarningA = Find(TEXT("A")).WarningVertices;
	TestTrue(TEXT("Random delay lies between 3 and 10"), DelayA >= 3.f && DelayA <= 10.f);
	TestTrue(TEXT("Warning captures actual ceiling faces"), WarningA.Num() > 0 && WarningA.Num() % 3 == 0);
	TestEqual(TEXT("Request is not immediate execution"), Lab->ExecutionRequestCount, 0);
	TestFalse(TEXT("B has not reached threshold"), Find(TEXT("B")).bCollapseRequested);
	Step(20);
	Lab->TriggerAftershock();
	const float DelayB = Find(TEXT("B")).WarningDelaySeconds;
	TestTrue(TEXT("B gets its own bounded delay"), DelayB >= 3.f && DelayB <= 10.f);
	TestEqual(TEXT("Repeated quake does not redraw A delay"), Find(TEXT("A")).WarningDelaySeconds, DelayA);
	// 여진 정지·표시 영역 이동·천장 초기화·검사 Actor 삭제는 이미 예약된 실행을 취소하지 않음
	Lab->IntervalSeconds = 0.f;
	Lab->RestartSchedule();
	Probe->SetActorLocation(FVector(5000));
	Soil->ResetSoil();
	const uint32 ResetHash = Soil->GetDensityHash();
	Probe->Destroy();
	TestTrue(TEXT("Warning world vertices frozen"), Find(TEXT("A")).WarningVertices == WarningA);
	Step(10);
	TestEqual(TEXT("No execution before minimum delay"), Lab->ExecutionRequestCount, 0);
	float Elapsed = 1.5f;
	for (int32 Frame = 0; Frame < 220; ++Frame)
	{
		Step(1);
		Elapsed += .05f;
		for (const FName Id : {FName(TEXT("A")), FName(TEXT("B"))})
		{
			const float Due = Find(Id).WarningDelaySeconds + (Id == TEXT("B") ? 1.f : 0.f);
			if (Elapsed < Due - .1f)
			{
				TestFalse(TEXT("Independent deadline not early"), Find(Id).bExecutionRequested);
			}
			if (Elapsed > Due + .15f)
			{
				TestTrue(TEXT("Execution requested after deadline despite stopped quakes"), Find(Id).bExecutionRequested);
			}
		}
	}
	TestEqual(TEXT("One execution per segment"), Lab->ExecutionRequestCount, 2);
	Lab->TriggerAftershock();
	Step(220);
	TestEqual(TEXT("No repeated execution"), Lab->ExecutionRequestCount, 2);
	TestEqual(TEXT("Timer callback never changes terrain"), Soil->GetDensityHash(), ResetHash);
	TestTrue(TEXT("Test reset really changed carved soil"), CarvedHash != ResetHash);
	ADigetherAftershockLab* Destroyed = World->SpawnActor<ADigetherAftershockLab>();
	Destroyed->RegisterSegment(TEXT("Destroyed"), 1.f);
	Destroyed->TriggerAftershock();
	Destroyed->Destroy();
	Step(220);
	TestEqual(TEXT("Actor destruction cancels its pending timer"), Destroyed->ExecutionRequestCount, 0);
	World->EndPlay(EEndPlayReason::Quit);
	Instance->Shutdown();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	Instance->RemoveFromRoot();
	return true;
}
#endif
