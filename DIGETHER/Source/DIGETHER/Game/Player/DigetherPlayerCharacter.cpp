#include "Game/Player/DigetherPlayerCharacter.h"

#include "Animation/AnimInstance.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "Debug/DebugDrawService.h"
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
#include "Game/Combat/DigetherMeleeHitComponent.h"
#include "Game/Player/DigetherPlayerHealthComponent.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
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
		// 스태미나: 0이 허용되는 값(소모·대기)과 반드시 양수인 값(최대치·회복량)을 구분한다
		RequirePositive(TEXT("MaxStamina"), Row.MaxStamina);
		RequirePositive(TEXT("StaminaRegenPerSec"), Row.StaminaRegenPerSec);
		const auto RequireNonNegative = [&OutProblems](const TCHAR* Name, float Value)
		{
			if (Value < 0.f)
			{
				OutProblems.Add(FString::Printf(TEXT("%s는 0 이상이어야 한다(현재 %g)"), Name, Value));
			}
		};
		RequireNonNegative(TEXT("SprintDrainPerSec"), Row.SprintDrainPerSec);
		RequireNonNegative(TEXT("JumpStaminaCost"), Row.JumpStaminaCost);
		RequireNonNegative(TEXT("StaminaRegenDelaySec"), Row.StaminaRegenDelaySec);
		RequireNonNegative(TEXT("SprintResumeThreshold"), Row.SprintResumeThreshold);
		if (Row.SprintResumeThreshold > Row.MaxStamina)
		{
			OutProblems.Add(TEXT("SprintResumeThreshold는 MaxStamina 이하여야 한다"));
		}
		if (Row.JumpStaminaCost > Row.MaxStamina)
		{
			OutProblems.Add(TEXT("JumpStaminaCost는 MaxStamina 이하여야 한다"));
		}
		RequirePositive(TEXT("MaxHealth"), Row.MaxHealth);
		return OutProblems.IsEmpty();
	}
}

ADigetherPlayerCharacter::ADigetherPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	PlayerStatTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/Player/DT_PlayerStats.DT_PlayerStats")));
	// 입력 데이터 에셋 기본 위치(Git 로직 폴더), 다른 에셋을 쓰면 BP 자식 클래스에서 지정
	InputData = TSoftObjectPtr<UDigetherPlayerInputData>(FSoftObjectPath(TEXT("/Game/Game/Input/Player/DA_PlayerInput.DA_PlayerInput")));
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
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BodyMeshAsset(TEXT("/Game/Characters/DwarfV15/SK_Ch_v15_Body.SK_Ch_v15_Body"));
	if (BodyMeshAsset.Succeeded())
	{
		BodyMesh->SetSkeletalMesh(BodyMeshAsset.Object);
	}
	static ConstructorHelpers::FClassFinder<UAnimInstance> BodyAnimAsset(TEXT("/Game/Characters/DwarfV15/Anims/ABP_UnarmedV15.ABP_UnarmedV15_C"));
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

	MeleeHit = CreateDefaultSubobject<UDigetherMeleeHitComponent>(TEXT("MeleeHit"));
	Health = CreateDefaultSubobject<UDigetherPlayerHealthComponent>(TEXT("Health"));

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

	// 사망·부활 표현은 모든 머신에서, 피해 표시는 소유 화면에서 쓴다
	Health->OnDied.AddUObject(this, &ADigetherPlayerCharacter::HandleDied);
	Health->OnRevived.AddUObject(this, &ADigetherPlayerCharacter::HandleRevived);
	Health->OnHealthChanged.AddUObject(this, &ADigetherPlayerCharacter::HandleHealthChanged);
	// 이미 죽은 채로 복제돼 들어온 경우(늦은 합류): 컴포넌트 BeginPlay 통지가 구독 전에 끝났을 수 있어 지금 반영
	if (Health->IsDead())
	{
		HandleDied();
	}

	// 장비는 서버가 만들어 복제한다
	if (HasAuthority() && DefaultWeaponClass != nullptr && EquippedWeapon == nullptr)
	{
		EquipWeapon(DefaultWeaponClass);
	}
	// 1인칭 팔은 화면 표현이라 화면이 없는 전용 서버에서는 건너뛴다
	if (GetNetMode() != NM_DedicatedServer)
	{
		RefreshVisuals();
#if !UE_BUILD_SHIPPING
		// 창(뷰포트)마다 호출되는 그리기 서비스에 등록, 호출 때 자기 컨트롤러의 창인지 걸러낸다
		StatusDebugDrawHandle = UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateUObject(this, &ADigetherPlayerCharacter::DrawStatusDebug));
