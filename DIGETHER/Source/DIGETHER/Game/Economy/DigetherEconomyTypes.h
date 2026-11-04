#pragma once

#include "CoreMinimal.h"
#include "DigetherEconomyTypes.generated.h"

UENUM(BlueprintType)
enum class EDigetherEconomyResult : uint8
{
	Success,
	InvalidAmount,
	InsufficientFunds,
	QuotaAlreadySatisfied,
	DepositClosed,
	InvalidInput,
	NotAuthority,
	DepositInputEmpty,
	DepositInputInvalidCharacter,
	DepositInputOverflow,
	StationOccupied,
	SessionAlreadyOpen,
	SessionNotFound,
	SessionOwnerMismatch,
	StationMismatch,
	PeriodMismatch,
	SessionExpired,
	DepositCreditTooSmall
};

// UObject 참조 없이 보관하므로 연결 해제 중에도 같은 경제 함수를 적용할 수 있다.
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherWalletState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 BalanceCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 LastRunPayoutCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 HiddenLifetimeCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 TotalDeathLossCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 LastPreDeathBalanceCredits = 0;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherQuotaState
{
	GENERATED_BODY()

	// 초기화 전에는 0으로 둔다. 시작할 때 MakeInitialQuotaState로 고정한 런 튜닝 스냅샷을 적용한다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 Day = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 TargetCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 DepositedCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 PeriodGeneration = 0;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherHiddenRevealEntry
{
	GENERATED_BODY()

	// 식별자 발급·안정성은 호출자가 책임진다. 온라인 모듈에 의존하지 않는다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	FString PlayerKey;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 HiddenLifetimeCredits = 0;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherQuotaPublicState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	FDigetherQuotaState Quota;

	// 게임 오버 공개를 확정한 조율자만 채운다. 그 전에는 빈 배열을 유지한다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	TArray<FDigetherHiddenRevealEntry> HiddenReveals;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherPlayerSettlementInput
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	bool bSurvived = false;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 ReturnedHiddenCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	bool bDied = false;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 PreDeathBalanceCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	bool bCorpseRecovered = false;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherPlayerSettlement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 GeneralCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 HiddenCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 PayoutCredits = 0;

	// 복구액은 런 지급액·히든 누적액과 구분한다. 회수자에게 지급하지 않는다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 CorpseRefundCredits = 0;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherRunSettlementResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	EDigetherEconomyResult Result = EDigetherEconomyResult::InvalidInput;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 GeneralShareCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 LostGeneralCredits = 0;

	// 입력 배열과 같은 순서. 실제 플레이어 식별과 중복 정산 방지는 조율자 책임이다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	TArray<FDigetherPlayerSettlement> Players;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherDeathLossResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 PreDeathBalanceCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 LossCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 RemainingBalanceCredits = 0;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherWalletUpdateResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	EDigetherEconomyResult Result = EDigetherEconomyResult::InvalidInput;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	FDigetherWalletState State;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherDepositResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	EDigetherEconomyResult Result = EDigetherEconomyResult::InvalidInput;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	FDigetherWalletState WalletState;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	FDigetherQuotaState QuotaState;

	// 효율과 무관하게 지갑에서 실제로 차감한 금액이다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 AcceptedCredits = 0;

	// 효율을 적용해 목표에 반영한 금액이며 실제 지갑 차감액과 구분한다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 CreditedCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	bool bTargetCompleted = false;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherDayAdvanceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	EDigetherEconomyResult Result = EDigetherEconomyResult::InvalidInput;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	FDigetherQuotaState State;
};
