#pragma once

#include "CoreMinimal.h"
#include "Game/Economy/DigetherEconomyTypes.h"
#include "DigetherDepositResponse.generated.h"

UENUM()
enum class EDigetherDepositOperation : uint8
{
	Open,
	Submit,
	Cancel
};

// 소유 연결의 결과 알림에만 쓴다. 레지스트리나 개인 지갑을 공개 복제하지 않는다.
USTRUCT()
struct DIGETHER_API FDigetherDepositResponse
{
	GENERATED_BODY()

	UPROPERTY()
	EDigetherEconomyResult Result = EDigetherEconomyResult::InvalidInput;

	UPROPERTY()
	FGuid SessionId;

	UPROPERTY()
	FName StationId;

	UPROPERTY()
	int32 PeriodGeneration = 0;

	UPROPERTY()
	int32 AcceptedCredits = 0;

	UPROPERTY()
	int32 CreditedCredits = 0;

	// P1 결과 enum을 바꾸지 않고 요청 경계의 빈도 거절을 구별한다.
	UPROPERTY()
	bool bRateLimited = false;
};