#endif
	}
}

void ADigetherPlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADigetherPlayerCharacter, EquippedWeapon);
	// 달리기 여부는 소유자 제외 모두에게(소유자는 로컬 예측 + Client_SprintStopped), 스태미나 수치는 소유자에게만 보낸다
	DOREPLIFETIME_CONDITION(ADigetherPlayerCharacter, bIsSprinting, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(ADigetherPlayerCharacter, ReplicatedStamina, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ADigetherPlayerCharacter, bReplicatedSprintLocked, COND_OwnerOnly);
}

namespace
{
	// 소유 클라이언트 예측값이 서버 값과 이만큼 이상 벌어졌을 때만 서버 값을 따른다
	// 서버 값은 지연만큼 과거 값이라 달리기 중에는 소모량 x 지연 정도의 차이가 정상이다(소모 20/s, 지연 0.2s면 약 4)
	constexpr float StaminaSnapTolerance = 8.f;
}

void ADigetherPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickStamina(DeltaSeconds);
}

void ADigetherPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
#if !UE_BUILD_SHIPPING
	if (StatusDebugDrawHandle.IsValid())
	{
		UDebugDrawService::Unregister(StatusDebugDrawHandle);
		StatusDebugDrawHandle.Reset();
	}
#endif
	Super::EndPlay(EndPlayReason);
}

