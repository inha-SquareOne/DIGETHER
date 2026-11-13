#include "DigetherExcavationLabCharacter.h"
#include "Game/Excavation/Dain/DigetherDiggableSoil.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"

ADigetherExcavationLabCharacter::ADigetherExcavationLabCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	// 개인 이동 시험 규격(cm), 팀 캐릭터 최종 규격과 분리
	GetCapsuleComponent()->InitCapsuleSize(35.f, 90.f);
	GetCharacterMovement()->MaxWalkSpeed = 300.f;
	GetCharacterMovement()->MaxStepHeight = 45.f;
	bUseControllerRotationYaw = true;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0, 0, 64));
	Camera->bUsePawnControlRotation = true;
}

void ADigetherExcavationLabCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);
	Input->BindAxisKey(EKeys::MouseX, this, &ADigetherExcavationLabCharacter::LookYaw);
	Input->BindAxisKey(EKeys::MouseY, this, &ADigetherExcavationLabCharacter::LookPitch);
	Input->BindKey(EKeys::One, IE_Pressed, this, &ADigetherExcavationLabCharacter::SelectPickaxe);
	Input->BindKey(EKeys::Two, IE_Pressed, this, &ADigetherExcavationLabCharacter::SelectShovel);
	Input->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ADigetherExcavationLabCharacter::StartDig);
	Input->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ADigetherExcavationLabCharacter::StopDig);
}
void ADigetherExcavationLabCharacter::LookYaw(float Value)
{
	AddControllerYawInput(Value);
}
void ADigetherExcavationLabCharacter::LookPitch(float Value)
{
	AddControllerPitchInput(-Value);
}
void ADigetherExcavationLabCharacter::StartDig()
{
	bDigHeld = true;
	DigOnce();
}
void ADigetherExcavationLabCharacter::StopDig()
{
	bDigHeld = false;
}

void ADigetherExcavationLabCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (!IsValid(PlayerController) || !IsLocallyControlled())
	{
		return;
	}
	// 직접 키 검사는 개인 맵에 한정, 공용 입력 구조와 분리
	const float Forward =
		float(PlayerController->IsInputKeyDown(EKeys::W)) - float(PlayerController->IsInputKeyDown(EKeys::S));
	const float Right =
		float(PlayerController->IsInputKeyDown(EKeys::D)) - float(PlayerController->IsInputKeyDown(EKeys::A));
	AddMovementInput(GetActorForwardVector(), Forward);
	AddMovementInput(GetActorRightVector(), Right);
	if (bDigHeld)
	{
		DigOnce();
	}
}

bool ADigetherExcavationLabCharacter::TraceTool(FHitResult& OutHit) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LabTool), true, this);
	const FVector Start = Camera->GetComponentLocation();
	return GetWorld()->LineTraceSingleByChannel(OutHit, Start, Start + Camera->GetForwardVector() * 400.f,
												ECC_Visibility, Params);
}

void ADigetherExcavationLabCharacter::SelectPickaxe()
{
	SelectedTool = EDigetherLabTool::Pickaxe;
}
void ADigetherExcavationLabCharacter::SelectShovel()
{
	SelectedTool = EDigetherLabTool::Shovel;
}
float ADigetherExcavationLabCharacter::GetSelectedRadiusCm() const
{
	return SelectedTool == EDigetherLabTool::Pickaxe ? PickaxeRadiusCm : SoilBrushRadius;
}
float ADigetherExcavationLabCharacter::GetSelectedIntervalSeconds() const
{
	return SelectedTool == EDigetherLabTool::Pickaxe ? PickaxeIntervalSeconds : HitInterval;
}

bool ADigetherExcavationLabCharacter::DigOnce()
{
	const float RadiusCm = GetSelectedRadiusCm();
	const float IntervalSeconds = GetSelectedIntervalSeconds();
	// 잘못된 C++ 설정값도 거절, 무효값으로 대기 시각 오염 방지
	if (!HasAuthority() || !FMath::IsFinite(RadiusCm) || RadiusCm <= 0.f ||
		!FMath::IsFinite(IntervalSeconds) || IntervalSeconds < .15f ||
		GetWorld()->GetTimeSeconds() < NextUseSeconds)
	{
		return false;
	}
	// 빗나간 시도도 사용 간격 소비, 보호 벽 뒤 재탐색 금지
	// 도구를 전환해도 직전 사용의 대기 시각 유지
	NextUseSeconds = GetWorld()->GetTimeSeconds() + IntervalSeconds;
	FHitResult Hit;
	if (!TraceTool(Hit))
	{
		return false;
	}
	// 첫 충돌만 판정해 보호 벽 뒤의 흙 굴착 방지
	ADigetherDiggableSoil* CurrentSoil = Cast<ADigetherDiggableSoil>(Hit.GetActor());
	if (IsValid(CurrentSoil))
	{
		return CurrentSoil->TryExcavate(Hit.ImpactPoint, RadiusCm) == EDigetherSoilEditResult::Applied;
	}
	return false;
}
