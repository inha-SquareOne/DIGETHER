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
	// 저장·BP 호환: 값 순서 변경 금지, 새 값은 뒤에만 추가
	// 붓기 중 거리·투입구 방향·가림·생존·연결 조건 불일치
	PourInteractionLost,
	// 바닥 금화를 이미 누가 주웠거나 사라짐
	FloorCoinUnavailable
};

// UObject 참조 없이 값만 보관, 연결 해제 중에도 같은 경제 함수 적용 가능
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

	// 기획 S33 결과 통계: 이번 게임에서 실제 지급받은 일반 몫 + 히든 수익 누적, 정산마다 가산
	// 구매·납부·사망으로 감소 없음, 시작 지급·시체 복구금·개발 지급 제외
	// 새 게임은 0
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 LifetimeEarnedCredits = 0;

	// 기획 S33: 시체 회수로 실제 돌려받은 금액 누적, 사망 손실 순액(TotalDeathLossCredits - 이 값) 계산 전용
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 LifetimeCorpseRefundCredits = 0;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherQuotaState
{
	GENERATED_BODY()

	// 기간 일차: 현재 할당량 기준 1~4일차, 4일차 탐사 후 마감 다음 날은 5
	// 납부 효율·마감·실패 판정 기준
	// 기존 BP 핀·저장 형식 호환으로 이름은 Day 유지(기획 1.1.0 02 개요 5행·공유 사양 "탐사·기간")
	// 초기화 전 0, 시작 시 MakeInitialQuotaState로 고정한 런 튜닝 스냅샷 적용
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 Day = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 TargetCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 DepositedCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 PeriodGeneration = 0;

	// 전체 일차: 게임 시작부터 센 날, 새 게임은 1, 탐사 종료 확정 시 기간 일차와 함께 +1(AdvanceDay)
	// 목표 달성으로 새 기간 시작 시 기간 일차만 1로 복귀, 이 값은 유지
	// 초기화 전 0
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 TotalDay = 0;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherHiddenRevealEntry
{
	GENERATED_BODY()

	// 식별자 발급·안정성은 호출자 책임, 온라인 모듈 비의존
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

	// 게임 오버 공개를 확정한 조율자만 채움, 그 전에는 빈 배열
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	TArray<FDigetherHiddenRevealEntry> HiddenReveals;
};

// 정산기 외부 3색 신호기 단계, 공동 인정액 숫자 대신 이 단계만 클라이언트 공개
UENUM(BlueprintType)
enum class EDigetherPaymentSignal : uint8
{
	// 목표 없음(초기화 전)
	None,
	Red,
	Orange,
	Green
};

// 클라이언트 복제용 할당량 공개 값, 정확한 누적 인정액(DepositedCredits)은 서버 판정·저장 전용
// 기획: 화면에는 누적 금액 대신 신호기 색만 전송
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherQuotaReplicatedState
{
	GENERATED_BODY()

	// 기간 일차(FDigetherQuotaState::Day와 같은 의미)
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 Day = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 TargetCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 PeriodGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	EDigetherPaymentSignal PaymentSignal = EDigetherPaymentSignal::None;

	// 전체 일차, 기간 일차와 같이 모든 참가자에게 공개
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

	// 복구액은 런 지급액·히든 누적액과 별도, 회수자에게 지급하지 않음
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

	// 입력 배열과 같은 순서, 실제 플레이어 식별과 중복 정산 방지는 조율자 책임
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

	// 효율과 무관하게 지갑에서 실제 차감한 금액
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 AcceptedCredits = 0;

	// 효율 적용 후 목표에 반영한 금액, 실제 지갑 차감액과 별개
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 CreditedCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	bool bTargetCompleted = false;
};

// 붓기 한 주기 값 계산 결과, 실패 시 지갑·목표는 입력 그대로이고 금액은 모두 0
// 기획 S24·PQ-34 안 A(임시, D27 확정 전): 지갑 차감 S, 목표 인정 C, 목표를 채우는 데 쓴 금액 N, 바닥 금화 F = S - N
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

	// 지갑에서 실제로 뺀 금액(S), 바닥 금화로 떨어진 몫 포함
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 SpentCredits = 0;

	// 그 순간 일차 효율로 목표에 인정한 금액(C), 남은 목표 이하
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 CreditedCredits = 0;

	// S 중 목표 인정에 쓰인 실제 금액(N), 초과가 없으면 S와 같음
	UPROPERTY(BlueprintReadOnly, Category="Digether|Economy")
	int32 ConsumedCredits = 0;

	// 목표 초과분으로 정산기 앞 바닥에 떨어질 실제 금화(F = S - N), 새 목표에 넣지 않음
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
