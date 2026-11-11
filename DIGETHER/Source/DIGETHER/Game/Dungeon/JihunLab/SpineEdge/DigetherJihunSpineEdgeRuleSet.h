#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DigetherJihunSpineEdgeTypes.h"
#include "DigetherJihunSpineEdgeRuleSet.generated.h"

class UDigetherJihunSpineEdgeModuleDef;

// 생성 규칙 수치와 사용할 모듈 목록. 생성기 입력(Catalog·Rules)으로 변환해 씀
UCLASS(BlueprintType)
class DIGETHER_API UDigetherJihunSpineEdgeRuleSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Digether|SpineEdge")
	FDigetherJihunSpineEdgeRules Rules;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Digether|SpineEdge")
	TArray<TObjectPtr<UDigetherJihunSpineEdgeModuleDef>> Modules;

	// 비어 있는 항목은 건너뜀. 결과 배열 인덱스 = Placement.ModuleIndex
	TArray<FDigetherJihunSpineEdgeModuleSpec> BuildCatalog() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
