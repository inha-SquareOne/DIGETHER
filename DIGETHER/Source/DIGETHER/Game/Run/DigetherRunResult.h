#pragma once

#include "CoreMinimal.h"
#include "DigetherRunResult.generated.h"

UENUM(BlueprintType)
enum class EDigetherRunReturnOutcome : uint8
{
	Returned,
	Died,
	Missing
};

// 한 탐사의 귀환 커밋 뒤 소유 플레이어에게만 전송, 저장 파일 장부를 대체하지 않음
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherRunResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run Result")
	FGuid RunId;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run Result")
	int32 CompletedDay = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run Result")
	EDigetherRunReturnOutcome Outcome = EDigetherRunReturnOutcome::Returned;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run Result")
	int32 GeneralCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run Result")
	int32 HiddenCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run Result")
	int32 DeathLossCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run Result")
	int32 CorpseRefundCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run Result")
	int32 PayoutCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run Result")
	int32 FinalBalanceCredits = 0;

	bool IsValid() const
	{
		const bool bValidOutcome = Outcome == EDigetherRunReturnOutcome::Returned
			|| Outcome == EDigetherRunReturnOutcome::Died || Outcome == EDigetherRunReturnOutcome::Missing;
		return RunId.IsValid() && CompletedDay > 0 && bValidOutcome && GeneralCredits >= 0
			&& HiddenCredits >= 0 && DeathLossCredits >= 0 && CorpseRefundCredits >= 0
			&& PayoutCredits >= 0 && FinalBalanceCredits >= 0;
	}
};
