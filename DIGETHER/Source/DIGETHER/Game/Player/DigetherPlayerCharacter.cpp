#include "Game/Player/DigetherPlayerCharacter.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Game/Data/DigetherPlayerStatRow.h"
#include "Game/Input/DigetherPlayerInputData.h"
#include "Game/Player/DigetherPlayerLog.h"
#include "Animation/AnimMontage.h"
#include "Game/Equipment/DigetherWeaponBase.h"
#include "Net/UnrealNetwork.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"

namespace
{
	// 표 값 범위 검사, 미설정(0)과 범위 밖 값을 모두 모아 반환한다
	bool CollectStatProblems(const FDigetherPlayerStatRow& Row, TArray<FString>& OutProblems)
	{
		const auto RequirePositive = [&OutProblems](const TCHAR* Name, float Value)
		{
			if (Value <= 0.f)
			{
				OutProblems.Add(FString::Printf(TEXT("%s는 0보다 커야 한다(현재 %g)"), Name, Value));
			}
		};
		RequirePositive(TEXT("WalkSpeed"), Row.WalkSpeed);
		RequirePositive(TEXT("SprintSpeed"), Row.SprintSpeed);
		RequirePositive(TEXT("CrouchSpeed"), Row.CrouchSpeed);
		RequirePositive(TEXT("JumpZVelocity"), Row.JumpZVelocity);
		RequirePositive(TEXT("GravityScale"), Row.GravityScale);
		RequirePositive(TEXT("ScaleX"), Row.ScaleX);
		RequirePositive(TEXT("ScaleY"), Row.ScaleY);
		RequirePositive(TEXT("ScaleZ"), Row.ScaleZ);
		RequirePositive(TEXT("PassWidthCm"), Row.PassWidthCm);
		RequirePositive(TEXT("PassHeightCm"), Row.PassHeightCm);
		RequirePositive(TEXT("CrouchHeightCm"), Row.CrouchHeightCm);
		if (Row.CrouchHeightCm >= Row.PassHeightCm)
		{
			OutProblems.Add(TEXT("CrouchHeightCm는 PassHeightCm보다 작아야 한다"));
		}
		if (Row.WalkSpeed > Row.SprintSpeed)
		{
			OutProblems.Add(TEXT("WalkSpeed는 SprintSpeed 이하여야 한다"));
		}
		return OutProblems.IsEmpty();
	}
}

ADigetherPlayerCharacter::ADigetherPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	PlayerStatTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/Player/DT_PlayerStats.DT_PlayerStats")));
	// 입력 데이터 에셋 이름 약속: 에디터에서 이 이름·위치로 만든다, 다르면 BP 자식 클래스에서 지정한다
	InputData = TSoftObjectPtr<UDigetherPlayerInputData>(FSoftObjectPath(TEXT("/Game/Developers/taewoong/Input/DA_PlayerInput.DA_PlayerInput")));
	bReplicates = true;

	// 스프링암을 캡슐에 붙이고 카메라는 암 끝에 붙인다 (높이는 블루프린트의 스프링암 위치로 맞춘다)
	// 암이 컨트롤러 회전을 따르므로 카메라 자체는 따르지 않는다(이중 적용 방지)
	CameraSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraSpringArm"));
	CameraSpringArm->SetupAttachment(GetCapsuleComponent());
	CameraSpringArm->TargetArmLength = 0.f;
	CameraSpringArm->bUsePawnControlRotation = true;
	CameraSpringArm->bDoCollisionTest = false;
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(CameraSpringArm, USpringArmComponent::SocketName);
	FirstPersonCamera->bUsePawnControlRotation = false;

	// 3인칭 몸: 본인 화면에서는 숨기고 다른 플레이어에게만 보이게 한다
	// 몸 메시·애니메이션 BP는 캐릭터 BP에서 지정한다. 코드의 기본값은 드워프 + 리타겟한 ABP다
	USkeletalMeshComponent* BodyMesh = GetMesh();
	BodyMesh->SetOwnerNoSee(true);
	BodyMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	// 숨겨져도 자세는 계속 계산해야 1인칭 팔이 따라 움직인다
	BodyMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BodyMeshAsset(TEXT("/Game/Characters/Dwarf/SK_Ch_Dwarf_Miner.SK_Ch_Dwarf_Miner"));
	if (BodyMeshAsset.Succeeded())
	{
		BodyMesh->SetSkeletalMesh(BodyMeshAsset.Object);
	}
	static ConstructorHelpers::FClassFinder<UAnimInstance> BodyAnimAsset(TEXT("/Game/Characters/Dwarf/Anims/ABP_UnarmedDG.ABP_UnarmedDG_C"));
	if (BodyAnimAsset.Succeeded())
	{
		BodyMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		BodyMesh->SetAnimInstanceClass(BodyAnimAsset.Class);
	}

	// 1인칭 팔: 본인 화면에서만 보이고 3인칭 몸의 자세를 따라 한다. 충돌·그림자는 쓰지 않는다
	// 전용 팔 모델이 오면 FirstPersonArmsMesh만 바꾼다
	FirstPersonArms = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonArms"));
	FirstPersonArms->SetupAttachment(BodyMesh);
	FirstPersonArms->SetOnlyOwnerSee(true);
	FirstPersonArms->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonArms->SetCastShadow(false);
	FirstPersonArms->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
	}
}

void ADigetherPlayerCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ApplyPlayerStats();
}

void ADigetherPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 장비는 서버가 만들어 복제한다
	if (HasAuthority() && DefaultWeaponClass != nullptr && EquippedWeapon == nullptr)
	{
		EquipWeapon(DefaultWeaponClass);
	}
	// 1인칭 팔은 화면 표현이라 화면이 없는 전용 서버에서는 건너뛴다
	if (GetNetMode() != NM_DedicatedServer)
	{
		RefreshVisuals();
	}
}

void ADigetherPlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADigetherPlayerCharacter, EquippedWeapon);
}

void ADigetherPlayerCharacter::EquipWeapon(TSubclassOf<ADigetherWeaponBase> WeaponClass)
{
	if (!HasAuthority())
	{
		return;
	}
	if (EquippedWeapon != nullptr)
	{
		EquippedWeapon->Destroy();
		EquippedWeapon = nullptr;
	}
	if (WeaponClass != nullptr)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.Instigator = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		EquippedWeapon = GetWorld()->SpawnActor<ADigetherWeaponBase>(WeaponClass, GetActorTransform(), Params);
	}
	// 서버(리슨 서버 호스트 포함)는 OnRep을 받지 않으므로 직접 붙인다
	AttachEquippedWeapon();
}

void ADigetherPlayerCharacter::OnRep_EquippedWeapon()
{
	AttachEquippedWeapon();
}

void ADigetherPlayerCharacter::AttachEquippedWeapon()
{
	if (EquippedWeapon != nullptr && GetNetMode() != NM_DedicatedServer)
	{
		EquippedWeapon->AttachToCharacter(GetMesh());
	}
}

#if WITH_EDITOR
void ADigetherPlayerCharacter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// PIE 중 인스턴스 값을 바꾸면 바로 반영해 눈으로 보며 맞출 수 있게 한다
	if (GetWorld() != nullptr && GetWorld()->IsGameWorld())
	{
		RefreshVisuals();
	}
}
#endif

void ADigetherPlayerCharacter::RefreshVisuals()
{
	/*
	// 1인칭 팔 메시를 정하고 3인칭 몸의 자세를 따라 하게 한다(같은 스켈레톤이어야 한다)
	USkeletalMesh* ArmsMesh = FirstPersonArmsMesh.IsNull() ? GetMesh()->GetSkeletalMeshAsset() : FirstPersonArmsMesh.LoadSynchronous();
	if (FirstPersonArms->GetSkeletalMeshAsset() != ArmsMesh)
	{
		FirstPersonArms->SetSkeletalMesh(ArmsMesh);
	}
	FirstPersonArms->SetLeaderPoseComponent(GetMesh());
	FirstPersonArms->SetRelativeTransform(FirstPersonArmsOffset);

	// 몸 전체를 팔 대신 쓰는 동안 머리·목이 카메라를 가리지 않게 숨긴다
	for (const FName Bone : FirstPersonHiddenBones)
	{
		if (FirstPersonArms->GetBoneIndex(Bone) != INDEX_NONE)
		{
			FirstPersonArms->HideBoneByName(Bone, PBO_None);
		}
	}
	*/
}

