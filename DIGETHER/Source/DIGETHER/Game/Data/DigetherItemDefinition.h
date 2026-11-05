#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DigetherItemDefinition.generated.h"

// 아이템 종류 하나의 고정값. 위치·소지자 같은 런타임 상태는 인스턴스가 가진다.
UCLASS(BlueprintType, Const)
class DIGETHER_API UDigetherItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Asset Manager 등록(DefaultGame.ini)과 같은 이름이어야 한다.
	static const FPrimaryAssetType PrimaryAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	bool IsCargo() const;
	bool IsEquipment() const;
	bool HasTrait(const FGameplayTag& Trait) const;

	// 저장값 검사. 에디터 DataValidation과 자동 테스트가 함께 쓴다.
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

	// 정산 가치. 장비는 정산하지 않으므로 0이어야 한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Item", meta=(ClampMin="0"))
	int32 ValueCredits = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Item", meta=(ClampMin="0"))
	float WeightKg = 0.f;
};
