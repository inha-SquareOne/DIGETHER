#include "DigetherDainCharacter.h"
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

void ADigetherDainCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);
	Input->BindAxisKey(EKeys::MouseX, this, &ADigetherDainCharacter::LookYaw);
	Input->BindAxisKey(EKeys::MouseY, this, &ADigetherDainCharacter::LookPitch);
	Input->BindKey(EKeys::One, IE_Pressed, this, &ADigetherDainCharacter::SelectPickaxe);
	Input->BindKey(EKeys::Two, IE_Pressed, this, &ADigetherDainCharacter::SelectShovel);
	Input->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ADigetherDainCharacter::StartDig);
	Input->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ADigetherDainCharacter::StopDig);
}
void ADigetherDainCharacter::LookYaw(float Value)
{
	AddControllerYawInput(Value);
}
void ADigetherDainCharacter::LookPitch(float Value)
{
	AddControllerPitchInput(-Value);
}
void ADigetherDainCharacter::StartDig()
{
	bDigHeld = true;
	DigOnce();
}
void ADigetherDainCharacter::StopDig()
{
	bDigHeld = false;
}

void ADigetherDainCharacter::Tick(float DeltaSeconds)
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

bool ADigetherDainCharacter::TraceTool(FHitResult& OutHit) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LabTool), true, this);
	const FVector Start = Camera->GetComponentLocation();
	return GetWorld()->LineTraceSingleByChannel(OutHit, Start, Start + Camera->GetForwardVector() * 400.f,
												ECC_Visibility, Params);
}

void ADigetherDainCharacter::SelectPickaxe()
{
	SelectedTool = EDigetherLabTool::Pickaxe;
}
void ADigetherDainCharacter::SelectShovel()
{
	SelectedTool = EDigetherLabTool::Shovel;
}
float ADigetherDainCharacter::GetSelectedRadiusCm() const
{
	return SelectedTool == EDigetherLabTool::Pickaxe ? PickaxeRadiusCm : SoilBrushRadius;
}
float ADigetherDainCharacter::GetSelectedIntervalSeconds() const
{
	return SelectedTool == EDigetherLabTool::Pickaxe ? PickaxeIntervalSeconds : HitInterval;
}

bool ADigetherDainCharacter::DigOnce()
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
