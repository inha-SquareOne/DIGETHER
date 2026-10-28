#include "DigetherPrototypePortal.h"
#include "Prototype/DigetherPrototypeCorpse.h"
#include "Prototype/DigetherPrototypeGameMode.h"
#include "Prototype/DigetherPrototypeGameState.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Components/PointLightComponent.h"
#include "Engine/Engine.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

ADigetherPrototypePortal::ADigetherPrototypePortal()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) Mesh->SetStaticMesh(Cube.Object);
	Mesh->SetRelativeScale3D(FVector(.20f, 1.5f, 2.4f));
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(RootComponent);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 160.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(30.f);
	Label->SetText(FText::FromString(TEXT("PORTAL")));
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("PortalLight"));
	Light->SetupAttachment(RootComponent);
	Light->SetIntensityUnits(ELightUnits::Lumens);
	Light->SetIntensity(3500.f);
	Light->SetAttenuationRadius(600.f);
	Light->SetLightColor(FLinearColor(.1f, .6f, 1.f));
	Light->SetCastShadows(false);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Metal(TEXT("/Game/Prototype/Materials/MI_PrototypeMetal.MI_PrototypeMetal"));
	if (Metal.Succeeded()) PortalSurfaceMaterial = Metal.Object;
	const FVector FramePositions[] = { FVector(0.f, -85.f, 0.f), FVector(0.f, 85.f, 0.f), FVector(0.f, 0.f, 130.f), FVector(0.f, 0.f, -130.f) };
	const FVector FrameSizes[] = { FVector(.35f, .20f, 2.8f), FVector(.35f, .20f, 2.8f), FVector(.35f, 1.9f, .20f), FVector(.35f, 1.9f, .20f) };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(FramePositions); ++Index)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("PortalFrame%d"), Index));
		Part->SetupAttachment(RootComponent);
		if (Cube.Succeeded()) Part->SetStaticMesh(Cube.Object);
		if (Metal.Succeeded()) Part->SetMaterial(0, Metal.Object);
		Part->SetRelativeLocation(FramePositions[Index]);
		Part->SetRelativeScale3D(FrameSizes[Index]);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Frame.Add(Part);
	}
}

void ADigetherPrototypePortal::BeginPlay()
{
	Super::BeginPlay();
	if (PortalSurfaceMaterial)
	{
		UMaterialInstanceDynamic* Surface = UMaterialInstanceDynamic::Create(PortalSurfaceMaterial, this);
		Surface->SetVectorParameterValue(TEXT("Tint"), FLinearColor(.04f, .55f, 1.f));
		Surface->SetVectorParameterValue(TEXT("Color"), FLinearColor(.04f, .55f, 1.f));
		Surface->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(.04f, 1.f, 3.f));
		Mesh->SetMaterial(0, Surface);
	}
}

void ADigetherPrototypePortal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer || !GEngine) return;
	APlayerController* LocalController = GEngine->GetFirstLocalPlayerController(World);
	if (!LocalController) return;
	FVector CameraLocation; FRotator CameraRotation;
	LocalController->GetPlayerViewPoint(CameraLocation, CameraRotation);
	FVector Direction = CameraLocation - Label->GetComponentLocation();
	Direction.Z = 0.f;
	if (!Direction.IsNearlyZero()) Label->SetWorldRotation(FRotator(0.f, Direction.Rotation().Yaw, 0.f));
}

void ADigetherPrototypePortal::SetArrivalCandidates(const TArray<FVector>& Candidates, const FRotator& Facing)
{
	if (HasAuthority()) { ArrivalCandidates = Candidates; ArrivalFacing = Facing; }
}

