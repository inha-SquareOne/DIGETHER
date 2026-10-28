#include "DigetherPrototypeCharacter.h"
#include "Prototype/World/DigetherPrototypePortal.h"

#include "DigetherPrototypePlayerController.h"
#include "Prototype/DigetherPrototypePlayerState.h"
#include "Prototype/DigetherPrototypeCorpse.h"
#include "Prototype/World/DigetherPrototypeWorld.h"
#include "Prototype/World/DigetherExcavationTerrain.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ADigetherPrototypeCharacter::ADigetherPrototypeCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	bUseControllerRotationYaw = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	if (USkeletalMeshComponent* SkeletalMesh = GetMesh())
	{
		SkeletalMesh->SetVisibility(false);
		SkeletalMesh->SetHiddenInGame(true);
		SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 원격 관전용 회색 primitive. 엔진 기본 실린더 메시를 참조만 하며 새 Content를 추가하지 않는다.
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetCapsuleComponent());
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CylinderMesh.Object);
	}
	BodyMesh->SetRelativeScale3D(FVector(.65f, .65f, 1.76f));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetOwnerNoSee(true);

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(GetCapsuleComponent());
	FollowCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	FollowCamera->bUsePawnControlRotation = true;

	Headlamp = CreateDefaultSubobject<USpotLightComponent>(TEXT("Headlamp"));
	Headlamp->SetupAttachment(GetCapsuleComponent());
	Headlamp->SetRelativeLocation(FVector(20.f, 0.f, 64.f));
	Headlamp->Intensity = 12000.f;
	Headlamp->AttenuationRadius = 1400.f;
	Headlamp->InnerConeAngle = 20.f;
	Headlamp->OuterConeAngle = 42.f;
	Headlamp->SetLightColor(FLinearColor(1.f, .78f, .50f));
	Headlamp->SetVisibility(false);

	// A small camera-attached placeholder shovel, visible only to its owner.
	ToolPivot = CreateDefaultSubobject<USceneComponent>(TEXT("ToolPivot"));
	ToolPivot->SetupAttachment(FollowCamera);
	ToolPivot->SetRelativeLocation(FVector(42.f, 22.f, -24.f));
	ToolPivot->SetRelativeRotation(FRotator(-15.f, -8.f, -24.f));
	ToolHandle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ToolHandle"));
	ToolHandle->SetupAttachment(ToolPivot);
	ToolBlade = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ToolBlade"));
	ToolBlade->SetupAttachment(ToolPivot);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded()) { ToolHandle->SetStaticMesh(CubeMesh.Object); ToolBlade->SetStaticMesh(CubeMesh.Object); }
	ToolHandle->SetRelativeScale3D(FVector(.55f, .035f, .035f));
	ToolBlade->SetRelativeLocation(FVector(32.f, 0.f, 0.f));
	ToolBlade->SetRelativeScale3D(FVector(.20f, .16f, .025f));
	for (UStaticMeshComponent* Part : { ToolHandle.Get(), ToolBlade.Get() })
	{
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetOnlyOwnerSee(true);
		Part->SetCastShadow(false);
	}
}

namespace
{
	// 납부 입력창이 열린 동안 게임 입력(이동·상호작용·슬롯·저장 등)을 차단한다.
	bool IsDepositInputBlocking(const APawn* Pawn)
	{
		const ADigetherPrototypePlayerController* PC = Pawn ? Cast<ADigetherPrototypePlayerController>(Pawn->GetController()) : nullptr;
		return PC && PC->IsDepositInputOpen();
	}
}

void ADigetherPrototypeCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADigetherPrototypeCharacter, bHeadlampOn);
	DOREPLIFETIME_CONDITION(ADigetherPrototypeCharacter, Stamina, COND_OwnerOnly);
}

ADigetherPrototypePlayerController* ADigetherPrototypeCharacter::GetPrototypeController() const
{
	return Cast<ADigetherPrototypePlayerController>(GetController());
}

