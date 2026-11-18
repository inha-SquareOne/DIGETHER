#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DigetherJihunGridGraphTypes.h"
#include "DigetherJihunGridGraphRuleSet.generated.h"

class UDigetherJihunGridGraphModuleDef;

// 생성 규칙 수치와 사용할 모듈 목록. 생성기 입력(Catalog·Rules)으로 변환해 씀
UCLASS(BlueprintType)
class DIGETHER_API UDigetherJihunGridGraphRuleSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Digether|GridGraph")
	FDigetherJihunGridGraphRules Rules;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Digether|GridGraph")
	TArray<TObjectPtr<UDigetherJihunGridGraphModuleDef>> Modules;

	// 비어 있는 항목은 건너뜀. 결과 배열 인덱스 = Placement.ModuleIndex. Modules가 비면 Builder가 실측 카탈로그 사용
	TArray<FDigetherJihunGridGraphModuleSpec> BuildCatalog() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
