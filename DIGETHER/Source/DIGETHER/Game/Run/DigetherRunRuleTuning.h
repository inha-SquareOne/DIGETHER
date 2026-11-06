#pragma once

#include "CoreMinimal.h"
#include "DigetherRunRuleTuning.generated.h"

// 여진 전용 임시값 모음, 런 스냅샷의 제한 시간·상승 페널티와 중복 저장 금지
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherRunRuleTuning
{
	GENERATED_BODY()

	// 임시값: 기획 미정(DEV-020), 2026-11-04
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Run", meta=(ClampMin="1"))
	int32 AftershockStageCount = 3;

	// 임시값: 기획 미정(DEV-020), 2026-11-04
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Run", meta=(ClampMin="0.0", Units="s"))
	double AftershockMaxIntervalSeconds = 30.0;

	// 임시값: 기획 미정(DEV-020), 2026-11-04
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Run", meta=(ClampMin="0.0", Units="s"))
	double AftershockMinIntervalSeconds = 5.0;
};