bool ADigetherPrototypeCharacter::IsLifeRestricted() const
{
	const ADigetherPrototypePlayerState* PS = GetPlayerState<ADigetherPrototypePlayerState>();
	// PlayerState가 아직 늦게 생성되었거나 Active가 아니면(매몰/사망/탈출) 행동을 제한한다.
	return !PS || PS->PublicLife != EPrototypeLife::Active;
}

float ADigetherPrototypeCharacter::MovementSpeed(bool bSprint) const
{
	const ADigetherPrototypePlayerState* PS = GetPlayerState<ADigetherPrototypePlayerState>();
	float Weight = 0.f;
	if (PS) for (const FPrototypeItem& Item : PS->PrivateState.Items) { Weight += Item.WeightKg; }
	const float Factor = Weight >= 20.f ? .65f : Weight >= 10.f ? .8f : 1.f;
	return (bSprint ? SprintSpeed : WalkSpeed) * Factor;
}

FString ADigetherPrototypeCharacter::DescribeAimTarget(const AActor* Target)
{
	if (!IsValid(Target))
	{
		return FString();
	}
	if (const ADigetherPrototypeStation* Station = Cast<ADigetherPrototypeStation>(Target))
	{
		switch (Station->Kind)
		{
		case EPrototypeStationKind::Start: return TEXT("출발 장치 — E 유지 / 게임 오버 시 F 재시작");
		case EPrototypeStationKind::Elevator: return TEXT("엘리베이터 — 탑승 원 안에서 E 유지");
		case EPrototypeStationKind::Deposit: return TEXT("납부대 — E를 눌러 금액 입력");
		case EPrototypeStationKind::Shop: return TEXT("시험 상점 — E 장비 / F 긴급 탈출 장치");
		case EPrototypeStationKind::EmergencyExit: return TEXT("긴급 탈출 — 회수품 없이 장치를 소지하고 E 유지");
		default: return TEXT("안내 단말 — E");
		}
	}
	if (Cast<ADigetherPrototypePortal>(Target)) return TEXT("이동 포탈 — E로 광산·폐교 왕복 / 회수·정산은 승강기");
	if (Cast<ADigetherExcavationTerrain>(Target)) return TEXT("흙 — 좌클릭 유지하여 굴착 / F 보강·보수");
	if (Cast<ADigetherPrototypeDigCell>(Target)) return TEXT("굴착 구간 — 좌클릭 또는 E / F 보강·보수");
	if (Cast<ADigetherPrototypePickup>(Target)) return TEXT("회수품 — E 일반 / F 히든");
	if (Cast<ADigetherPrototypeCorpse>(Target)) return TEXT("시체 — E 운반·내려놓기");
	if (Cast<ADigetherPrototypeHazard>(Target)) return TEXT("시험 위험체 — 접근 주의");
	if (const APawn* Pawn = Cast<APawn>(Target))
	{
		const ADigetherPrototypePlayerState* Other = Pawn->GetPlayerState<ADigetherPrototypePlayerState>();
		return Other && Other->PublicLife == EPrototypeLife::Buried ? TEXT("매몰된 동료 — E 유지하여 구조") : TEXT("동료 — 좌클릭 장비 공격");
	}
	return TEXT("상호작용 가능 대상 — E");
}

bool ADigetherPrototypeCharacter::GetPrototypeAimOrigin(FVector& OutOrigin) const
{
	if (!IsValid(FollowCamera)) { return false; }
	OutOrigin = FollowCamera->GetComponentLocation();
	return !OutOrigin.ContainsNaN();
}

void ADigetherPrototypeCharacter::UpdateAimTrace()
{
	ADigetherPrototypePlayerController* PC = GetPrototypeController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World || !FollowCamera)
	{
		return;
	}
	FVector Start;
	if (!GetPrototypeAimOrigin(Start)) { PC->SetAimTarget(nullptr, FString()); return; }
	const FVector End = Start + FollowCamera->GetForwardVector() * InteractTraceDistance;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PrototypeAimTrace), false, this);
	FHitResult Hit;
	AActor* Found = nullptr;
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) &&
		(Cast<IDigetherPrototypeInteractable>(Hit.GetActor()) || Cast<APawn>(Hit.GetActor()) || Cast<ADigetherPrototypeHazard>(Hit.GetActor()) || Cast<ADigetherExcavationTerrain>(Hit.GetActor())))
	{
		Found = Hit.GetActor();
	}
	PC->SetAimTarget(Found, Found ? DescribeAimTarget(Found) : FString());
}

void ADigetherPrototypeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (IsLocallyControlled())
	{
		const float Forward = (HeldMoveKeys.Contains(EKeys::W) ? 1.f : 0.f) - (HeldMoveKeys.Contains(EKeys::S) ? 1.f : 0.f);
		const float Right = (HeldMoveKeys.Contains(EKeys::D) ? 1.f : 0.f) - (HeldMoveKeys.Contains(EKeys::A) ? 1.f : 0.f);
		MoveForward(Forward);
		MoveRight(Right);
	}
	const ADigetherPrototypePlayerState* VisibleState = GetPlayerState<ADigetherPrototypePlayerState>();
	if (VisibleState)
	{
		const bool bGone = VisibleState->PublicLife == EPrototypeLife::Dead || VisibleState->PublicLife == EPrototypeLife::Escaped;
		BodyMesh->SetVisibility(!bGone);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, bGone ? ECR_Ignore : ECR_Block);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, bGone ? ECR_Ignore : ECR_Block);
	}
	UpdateAimTrace();
	if (Headlamp) { Headlamp->SetWorldRotation(GetBaseAimRotation()); }
	if (IsLocallyControlled())
	{
		bool bShowTool = false;
		if (VisibleState && VisibleState->PublicLife == EPrototypeLife::Active)
		{
			for (const FPrototypeItem& Item : VisibleState->PrivateState.Items) { bShowTool |= Item.bEquipment; }
			for (const FPrototypeItem& Item : VisibleState->PrivateState.Items) { if (Item.bTwoHanded) { bShowTool = false; break; } }
		}
		ToolHandle->SetVisibility(bShowTool);
		ToolBlade->SetVisibility(bShowTool);
		if (IsLifeRestricted() || IsDepositInputBlocking(this)) { bHoldingDig = false; }
		ToolSwingRemaining = FMath::Max(0.f, ToolSwingRemaining - DeltaSeconds);
		const float Swing = FMath::Sin((ToolSwingRemaining / .22f) * PI);
		ToolPivot->SetRelativeRotation(FRotator(-15.f - 40.f * Swing, -8.f, -24.f));
		if (bHoldingDig && !IsLifeRestricted() && GetWorld() && GetWorld()->GetTimeSeconds() >= NextDigInputTime)
		{
			NextDigInputTime = GetWorld()->GetTimeSeconds() + .28;
			if (ADigetherPrototypePlayerController* PC = GetPrototypeController()) { PC->ServerDig(); }
		}
	}

	if (HasAuthority())
	{
		const bool bRestricted = IsLifeRestricted();
		if (bRestricted)
		{
			bWantsSprint = false;
			if (GetCharacterMovement()->MovementMode != MOVE_None) { GetCharacterMovement()->DisableMovement(); }
		}
		else if (GetCharacterMovement()->MovementMode == MOVE_None) { GetCharacterMovement()->SetMovementMode(MOVE_Walking); }
		if (bWantsSprint && !bRestricted && GetVelocity().SizeSquared2D() > FMath::Square(10.f) && !GetCharacterMovement()->IsCrouching())
		{
			Stamina = FMath::Max(0.f, Stamina - StaminaDrainPerSecond * DeltaSeconds);
			if (Stamina <= 0.f) { bWantsSprint = false; }
		}
		else
		{
			Stamina = FMath::Min(MaxStamina, Stamina + StaminaRegenPerSecond * DeltaSeconds);
		}
		GetCharacterMovement()->MaxWalkSpeed = MovementSpeed(bWantsSprint && Stamina > 0.f);
	}
	if (IsLocallyControlled() && (Stamina <= 0.f || IsLifeRestricted()))
	{
		bIsSprinting = false;
		bWantsSprint = false;
	}
	if (IsLocallyControlled() && !HasAuthority())
	{
		GetCharacterMovement()->MaxWalkSpeed = MovementSpeed(bIsSprinting && Stamina > 0.f);
	}
}

