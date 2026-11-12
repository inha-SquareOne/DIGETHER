#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "DigetherPlayerAnimInstance.generated.h"

class ADigetherPlayerCharacter;

// 3인칭 몸(마네킹)의 애니메이션 상태를 캐릭터에서 읽어 ABP_DigetherPlayer(블루프린트)에 넘긴다
// 값은 모든 머신이 이미 가진 복제·예측 상태(속도, 앉음, 공중)에서 계산하므로 별도 복제가 없다
// 이동 블렌드 스페이스(BS_)는 GroundSpeed를 축으로 쓰고, 앉아 있을 때도 같은 걷기 블렌드를 쓴다
// 공격은 몽타주를 재생하며 ABP에 AttackSlotName 슬롯 노드가 있어야 보인다
UCLASS()
class DIGETHER_API UDigetherPlayerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// 바닥 평면 속도(cm/s), 블렌드 스페이스 축
	UPROPERTY(BlueprintReadOnly, Category="Digether|Anim")
	float GroundSpeed = 0.f;

	// 이동 방향(-180~180도), 몸이 바라보는 방향 기준 속도 방향, 방향별 블렌드 스페이스의 X 축
	UPROPERTY(BlueprintReadOnly, Category="Digether|Anim")
	float Direction = 0.f;

	// 앉은 상태, 앉기 전환·앉아 걷기 상태머신에 사용
	UPROPERTY(BlueprintReadOnly, Category="Digether|Anim")
	bool bIsCrouching = false;

	// 공중(점프·낙하)
	UPROPERTY(BlueprintReadOnly, Category="Digether|Anim")
	bool bIsFalling = false;

	// 점프로 상승 중(공중이면서 위로 이동), 하강은 bIsFalling && !bIsJumping으로 구분
	UPROPERTY(BlueprintReadOnly, Category="Digether|Anim")
	bool bIsJumping = false;

	// 달리기 중, 걷기와 달리기 블렌드 구분에 사용
	UPROPERTY(BlueprintReadOnly, Category="Digether|Anim")
	bool bIsSprinting = false;

	// 움직이는 중(속도가 있고 가속 중)
	UPROPERTY(BlueprintReadOnly, Category="Digether|Anim")
	bool bIsMoving = false;
	
	// 속도
	UPROPERTY(BlueprintReadOnly, Category="Digether|Anim")
	FVector Velocity = FVector::ZeroVector;

	// 공격 몽타주를 연결할 슬롯 이름, ABP 애님 그래프의 Slot 노드 이름과 같아야 한다
	static const FName AttackSlotName;

private:
	TWeakObjectPtr<ADigetherPlayerCharacter> OwnerCharacter;
};
