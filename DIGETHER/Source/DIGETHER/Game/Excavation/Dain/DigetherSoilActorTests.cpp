#if WITH_DEV_AUTOMATION_TESTS
#include "DigetherDiggableSoil.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"

class FDigetherSoilActorTestAccess
{
public:
	static bool RejectMissingCookedCollision(ADigetherDiggableSoil& Soil)
	{
		UProceduralMeshComponent* Next = Soil.bFrontActive ? Soil.SoilBack.Get() : Soil.SoilMesh.Get();
		UBodySetup* Body = Next->GetBodySetup();
		Body->bNeverNeedsCookedCollisionData = true;
		TArray<uint8> Candidate = Soil.Density; FBox Changed;
		DigetherSoil::Carve(Soil.AppliedGrid, Candidate, FVector(25,200,200),100,{},Changed);
		const bool bRejected = !Soil.Apply(Soil.AppliedGrid, Candidate);
		Body->bNeverNeedsCookedCollisionData = false;
		return bRejected;
	}
	static bool RejectInvalidCandidate(ADigetherDiggableSoil& Soil)
	{
		TArray<uint8> Bad = Soil.Density; Bad.Pop();
		return !Soil.Apply(Soil.AppliedGrid, Bad);
	}
};
namespace
{
struct FSoilWorld
{
	UGameInstance* Instance = nullptr;
	UWorld* World = nullptr;
	FSoilWorld()
	{
		Instance = NewObject<UGameInstance>(GEngine); Instance->AddToRoot();
		Instance->InitializeStandalone(); World = Instance->GetWorld();
		const FURL URL(nullptr,TEXT("/Engine/Maps/Entry?game=/Script/Engine.GameModeBase"),TRAVEL_Absolute);
		World->SetGameMode(URL); World->InitializeActorsForPlay(URL); World->BeginPlay();
	}
	~FSoilWorld()
	{
		World->EndPlay(EEndPlayReason::Quit); Instance->Shutdown(); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); Instance->RemoveFromRoot();
	}
	void Step(ACharacter* Character, int32 Frames, bool bWalk=false, float Direction=1.f)
	{
		for(int32 I=0;I<Frames;++I)
		{
			if(bWalk) { Character->AddMovementInput(FVector::ForwardVector,Direction); }
			++GFrameCounter; World->Tick(LEVELTICK_All,1.f/60.f);
		}
	}
	void Floor()
	{
		AActor* A=World->SpawnActor<AActor>();
		UBoxComponent* Box=NewObject<UBoxComponent>(A); A->SetRootComponent(Box); A->AddInstanceComponent(Box);
		Box->SetBoxExtent(FVector(2000,2000,25)); Box->SetCollisionProfileName(TEXT("BlockAll"));
		Box->RegisterComponent(); A->SetActorLocation(FVector(0,0,-25));
	}
	ACharacter* Character(FVector P)
	{
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACharacter* C=World->SpawnActor<ACharacter>(P,FRotator::ZeroRotator,Params);
		C->GetCapsuleComponent()->SetCapsuleSize(35,90);
		C->GetCharacterMovement()->MaxWalkSpeed=250;
		C->GetCharacterMovement()->MaxStepHeight=35;
		// Headless world has no LocalPlayer; let the real movement component consume test input.
        C->GetCharacterMovement()->bRunPhysicsWithNoController = true;
        C->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		return C;
	}
	bool Ray(FVector From,FVector To,FHitResult& Hit)
	{
		return World->LineTraceSingleByChannel(Hit,From,To,ECC_Visibility);
	}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoilActorTransactionTest,"DIGETHER.Dain.SoilActor.Transaction",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSoilActorTransactionTest::RunTest(const FString& Parameters)
{
	FSoilWorld F; ADigetherDiggableSoil* Soil=F.World->SpawnActor<ADigetherDiggableSoil>();
	FHitResult Hit;
	TestTrue(TEXT("initial cooked wall blocks"),F.Ray(FVector(-100,200,200),FVector(500,200,200),Hit)&&Hit.GetActor()==Soil);
	const uint32 Hash=Soil->GetDensityHash();
	TestTrue(TEXT("missing cooked collision rejected"),FDigetherSoilActorTestAccess::RejectMissingCookedCollision(*Soil));
	TestTrue(TEXT("bad candidate rejected"),FDigetherSoilActorTestAccess::RejectInvalidCandidate(*Soil));
	TestEqual(TEXT("failed prepare preserves density"),Soil->GetDensityHash(),Hash);
	TestEqual(TEXT("failed prepare preserves revision"),Soil->GetRevision(),0u);
	TestTrue(TEXT("failed prepare preserves actual collision"),F.Ray(FVector(-100,200,200),FVector(500,200,200),Hit));
	Soil->ProtectedRegions={FBox(FVector::ZeroVector,FVector(400))};
	TestTrue(TEXT("protected wall unchanged"),Soil->TryExcavate(FVector(25,200,200),120)==EDigetherSoilEditResult::NoChange);
	Soil->ProtectedRegions.Reset();
	TestTrue(TEXT("whole soil removal"),Soil->TryExcavate(FVector(200),600)==EDigetherSoilEditResult::Applied);
	TestFalse(TEXT("empty has no old collision"),F.Ray(FVector(-100,200,200),FVector(500,200,200),Hit));
	TArray<UProceduralMeshComponent*> Meshes; Soil->GetComponents(Meshes);
	bool bEmpty=true; for(auto* M:Meshes) { bEmpty &= M->GetCollisionEnabled()==ECollisionEnabled::NoCollision && !M->IsVisible(); }
	TestTrue(TEXT("empty hides both buffers and disables collision"),bEmpty);
	TestTrue(TEXT("reset restores collision"),Soil->ResetSoil()&&F.Ray(FVector(-100,200,200),FVector(500,200,200),Hit));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoilActorWalkTest,"DIGETHER.Dain.SoilActor.Walk",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSoilActorWalkTest::RunTest(const FString& Parameters)
{
	FSoilWorld F; F.Floor();
	ADigetherDiggableSoil* Soil=F.World->SpawnActor<ADigetherDiggableSoil>(FVector(200,-200,-25),FRotator::ZeroRotator);
	ACharacter* C=F.Character(FVector(0,0,95));
	F.Step(C,180,true);
	const float Blocked=C->GetActorLocation().X;
	AddInfo(FString::Printf(TEXT("intact character=%s velocity=%s soil=%s"),*C->GetActorLocation().ToString(),*C->GetVelocity().ToString(),*Soil->GetActorLocation().ToString()));
	if(!TestTrue(TEXT("walking blocked at intact wall"),Blocked>100&&Blocked<225)) { return false; }
	FHitResult Hit;
	F.Ray(FVector(0,0,100),FVector(800,0,100),Hit);
	TestTrue(TEXT("first surface dig"),Soil->TryExcavate(Hit.ImpactPoint,100)==EDigetherSoilEditResult::Applied);
	TestTrue(TEXT("one hit leaves soil"),F.Ray(FVector(0,0,100),FVector(800,0,100),Hit)&&Hit.GetActor()==Soil);
	for(int32 Pass=0;Pass<10;++Pass)
		for(double Z:{50.0,130.0}) for(double Y:{-40.0,40.0})
			if(F.Ray(FVector(0,Y,Z),FVector(800,Y,Z),Hit)&&Hit.GetActor()==Soil) { Soil->TryExcavate(Hit.ImpactPoint,100); }
	F.Step(C,180,true);
	TestTrue(TEXT("multiple digs applied"),Soil->GetRevision()>1);
	TestTrue(TEXT("actual CharacterMovement crosses hole"),C->GetActorLocation().X>650&&FMath::Abs(C->GetActorLocation().Y)<10);
	AddInfo(FString::Printf(TEXT("blockedX=%.2f crossedX=%.2f z=%.2f digs=%u no teleport"),Blocked,C->GetActorLocation().X,C->GetActorLocation().Z,Soil->GetRevision()));
    F.Step(C,240,true,-1.f);
    TestTrue(TEXT("character returns through same tunnel"),C->GetActorLocation().X<150 && FMath::Abs(C->GetActorLocation().Y)<10);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoilActorVerticalTest,"DIGETHER.Dain.SoilActor.Vertical",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSoilActorVerticalTest::RunTest(const FString& Parameters)
{
	// Floor: stand on actual cooked soil, remove a shaft, then fall through it.
	{
		FSoilWorld F; auto* Soil=F.World->SpawnActor<ADigetherDiggableSoil>();
		ACharacter* C=F.Character(FVector(200,200,475)); F.Step(C,90);
		TestTrue(TEXT("standing on soil"),FMath::Abs(C->GetActorLocation().Z-467)<8);
		FHitResult Hit;
		for(int32 I=0;I<12;++I)
			if(F.Ray(FVector(200,200,900),FVector(200,200,-100),Hit)&&Hit.GetActor()==Soil) { Soil->TryExcavate(Hit.ImpactPoint,120); }
		F.Step(C,90);
		TestTrue(TEXT("character falls through excavated floor"),C->GetActorLocation().Z<0);
		AddInfo(FString::Printf(TEXT("floor finalZ=%.2f digs=%u"),C->GetActorLocation().Z,Soil->GetRevision()));
	}
	// Ceiling: upward traces hit the underside; a character jumps through the shaft.
	{
		FSoilWorld F; F.Floor(); auto* Soil=F.World->SpawnActor<ADigetherDiggableSoil>(FVector(0,0,300),FRotator::ZeroRotator);
		ACharacter* C=F.Character(FVector(200,200,95)); F.Step(C,60);
		FHitResult Hit; TestTrue(TEXT("ceiling underside blocks"),F.Ray(FVector(200,200,200),FVector(200,200,900),Hit)&&Hit.GetActor()==Soil);
		for(int32 I=0;I<12;++I)
			if(F.Ray(FVector(200,200,200),FVector(200,200,900),Hit)&&Hit.GetActor()==Soil) { Soil->TryExcavate(Hit.ImpactPoint,120); }
		C->GetCharacterMovement()->JumpZVelocity=1400; C->Jump();
		double Highest=C->GetActorLocation().Z;
		for(int32 I=0;I<100;++I) { F.Step(C,1); Highest=FMath::Max(Highest,C->GetActorLocation().Z); }
		TestTrue(TEXT("character jumps above excavated ceiling"),Highest>765);
		AddInfo(FString::Printf(TEXT("ceiling highestZ=%.2f digs=%u (test jump speed)"),Highest,Soil->GetRevision()));
	}
	return true;
}
#endif






