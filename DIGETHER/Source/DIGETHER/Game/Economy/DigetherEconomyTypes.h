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
	DepositCreditTooSmall,
	// 값 순서는 저장·BP에 남을 수 있어 바꾸지 않고 뒤에만 추가한다.
	// 붓기 중 거리·투입구 방향·가림·생존·연결 조건이 어긋났다.
	PourInteractionLost,
	// 바닥 금화를 이미 누가 주웠거나 사라졌다.
	FloorCoinUnavailable
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

	// 기획 S33 결과 통계: 이번 게임에서 실제로 지급받은 일반 몫 + 히든 수익의 누적이다. 정산 때마다 더하며
	// 구매·납부·사망으로 줄지 않는다. 시작 지급·시체 복구금·개발 지급은 넣지 않는다. 새 게임은 0이다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 LifetimeEarnedCredits = 0;

	// 기획 S33: 시체 회수로 실제로 돌려받은 금액의 누적이다. 사망 손실 순액(TotalDeathLossCredits - 이 값)에만 쓴다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 LifetimeCorpseRefundCredits = 0;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherQuotaState
{
	GENERATED_BODY()

	// 기간 일차(현재 할당량 기준 1~4일차, 4일차 탐사가 끝난 마감 다음 날은 5)다. 납부 효율·마감·실패 판정은 이 값을 쓴다.
	// 기존 BP 핀·저장 형식 호환을 위해 이름은 Day로 둔다(기획 1.1.0 02 개요 5행·공유 사양 "탐사·기간").
	// 초기화 전에는 0으로 둔다. 시작할 때 MakeInitialQuotaState로 고정한 런 튜닝 스냅샷을 적용한다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 Day = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 TargetCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 DepositedCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 PeriodGeneration = 0;

	// 전체 일차(게임 시작부터 센 날)다. 새 게임은 1이고, 탐사 종료가 확정될 때 기간 일차와 함께 1 오른다(AdvanceDay).
	// 목표 달성으로 새 기간이 시작되면 기간 일차만 1로 돌아가고 이 값은 그대로다. 초기화 전에는 0이다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 TotalDay = 0;
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

// 정산기 외부 3색 신호기 단계다. 공동 인정액 숫자 대신 이 단계만 클라이언트에 공개한다.
UENUM(BlueprintType)
enum class EDigetherPaymentSignal : uint8
{
	// 목표가 아직 없다(초기화 전).
	None,
	Red,
	Orange,
	Green
};

// 클라이언트로 복제하는 할당량 공개 값이다. 정확한 누적 인정액(DepositedCredits)은 서버 판정·저장에만 두고
// 여기에는 넣지 않는다(기획: 화면에는 정확한 누적 금액 대신 신호기 색만 보낸다).
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherQuotaReplicatedState
{
	GENERATED_BODY()

	// 기간 일차다(FDigetherQuotaState::Day와 같은 의미).
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 Day = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 TargetCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 PeriodGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	EDigetherPaymentSignal PaymentSignal = EDigetherPaymentSignal::None;

	// 전체 일차다. 기간 일차와 같이 모든 참가자에게 공개한다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 TotalDay = 0;

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

// 붓기 한 주기의 값 계산 결과다. 실패하면 지갑·목표는 입력 그대로이고 금액은 모두 0이다.
// 기획 S24·PQ-34 안 A(임시, D27 확정 전): 지갑 차감 S, 목표 인정 C, 목표를 채우는 데 쓴 금액 N, 바닥 금화 F = S - N.
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherPourTickResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	EDigetherEconomyResult Result = EDigetherEconomyResult::InvalidInput;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	FDigetherWalletState WalletState;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	FDigetherQuotaState QuotaState;

	// 지갑에서 실제로 뺀 금액(S)이다. 바닥 금화로 떨어진 몫도 포함한다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 SpentCredits = 0;

	// 그 순간 일차 효율로 목표에 인정한 금액(C)이다. 남은 목표를 넘지 않는다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 CreditedCredits = 0;

	// S 가운데 목표 인정에 쓰인 실제 금액(N)이다. 초과가 없으면 S와 같다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 ConsumedCredits = 0;

	// 목표를 넘어 정산기 앞 바닥에 떨어질 실제 금화(F = S - N)다. 새 목표에 넣지 않는다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 FloorCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 EfficiencyPercent = 0;

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
