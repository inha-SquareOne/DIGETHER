#pragma once

#include "CoreMinimal.h"
#include "DigetherElevatorTuning.generated.h"

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherElevatorTuning
{
	GENERATED_BODY()

	// 임시값: 기획 미정(DEV-019), 2026-11-04
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Elevator", meta=(ClampMin="0", ClampMax="1"))
	float LightMaxCapacityRatio = 0.3f;

	// 임시값: 기획 미정(DEV-019), 2026-11-04
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Elevator", meta=(ClampMin="0", ClampMax="1"))
	float MediumMaxCapacityRatio = 0.7f;

	// 임시값: 기획 미정(DEV-034), 2026-11-04
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Elevator", meta=(ClampMin="0", Units="s"))
	float UpperStopSeconds = 3.f;

	// 임시값: 기획 미정(DEV-066), 2026-11-04
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Elevator", meta=(ClampMin="0", Units="s"))
	float AscentHoldSeconds = 3.f;

	// 임시값: 기획 미정(DEV-061), 2026-11-04
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Elevator", meta=(ClampMin="0", Units="s"))
	float DepartureHoldSeconds = 3.f;

	// 보드 확정값(DEV-061)
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="Digether|Elevator")
	int32 WarningDayNumber = 4;

	// 보드 확정값(DEV-061)
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="Digether|Elevator")
	float WarningDayHoldMultiplier = 2.f;
};
