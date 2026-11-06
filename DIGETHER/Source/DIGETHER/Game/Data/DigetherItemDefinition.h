#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DigetherItemDefinition.generated.h"

// 아이템 종류 하나의 고정값, 위치·소지자 등 런타임 상태는 인스턴스 소유
UCLASS(BlueprintType, Const)
class DIGETHER_API UDigetherItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Asset Manager 등록(DefaultGame.ini) 이름과 일치 필요
	static const FPrimaryAssetType PrimaryAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	bool IsCargo() const;
	bool IsEquipment() const;
	bool HasTrait(const FGameplayTag& Trait) const;

	// 저장값 검사, 에디터 DataValidation과 자동 테스트 공용
	bool CollectProblems(TArray<FText>& OutProblems) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Item", meta=(Categories="Item.Type"))
	FGameplayTag ItemType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Item", meta=(Categories="Item.Trait"))
	FGameplayTagContainer Traits;

	// 정산 가치, 장비는 정산 제외라 0
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Item", meta=(ClampMin="0"))
	int32 ValueCredits = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Item", meta=(ClampMin="0"))
	float WeightKg = 0.f;
};