void ADigetherPrototypeCharacter::PlayConfirmedDigSwing()
{
	if (IsLocallyControlled()) { ToolSwingRemaining = .22f; }
}

void ADigetherPrototypeCharacter::ApplyHeadlampVisual(bool bOn) const
{
	if (Headlamp)
	{
		Headlamp->SetVisibility(bOn);
	}
}

void ADigetherPrototypeCharacter::OnRep_Headlamp()
{
	ApplyHeadlampVisual(bHeadlampOn);
}

void ADigetherPrototypeCharacter::ServerSetHeadlamp_Implementation(bool bOn)
{
	if (IsLifeRestricted()) { return; }
	const ADigetherPrototypePlayerState* PS = GetPlayerState<ADigetherPrototypePlayerState>();
	if (PS) for (const FPrototypeItem& Item : PS->PrivateState.Items) { if (Item.bTwoHanded) { return; } }
	bHeadlampOn = bOn;
	ApplyHeadlampVisual(bOn);
}

void ADigetherPrototypeCharacter::ServerSetSprinting_Implementation(bool bSprint)
{
	if (!HasAuthority())
	{
		return;
	}
	bWantsSprint = bSprint && !IsLifeRestricted() && Stamina > 0.f;
	GetCharacterMovement()->MaxWalkSpeed = MovementSpeed(bWantsSprint);
}

void ADigetherPrototypeCharacter::MoveForward(float Value)
{
	if (IsLifeRestricted() || FMath::IsNearlyZero(Value) || !Controller)
	{
		return;
	}
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Value);
}

void ADigetherPrototypeCharacter::MoveRight(float Value)
{
	if (IsLifeRestricted() || FMath::IsNearlyZero(Value) || !Controller)
	{
		return;
	}
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Value);
}

void ADigetherPrototypeCharacter::MoveKeyPressed(FKey Key) { if (!IsDepositInputBlocking(this)) { HeldMoveKeys.Add(Key); } }
void ADigetherPrototypeCharacter::MoveKeyReleased(FKey Key) { HeldMoveKeys.Remove(Key); }

void ADigetherPrototypeCharacter::LookYaw(float Value)
{
	AddControllerYawInput(Value);
}

void ADigetherPrototypeCharacter::LookPitch(float Value)
{
	AddControllerPitchInput(Value);
}

void ADigetherPrototypeCharacter::StartSprint()
{
	if (IsLifeRestricted() || IsDepositInputBlocking(this))
	{
		return;
	}
	bWantsSprint = true;
	if (!bIsSprinting && Stamina > 0.f)
	{
		bIsSprinting = true;
		GetCharacterMovement()->MaxWalkSpeed = MovementSpeed(true);
		ServerSetSprinting(true);
	}
}

void ADigetherPrototypeCharacter::StopSprint()
{
	bWantsSprint = false;
	if (bIsSprinting)
	{
		bIsSprinting = false;
		GetCharacterMovement()->MaxWalkSpeed = MovementSpeed(false);
		ServerSetSprinting(false);
	}
}

void ADigetherPrototypeCharacter::StartCrouchPrototype()
{
	if (!IsLifeRestricted() && !IsDepositInputBlocking(this))
	{
		Crouch();
	}
}

void ADigetherPrototypeCharacter::StopCrouchPrototype()
{
	UnCrouch();
}

void ADigetherPrototypeCharacter::JumpPrototype()
{
	if (!IsLifeRestricted() && !IsDepositInputBlocking(this))
	{
		Jump();
	}
}