#if !UE_BUILD_SHIPPING
namespace
{
	// 임시 확인용 체력·스태미나 표시, 정식 HUD(P2-7)가 생기면 제거한다
	TAutoConsoleVariable<int32> CVarStaminaDebug(
		TEXT("Digether.Player.StaminaDebug"), 1,
		TEXT("1이면 직접 조종하는 플레이어 화면에 임시 스태미나 표시를 그린다"),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarHealthDebug(
		TEXT("Digether.Player.HealthDebug"), 1,
		TEXT("1이면 직접 조종하는 플레이어 화면에 임시 체력 표시를 그린다"),
		ECVF_Default);

	// 0~1 비율을 문자 막대로
	FString MakeTextBar(float Fraction)
	{
		constexpr int32 BarLength = 20;
		const int32 Filled = FMath::RoundToInt(FMath::Clamp(Fraction, 0.f, 1.f) * BarLength);
		FString Bar;
		for (int32 Index = 0; Index < BarLength; ++Index)
		{
			Bar += Index < Filled ? TEXT("|") : TEXT(".");
		}
		return Bar;
	}

	// 피해를 받은 뒤 체력 줄을 흰색으로 강조하는 시간(초), 화면 확인용
	constexpr double DamageFlashSeconds = 0.6;
}

void ADigetherPlayerCharacter::DrawStatusDebug(UCanvas* Canvas, APlayerController* ViewportController)
{
	// 같은 프로세스의 PIE 창마다 호출되므로, 이 캐릭터를 조종하는 컨트롤러의 창에서만 그린다
	// (엔진 전역 화면 메시지를 쓰면 모든 창이 한 줄을 공유해 다른 플레이어 값이 보인다)
	if (Canvas == nullptr || ViewportController == nullptr || ViewportController != GetController() || !IsLocallyControlled())
	{
		return;
	}
	constexpr float Left = 20.f;
	constexpr float TextScale = 1.6f;
	float Y = 20.f;

	if (CVarHealthDebug.GetValueOnGameThread() != 0 && Health != nullptr && Health->IsReady())
	{
		const float Fraction = Health->GetHealthFraction();
		const bool bFlash = LastDamagedRealTimeSeconds >= 0.0 && FPlatformTime::Seconds() - LastDamagedRealTimeSeconds < DamageFlashSeconds;
		FColor Color = Fraction > 0.5f ? FColor::Green : (Fraction > 0.25f ? FColor::Orange : FColor::Red);
		if (bFlash)
		{
			Color = FColor::White;
		}
		const TCHAR* Status = Health->IsDead() ? TEXT("  [사망]") : TEXT("");
		Canvas->SetDrawColor(Color);
		Canvas->DrawText(GEngine->GetSmallFont(),
			FString::Printf(TEXT("체력     [%s] %.0f / %.0f%s"), *MakeTextBar(Fraction), Health->GetCurrentHealth(), Health->GetMaxHealth(), Status),
			Left, Y, TextScale, TextScale);
		Y += 30.f;
	}

	if (CVarStaminaDebug.GetValueOnGameThread() != 0 && bStaminaReady)
	{
		const TCHAR* Status = StaminaState.bSprintLocked ? TEXT("  [잠김: 회복 중]") : (bIsSprinting ? TEXT("  [달리기]") : TEXT(""));
		// 색은 상태에 따라 노랑/빨강, 이 창의 캔버스에만 그린다
		Canvas->SetDrawColor(StaminaState.bSprintLocked ? FColor::Red : FColor::Yellow);
		Canvas->DrawText(GEngine->GetSmallFont(),
			FString::Printf(TEXT("스태미나 [%s] %.0f / %.0f%s"), *MakeTextBar(GetStaminaFraction()), StaminaState.Current, StaminaParams.MaxStamina, Status),
			Left, Y, TextScale, TextScale);
	}
}
#endif

void ADigetherPlayerCharacter::TickStamina(float DeltaSeconds)
{
	// 서버와 직접 조종하는 클라이언트만 계산한다(다른 사람의 스태미나는 알 필요가 없다)
	if (!bStaminaReady || !(HasAuthority() || IsLocallyControlled()))
	{
		return;
	}

	// 웅크리면 달리기를 끝낸다
	if (bIsSprinting && bIsCrouched)
	{
		SetSprintingState(false);
		if (HasAuthority() && !IsLocallyControlled())
		{
			Client_SprintStopped();
		}
	}

	// 소모는 실제로 수평 이동 중일 때만 한다(제자리에서 Shift만 누르고 있으면 회복)
	constexpr float MovingSpeedThreshold = 10.f;
	const bool bMoving = GetVelocity().SizeSquared2D() > FMath::Square(MovingSpeedThreshold);
	const bool bDraining = bIsSprinting && bMoving;
	const bool bCanContinue = FDigetherStaminaRules::Advance(StaminaState, StaminaParams, DeltaSeconds, bDraining);
	if (!bCanContinue && bIsSprinting)
	{
		// 바닥났다: 달리기 종료
		SetSprintingState(false);
		if (HasAuthority() && !IsLocallyControlled())
		{
			Client_SprintStopped();
		}
	}

	if (HasAuthority())
	{
		ReplicatedStamina = StaminaState.Current;
		bReplicatedSprintLocked = StaminaState.bSprintLocked;
	}
}

void ADigetherPlayerCharacter::ResetStamina()
{
	if (!bStaminaReady)
	{
		return;
	}
	StaminaState = FDigetherStaminaRules::MakeFull(StaminaParams);
	SetSprintingState(false);
	if (HasAuthority())
	{
		ReplicatedStamina = StaminaState.Current;
		bReplicatedSprintLocked = StaminaState.bSprintLocked;
		if (!IsLocallyControlled())
		{
			Client_SprintStopped();
		}
	}
}

void ADigetherPlayerCharacter::OnRep_Stamina()
{
	// 직접 조종하는 클라이언트만 받는다. 크게 벗어났을 때만 서버 값을 따르고, 잠금 여부는 항상 서버를 따른다
	if (!bStaminaReady)
	{
		return;
	}
	if (FMath::Abs(StaminaState.Current - ReplicatedStamina) > StaminaSnapTolerance)
	{
		StaminaState.Current = ReplicatedStamina;
	}
	StaminaState.bSprintLocked = bReplicatedSprintLocked;
	if (StaminaState.bSprintLocked && bIsSprinting)
	{
		SetSprintingState(false);
	}
}

void ADigetherPlayerCharacter::OnRep_IsSprinting()
{
	// 다른 플레이어 화면: 달리기 속도 반영(애니메이션은 IsSprinting을 읽는다)
	RefreshMaxWalkSpeed();
}

void ADigetherPlayerCharacter::SetSprintingState(bool bSprint)
{
	bIsSprinting = bSprint;
	RefreshMaxWalkSpeed();
}

void ADigetherPlayerCharacter::RefreshMaxWalkSpeed()
{
	if (!bPlayerStatsApplied)
	{
		return;
	}
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		// 웅크리기 속도는 MaxWalkSpeedCrouched가 따로 있으므로 서 있을 때만 달리기 속도를 쓴다
		Movement->MaxWalkSpeed = (bIsSprinting && !bIsCrouched) ? SprintSpeedCmPerSec : WalkSpeedCmPerSec;
	}
}

