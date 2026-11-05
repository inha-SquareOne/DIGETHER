#pragma once

#include "CoreMinimal.h"
#include "DigetherEconomyTuning.generated.h"

// 서버가 소유하고 계산 함수에 전달하는 중앙 기본값이다. 최종 기획 확정 시 임시 필드를 갱신한다.
// 사망 손실률·시체 복구율·첫 목표는 런 시작 때 고정한 FDigetherRunTuning 스냅샷에서 받는다.
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherEconomyTuning
{
	GENERATED_BODY()

	// PAY_DAY_EFFICIENCY[1일차]. 임시값: 2026-11-04 기획 답변 초안(DEV-025). 정식 수치는 경제 담당이 정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="1", ClampMax="100"))
	int32 DayOneEfficiencyPercent = 30;

	// PAY_DAY_EFFICIENCY[2일차]. 임시값: 2026-11-04 기획 답변 초안(DEV-025).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="1", ClampMax="100"))
	int32 DayTwoEfficiencyPercent = 50;

	// PAY_DAY_EFFICIENCY[3일차]. 임시값: 2026-11-04 기획 답변 초안(DEV-025).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="1", ClampMax="100"))
	int32 DayThreeEfficiencyPercent = 70;

	// PAY_DAY_EFFICIENCY[4일차]. 보드 확정값(DEV-025)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="1", ClampMax="100"))
	int32 DayFourEfficiencyPercent = 100;

	// PAY_TARGET_STEP. 목표 달성 때마다 더하는 증가량. 임시값: 2026-11-04 기획 답변 초안(DEV-022).
	// 첫 목표(PAY_TARGET_FIRST)만 런 스냅샷 InitialQuotaCredits에서 받고, 증가량은 이 값을 쓴다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="0"))
	int32 TargetIncreaseCredits = 100;

	// 미사용(저장 호환). 폐기된 금액 입력 납부 세션의 수명이다. 저장 형식(v3 본문)에 들어 있어 필드와 검증을 남긴다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="0.0"))
	double DepositSessionLifetimeSeconds = 10.0;

	// PAY_UNIT_AMOUNT. 붓기 한 주기에 지갑에서 빼는 기준액(정수, 1 이상)이다. 잔고가 이보다 적으면 잔고 전부를 붓는다.
	// 임시값: 기획 미정(경제 담당 확인 대상), 2026-11-05. 저장 형식에는 넣지 않는다(불러와도 현재 서버 값 유지).
	// 주기 간격(PAY_TICK_INTERVAL)은 런 스냅샷 FDigetherRunTuning::DepositIntervalSeconds에서 받는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="1"))
	int32 PourUnitCredits = 10;

	// PAY_SIGNAL_THRESHOLDS[0]. 목표액 대비 공동 인정액 비율이 이 값 이상이면 주황, 미만이면 빨강이다.
	// 임시값: 기획 시험 기준 0.5(경제 담당 검증 전), 2026-11-05
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="0.0", ClampMax="1.0"))
	double PaymentSignalOrangeRatio = 0.5;

	// PAY_SIGNAL_THRESHOLDS[1]. 목표액 대비 공동 인정액 비율이 이 값 이상이면 초록이다.
	// 임시값: 기획 시험 기준 0.8(경제 담당 검증 전), 2026-11-05
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="0.0", ClampMax="1.0"))
	double PaymentSignalGreenRatio = 0.8;
};
