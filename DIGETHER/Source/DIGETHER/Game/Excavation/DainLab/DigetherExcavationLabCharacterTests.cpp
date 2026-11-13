#if WITH_DEV_AUTOMATION_TESTS
#include "DigetherExcavationLabCharacter.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherLabToolContractTest, "DIGETHER.Excavation.Soil.LabToolContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherLabToolContractTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->AddToRoot();
	Instance->InitializeStandalone();
	UWorld* World = Instance->GetWorld();
	const FURL URL(nullptr, TEXT("/Engine/Maps/Entry?game=/Script/Engine.GameModeBase"), TRAVEL_Absolute);
	World->SetGameMode(URL);
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADigetherExcavationLabCharacter* Character = World->SpawnActor<ADigetherExcavationLabCharacter>(
		FVector(0, 0, 95), FRotator::ZeroRotator, Spawn);
	UCameraComponent* Camera = Character->FindComponentByClass<UCameraComponent>();
	Camera->bUsePawnControlRotation = false;
	ADigetherDiggableSoil* Soil = World->SpawnActor<ADigetherDiggableSoil>(
		FVector(200, -200, -25), FRotator::ZeroRotator, Spawn);
	TestTrue(TEXT("initialize eight-cell soil"), Soil->SetComparisonChunkCells(8));
	const uint32 InitialHash = Soil->GetDensityHash();
	FHitResult Hit;
	TestEqual(TEXT("default reach preserves 400cm"), Character->ToolReachCm, 400.f);
	TestTrue(TEXT("default reach hits soil"), Character->TraceTool(Hit) && Hit.GetActor() == Soil);
	// 이전 타격 정보가 무효 사거리 검사 뒤 남지 않는지 함께 확인
	for (float Reach : {0.f, -1.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
	{
		Character->ToolReachCm = Reach;
		TestFalse(TEXT("invalid reach rejects trace"), Character->TraceTool(Hit));
		TestNull(TEXT("invalid reach clears hit"), Hit.GetActor());
		TestTrue(TEXT("invalid reach rejects use"), Character->TryDigOnce() == EDigetherSoilEditResult::Rejected);
	}
	Character->ToolReachCm = 100.f;
	TestFalse(TEXT("short reach misses soil"), Character->TraceTool(Hit));
	TestTrue(TEXT("miss rejected"), Character->TryDigOnce() == EDigetherSoilEditResult::Rejected);
	Character->ToolReachCm = 400.f;
	TestTrue(TEXT("miss cooldown preserved"), Character->TryDigOnce() == EDigetherSoilEditResult::Rejected);
	TestEqual(TEXT("rejected attempts preserve density"), Soil->GetDensityHash(), InitialHash);
	// 물리 이동 없이 시험 시각만 진행, 사거리 비교 중 원점 고정
	World->Tick(LEVELTICK_TimeOnly, 1.f);
	TestTrue(TEXT("applied result forwarded"), Character->TryDigOnce() == EDigetherSoilEditResult::Applied);
	TestEqual(TEXT("one edit committed"), Soil->GetRevision(), uint32(1));
	const uint32 EditedHash = Soil->GetDensityHash();
	TestTrue(TEXT("cooldown rejected"), Character->TryDigOnce() == EDigetherSoilEditResult::Rejected);
	Character->SelectPickaxe();
	TestFalse(TEXT("legacy bool preserves switch cooldown"), Character->DigOnce());
	TestEqual(TEXT("cooldown preserves edit"), Soil->GetDensityHash(), EditedHash);
	World->Tick(LEVELTICK_TimeOnly, 1.f);
	// 실제 표면은 남기고 모든 표본을 보호해 NoChange와 Rejected를 구분
	Soil->ProtectedRegions.Add(FBox(FVector(-1000), FVector(1000)));
	TestTrue(TEXT("protected soil remains traceable"), Character->TraceTool(Hit) && Hit.GetActor() == Soil);
	TestTrue(TEXT("no-change result forwarded"), Character->TryDigOnce() == EDigetherSoilEditResult::NoChange);
	TestEqual(TEXT("protected edit preserves density"), Soil->GetDensityHash(), EditedHash);
	TestEqual(TEXT("protected edit preserves revision"), Soil->GetRevision(), uint32(1));
	World->EndPlay(EEndPlayReason::Quit);
	Instance->Shutdown();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	Instance->RemoveFromRoot();
	return true;
}
#endif