bool ADigetherPlayerCharacter::CanStartSprint() const
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	return !IsDead() && bStaminaReady && !bIsCrouched && Movement != nullptr && Movement->IsMovingOnGround() && FDigetherStaminaRules::CanSprint(StaminaState);
}

void ADigetherPlayerCharacter::Server_SetSprinting_Implementation(bool bSprint)
{
	if (bSprint && !CanStartSprint())
	{
		// 서버가 거절: 소유 클라이언트가 이미 켰을 수 있으므로 되돌리라고 알린다
		SetSprintingState(false);
		if (!IsLocallyControlled())
		{
			Client_SprintStopped();
		}
		return;
	}
	SetSprintingState(bSprint);
}

void ADigetherPlayerCharacter::Client_SprintStopped_Implementation()
{
	SetSprintingState(false);
}

void ADigetherPlayerCharacter::HandleSprintStarted()
{
	// 로컬에서 먼저 예측해 켜고 서버에 알린다. 서버가 거절하면 Client_SprintStopped로 돌아온다
	if (!CanStartSprint())
	{
		return;
	}
	SetSprintingState(true);
	Server_SetSprinting(true);
}

void ADigetherPlayerCharacter::HandleSprintCompleted()
{
	if (!bIsSprinting)
	{
		return;
	}
	SetSprintingState(false);
	Server_SetSprinting(false);
}

bool ADigetherPlayerCharacter::CanJumpInternal_Implementation() const
{
	if (!Super::CanJumpInternal_Implementation() || IsDead())
	{
		return false;
	}
	// 표가 적용되지 않았으면 스태미나 규칙을 걸지 않는다(표 오류는 로그로 이미 알림)
	return !bStaminaReady || FDigetherStaminaRules::CanJump(StaminaState, StaminaParams);
}

void ADigetherPlayerCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();
	// 점프 1회에 한 번만 소모한다. 서버와 직접 조종하는 클라이언트가 각자 같은 규칙으로 소모한다
	if (bStaminaReady && (HasAuthority() || IsLocallyControlled()))
	{
		FDigetherStaminaRules::TryConsumeJump(StaminaState, StaminaParams);
	}
}

void ADigetherPlayerCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	UpdateEyeHeight();
}

void ADigetherPlayerCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	UpdateEyeHeight();
}