bool ADigetherPrototypePortal::InteractPrototype(APlayerController* Caller, bool bAlternate, FString& OutMessage)
{
	UWorld* World = GetWorld();
	ADigetherPrototypeGameMode* Mode = World ? World->GetAuthGameMode<ADigetherPrototypeGameMode>() : nullptr;
	const ADigetherPrototypeGameState* State = World ? World->GetGameState<ADigetherPrototypeGameState>() : nullptr;
	if (!HasAuthority() || !Mode || !Mode->CanInteract(Caller, this, OutMessage)) return false;
	if (!State || State->PublicState.Phase != EPrototypePhase::Expedition)
	{
		OutMessage = TEXT("탐사 중에만 포털을 사용할 수 있습니다.");
		return false;
	}
	APawn* Pawn = Caller->GetPawn();
	FVector Eye; FRotator EyeRotation;
	Pawn->GetActorEyesViewPoint(Eye, EyeRotation);
	FHitResult SightHit;
	FCollisionQueryParams SightParams(SCENE_QUERY_STAT(PrototypePortalSight), false, Pawn);
	if (World->LineTraceSingleByChannel(SightHit, Eye, GetActorLocation(), ECC_Visibility, SightParams) && SightHit.GetActor() != this)
	{
		OutMessage = TEXT("포털이 시야에서 가려져 있습니다.");
		return false;
	}
	ADigetherPrototypeCorpse* CarriedCorpse = nullptr;
	for (TActorIterator<ADigetherPrototypeCorpse> It(World); It; ++It)
	{
		if (It->Carrier != Pawn) continue;
		// Existing carrying attaches exactly one corpse to the pawn. Refuse an
		// inconsistent or elevator-reserved state without changing either actor.
		if (CarriedCorpse || It->bReserved || It->GetAttachParentActor() != Pawn)
		{
			OutMessage = TEXT("운반 중인 시체의 상태를 확인할 수 없습니다.");
			return false;
		}
		CarriedCorpse = *It;
	}
	ACharacter* Character = Cast<ACharacter>(Pawn);
	UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Capsule || !Capsule->IsQueryCollisionEnabled())
	{
		OutMessage = TEXT("캐릭터 충돌 상태를 확인할 수 없습니다.");
		return false;
	}
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), HalfHeight);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PrototypePortalArrival), false, Pawn);
	if (CarriedCorpse) Params.AddIgnoredActor(CarriedCorpse);
	FCollisionObjectQueryParams Pawns;
	Pawns.AddObjectTypesToQuery(ECC_Pawn);
	for (const FVector& Nominal : ArrivalCandidates)
	{
		// Candidate Z represents a standing 96cm capsule; place current capsule
		// four centimetres above the same floor, including while crouched.
		const FVector Candidate = Nominal + FVector(0.f, 0.f, HalfHeight - 96.f + 4.f);
		if (Candidate.ContainsNaN() ||
			World->OverlapBlockingTestByProfile(Candidate, FQuat::Identity, Capsule->GetCollisionProfileName(), Shape, Params) ||
			World->OverlapAnyTestByObjectType(Candidate, FQuat::Identity, Pawns, Shape, Params)) continue;
		// Do not allow the engine to silently adjust the pawn to a different
		// location after the carried-body clearance was checked.
		FVector CheckedLocation = Candidate;
		if (!World->FindTeleportSpot(Pawn, CheckedLocation, ArrivalFacing) || !CheckedLocation.Equals(Candidate, .1f)) continue;
		if (CarriedCorpse)
		{
			// Existing carry offset is (90,0,0); the corpse's box fits a 35cm
			// sphere at any rotation. Check the entire destination and attachment path.
			const FVector BodyLocation = Candidate + ArrivalFacing.RotateVector(FVector(90.f, 0.f, 0.f));
			const FCollisionShape BodyShape = FCollisionShape::MakeSphere(35.f);
			FHitResult BodyHit;
			if (World->OverlapBlockingTestByChannel(BodyLocation, FQuat::Identity, ECC_Visibility, BodyShape, Params) ||
				World->OverlapBlockingTestByChannel(BodyLocation, FQuat::Identity, ECC_Pawn, BodyShape, Params) ||
				World->OverlapAnyTestByObjectType(BodyLocation, FQuat::Identity, Pawns, BodyShape, Params) ||
				World->SweepSingleByChannel(BodyHit, Candidate, BodyLocation, FQuat::Identity, ECC_Visibility, BodyShape, Params) ||
				World->SweepSingleByChannel(BodyHit, Candidate, BodyLocation, FQuat::Identity, ECC_Pawn, BodyShape, Params) ||
				World->LineTraceSingleByChannel(BodyHit, Candidate + FVector(0.f, 0.f, 50.f), BodyLocation, ECC_Visibility, Params)) continue;
		}
		if (Pawn->TeleportTo(Candidate, ArrivalFacing, false, false))
		{
			if (CarriedCorpse) CarriedCorpse->ForceNetUpdate();
			Character->GetCharacterMovement()->StopMovementImmediately();
			Caller->SetControlRotation(ArrivalFacing);
			Caller->ClientSetRotation(ArrivalFacing, true);
			Pawn->ForceNetUpdate();
			OutMessage = TEXT("포털을 통과했습니다.");
			return true;
		}
	}
	OutMessage = TEXT("도착 지점이 막혀 이동할 수 없습니다.");
	return false;
}
