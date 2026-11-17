#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Inventory/DigetherWorldItemActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Game/Data/DigetherItemDefinition.h"
#include "Misc/AutomationTest.h"

namespace
{
	class FDigetherWorldItemTestWorld
	{
	public:
		FDigetherWorldItemTestWorld()
		{
			UWorld::InitializationValues Values;
			Values.InitializeScenes(false).AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false)
				.CreateFXSystem(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &Values);
		}

		~FDigetherWorldItemTestWorld()
		{
			if (World != nullptr)
			{
				World->DestroyWorld(false);
			}
		}

		FDigetherWorldItemTestWorld(const FDigetherWorldItemTestWorld&) = delete;
		FDigetherWorldItemTestWorld& operator=(const FDigetherWorldItemTestWorld&) = delete;

		UWorld* World = nullptr;
	};
}

// 바닥 아이템의 보이는 크기가 정의의 WorldMesh 크기 × WorldMeshScale과 같은지(곡괭이·삽 실제 에셋)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherWorldItemScaleTest,
	"DIGETHER.Game.Inventory.WorldItem.MeshScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherWorldItemScaleTest::RunTest(const FString& Parameters)
{
	FDigetherWorldItemTestWorld Fixture;
	if (!TestNotNull(TEXT("독립 검사 월드"), Fixture.World))
	{
		return false;
	}
	for (const TCHAR* DefinitionName : { TEXT("DA_Item_Pickaxe"), TEXT("DA_Item_Shovel") })
	{
		const UDigetherItemDefinition* Definition = UDigetherItemDefinition::FindDefinition(DefinitionName);
		if (!IsValid(Definition) || Definition->WorldMesh.IsNull())
		{
			AddInfo(FString::Printf(TEXT("%s 정의나 WorldMesh가 없어 건너뜀"), DefinitionName));
			continue;
		}
		const UStaticMesh* Mesh = Definition->WorldMesh.LoadSynchronous();
		ADigetherWorldItemActor* Actor = ADigetherWorldItemActor::DropItem(Fixture.World, FDigetherItemInstance::MakeNew(DefinitionName),
			FVector(0.f, 0.f, 1000.f), FRotator::ZeroRotator, FVector::ForwardVector, FVector::ZeroVector);
		if (!TestNotNull(FString::Printf(TEXT("%s 월드 아이템 생성"), DefinitionName), Actor))
		{
			continue;
		}
		if (!Actor->HasActorBegunPlay())
		{
			// 레벨에 놓으며 액터 배율이 바뀐 경우(예전 구조의 0.3)도 시작하면 정의 크기로 돌아와야 함
			Actor->SetActorScale3D(FVector(0.3f));
			Actor->DispatchBeginPlay();
		}
		TestTrue(FString::Printf(TEXT("%s 시작 후 액터 배율 1"), DefinitionName), Actor->GetActorScale3D().Equals(FVector::OneVector));
		const UStaticMeshComponent* MeshComponent = Actor->FindComponentByClass<UStaticMeshComponent>();
		if (!TestNotNull(TEXT("메시 컴포넌트"), MeshComponent))
		{
			continue;
		}
		TestTrue(FString::Printf(TEXT("%s 메시"), DefinitionName), MeshComponent->GetStaticMesh() == Mesh);
		const FVector Expected = Mesh->GetBounds().BoxExtent * Definition->WorldMeshScale;
		const FVector Actual = MeshComponent->Bounds.BoxExtent;
		AddInfo(FString::Printf(TEXT("%s 배율 %.2f 기대 반 크기 %s 실제 %s, 컴포넌트 월드 배율 %s"), DefinitionName,
			Definition->WorldMeshScale, *Expected.ToString(), *Actual.ToString(), *MeshComponent->GetComponentScale().ToString()));
		TestTrue(FString::Printf(TEXT("%s 보이는 크기 = 메시 × WorldMeshScale"), DefinitionName), Actual.Equals(Expected, 1.f));
		Actor->Destroy();
	}
	return true;
}

#endif