void ADigetherPlayerCharacter::UpdateEyeHeight()
{
	// 스프링암은 캡슐 중심 기준(액터 스케일 적용 전)이므로 월드 거리를 스케일로 나눠 넣는다
	const float ScaleZ = FMath::Max(GetActorScale3D().Z, KINDA_SMALL_NUMBER);
	const float EyeZ = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - EyeDepthFromTopCm / ScaleZ;
	FVector ArmLocation = CameraSpringArm->GetRelativeLocation();
	ArmLocation.Z = EyeZ;
	CameraSpringArm->SetRelativeLocation(ArmLocation);
	// 시점 기준 트레이스(GetPawnViewLocation 등)도 같은 눈높이를 쓰게 한다
	BaseEyeHeight = EyeZ * ScaleZ;
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
	// 판정 값은 서버만 쓰므로 서버에서만 넣는다
	if (EquippedWeapon != nullptr)
	{
		MeleeHit->SetHitSource(EquippedWeapon->GetMeleeHitParams(), EquippedWeapon);
	}
	else
	{
		MeleeHit->ClearHitSource();
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

	// 몸 키를 캡슐 높이(통과 높이)에 맞춰 균등 확대·축소하고, 발바닥을 캡슐 바닥에 붙인다
	// 기준 자세 바운드로 재므로 메시 원점이 발바닥이 아니어도 맞는다
	if (const USkeletalMesh* BodyAsset = GetMesh()->GetSkeletalMeshAsset())
	{
		const FBoxSphereBounds MeshBounds = BodyAsset->GetImportedBounds();
		const float MeshHeight = MeshBounds.BoxExtent.Z * 2.f;
		const float MeshScale = MeshHeight > KINDA_SMALL_NUMBER ? CapsuleHalfHeight * 2.f / MeshHeight : 1.f;
		const float MeshBottomZ = MeshBounds.Origin.Z - MeshBounds.BoxExtent.Z;
		GetMesh()->SetRelativeScale3D(FVector(MeshScale));
		GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -CapsuleHalfHeight - MeshBottomZ * MeshScale));
	}
	else
	{
		GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -CapsuleHalfHeight));
	}
	// ACharacter가 PostInitializeComponents에서 저장한 기본 메시 위치로 이동 보정이 매 틱 되돌리므로, 새 위치를 다시 저장한다
	CacheInitialMeshOffset(GetMesh()->GetRelativeLocation(), GetMesh()->GetRelativeRotation());
	UpdateEyeHeight();
	RefreshVisuals();

	SprintSpeedCmPerSec = Row->SprintSpeed;
	WalkSpeedCmPerSec = Row->WalkSpeed;
	StaminaParams = FDigetherStaminaParams::FromRow(*Row);
	StaminaState = FDigetherStaminaRules::MakeFull(StaminaParams);
	ReplicatedStamina = StaminaState.Current;
	bStaminaReady = true;
	Health->InitializeFromMaxHealth(Row->MaxHealth);
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
	Input->BindAction(Data->SprintAction, ETriggerEvent::Started, this, &ADigetherPlayerCharacter::HandleSprintStarted);
	Input->BindAction(Data->SprintAction, ETriggerEvent::Completed, this, &ADigetherPlayerCharacter::HandleSprintCompleted);

	// 입력 바인딩이 컨트롤러 소유 이후에 만들어지는 경우를 위해 지금 컨트롤러에도 등록한다
	SetMappingContext(Controller, true);
}

void ADigetherPlayerCharacter::HandleMove(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (Controller == nullptr || IsDead())
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
	if (IsDead())
	{
		return;
	}
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
	if (IsDead())
	{
		return;
	}
	// 웅크리면 달리기를 끝낸다(서버도 TickStamina에서 웅크림 상태를 보고 끝낸다)
	HandleSprintCompleted();
	Crouch();
}

void ADigetherPlayerCharacter::HandleCrouchCompleted()
{
	UnCrouch();
}

void ADigetherPlayerCharacter::HandleAttackPressed()
{
	if (IsDead())
	{
		return;
	}
	Server_Attack();
}

