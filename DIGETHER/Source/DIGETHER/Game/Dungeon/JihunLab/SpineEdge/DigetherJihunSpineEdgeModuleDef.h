#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DigetherJihunSpineEdgeTypes.h"
#include "DigetherJihunSpineEdgeModuleDef.generated.h"

// 기획 모듈 umap 하나의 생성용 정의
// 시험 단계라 Primary Data Asset 대신 UDataAsset(공유 Asset Manager 설정 변경 회피), 본편 이관 시 전환
UCLASS(BlueprintType)
class DIGETHER_API UDigetherJihunSpineEdgeModuleDef : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Digether|SpineEdge")
	FDigetherJihunSpineEdgeModuleSpec Spec;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
