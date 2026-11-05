#pragma once

#include "CoreMinimal.h"
#include "Game/Economy/DigetherEconomyTypes.h"
#include "DigetherDepositResponse.generated.h"

UENUM(BlueprintType)
enum class EDigetherDepositOperation : uint8
{
	// Open·Cancel은 폐기된 금액 입력 방식 값이다. BP·저장에 남을 수 있어 지우지 않는다.
	Open,
	// 붓기 한 주기가 확정됐다. AcceptedCredits = 지갑 차감, CreditedCredits = 목표 인정(본인에게만 보낸다).
	Submit,
	Cancel,
	// 값은 뒤에만 추가한다.
	PourStarted,
	// Result에 멈춘 이유를 담는다(Success = 손을 뗌).
	PourStopped,
	// 바닥 금화 줍기 결과다. AcceptedCredits = 주운 금액.
	Pickup
};

// 소유 연결의 결과 알림에만 쓴다. 레지스트리나 개인 지갑을 공개 복제하지 않는다.
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherDepositResponse
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Deposit")
	EDigetherEconomyResult Result = EDigetherEconomyResult::InvalidInput;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Deposit")
	FGuid SessionId;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Deposit")
	FName StationId;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Deposit")
	int32 PeriodGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Deposit")
	int32 AcceptedCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Deposit")
	int32 CreditedCredits = 0;

	// P1 결과 enum을 바꾸지 않고 요청 경계의 빈도 거절을 구별한다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Deposit")
	bool bRateLimited = false;
};
