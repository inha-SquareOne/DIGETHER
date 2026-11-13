#if WITH_DEV_AUTOMATION_TESTS
#include "DigetherTunnelLayoutCheck.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherTunnelLayoutCheckTest,
	"DIGETHER.Game.Tunnel.LayoutPoints", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherTunnelLayoutCheckTest::RunTest(const FString& Parameters)
{
	// 빈 독립 월드에서 검사, 사용자 레벨과 에셋은 변경하지 않음
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->AddToRoot();
	Instance->InitializeStandalone();
	UWorld* World = Instance->GetWorld();
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADigetherTunnelLayoutCheck* Check = World->SpawnActor<ADigetherTunnelLayoutCheck>();
	TestFalse(TEXT("Both missing"), Check->CheckLayout());
	APlayerStart* Start = World->SpawnActor<APlayerStart>(SpawnParams);
	TestFalse(TEXT("Entrance missing"), Check->CheckLayout());
	ATargetPoint* Entrance = World->SpawnActor<ATargetPoint>();
	TestFalse(TEXT("Untagged point is not an entrance"), Check->CheckLayout());
	Entrance->Tags.Add(ADigetherTunnelLayoutCheck::EntranceTag);
	TestTrue(TEXT("One of each"), Check->CheckLayout());
	TestTrue(TEXT("Returns exact start"), Check->StartPoint == Start);
	TestTrue(TEXT("Returns exact entrance"), Check->DungeonEntrance == Entrance);
	TestTrue(TEXT("Repeated checks preserve valid result"), Check->CheckLayout());
	APlayerStart* ExtraStart = World->SpawnActor<APlayerStart>(SpawnParams);
	TestFalse(TEXT("Duplicate start"), Check->CheckLayout());
	TestTrue(TEXT("Failure clears previous references"), !Check->StartPoint && !Check->DungeonEntrance);
	ExtraStart->Destroy();
	ATargetPoint* ExtraEntrance = World->SpawnActor<ATargetPoint>();
	ExtraEntrance->Tags.Add(ADigetherTunnelLayoutCheck::EntranceTag);
	TestFalse(TEXT("Duplicate entrance"), Check->CheckLayout());
	Start->Destroy();
	TestFalse(TEXT("Missing start and duplicate entrance together"), Check->CheckLayout());
	TestTrue(TEXT("Reports both counts"), Check->CheckMessage.ToString().Contains(TEXT("0")) &&
		Check->CheckMessage.ToString().Contains(TEXT("2")));
	ExtraEntrance->Destroy();
	Start = World->SpawnActor<APlayerStart>(SpawnParams);
	TestTrue(TEXT("Recovers after correction"), Check->CheckLayout());
	TestTrue(TEXT("Returns replacement start"), Check->StartPoint == Start);
	Instance->Shutdown();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	Instance->RemoveFromRoot();
	return true;
}
#endif