void ADigetherPlayerCharacter::ApplyPlayerStats()
{
	const UDataTable* Table = PlayerStatTable.LoadSynchronous();
	if (Table == nullptr)
	{
		UE_LOG(LogDigetherPlayer, Error, TEXT("플레이어 수치 표를 불러오지 못했다: %s, CSV를 FDigetherPlayerStatRow로 임포트해야 한다"), *PlayerStatTable.ToString());
		return;
	}
	const FDigetherPlayerStatRow* Row = Table->FindRow<FDigetherPlayerStatRow>(PlayerStatProfile, TEXT("ADigetherPlayerCharacter::ApplyPlayerStats"), false);
	if (Row == nullptr)
	{
		UE_LOG(LogDigetherPlayer, Error, TEXT("플레이어 수치 표 %s에 행 %s가 없다"), *Table->GetName(), *PlayerStatProfile.ToString());
		return;
	}
	TArray<FString> Problems;
	if (!CollectStatProblems(*Row, Problems))
	{
		for (const FString& Problem : Problems)
		{
			UE_LOG(LogDigetherPlayer, Error, TEXT("플레이어 수치 행 %s 오류: %s"), *PlayerStatProfile.ToString(), *Problem);
		}
		return;
	}

	// 통과 폭·높이는 스케일이 적용된 월드 크기이므로 스케일로 나눠 캡슐 원래 크기를 구한다
	// D26(캡슐 매핑) 확정 전 임시 환산이다. 확정되면 이 식만 바꾼다
	const float CapsuleRadius = Row->PassWidthCm * 0.5f / FMath::Min(Row->ScaleX, Row->ScaleY);
	const float CapsuleHalfHeight = Row->PassHeightCm * 0.5f / Row->ScaleZ;
	const float CrouchedHalfHeight = Row->CrouchHeightCm * 0.5f / Row->ScaleZ;

	SetActorScale3D(FVector(Row->ScaleX, Row->ScaleY, Row->ScaleZ));
	GetCapsuleComponent()->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = Row->WalkSpeed;
	Movement->MaxWalkSpeedCrouched = Row->CrouchSpeed;
	Movement->JumpZVelocity = Row->JumpZVelocity;
	Movement->GravityScale = Row->GravityScale;
	Movement->SetCrouchedHalfHeight(CrouchedHalfHeight);

	// 몸은 캡슐 바닥에 맞춰 내린다
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -CapsuleHalfHeight));
	// ACharacter가 PostInitializeComponents에서 저장한 기본 메시 위치로 이동 보정이 매 틱 되돌리므로, 새 위치를 다시 저장한다
	CacheInitialMeshOffset(GetMesh()->GetRelativeLocation(), GetMesh()->GetRelativeRotation());
	RefreshVisuals();

	SprintSpeedCmPerSec = Row->SprintSpeed;
	bPlayerStatsApplied = true;
}

void ADigetherPlayerCharacter::SetMappingContext(AController* InController, bool bAdd) const
{
	const APlayerController* PlayerController = Cast<APlayerController>(InController);
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (Subsystem == nullptr || MappingContext == nullptr)
	{
		return;
	}
	if (bAdd)
	{
		Subsystem->AddMappingContext(MappingContext, 0);
	}
	else
	{
		Subsystem->RemoveMappingContext(MappingContext);
	}
}

void ADigetherPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	const UDigetherPlayerInputData* Data = InputData.LoadSynchronous();
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (Data == nullptr || Input == nullptr)
	{
		UE_LOG(LogDigetherPlayer, Error, TEXT("입력 데이터를 불러오지 못했다: %s, DA_PlayerInput을 만들고 액션을 지정해야 한다"), *InputData.ToString());
		return;
	}
	TArray<FName> Missing;
	if (!Data->CollectMissingActions(Missing))
	{
		for (const FName Name : Missing)
		{
			UE_LOG(LogDigetherPlayer, Error, TEXT("입력 데이터 %s에 %s가 비어 있다"), *Data->GetName(), *Name.ToString());
		}
		return;
	}

	// 컨텍스트는 컨트롤러가 바뀔 때(NotifyControllerChanged) 등록하므로 여기서 만들어 둔다
	MappingContext = Data->BuildMappingContext(this);

	Input->BindAction(Data->MoveAction, ETriggerEvent::Triggered, this, &ADigetherPlayerCharacter::HandleMove);
	Input->BindAction(Data->LookAction, ETriggerEvent::Triggered, this, &ADigetherPlayerCharacter::HandleLook);
	Input->BindAction(Data->JumpAction, ETriggerEvent::Started, this, &ADigetherPlayerCharacter::HandleJumpPressed);
	Input->BindAction(Data->JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	Input->BindAction(Data->CrouchAction, ETriggerEvent::Started, this, &ADigetherPlayerCharacter::HandleCrouchStarted);
	Input->BindAction(Data->CrouchAction, ETriggerEvent::Completed, this, &ADigetherPlayerCharacter::HandleCrouchCompleted);
	Input->BindAction(Data->AttackAction, ETriggerEvent::Started, this, &ADigetherPlayerCharacter::HandleAttackPressed);

	// 입력 바인딩이 컨트롤러 소유 이후에 만들어지는 경우를 위해 지금 컨트롤러에도 등록한다
	SetMappingContext(Controller, true);
}

void ADigetherPlayerCharacter::HandleMove(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (Controller == nullptr)
	{
		return;
	}
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Axis.X);
}

void ADigetherPlayerCharacter::HandleLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void ADigetherPlayerCharacter::HandleJumpPressed()
{
	// 앉은 상태에서는 점프가 막히므로 먼저 일어선다
	// 웅크리기 요청만 남고 아직 웅크리지 않은 상태도 점프를 막으므로 함께 푼다
	if (bIsCrouched || (GetCharacterMovement() != nullptr && GetCharacterMovement()->bWantsToCrouch))
	{
		UnCrouch();
		return;
	}
	Jump();
}

void ADigetherPlayerCharacter::HandleCrouchStarted()
{
	Crouch();
}

void ADigetherPlayerCharacter::HandleCrouchCompleted()
{
	UnCrouch();
}

void ADigetherPlayerCharacter::HandleAttackPressed()
{
	Server_Attack();
}

void ADigetherPlayerCharacter::Server_Attack_Implementation()
{
	// 서버가 최종 판정: 들고 있는 장비의 몽타주 길이 동안 다음 공격을 막는다(별도 쿨다운 수치를 코드에 두지 않는다)
	const UWorld* World = GetWorld();
	const UAnimMontage* Montage = EquippedWeapon != nullptr ? EquippedWeapon->GetAttackMontage() : nullptr;
	if (World == nullptr || Montage == nullptr)
	{
		UE_LOG(LogDigetherPlayer, Warning, TEXT("공격할 수 없다: 장비가 없거나 장비에 공격 몽타주가 없다"));
		return;
	}
	if (World->GetTimeSeconds() < NextAttackTimeSeconds)
	{
		return;
	}
	NextAttackTimeSeconds = World->GetTimeSeconds() + Montage->GetPlayLength();
	Multicast_PlayAttack(EquippedWeapon);
}

void ADigetherPlayerCharacter::Multicast_PlayAttack_Implementation(ADigetherWeaponBase* Weapon)
{
	// 화면이 없는 전용 서버는 재생하지 않는다. 본인 화면에서는 3인칭 몸이 숨겨져 있어도 몽타주는 같이 진행되고 1인칭 팔이 따라 한다
	if (GetNetMode() == NM_DedicatedServer || Weapon == nullptr)
	{
		return;
	}
	if (UAnimMontage* Montage = Weapon->GetAttackMontage())
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_Play(Montage);
		}
	}
}

void ADigetherPlayerCharacter::NotifyControllerChanged()
{
	SetMappingContext(PreviousController, false);
	Super::NotifyControllerChanged();
	SetMappingContext(Controller, true);
}
