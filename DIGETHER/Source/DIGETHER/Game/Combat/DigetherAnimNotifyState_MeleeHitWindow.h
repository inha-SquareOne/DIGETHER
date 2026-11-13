#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "DigetherAnimNotifyState_MeleeHitWindow.generated.h"

// 몽타주에 놓는 근접 판정 구간, 이 구간 동안만 히트박스가 존재함(시작·길이는 몽타주 타임라인에서 끌어서 조정)
// 몽타주를 재생하는 모든 머신에서 불리지만 판정은 소유 액터의 UDigetherMeleeHitComponent가 서버에서만 함
// 한 몽타주에 여러 개를 두면 연타·콤보가 되고, 구간마다 DamageScale로 피해를 다르게 줄 수 있음
UCLASS(meta=(DisplayName="Digether Melee Hit Window"))
class DIGETHER_API UDigetherAnimNotifyState_MeleeHitWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	// 이 구간의 피해 배율, 장비 판정 값의 Damage에 곱함
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Digether|Melee", meta=(ClampMin="0"))
	float DamageScale = 1.f;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