void ADigetherPlayerCharacter::Server_Attack_Implementation()
{
	// 서버가 최종 판정: 들고 있는 장비의 몽타주 길이 동안 다음 공격을 막는다(별도 쿨다운 수치를 코드에 두지 않는다)
	const UWorld* World = GetWorld();
	const UAnimMontage* Montage = EquippedWeapon != nullptr ? EquippedWeapon->GetAttackMontage() : nullptr;
	if (IsDead())
	{
		return;
	}
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
	// 전용 서버도 재생한다: 몽타주의 판정 구간 노티파이가 서버에서 불려야 히트박스 판정이 된다(몸 메시는 항상 포즈를 계산한다)
	// 본인 화면에서는 3인칭 몸이 숨겨져 있어도 몽타주는 같이 진행되고 1인칭 팔이 따라 한다
	if (Weapon == nullptr)
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

bool ADigetherPlayerCharacter::IsDead() const
{
	return Health != nullptr && Health->IsDead();
}

void ADigetherPlayerCharacter::HandleHealthChanged(float PreviousHealth, float CurrentHealth)
{
	if (CurrentHealth < PreviousHealth && IsLocallyControlled())
	{
		LastDamagedRealTimeSeconds = FPlatformTime::Seconds();
	}
}

void ADigetherPlayerCharacter::HandleDied()
{
	// 모든 머신: 달리기·웅크리기를 끝내고 이동을 멈춘다(MOVE_None)
	SetSprintingState(false);
	UnCrouch();
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
	// 진행 중인 공격 몽타주를 끊는다(판정 구간도 노티파이 종료로 닫힌다)
	if (UAnimInstance* AnimInstance = GetMesh() != nullptr ? GetMesh()->GetAnimInstance() : nullptr)
	{
		AnimInstance->StopAllMontages(0.1f);
	}

	if (!HasAuthority())
	{
		return;
	}
	// 정식 GameMode(PlayerState에 생존 상태 컴포넌트가 있는 경우)에만 사망을 알린다. 탈출과 먼저 승인된 쪽만 확정된다
	// 드롭·지갑 손실·관전 전환은 런 흐름 담당이며 아직 이 사망 경로에 연결되지 않았다
	APlayerState* OwnerPlayerState = GetPlayerState();
	UDigetherLifeStateComponent* Life = OwnerPlayerState != nullptr ? OwnerPlayerState->FindComponentByClass<UDigetherLifeStateComponent>() : nullptr;
	if (Life == nullptr)
	{
		UE_LOG(LogDigetherPlayer, Log, TEXT("%s 사망: PlayerState에 생존 상태가 없어 캐릭터 정지만 한다(시험 맵)"), *GetName());
		return;
	}
	const EDigetherLifeResult Result = Life->ApplyDeath(EDigetherDeathCause::Killed);
	UE_LOG(LogDigetherPlayer, Log, TEXT("%s 사망: 생존 상태 통지 결과 %s"), *GetName(), *UEnum::GetValueAsString(Result));
}

void ADigetherPlayerCharacter::HandleRevived()
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}
	ResetStamina();
}

void ADigetherPlayerCharacter::DigetherDamageSelf(float Amount)
{
	Server_DevDamageSelf(Amount);
}

void ADigetherPlayerCharacter::DigetherResetHealth()
{
	Server_DevResetHealth();
}

void ADigetherPlayerCharacter::Server_DevDamageSelf_Implementation(float Amount)
{
#if !UE_BUILD_SHIPPING
	// 실제 피해와 같은 경로(엔진 ApplyDamage → 체력 컴포넌트)를 탄다
	UGameplayStatics::ApplyDamage(this, Amount, GetController(), this, UDamageType::StaticClass());
#endif
}

void ADigetherPlayerCharacter::Server_DevResetHealth_Implementation()
{
#if !UE_BUILD_SHIPPING
	// 생존 상태(PlayerState) 부활은 런 흐름 책임이라 여기서는 캐릭터 체력만 되돌린다
	Health->ResetToFull();
#endif
}
