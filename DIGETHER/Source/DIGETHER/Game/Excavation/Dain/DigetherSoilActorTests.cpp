#if WITH_DEV_AUTOMATION_TESTS
#include "DigetherDiggableSoil.h"
#include "Game/Excavation/DainLab/DigetherDainCharacter.h"
#include "Camera/CameraComponent.h"
#include "Misc/AutomationTest.h"
#include "DigetherSoilProfiling.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/GarbageCollection.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"

// 실패 주입·후보 검증 전용 friend 접근자, 런타임 게임 API와 분리
class FDigetherSoilActorTestAccess
{
public:
	static int32 CountAir(const ADigetherDiggableSoil& Soil)
	{
		return Soil.Density.FilterByPredicate([](uint8 Value) { return Value < 128; }).Num();
	}
	static void SetFailure(ADigetherDiggableSoil& Soil, int32 Index)
	{
		Soil.FailPreparedChunkForTest = Index;
	}
	static bool RejectMissingCookedCollision(ADigetherDiggableSoil& Soil)
	{
		UProceduralMeshComponent* Next = Soil.bFrontActive ? Soil.SoilBack.Get() : Soil.SoilMesh.Get();
		UBodySetup* Body = Next->GetBodySetup();
		Body->bNeverNeedsCookedCollisionData = true;
		TArray<uint8> Candidate = Soil.Density;
		FBox Changed;
		DigetherSoil::Carve(Soil.AppliedGrid, Candidate, FVector(25, 200, 200), 100, {}, Changed);
		const bool bRejected = !Soil.Apply(Soil.AppliedGrid, Candidate);
		Body->bNeverNeedsCookedCollisionData = false;
		return bRejected;
	}
	static bool RejectInvalidCandidate(ADigetherDiggableSoil& Soil)
	{
		TArray<uint8> Bad = Soil.Density;
		Bad.Pop();
		return !Soil.Apply(Soil.AppliedGrid, Bad);
	}
};
namespace
{
	// GameInstance·World 단독 소유, 복사 금지와 소멸 정리로 시험 간 상태 격리
	struct FDigetherSoilTestWorld
	{
		UGameInstance* Instance = nullptr;
		UWorld* World = nullptr;
		FDigetherSoilTestWorld()
		{
			Instance = NewObject<UGameInstance>(GEngine);
			Instance->AddToRoot();
			Instance->InitializeStandalone();
			World = Instance->GetWorld();
			const FURL URL(nullptr, TEXT("/Engine/Maps/Entry?game=/Script/Engine.GameModeBase"), TRAVEL_Absolute);
			World->SetGameMode(URL);
			World->InitializeActorsForPlay(URL);
			World->BeginPlay();
		}
		// 월드 수명은 이 도우미 하나에서만 관리
		FDigetherSoilTestWorld(const FDigetherSoilTestWorld&) = delete;
		FDigetherSoilTestWorld& operator=(const FDigetherSoilTestWorld&) = delete;
		~FDigetherSoilTestWorld()
		{
			World->EndPlay(EEndPlayReason::Quit);
			Instance->Shutdown();
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
			Instance->RemoveFromRoot();
		}
		// 입력과 프레임 수 입력 → 실제 CharacterMovement 진행, 위치 직접 변경 없음
		void Step(ACharacter* Character, int32 Frames, bool bWalk = false, float Direction = 1.f)
		{
			for (int32 FrameIndex = 0; FrameIndex < Frames; ++FrameIndex)
			{
				if (bWalk)
				{
					Character->AddMovementInput(FVector::ForwardVector, Direction);
				}
				++GFrameCounter;
				World->Tick(LEVELTICK_All, 1.f / 60.f);
			}
		}
		void CreateFloor()
		{
			AActor* FloorActor = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(FloorActor);
			FloorActor->SetRootComponent(Box);
			FloorActor->AddInstanceComponent(Box);
			Box->SetBoxExtent(FVector(2000, 2000, 25));
			Box->SetCollisionProfileName(TEXT("BlockAll"));
			Box->RegisterComponent();
			FloorActor->SetActorLocation(FVector(0, 0, -25));
		}
		ACharacter* CreateCharacter(FVector Position)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ACharacter* Character = World->SpawnActor<ACharacter>(Position, FRotator::ZeroRotator, Params);
			Character->GetCapsuleComponent()->SetCapsuleSize(35, 90);
			Character->GetCharacterMovement()->MaxWalkSpeed = 250;
			Character->GetCharacterMovement()->MaxStepHeight = 35;
			// LocalPlayer 없는 시험 월드에서 실제 이동 컴포넌트의 입력 처리 허용
			Character->GetCharacterMovement()->bRunPhysicsWithNoController = true;
			Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			return Character;
		}
		bool TraceVisibility(FVector From, FVector To, FHitResult& OutHit)
		{
			return World->LineTraceSingleByChannel(OutHit, From, To, ECC_Visibility);
		}
	};
} // namespace
// 비교 모드 입력 → 동일 이동·충돌 시나리오 실행, 검사 문구·임계값 유지
static bool VerifyWalk(FAutomationTestBase& Test, int32 ChunkCells)
{
	FDigetherSoilTestWorld TestWorld;
	TestWorld.CreateFloor();
	ADigetherDiggableSoil* Soil =
		TestWorld.World->SpawnActor<ADigetherDiggableSoil>(FVector(200, -200, -25), FRotator::ZeroRotator);
	if (ChunkCells != 0)
	{
		Test.TestTrue(TEXT("chunk initialization"), Soil->SetComparisonChunkCells(ChunkCells));
	}
	ACharacter* Character = TestWorld.CreateCharacter(FVector(0, 0, 95));
	TestWorld.Step(Character, 180, true);
	const float Blocked = Character->GetActorLocation().X;
	Test.AddInfo(FString::Printf(TEXT("intact character=%s velocity=%s soil=%s"), *Character->GetActorLocation().ToString(),
							*Character->GetVelocity().ToString(), *Soil->GetActorLocation().ToString()));
	if (!Test.TestTrue(TEXT("walking blocked at intact wall"), Blocked > 100 && Blocked < 225))
	{
		return false;
	}
	FHitResult Hit;
	TestWorld.TraceVisibility(FVector(0, 0, 100), FVector(800, 0, 100), Hit);
	Test.TestTrue(TEXT("first surface dig"), Soil->TryExcavate(Hit.ImpactPoint, 100) == EDigetherSoilEditResult::Applied);
	Test.TestTrue(TEXT("one hit leaves soil"),
			 TestWorld.TraceVisibility(FVector(0, 0, 100), FVector(800, 0, 100), Hit) && Hit.GetActor() == Soil);
	for (int32 Pass = 0; Pass < 10; ++Pass)
	{
		for (double Z : {50.0, 130.0})
		{
			for (double Y : {-40.0, 40.0})
			{
				if (TestWorld.TraceVisibility(FVector(0, Y, Z), FVector(800, Y, Z), Hit) && Hit.GetActor() == Soil)
				{
					Soil->TryExcavate(Hit.ImpactPoint, 100);
				}
			}
		}
	}
	TestWorld.Step(Character, 180, true);
	Test.TestTrue(TEXT("multiple digs applied"), Soil->GetRevision() > 1);
	Test.TestTrue(TEXT("actual CharacterMovement crosses hole"),
			 Character->GetActorLocation().X > 650 && FMath::Abs(Character->GetActorLocation().Y) < 10);
	Test.AddInfo(FString::Printf(TEXT("blockedX=%.2f crossedX=%.2f z=%.2f digs=%u no teleport"), Blocked,
							Character->GetActorLocation().X, Character->GetActorLocation().Z, Soil->GetRevision()));
	TestWorld.Step(Character, 240, true, -1.f);
	Test.TestTrue(TEXT("character returns through same tunnel"),
			 Character->GetActorLocation().X < 150 && FMath::Abs(Character->GetActorLocation().Y) < 10);
	return true;
}

