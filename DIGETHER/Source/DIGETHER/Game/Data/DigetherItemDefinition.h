#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DigetherItemDefinition.generated.h"

class ADigetherWeaponBase;
class UStaticMesh;

// 아이템 종류 하나의 고정값, 위치·소지자 등 런타임 상태는 인스턴스 소유
UCLASS(BlueprintType, Const)
class DIGETHER_API UDigetherItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Asset Manager 등록(DefaultGame.ini) 이름과 일치 필요
	static const FPrimaryAssetType PrimaryAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// 정의 이름(DefinitionId)으로 에셋을 찾아 불러옴, Asset Manager에 등록된 폴더에 없으면 nullptr
	static const UDigetherItemDefinition* FindDefinition(FName DefinitionId);

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

	// 바닥에 놓였을 때 보이는 메시, 비우면 월드 아이템이 기본 도형(상자)으로 대신함
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Item")
	TSoftObjectPtr<UStaticMesh> WorldMesh;

	// 바닥에 놓였을 때 WorldMesh 배율, 원본 메시 크기가 게임 기준과 다를 때 맞춤(손에 든 크기는 장비 BP에서 따로 정함)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Item", meta=(ClampMin="0.01"))
	float WorldMeshScale = 1.f;

	// 손에 들었을 때 생성할 장비 클래스(BP_Weapon_*), 비우면 들어도 손에 보이지 않음(빈손과 같음)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Item")
	TSoftClassPtr<ADigetherWeaponBase> EquipmentClass;
};
