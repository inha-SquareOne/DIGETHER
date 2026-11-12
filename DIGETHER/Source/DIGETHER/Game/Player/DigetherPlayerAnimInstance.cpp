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

	// 에디터 미리보기처럼 소유 캐릭터가 없을 때는 이전 값을 그대로 둔다
	const ADigetherPlayerCharacter* Character = OwnerCharacter.Get();
	if (Character == nullptr)
	{
		OwnerCharacter = Cast<ADigetherPlayerCharacter>(TryGetPawnOwner());
		return;
	}
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (Movement == nullptr)
	{
		return;
	}
	GroundSpeed = Movement->Velocity.Size2D();
	bIsCrouching = Character->bIsCrouched;
	bIsFalling = Movement->IsFalling();
	bIsJumping = bIsFalling && Movement->Velocity.Z > 0.f;
	// 달리기 입력이 없으므로 걷기 최고 속도를 넘을 때만 달리기로 본다
	bIsSprinting = !Character->bIsCrouched && Movement->IsMovingOnGround() && GroundSpeed > Movement->MaxWalkSpeed + 1.f;
	bIsMoving = GroundSpeed > UE_KINDA_SMALL_NUMBER && !Movement->GetCurrentAcceleration().IsNearlyZero();
	Velocity = Movement->Velocity;
	// 멈춰 있을 때는 방향을 0으로 둬서 정면 대기 자세가 나오게 한다
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