void ADigetherPrototypeCharacter::Interact()
{
	if (IsLifeRestricted() || IsDepositInputBlocking(this) || ActiveInteractionKey.IsValid())
	{
		return;
	}
	ADigetherPrototypePlayerController* PC = GetPrototypeController();
	AActor* Target = PC ? PC->GetAimTarget() : nullptr;
	if (PC && Target)
	{
		ActiveInteractionKey = EKeys::E;
		if (APawn* Pawn = Cast<APawn>(Target)) { PC->ServerPawnAction(Pawn, false); }
		else { PC->ServerInteract(Target, false); }
	}
}

void ADigetherPrototypeCharacter::AttackOrDig()
{
	if (IsLifeRestricted() || IsDepositInputBlocking(this) || ActiveInteractionKey.IsValid()) { return; }
	ADigetherPrototypePlayerController* PC = GetPrototypeController();
	AActor* Target = PC ? PC->GetAimTarget() : nullptr;
	if (!PC || !Target) { return; }
	ActiveInteractionKey = EKeys::LeftMouseButton;
	if (Cast<ADigetherExcavationTerrain>(Target))
	{
		bHoldingDig = true;
		NextDigInputTime = GetWorld() ? GetWorld()->GetTimeSeconds() + .28 : 0.0;
		PC->ServerDig();
	}
	else if (APawn* Pawn = Cast<APawn>(Target)) { PC->ServerPawnAction(Pawn, true); }
	else { PC->ServerInteract(Target, false); }
}

void ADigetherPrototypeCharacter::InteractAlternate()
{
	if (IsLifeRestricted() || IsDepositInputBlocking(this) || ActiveInteractionKey.IsValid())
	{
		return;
	}
	ADigetherPrototypePlayerController* PC = GetPrototypeController();
	AActor* Target = PC ? PC->GetAimTarget() : nullptr;
	if (PC && Target)
	{
		ActiveInteractionKey = EKeys::F;
		if (Cast<ADigetherExcavationTerrain>(Target)) { PC->ServerSupportTerrain(); }
		else { PC->ServerInteract(Target, true); }
	}
}

void ADigetherPrototypeCharacter::CancelInteract()
{
	bHoldingDig = false;
	ActiveInteractionKey = FKey();
	if (ADigetherPrototypePlayerController* PC = GetPrototypeController())
	{
		PC->ServerCancelInteraction();
	}
}

void ADigetherPrototypeCharacter::ReleaseInteractionKey(const FKey& Key)
{
	if (ActiveInteractionKey == Key) { CancelInteract(); }
}
void ADigetherPrototypeCharacter::ReleaseInteract() { ReleaseInteractionKey(EKeys::E); }
void ADigetherPrototypeCharacter::ReleaseAttack() { bHoldingDig = false; ReleaseInteractionKey(EKeys::LeftMouseButton); }
void ADigetherPrototypeCharacter::ReleaseAlternate() { ReleaseInteractionKey(EKeys::F); }

void ADigetherPrototypeCharacter::ResetLocalInput()
{
	bHoldingDig = false;
	ActiveInteractionKey = FKey();
	HeldMoveKeys.Reset();
	StopSprint();
}

void ADigetherPrototypeCharacter::SelectSlot1() { if (!IsDepositInputBlocking(this)) { SelectedSlot = 1; } }
void ADigetherPrototypeCharacter::SelectSlot2() { if (!IsDepositInputBlocking(this)) { SelectedSlot = 2; } }
void ADigetherPrototypeCharacter::SelectSlot3() { if (!IsDepositInputBlocking(this)) { SelectedSlot = 3; } }
void ADigetherPrototypeCharacter::SelectSlot4() { if (!IsDepositInputBlocking(this)) { SelectedSlot = 4; } }
void ADigetherPrototypeCharacter::SelectSlot5() { if (!IsDepositInputBlocking(this)) { SelectedSlot = 5; } }

void ADigetherPrototypeCharacter::ToggleHeadlampInput()
{
	if (IsDepositInputBlocking(this)) { return; }
	ServerSetHeadlamp(!bHeadlampOn);
}

