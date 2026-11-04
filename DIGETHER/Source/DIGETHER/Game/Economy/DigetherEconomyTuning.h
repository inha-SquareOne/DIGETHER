#pragma once

#include "CoreMinimal.h"
#include "DigetherEconomyTuning.generated.h"

// 서버가 소유하고 계산 함수에 전달하는 중앙 기본값이다. 최종 기획 확정 시 임시 필드를 갱신한다.
// 사망 손실률·시체 복구율·첫 목표는 런 시작 때 고정한 FDigetherRunTuning 스냅샷에서 받는다.
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherEconomyTuning
{
	GENERATED_BODY()

	// 임시값: 기획 미정(DEV-025), 2026-11-04
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="1", ClampMax="100"))
	int32 DayOneEfficiencyPercent = 70;

	// 임시값: 기획 미정(DEV-025), 2026-11-04
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="1", ClampMax="100"))
	int32 DayTwoEfficiencyPercent = 80;

	// 임시값: 기획 미정(DEV-025), 2026-11-04
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="1", ClampMax="100"))
	int32 DayThreeEfficiencyPercent = 90;

	// 보드 확정값(DEV-025)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="1", ClampMax="100"))
	int32 DayFourEfficiencyPercent = 100;

	// 임시값: 기획 미정(DEV-022), 2026-11-04
	// 프로필 InitialQuotaCredits의 주석과 달리 첫 목표만 런 스냅샷에서 받고, 증가량은 이 경제 임시값을 쓴다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="0"))
	int32 TargetIncreaseCredits = 100;

	// 임시값: 기획 미정(DEV-025), 2026-11-04
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="0.0"))
	double DepositSessionLifetimeSeconds = 10.0;
};
