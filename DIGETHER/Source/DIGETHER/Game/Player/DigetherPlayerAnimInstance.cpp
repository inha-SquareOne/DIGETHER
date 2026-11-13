#include "Game/Player/DigetherPlayerAnimInstance.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "Game/Player/DigetherPlayerCharacter.h"

const FName UDigetherPlayerAnimInstance::AttackSlotName(TEXT("Attack"));

void UDigetherPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	OwnerCharacter = Cast<ADigetherPlayerCharacter>(TryGetPawnOwner());
}

void UDigetherPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 에디터 미리보기처럼 소유 캐릭터가 없을 때는 이전 값을 그대로 둠
	const ADigetherPlayerCharacter* Character = OwnerCharacter.Get();
	if (!IsValid(Character))
	{
		OwnerCharacter = Cast<ADigetherPlayerCharacter>(TryGetPawnOwner());
		return;
	}
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (!IsValid(Movement))
	{
		return;
	}
	GroundSpeed = Movement->Velocity.Size2D();
	bIsCrouching = Character->bIsCrouched;
	const float CrouchTarget = bIsCrouching ? 1.f : 0.f;
	CrouchAlpha = CrouchBlendSeconds > 0.f
		? FMath::FInterpConstantTo(CrouchAlpha, CrouchTarget, DeltaSeconds, 1.f / CrouchBlendSeconds)
		: CrouchTarget;
	bIsFalling = Movement->IsFalling();
	bIsJumping = bIsFalling && Movement->Velocity.Z > 0.f;
	// 달리기 의도는 캐릭터가 갖고 있음(달리기 중에는 MaxWalkSpeed가 올라가므로 속도 비교로는 판정할 수 없음)
	// 제자리에서 Shift만 누른 경우는 달리기 모션을 내지 않도록 실제로 움직일 때만 봄
	bIsSprinting = Character->IsSprinting() && !Character->bIsCrouched && Movement->IsMovingOnGround() && GroundSpeed > 10.f;
	bIsMoving = GroundSpeed > UE_KINDA_SMALL_NUMBER && !Movement->GetCurrentAcceleration().IsNearlyZero();
	Velocity = Movement->Velocity;
	// 멈춰 있을 때는 방향을 0으로 둬서 정면 대기 자세가 나오게 함
	if (GroundSpeed > UE_KINDA_SMALL_NUMBER)
	{
		const float VelocityYaw = Movement->Velocity.Rotation().Yaw;
		const float ActorYaw = Character->GetActorRotation().Yaw;
		Direction = FMath::FindDeltaAngleDegrees(ActorYaw, VelocityYaw);
	}
	else
	{
		Direction = 0.f;
	}
}