// 비교 모드 입력 → 동일 이동·충돌 시나리오 실행, 검사 문구·임계값 유지
static bool VerifyVertical(FAutomationTestBase& Test, int32 ChunkCells)
{
	// 바닥 충돌 위에 선 뒤 굴착한 구멍으로 낙하하는지 확인
	{
		FDigetherSoilTestWorld TestWorld;
		ADigetherDiggableSoil* Soil = TestWorld.World->SpawnActor<ADigetherDiggableSoil>();
		if (ChunkCells != 0)
		{
			Test.TestTrue(TEXT("chunk initialization"), Soil->SetComparisonChunkCells(ChunkCells));
		}
		ACharacter* Character = TestWorld.CreateCharacter(FVector(200, 200, 475));
		TestWorld.Step(Character, 90);
		Test.TestTrue(TEXT("standing on soil"), FMath::Abs(Character->GetActorLocation().Z - 467) < 8);
		FHitResult Hit;
		for (int32 FrameIndex = 0; FrameIndex < 12; ++FrameIndex)
		{
			if (TestWorld.TraceVisibility(FVector(200, 200, 900), FVector(200, 200, -100), Hit) &&
				Hit.GetActor() == Soil)
			{
				Soil->TryExcavate(Hit.ImpactPoint, 120);
			}
		}
		TestWorld.Step(Character, 90);
		Test.TestTrue(TEXT("character falls through excavated floor"), Character->GetActorLocation().Z < 0);
		Test.AddInfo(
			FString::Printf(TEXT("floor finalZ=%.2f digs=%u"), Character->GetActorLocation().Z, Soil->GetRevision()));
	}
	// 천장 아래면 굴착 후 시험 점프 속도로 관통 확인, 정식 캐릭터 수치와 분리
	{
		FDigetherSoilTestWorld TestWorld;
		TestWorld.CreateFloor();
		ADigetherDiggableSoil* Soil =
			TestWorld.World->SpawnActor<ADigetherDiggableSoil>(FVector(0, 0, 300), FRotator::ZeroRotator);
		if (ChunkCells != 0)
		{
			Test.TestTrue(TEXT("chunk initialization"), Soil->SetComparisonChunkCells(ChunkCells));
		}
		ACharacter* Character = TestWorld.CreateCharacter(FVector(200, 200, 95));
		TestWorld.Step(Character, 60);
		FHitResult Hit;
		Test.TestTrue(TEXT("ceiling underside blocks"),
				 TestWorld.TraceVisibility(FVector(200, 200, 200), FVector(200, 200, 900), Hit) &&
					 Hit.GetActor() == Soil);
		for (int32 FrameIndex = 0; FrameIndex < 12; ++FrameIndex)
		{
			if (TestWorld.TraceVisibility(FVector(200, 200, 200), FVector(200, 200, 900), Hit) &&
				Hit.GetActor() == Soil)
			{
				Soil->TryExcavate(Hit.ImpactPoint, 120);
			}
		}
		Character->GetCharacterMovement()->JumpZVelocity = 1400;
		Character->Jump();
		double Highest = Character->GetActorLocation().Z;
		for (int32 FrameIndex = 0; FrameIndex < 100; ++FrameIndex)
		{
			TestWorld.Step(Character, 1);
			Highest = FMath::Max(Highest, Character->GetActorLocation().Z);
		}
		Test.TestTrue(TEXT("character jumps above excavated ceiling"), Highest > 765);
		Test.AddInfo(FString::Printf(TEXT("ceiling highestZ=%.2f digs=%u (test jump speed)"), Highest, Soil->GetRevision()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSoilActorTransactionTest, "DIGETHER.Excavation.SoilActor.Transaction",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherSoilActorTransactionTest::RunTest(const FString& Parameters)
{
	FDigetherSoilTestWorld TestWorld;
	ADigetherDiggableSoil* Soil = TestWorld.World->SpawnActor<ADigetherDiggableSoil>();
	FHitResult Hit;
	TestTrue(TEXT("initial cooked wall blocks"),
			 TestWorld.TraceVisibility(FVector(-100, 200, 200), FVector(500, 200, 200), Hit) && Hit.GetActor() == Soil);
	const uint32 Hash = Soil->GetDensityHash();
	TestTrue(TEXT("missing cooked collision rejected"),
			 FDigetherSoilActorTestAccess::RejectMissingCookedCollision(*Soil));
	TestTrue(TEXT("bad candidate rejected"), FDigetherSoilActorTestAccess::RejectInvalidCandidate(*Soil));
	TestEqual(TEXT("failed prepare preserves density"), Soil->GetDensityHash(), Hash);
	TestEqual(TEXT("failed prepare preserves revision"), Soil->GetRevision(), 0u);
	TestTrue(TEXT("failed prepare preserves actual collision"),
			 TestWorld.TraceVisibility(FVector(-100, 200, 200), FVector(500, 200, 200), Hit));
	Soil->ProtectedRegions = {FBox(FVector::ZeroVector, FVector(400))};
	TestTrue(TEXT("protected wall unchanged"),
			 Soil->TryExcavate(FVector(25, 200, 200), 120) == EDigetherSoilEditResult::NoChange);
	Soil->ProtectedRegions.Reset();
	TestTrue(TEXT("whole soil removal"), Soil->TryExcavate(FVector(200), 600) == EDigetherSoilEditResult::Applied);
	TestFalse(TEXT("empty has no old collision"),
			  TestWorld.TraceVisibility(FVector(-100, 200, 200), FVector(500, 200, 200), Hit));
	TArray<UProceduralMeshComponent*> Meshes;
	Soil->GetComponents(Meshes);
	bool bEmpty = true;
	for (UProceduralMeshComponent* Mesh : Meshes)
	{
		bEmpty &= Mesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Mesh->IsVisible();
	}
	TestTrue(TEXT("empty hides both buffers and disables collision"), bEmpty);
	TestTrue(TEXT("reset restores collision"),
			 Soil->ResetSoil() && TestWorld.TraceVisibility(FVector(-100, 200, 200), FVector(500, 200, 200), Hit));
	// 초기화 경로 통합 후에도 모드·실패 보존·원상 복원 계약 유지
	for (const int32 Cells : {0, 8, 16})
	{
		TestTrue(TEXT("select reset mode"), Soil->SetComparisonChunkCells(Cells));
		const uint32 InitialHash = Soil->GetDensityHash();
		TestTrue(TEXT("dig before reset"), Soil->TryExcavate(FVector(25, 200, 200), 100) == EDigetherSoilEditResult::Applied);
		const uint32 DugHash = Soil->GetDensityHash();
		Soil->CellSpacing = 0;
		TestFalse(TEXT("invalid reset rejected"), Soil->ResetSoil());
		TestEqual(TEXT("failed reset preserves density"), Soil->GetDensityHash(), DugHash);
		TestEqual(TEXT("failed reset preserves revision"), Soil->GetRevision(), 1u);
		Soil->CellSpacing = 50;
		TestTrue(TEXT("valid reset succeeds"), Soil->ResetSoil());
		TestEqual(TEXT("reset preserves mode"), Soil->GetComparisonChunkCells(), Cells);
		TestEqual(TEXT("reset restores initial density"), Soil->GetDensityHash(), InitialHash);
		TestEqual(TEXT("reset clears revision"), Soil->GetRevision(), 0u);
		TestTrue(TEXT("reset restores blocking surface"),
			TestWorld.TraceVisibility(FVector(-100, 200, 200), FVector(500, 200, 200), Hit) && Hit.GetActor() == Soil);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSoilActorWalkTest, "DIGETHER.Excavation.SoilActor.Walk",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherSoilActorWalkTest::RunTest(const FString& Parameters)
{
	return VerifyWalk(*this, 0);
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSoilActorVerticalTest, "DIGETHER.Excavation.SoilActor.Vertical",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherSoilActorVerticalTest::RunTest(const FString& Parameters)
{
	return VerifyVertical(*this, 0);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSoilPerformanceTest, "DIGETHER.Excavation.SoilActor.Performance",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherSoilPerformanceTest::RunTest(const FString& Parameters)
{
	FDigetherSoilTestWorld TestWorld;
	ADigetherDiggableSoil* Soil = TestWorld.World->SpawnActor<ADigetherDiggableSoil>();
	Soil->GridCells = FIntVector(9, 35, 19);
	Soil->CellSpacing = 25;
	const int32 PreviousMode = DigetherSoilProfiling::Enabled.GetValueOnGameThread();
	DigetherSoilProfiling::Enabled->Set(1, ECVF_SetByCode);
	FString Csv = TEXT(
		"round,hit,result,copy_ms,carve_ms,surface_ms,normals_ms,remove_ms,mesh_collision_ms,commit_ms,total_ms\n");
	// 개인 맵과 동일한 격자, 워밍업 5회 후 20회 반복, 실제 충돌 Trace로 다음 타격점 선택
	for (int32 Round = -5; Round < 20; ++Round)
	{
		TestTrue(TEXT("reset benchmark volume"), Soil->ResetSoil());
		for (int32 HitIndex = 0; HitIndex < 6; ++HitIndex)
		{
			FHitResult Hit;
			if (!TestWorld.TraceVisibility(FVector(-100, 437.5, 237.5), FVector(400, 437.5, 237.5), Hit) ||
				Hit.GetActor() != Soil)
			{
				break;
			}
			Soil->TryExcavate(Hit.ImpactPoint, 75);
			const DigetherSoilProfiling::FDigetherSoilTimings& Value = DigetherSoilProfiling::Last;
			if (Round >= 0)
			{
				Csv += FString::Printf(TEXT("%d,%d,%d,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n"), Round, HitIndex,
									   Value.Result, Value.Copy, Value.Carve, Value.Surface, Value.Normals,
									   Value.Remove, Value.MeshCollision, Value.Commit, Value.Total);
			}
		}
	}
	DigetherSoilProfiling::Enabled->Set(PreviousMode, ECVF_SetByCode);
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("ExcavationMeasurements");
	IFileManager::Get().MakeDirectory(*Directory, true);
	TestTrue(TEXT("write timing evidence"), FFileHelper::SaveStringToFile(Csv, *(Directory / TEXT("timings.csv"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSoilActorChunk8WalkTest, "DIGETHER.Excavation.SoilActor.Chunk8Walk",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherSoilActorChunk8WalkTest::RunTest(const FString& Parameters)
{
	return VerifyWalk(*this, 8);
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSoilActorChunk8VerticalTest, "DIGETHER.Excavation.SoilActor.Chunk8Vertical",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherSoilActorChunk8VerticalTest::RunTest(const FString& Parameters)
{
	return VerifyVertical(*this, 8);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSoilActorChunk16WalkTest, "DIGETHER.Excavation.SoilActor.Chunk16Walk",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherSoilActorChunk16WalkTest::RunTest(const FString& Parameters)
{
	return VerifyWalk(*this, 16);
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSoilActorChunk16VerticalTest, "DIGETHER.Excavation.SoilActor.Chunk16Vertical",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherSoilActorChunk16VerticalTest::RunTest(const FString& Parameters)
{
	return VerifyVertical(*this, 16);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSoilChunkRollbackTest, "DIGETHER.Excavation.SoilActor.ChunkRollback",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherSoilChunkRollbackTest::RunTest(const FString& Parameters)
{
	for (int32 Size : {8, 16})
	{
		FDigetherSoilTestWorld TestWorld;
		ADigetherDiggableSoil* Soil = TestWorld.World->SpawnActor<ADigetherDiggableSoil>();
		Soil->GridCells = FIntVector(9, 35, 19);
		Soil->CellSpacing = 25;
		TestTrue(TEXT("initialize"), Soil->SetComparisonChunkCells(Size));
		const uint32 Before = Soil->GetDensityHash();
		FHitResult BeforeHit, AfterHit;
		TestWorld.TraceVisibility(FVector(-100, 400, 200), FVector(500, 400, 200), BeforeHit);
		FDigetherSoilActorTestAccess::SetFailure(*Soil, 1);
		TestTrue(TEXT("failure after multiple prepared candidates"),
				 Soil->TryExcavate(FVector(12.5, 400, 200), 75) == EDigetherSoilEditResult::ApplyFailed);
		TestEqual(TEXT("density preserved"), Soil->GetDensityHash(), Before);
		TestEqual(TEXT("revision preserved"), Soil->GetRevision(), 0u);
		TestTrue(TEXT("old collision remains"),
				 TestWorld.TraceVisibility(FVector(-100, 400, 200), FVector(500, 400, 200), AfterHit));
		TestTrue(TEXT("same old impact point"), BeforeHit.ImpactPoint.Equals(AfterHit.ImpactPoint, .001));
		FDigetherSoilActorTestAccess::SetFailure(*Soil, -1);
		TestTrue(TEXT("retry succeeds"),
				 Soil->TryExcavate(FVector(12.5, 400, 200), 75) == EDigetherSoilEditResult::Applied);
		// 교체된 활성 버퍼에서도 실패 보존 후 반대 버퍼 재사용 확인
		const uint32 AfterRetry = Soil->GetDensityHash();
		TestEqual(TEXT("retry increments revision once"), Soil->GetRevision(), 1u);
		TestWorld.TraceVisibility(FVector(-100, 400, 200), FVector(500, 400, 200), BeforeHit);
		FDigetherSoilActorTestAccess::SetFailure(*Soil, 0);
		TestTrue(TEXT("failure after buffer swap"),
			Soil->TryExcavate(FVector(100, 400, 200), 75) == EDigetherSoilEditResult::ApplyFailed);
		TestEqual(TEXT("swapped density preserved"), Soil->GetDensityHash(), AfterRetry);
		TestEqual(TEXT("swapped revision preserved"), Soil->GetRevision(), 1u);
		TestTrue(TEXT("swapped collision remains"),
			TestWorld.TraceVisibility(FVector(-100, 400, 200), FVector(500, 400, 200), AfterHit));
		TestTrue(TEXT("swapped impact preserved"), BeforeHit.ImpactPoint.Equals(AfterHit.ImpactPoint, .001));
		FDigetherSoilActorTestAccess::SetFailure(*Soil, -1);
		TestTrue(TEXT("swapped retry succeeds"),
			Soil->TryExcavate(FVector(100, 400, 200), 75) == EDigetherSoilEditResult::Applied);
		TestEqual(TEXT("second retry increments revision once"), Soil->GetRevision(), 2u);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSoilChunkBenchmarkTest, "DIGETHER.Excavation.SoilActor.ChunkBenchmark",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherSoilChunkBenchmarkTest::RunTest(const FString& Parameters)
{
	int32 Size = 0;
	FParse::Value(FCommandLine::Get(), TEXT("SoilCompare="), Size);
	if (Size != 0 && Size != 8 && Size != 16)
	{
		AddError(TEXT("invalid comparison mode"));
		return false;
	}
	FDigetherSoilTestWorld TestWorld;
	ADigetherDiggableSoil* Soil = TestWorld.World->SpawnActor<ADigetherDiggableSoil>();
	Soil->GridCells = FIntVector(9, 35, 19);
	Soil->CellSpacing = 25;
	const int32 PreviousMode = DigetherSoilProfiling::Enabled.GetValueOnGameThread();
	DigetherSoilProfiling::Enabled->Set(1, ECVF_SetByCode);
	FString Csv = TEXT("mode,scenario,round,hit,initial_ms,result,total_ms,surface_ms,normals_ms,remove_ms,mesh_"
					   "collision_ms,commit_ms,updated_chunks,generated_triangles,pmcs,mesh_buffer_bytes,process_"
					   "physical_bytes,density_hash,hit_x,hit_y,hit_z\n");
	const FVector AimPoints[] = {FVector(0, 437.5, 237.5), FVector(0, 200, 200), FVector(0, 400, 400)};
	for (int32 Scenario = 0; Scenario < 3; ++Scenario)
	{
		for (int32 Round = -5; Round < 20; ++Round)
		{
			const double Start = FPlatformTime::Seconds();
			TestTrue(TEXT("benchmark initialize"), Soil->SetComparisonChunkCells(Size));
			const double InitialMs = (FPlatformTime::Seconds() - Start) * 1000;
			CollectGarbage(RF_NoFlags);
			for (int32 HitIndex = 0; HitIndex < 6; ++HitIndex)
			{
				FHitResult Hit;
				const FVector StartPoint = AimPoints[Scenario] + FVector(-100, 0, 0),
							  EndPoint = AimPoints[Scenario] + FVector(400, 0, 0);
				if (!TestWorld.TraceVisibility(StartPoint, EndPoint, Hit) || Hit.GetActor() != Soil)
				{
					break;
				}
				TestTrue(TEXT("benchmark excavation succeeds"),
						 Soil->TryExcavate(Hit.ImpactPoint, 75) == EDigetherSoilEditResult::Applied);
				const DigetherSoilProfiling::FDigetherSoilTimings Value = DigetherSoilProfiling::Last;
				TArray<UProceduralMeshComponent*> Meshes;
				Soil->GetComponents(Meshes);
				uint64 BufferBytes = 0;
				for (UProceduralMeshComponent* Mesh : Meshes)
				{
					for (int32 SectionIndex = 0; SectionIndex < Mesh->GetNumSections(); ++SectionIndex)
					{
						const FProcMeshSection* Section = Mesh->GetProcMeshSection(SectionIndex);
						BufferBytes +=
							Section->ProcVertexBuffer.GetAllocatedSize() + Section->ProcIndexBuffer.GetAllocatedSize();
					}
				}
				if (Round >= 0)
				{
					Csv += FString::Printf(
						TEXT(
							"%d,%d,%d,%d,%.9f,%d,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%d,%d,%d,%llu,%llu,%u,%.9f,%.9f,%.9f\n"),
						Size, Scenario, Round, HitIndex, InitialMs, Value.Result, Value.Total, Value.Surface,
						Value.Normals, Value.Remove, Value.MeshCollision, Value.Commit, Soil->GetLastUpdatedChunks(),
						Soil->GetLastGeneratedTriangles(), Meshes.Num(), BufferBytes,
						FPlatformMemory::GetStats().UsedPhysical, Soil->GetDensityHash(), Hit.ImpactPoint.X,
						Hit.ImpactPoint.Y, Hit.ImpactPoint.Z);
				}
			}
		}
	}
	DigetherSoilProfiling::Enabled->Set(PreviousMode, ECVF_SetByCode);
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("ExcavationMeasurements");
	IFileManager::Get().MakeDirectory(*Directory, true);
	TestTrue(TEXT("save comparison"), FFileHelper::SaveStringToFile(Csv, *(Directory / TEXT("chunk-benchmark.csv"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSoilChunkSeamCollisionTest, "DIGETHER.Excavation.SoilActor.ChunkSeamCollision",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherSoilChunkSeamCollisionTest::RunTest(const FString& Parameters)
{
	FDigetherSoilTestWorld TestWorld;
	ADigetherDiggableSoil* Soil = TestWorld.World->SpawnActor<ADigetherDiggableSoil>();
	Soil->GridCells = FIntVector(9, 35, 19);
	Soil->CellSpacing = 25;
	DigetherSoil::FDigetherSoilGrid ReferenceGrid;
	ReferenceGrid.Cells = Soil->GridCells;
	ReferenceGrid.SpacingCm = Soil->CellSpacing;
	TArray<uint8> ReferenceDensity;
	DigetherSoil::Initialize(ReferenceGrid, ReferenceDensity);
	FBox ReferenceChanged;
	DigetherSoil::Carve(ReferenceGrid, ReferenceDensity, FVector(12.5, 200, 200), 75, {}, ReferenceChanged);
	DigetherSoil::FDigetherSoilSurface ReferenceSurface;
	DigetherSoil::BuildSurface(ReferenceGrid, ReferenceDensity, ReferenceSurface);
	TArray<FHitResult> Expected;
	uint32 ExpectedHash = 0;
	for (int32 Mode : {0, 8, 16})
	{
		TestTrue(TEXT("initialize seam case"), Soil->SetComparisonChunkCells(Mode));
		Soil->TryExcavate(FVector(12.5, 200, 200), 75);
		if (Mode == 0)
		{
			ExpectedHash = Soil->GetDensityHash();
		}
		else
		{
			TestEqual(TEXT("density same as full"), Soil->GetDensityHash(), ExpectedHash);
		}
		int32 Index = 0;
		for (float Boundary : {200.f, 400.f})
		{
			for (float Offset : {-.01f, 0.f, .01f})
			{
				FHitResult Hit;
				TestTrue(TEXT("seam trace remains blocked"),
						 TestWorld.TraceVisibility(FVector(-100, Boundary + Offset, 200),
												   FVector(400, Boundary + Offset, 200), Hit));
				if (Mode == 0)
				{
					Expected.Add(Hit);
				}
				else
				{
					TestTrue(TEXT("seam collision position matches"),
							 Hit.ImpactPoint.Equals(Expected[Index].ImpactPoint, .01));
					// 모서리에서는 BVH가 다른 인접 면을 선택할 수 있어 실제 접촉 삼각형으로 판정
					bool bIncidentNormal = false;
					for (int32 Triangle = 0; Triangle < ReferenceSurface.Vertices.Num(); Triangle += 3)
					{
						const FVector& A = ReferenceSurface.Vertices[Triangle];
						const FVector& B = ReferenceSurface.Vertices[Triangle + 1];
						const FVector& C = ReferenceSurface.Vertices[Triangle + 2];
						const FVector Closest = FMath::ClosestPointOnTriangleToPoint(Hit.ImpactPoint, A, B, C);
						const FVector FaceNormal = -FVector::CrossProduct(B - A, C - A).GetSafeNormal();
						bIncidentNormal |=
							Closest.Equals(Hit.ImpactPoint, .02) && FaceNormal.Equals(Hit.ImpactNormal, .01);
					}
					TestTrue(TEXT("collision normal belongs to actual incident face"), bIncidentNormal);
					if (!Hit.ImpactNormal.Equals(Expected[Index].ImpactNormal, .01))
					{
						AddInfo(FString::Printf(
							TEXT(
								"edge tie mode=%d y=%.4f offset=%.4f hit=%s wholeNormal=%s chunkNormal=%s incident=%d"),
							Mode, Boundary, Offset, *Hit.ImpactPoint.ToString(),
							*Expected[Index].ImpactNormal.ToString(), *Hit.ImpactNormal.ToString(), bIncidentNormal));
					}
				}
				++Index;
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSoilChunkBoundaryMovementTest,
								 "DIGETHER.Excavation.SoilActor.ChunkBoundaryMovement",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherSoilChunkBoundaryMovementTest::RunTest(const FString& Parameters)
{
	// 25cm * 8셀 접합면과 양옆을 실제 캡슐 이동으로 검사, 시험 중 순간이동 없음
	for (double Offset : {-0.01, 0.0, 0.01})
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			FDigetherSoilTestWorld TestWorld;
			const bool bWall = Axis == 0, bCeiling = Axis == 2;
			if (bWall || bCeiling)
			{
				TestWorld.CreateFloor();
			}
			const FVector Origin = bWall ? FVector(200, -200, -112.5) : FVector(0, 0, bCeiling ? 300 : 0);
			ADigetherDiggableSoil* Soil =
				TestWorld.World->SpawnActor<ADigetherDiggableSoil>(Origin, FRotator::ZeroRotator);
			Soil->GridCells = bWall ? FIntVector(9, 17, 17) : FIntVector(17, 17, 9);
			Soil->CellSpacing = 25;
			if (!TestTrue(TEXT("multi-chunk initialization"), Soil->SetComparisonChunkCells(8)))
			{
				return false;
			}
			ACharacter* Character = TestWorld.CreateCharacter(
				bWall ? FVector(0, Offset, 95) : FVector(200 + Offset, 200 + Offset, bCeiling ? 95 : 310));
			TestWorld.Step(Character, 120, bWall);
			const FVector Before = Character->GetActorLocation();
			if (bWall)
			{
				TestTrue(TEXT("intact seam wall blocks"), Before.X > 100 && Before.X < 215);
			}
			else if (!bCeiling)
			{
				TestTrue(TEXT("stands on multi-chunk floor seam"), FMath::Abs(Before.Z - 302.5) < 8);
			}
			double BlockedHighest = 0;
			if (bCeiling)
			{
				Character->GetCharacterMovement()->JumpZVelocity = 1400;
				Character->Jump();
				for (int32 Frame = 0; Frame < 120; ++Frame)
				{
					TestWorld.Step(Character, 1);
					BlockedHighest = FMath::Max(BlockedHighest, Character->GetActorLocation().Z);
					Character->StopJumping();
				}
				TestTrue(TEXT("intact ceiling stops actual jump"), BlockedHighest > 100 && BlockedHighest < 240);
			}
			int32 MaxUpdated = 0;
			for (int32 Pass = 0; Pass < 10; ++Pass)
			{
				for (int32 Probe = 0; Probe < (bWall ? 2 : 1); ++Probe)
				{
					const FVector Start = bWall ? FVector(0, Offset, Probe ? 130 : 50)
												: FVector(200 + Offset, 200 + Offset, bCeiling ? 200 : 500);
					const FVector End = bWall ? FVector(800, Offset, Probe ? 130 : 50)
											  : FVector(200 + Offset, 200 + Offset, bCeiling ? 800 : -100);
					FHitResult Hit;
					if (TestWorld.TraceVisibility(Start, End, Hit) && Hit.GetActor() == Soil)
					{
						TestTrue(TEXT("boundary carve applied"),
								 Soil->TryExcavate(Hit.ImpactPoint, 75) == EDigetherSoilEditResult::Applied);
						MaxUpdated = FMath::Max(MaxUpdated, Soil->GetLastUpdatedChunks());
					}
				}
			}
			TestTrue(TEXT("multiple chunks were updated"), MaxUpdated > 1);
			TestTrue(TEXT("repeated excavation required"), Soil->GetRevision() > 1);
			double Highest = 0;
			if (bWall)
			{
				TestWorld.Step(Character, 180, true);
				TestTrue(TEXT("walk across depth and lateral seams"),
						 Character->GetActorLocation().X > 500 &&
							 FMath::Abs(Character->GetActorLocation().Y - Offset) < 10);
				TestWorld.Step(Character, 240, true, -1);
				TestTrue(TEXT("return across seam tunnel"), Character->GetActorLocation().X < 150);
			}
			else if (!bCeiling)
			{
				TestWorld.Step(Character, 90);
				TestTrue(TEXT("fall through seam floor"), Character->GetActorLocation().Z < 0);
			}
			else
			{
				Character->Jump();
				for (int32 Frame = 0; Frame < 100; ++Frame)
				{
					TestWorld.Step(Character, 1);
					Highest = FMath::Max(Highest, Character->GetActorLocation().Z);
					Character->StopJumping();
				}
				TestTrue(TEXT("capsule clears excavated ceiling"), Highest > 620);
			}
			if (!bWall)
			{
				TestTrue(TEXT("vertical crossing stayed at seam"),
						 FMath::Abs(Character->GetActorLocation().X - 200 - Offset) < 10 &&
							 FMath::Abs(Character->GetActorLocation().Y - 200 - Offset) < 10);
			}
			AddInfo(FString::Printf(TEXT("BOUNDARY axis=%d offset=%.3f before=%s final=%s blockedHighest=%.3f "
										 "highest=%.3f digs=%u maxChunks=%d teleports=0"),
									Axis, Offset, *Before.ToString(), *Character->GetActorLocation().ToString(),
									BlockedHighest, Highest, Soil->GetRevision(), MaxUpdated));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSoilMultiActorTest, "DIGETHER.Excavation.SoilActor.MultiActor",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherSoilMultiActorTest::RunTest(const FString& Parameters)
{
	int32 Count = 1;
	FParse::Value(FCommandLine::Get(), TEXT("SoilActorCount="), Count);
	if (Count != 1 && Count != 4 && Count != 8)
	{
		AddError(TEXT("actor count must be 1,4,8"));
		return false;
	}
	const int32 PreviousMode = DigetherSoilProfiling::Enabled.GetValueOnGameThread();
	DigetherSoilProfiling::Enabled->Set(1, ECVF_SetByCode);
	FString Csv = TEXT("actors,round,shot,actor,total_ms,mesh_collision_ms,chunks,triangles,density_hash\n");
	FString Batches = TEXT("actors,round,shot,batch_ms\n");
	FString Memory = TEXT("actors,round,initial_ms,pmcs,buffer_bytes,process_before,process_after\n");
	for (int32 Round = -2; Round < 10; ++Round)
	{
		FDigetherSoilTestWorld TestWorld;
		CollectGarbage(RF_NoFlags);
		const uint64 Before = FPlatformMemory::GetStats().UsedPhysical;
		TArray<ADigetherDiggableSoil*> Soils;
		const double Begin = FPlatformTime::Seconds();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			ADigetherDiggableSoil* Soil =
				TestWorld.World->SpawnActor<ADigetherDiggableSoil>(FVector(0, Index * 2000, 0), FRotator::ZeroRotator);
			Soil->GridCells = FIntVector(9, 35, 19);
			Soil->CellSpacing = 25;
			TestTrue(TEXT("initialize each wall"), Soil->SetComparisonChunkCells(8));
			Soils.Add(Soil);
		}
		const double InitialMs = (FPlatformTime::Seconds() - Begin) * 1000;
		CollectGarbage(RF_NoFlags);
		for (int32 Shot = 0; Shot < 3; ++Shot)
		{
			// 동일 프레임에서 여러 요청을 순차 처리하는 비용, 네트워크 동시성 시험 아님
			const double BatchStart = FPlatformTime::Seconds();
			FString Rows;
			uint32 ExpectedHash = 0;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				ADigetherDiggableSoil* Soil = Soils[Index];
				const FVector Aim = Soil->GetActorLocation() + FVector(0, 437.5, 237.5);
				FHitResult Hit;
				if (!TestTrue(TEXT("same local hit on each wall"),
							  TestWorld.TraceVisibility(Aim - FVector(100, 0, 0), Aim + FVector(400, 0, 0), Hit) &&
								  Hit.GetActor() == Soil))
				{
					continue;
				}
				TestTrue(TEXT("multi actor excavation"),
						 Soil->TryExcavate(Hit.ImpactPoint, 75) == EDigetherSoilEditResult::Applied);
				const auto Value = DigetherSoilProfiling::Last;
				if (Index == 0)
				{
					ExpectedHash = Soil->GetDensityHash();
				}
				else
				{
					TestEqual(TEXT("independent walls have equivalent density"), Soil->GetDensityHash(), ExpectedHash);
				}
				if (Round >= 0)
				{
					Rows += FString::Printf(TEXT("%d,%d,%d,%d,%.9f,%.9f,%d,%d,%u\n"), Count, Round, Shot, Index,
											Value.Total, Value.MeshCollision, Soil->GetLastUpdatedChunks(),
											Soil->GetLastGeneratedTriangles(), Soil->GetDensityHash());
				}
			}
			const double BatchMs = (FPlatformTime::Seconds() - BatchStart) * 1000;
			if (Round >= 0)
			{
				Csv += Rows;
				Batches += FString::Printf(TEXT("%d,%d,%d,%.9f\n"), Count, Round, Shot, BatchMs);
			}
		}
		int32 PmcCount = 0;
		uint64 BufferBytes = 0;
		for (ADigetherDiggableSoil* Soil : Soils)
		{
			TArray<UProceduralMeshComponent*> Meshes;
			Soil->GetComponents(Meshes);
			PmcCount += Meshes.Num();
			for (UProceduralMeshComponent* Mesh : Meshes)
			{
				for (int32 SectionIndex = 0; SectionIndex < Mesh->GetNumSections(); ++SectionIndex)
				{
					const FProcMeshSection* Section = Mesh->GetProcMeshSection(SectionIndex);
					BufferBytes +=
						Section->ProcVertexBuffer.GetAllocatedSize() + Section->ProcIndexBuffer.GetAllocatedSize();
				}
			}
		}
		TestEqual(TEXT("62 PMC per wall"), PmcCount, Count * 62);
		if (Round >= 0)
		{
			Memory += FString::Printf(TEXT("%d,%d,%.9f,%d,%llu,%llu,%llu\n"), Count, Round, InitialMs, PmcCount,
									  BufferBytes, Before, FPlatformMemory::GetStats().UsedPhysical);
		}
	}
	DigetherSoilProfiling::Enabled->Set(PreviousMode, ECVF_SetByCode);
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("ExcavationMeasurements");
	IFileManager::Get().MakeDirectory(*Directory, true);
	TestTrue(TEXT("save multi timings"), FFileHelper::SaveStringToFile(Csv, *(Directory / TEXT("multi-timings.csv"))));
	TestTrue(TEXT("save multi batch"), FFileHelper::SaveStringToFile(Batches, *(Directory / TEXT("multi-batch.csv"))));
	TestTrue(TEXT("save multi memory"), FFileHelper::SaveStringToFile(Memory, *(Directory / TEXT("multi-memory.csv"))));
	return true;
}

// 실제 시험 캐릭터의 Trace·간격·반경 전달과 두 도구의 통과 결과 검증
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherLabToolsTest, "DIGETHER.Excavation.Soil.LabTools",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherLabToolsTest::RunTest(const FString& Parameters)
{
	int32 RemovedSamples[2] = {};
	for (int32 ToolIndex = 0; ToolIndex < 2; ++ToolIndex)
	{
		FDigetherSoilTestWorld TestWorld;
		TestWorld.CreateFloor();
		ADigetherDiggableSoil* Soil = TestWorld.World->SpawnActor<ADigetherDiggableSoil>(
			FVector(200, -437.5, -25), FRotator::ZeroRotator);
		Soil->GridCells = FIntVector(9, 35, 19);
		Soil->CellSpacing = 25;
		TestTrue(TEXT("eight-cell lab wall"), Soil->SetComparisonChunkCells(8));
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADigetherDainCharacter* Character = TestWorld.World->SpawnActor<ADigetherDainCharacter>(
			FVector(0, 0, 95), FRotator::ZeroRotator, Spawn);
		Character->GetCharacterMovement()->bRunPhysicsWithNoController = true;
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		UCameraComponent* Camera = Character->FindComponentByClass<UCameraComponent>();
		Camera->bUsePawnControlRotation = false;
		const auto Aim = [&](FVector Target)
		{
			Camera->SetWorldRotation((Target - Camera->GetComponentLocation()).Rotation());
		};
		TestTrue(TEXT("default shovel preserves previous lab"), Character->GetSelectedTool() == EDigetherLabTool::Shovel);
		if (ToolIndex == 0)
		{
			Character->SelectPickaxe();
		}
		const float Radius = ToolIndex == 0 ? 50.f : 75.f;
		TestEqual(TEXT("selected radius"), Character->GetSelectedRadiusCm(), Radius);
		TestWorld.Step(Character, 180, true);
		TestTrue(TEXT("intact wall blocks character"), Character->GetActorLocation().X > 100 && Character->GetActorLocation().X < 225);
		Aim(FVector(500, 0, Camera->GetComponentLocation().Z));
		FHitResult Hit;
		if (!TestTrue(TEXT("tool traces soil"), Character->TraceTool(Hit) && Hit.GetActor() == Soil))
		{
			return false;
		}
		const int32 AirBefore = FDigetherSoilActorTestAccess::CountAir(*Soil);
		TestTrue(TEXT("selected tool excavates"), Character->DigOnce());
		RemovedSamples[ToolIndex] = FDigetherSoilActorTestAccess::CountAir(*Soil) - AirBefore;
		const uint32 AfterHit = Soil->GetDensityHash();
		TestFalse(TEXT("immediate repeated use rejected"), Character->DigOnce());
		// 전환·되돌리기 모두 직전 도구의 대기 시각을 보존
		Character->SelectPickaxe();
		Character->SelectShovel();
		TestFalse(TEXT("switch does not reset cooldown"), Character->DigOnce());
		TestEqual(TEXT("cooldown preserves density"), Soil->GetDensityHash(), AfterHit);
		if (ToolIndex == 0)
		{
			TestWorld.Step(Character, 24);
			TestFalse(TEXT("shovel cannot shorten previous pickaxe interval"), Character->DigOnce());
			Character->SelectPickaxe();
		}
		TestWorld.Step(Character, 36);
		// 바닥은 굴착 불가, 실패한 타격도 사용 간격 소비
		Aim(Character->GetActorLocation() - FVector(0, 0, 300));
		TestFalse(TEXT("floor is not soil"), Character->DigOnce());
		TestEqual(TEXT("floor hit preserves soil"), Soil->GetDensityHash(), AfterHit);
		Aim(FVector(500, 0, Camera->GetComponentLocation().Z));
		TestFalse(TEXT("miss consumes interval"), Character->DigOnce());
		TestWorld.Step(Character, 36);
		TestTrue(TEXT("use resumes after interval"), Character->DigOnce());
		// 전면을 넓혀 반복 굴착, 캐릭터 위치는 이동 컴포넌트만 변경
		for (int32 Pass = 0; Pass < 10; ++Pass)
		{
			for (double Y : {-70.0, 0.0, 70.0})
			{
				for (double Z : {0.0, 60.0, 120.0, 180.0, 240.0})
				{
					TestWorld.Step(Character, 36);
					Aim(FVector(FMath::Max(200.0 + Pass * 25, Camera->GetComponentLocation().X + 80.0), Y, Z));
					Character->DigOnce();
				}
			}
			if (Character->GetActorLocation().X < 450)
			{
				TestWorld.Step(Character, 30, true);
			}
		}
		TestWorld.Step(Character, 180, true);
		AddInfo(FString::Printf(TEXT("crossedX=%.2f"), Character->GetActorLocation().X));
		TestTrue(TEXT("selected tool tunnel permits CharacterMovement passage"), Character->GetActorLocation().X > 650);
		TestWorld.Step(Character, 360, true, -1.f);
		TestTrue(TEXT("character returns without teleport"), Character->GetActorLocation().X < 150);
		AddInfo(FString::Printf(TEXT("tool=%d radius=%.0f removedSamples=%d digs=%u returnX=%.2f"),
			ToolIndex, Radius, RemovedSamples[ToolIndex], Soil->GetRevision(), Character->GetActorLocation().X));
	}
	TestTrue(TEXT("pickaxe removes soil"), RemovedSamples[0] > 0);
	TestTrue(TEXT("shovel removes more soil than pickaxe at same hit"), RemovedSamples[1] > RemovedSamples[0]);
	return true;
}
#endif
