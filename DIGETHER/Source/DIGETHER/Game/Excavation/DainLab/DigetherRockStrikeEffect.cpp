#include "DigetherRockStrikeEffect.h"

#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundWave.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float SparkLifetime = .18f;
}

ADigetherRockStrikeEffect::ADigetherRockStrikeEffect()
{
	PrimaryActorTick.bCanEverTick = true;
	InitialLifeSpan = .6f;
	Sparks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Sparks"));
	SetRootComponent(Sparks);
	Sparks->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sparks->SetCanEverAffectNavigation(false);
	Sparks->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/Developers/Dain/Excavation/Feedback/M_RockStrikeSpark"));
	Sparks->SetStaticMesh(Mesh.Object);
	Sparks->SetMaterial(0, Glow.Object);

	UAudioComponent* Clang = CreateDefaultSubobject<UAudioComponent>(TEXT("Clang"));
	Clang->SetupAttachment(Sparks);
	static ConstructorHelpers::FObjectFinder<USoundWave> Sound(TEXT("/Game/Developers/Dain/Excavation/Feedback/S_RockToolClang"));
	Clang->SetSound(Sound.Object);
	Clang->bOverrideAttenuation = true;
	Clang->AttenuationOverrides.AttenuationShapeExtents.X = 120.f;
	Clang->AttenuationOverrides.FalloffDistance = 650.f;
	Clang->bAutoActivate = true;
}

void ADigetherRockStrikeEffect::BeginPlay()
{
	Super::BeginPlay();
	for (int32 Index = 0; Index < 7; ++Index)
	{
		Velocities.Add(FMath::VRandCone(FVector::ForwardVector, 1.22f) * FMath::FRandRange(70.f, 155.f));
		Sparks->AddInstance(FTransform(Velocities.Last().Rotation(), FVector::ForwardVector, FVector(.045, .012, .012)));
	}
}

void ADigetherRockStrikeEffect::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Age = GetGameTimeSinceCreation();
	if (Age >= SparkLifetime)
	{
		Sparks->SetVisibility(false);
		SetActorTickEnabled(false);
		return;
	}
	const FVector Gravity = GetActorQuat().UnrotateVector(FVector(0, 0, -340.));
	const float Fade = 1.f - Age / SparkLifetime;
	for (int32 Index = 0; Index < Velocities.Num(); ++Index)
	{
		const FVector& V = Velocities[Index];
		const FVector Offset = FVector::ForwardVector + V * Age + .5f * Gravity * Age * Age;
		const FTransform Pose((V + Gravity * Age).Rotation(), Offset, FVector(.045, .012, .012) * Fade);
		Sparks->UpdateInstanceTransform(Index, Pose, false, Index == Velocities.Num() - 1, true);
	}
}
