#include "DigetherDainCharacter.h"
#include "DigetherDainSoilWall.h"
#include "Game/Excavation/Dain/DigetherDiggableSoil.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"

ADigetherDainCharacter::ADigetherDainCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(35.f, 90.f);
	GetCharacterMovement()->MaxWalkSpeed = 300.f;
	GetCharacterMovement()->MaxStepHeight = 45.f;
	bUseControllerRotationYaw = true;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0, 0, 64));
	Camera->bUsePawnControlRotation = true;
}

void ADigetherDainCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);
	Input->BindAxisKey(EKeys::MouseX, this, &ADigetherDainCharacter::LookYaw);
	Input->BindAxisKey(EKeys::MouseY, this, &ADigetherDainCharacter::LookPitch);
	Input->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ADigetherDainCharacter::StartDig);
	Input->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ADigetherDainCharacter::StopDig);
}
void ADigetherDainCharacter::LookYaw(float Value) { AddControllerYawInput(Value); }
void ADigetherDainCharacter::LookPitch(float Value) { AddControllerPitchInput(-Value); }
void ADigetherDainCharacter::StartDig() { bDigHeld = true; DigOnce(); }
void ADigetherDainCharacter::StopDig() { bDigHeld = false; }

void ADigetherDainCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC || !IsLocallyControlled()) { return; }
	// Raw keys are deliberately confined to this disposable lab adapter.
	const float Forward = float(PC->IsInputKeyDown(EKeys::W)) - float(PC->IsInputKeyDown(EKeys::S));
	const float Right = float(PC->IsInputKeyDown(EKeys::D)) - float(PC->IsInputKeyDown(EKeys::A));
	AddMovementInput(GetActorForwardVector(), Forward);
	AddMovementInput(GetActorRightVector(), Right);
	if (bDigHeld) { DigOnce(); }
}

bool ADigetherDainCharacter::TraceTool(FHitResult& OutHit) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LabTool), true, this);
	const FVector Start = Camera->GetComponentLocation();
	return GetWorld()->LineTraceSingleByChannel(OutHit, Start, Start + Camera->GetForwardVector() * 400.f, ECC_Visibility, Params);
}

bool ADigetherDainCharacter::DigOnce()
{
	if (!HasAuthority() || GetWorld()->GetTimeSeconds() - LastHitTime < HitInterval) { return false; }
	LastHitTime = GetWorld()->GetTimeSeconds();
	FHitResult Hit;
	if (!TraceTool(Hit)) { return false; }
	// The FIRST blocking hit must be soil. Never trace through a protected wall.
	if (ADigetherDiggableSoil* CurrentSoil = Cast<ADigetherDiggableSoil>(Hit.GetActor()))
	{
		return CurrentSoil->TryExcavate(Hit.ImpactPoint, SoilBrushRadius) == EDigetherSoilEditResult::Applied;
	}
	ADigetherDainSoilWall* Soil = Cast<ADigetherDainSoilWall>(Hit.GetActor());
	return Soil && Soil->TryExcavate(Hit.ImpactPoint, BrushRadius);
}


