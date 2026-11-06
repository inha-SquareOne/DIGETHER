#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DigetherMonsterDefinition.generated.h"

// 종별 고정 정의만 보관, 현재 체력·생명 상태는 런타임 상태 소유
UCLASS(BlueprintType, Const)
class DIGETHER_API UDigetherMonsterDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType PrimaryAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// 호출자가 모은 문제는 보존, 이 정의의 문제만 뒤에 추가
	bool CollectProblems(TArray<FText>& OutProblems) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster")
	FText DisplayName;

	// MON_KILLABLE, 일반 피해로 사망 가능한지(종별 값)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster")
	bool bKillable = false;

	// MON_HP, 종 데이터 에셋의 종별 값, 코드 기본값으로 처치 가능 체력을 추정하지 않음
	// 임시값: 땅거미 100(기본 공격 12.5 가정 약 8회 처치, 2026-11-04 기획 답변 초안 기준)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster", meta=(ClampMin="0"))
	float MaxHealth = 0.f;
};