void ADigetherPrototypeCharacter::DropSelected() { if (IsDepositInputBlocking(this)) { return; } if (ADigetherPrototypePlayerController* PC = GetPrototypeController()) { PC->ServerDrop(SelectedSlot); } }
void ADigetherPrototypeCharacter::SelfEscape() { if (IsDepositInputBlocking(this)) { return; } if (ADigetherPrototypePlayerController* PC = GetPrototypeController()) { PC->ServerSelfEscape(); } }
void ADigetherPrototypeCharacter::SaveFixture() { if (IsDepositInputBlocking(this)) { return; } if (ADigetherPrototypePlayerController* PC = GetPrototypeController()) { PC->ServerSaveOrLoad(false); } }
void ADigetherPrototypeCharacter::LoadFixture() { if (IsDepositInputBlocking(this)) { return; } if (ADigetherPrototypePlayerController* PC = GetPrototypeController()) { PC->ServerSaveOrLoad(true); } }

void ADigetherPrototypeCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (!PlayerInputComponent)
	{
		return;
	}

	// 외부 InputAction/Mapping 자산 없이 즉시 동작하는 legacy raw-key 바인딩.
	for (const FKey& Key : { EKeys::W, EKeys::A, EKeys::S, EKeys::D })
	{
		PlayerInputComponent->BindKey(Key, IE_Pressed, this, &ADigetherPrototypeCharacter::MoveKeyPressed);
		PlayerInputComponent->BindKey(Key, IE_Released, this, &ADigetherPrototypeCharacter::MoveKeyReleased);
	}
	PlayerInputComponent->BindAxisKey(EKeys::MouseX).AxisDelegate.GetDelegateForManualSet().BindLambda([this](float Value) { LookYaw(Value); });
	PlayerInputComponent->BindAxisKey(EKeys::MouseY).AxisDelegate.GetDelegateForManualSet().BindLambda([this](float Value) { LookPitch(-Value); });

	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ADigetherPrototypeCharacter::JumpPrototype);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, &ACharacter::StopJumping);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &ADigetherPrototypeCharacter::StartSprint);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &ADigetherPrototypeCharacter::StopSprint);
	PlayerInputComponent->BindKey(EKeys::LeftControl, IE_Pressed, this, &ADigetherPrototypeCharacter::StartCrouchPrototype);
	PlayerInputComponent->BindKey(EKeys::LeftControl, IE_Released, this, &ADigetherPrototypeCharacter::StopCrouchPrototype);

	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &ADigetherPrototypeCharacter::Interact);
	PlayerInputComponent->BindKey(EKeys::E, IE_Released, this, &ADigetherPrototypeCharacter::ReleaseInteract);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ADigetherPrototypeCharacter::AttackOrDig);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ADigetherPrototypeCharacter::ReleaseAttack);
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &ADigetherPrototypeCharacter::InteractAlternate);
	PlayerInputComponent->BindKey(EKeys::F, IE_Released, this, &ADigetherPrototypeCharacter::ReleaseAlternate);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ADigetherPrototypeCharacter::CancelInteract);

	PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &ADigetherPrototypeCharacter::SelectSlot1);
	PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ADigetherPrototypeCharacter::SelectSlot2);
	PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ADigetherPrototypeCharacter::SelectSlot3);
	PlayerInputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ADigetherPrototypeCharacter::SelectSlot4);
	PlayerInputComponent->BindKey(EKeys::Five, IE_Pressed, this, &ADigetherPrototypeCharacter::SelectSlot5);

	PlayerInputComponent->BindKey(EKeys::L, IE_Pressed, this, &ADigetherPrototypeCharacter::ToggleHeadlampInput);
	PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &ADigetherPrototypeCharacter::DropSelected);
	PlayerInputComponent->BindKey(EKeys::B, IE_Pressed, this, &ADigetherPrototypeCharacter::SelfEscape);
	PlayerInputComponent->BindKey(EKeys::F5, IE_Pressed, this, &ADigetherPrototypeCharacter::SaveFixture);
	PlayerInputComponent->BindKey(EKeys::F9, IE_Pressed, this, &ADigetherPrototypeCharacter::LoadFixture);
}
