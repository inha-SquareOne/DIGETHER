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

	// --- 화면 위쪽: 탐사 공통 값(런 플로우 9.5), 다른 사람 금액은 담지 않음 ---
	// 이번 탐사 파밍액, 유효 일반 회수 총액
	UPROPERTY(BlueprintReadOnly, Category="Digether|Run Result")
	int32 FarmingCredits = 0;

	// 사망자·실종자 이름은 타인 상세 비공개의 예외, 표시 이름만
	UPROPERTY(BlueprintReadOnly, Category="Digether|Run Result")
	TArray<FString> DeadPlayerNames;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run Result")
	TArray<FString> MissingPlayerNames;

	// 파밍액 - 생존자에게 지급한 일반 몫 합계
	UPROPERTY(BlueprintReadOnly, Category="Digether|Run Result")
	int32 ExcludedCredits = 0;

	// --- 화면 아래쪽: 본인 값 ---
	UPROPERTY(BlueprintReadOnly, Category="Digether|Run Result")
	int32 PreviousBalanceCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run Result")
	int32 GeneralCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run Result")
	int32 HiddenCredits = 0;

	// 실제 차감액, 비율은 시체 유효 회수 여부로 정산 때 확정(복구금 없음)
	UPROPERTY(BlueprintReadOnly, Category="Digether|Run Result")
	int32 DeathLossCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run Result")
	int32 DeathLossPercent = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run Result")
	bool bCorpseRecovered = false;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run Result")
	int32 PayoutCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run Result")
	int32 FinalBalanceCredits = 0;

	bool IsValid() const
	{
		const bool bValidOutcome = Outcome == EDigetherRunReturnOutcome::Returned
			|| Outcome == EDigetherRunReturnOutcome::Died || Outcome == EDigetherRunReturnOutcome::Missing;
		const bool bDied = Outcome != EDigetherRunReturnOutcome::Returned;
		return RunId.IsValid() && CompletedDay > 0 && bValidOutcome && FarmingCredits >= 0
			&& ExcludedCredits >= 0 && ExcludedCredits <= FarmingCredits && PreviousBalanceCredits >= 0
			&& GeneralCredits >= 0 && HiddenCredits >= 0 && DeathLossCredits >= 0
			&& DeathLossPercent >= 0 && DeathLossPercent <= 100 && (bDied || !bCorpseRecovered)
			&& PayoutCredits >= 0 && FinalBalanceCredits >= 0;
	}
};
