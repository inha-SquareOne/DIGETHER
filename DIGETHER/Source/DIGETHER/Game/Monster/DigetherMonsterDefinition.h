#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DigetherMonsterDefinition.generated.h"

// 종별 고정 정의만 보관한다. 현재 체력과 생명 상태는 런타임 상태가 가진다.
UCLASS(BlueprintType, Const)
class DIGETHER_API UDigetherMonsterDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType PrimaryAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// 호출자가 수집한 문제를 보존하고 이 정의의 문제만 뒤에 추가한다.
	bool CollectProblems(TArray<FText>& OutProblems) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster")
	bool bKillable = false;

	// 승인된 종별 체력이 없으므로 기본값으로 처치 가능한 체력을 추정하지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster", meta=(ClampMin="0"))
	float MaxHealth = 0.f;
};
