#pragma once

#include "CoreMinimal.h"
#include "DigetherEconomyTuning.generated.h"

// 서버 소유 중앙 기본값, 계산 함수에 인자로 전달
// 임시 필드는 최종 기획 확정 시 갱신
// 사망 손실률·시체 복구율·첫 목표는 런 시작 때 고정한 FDigetherRunTuning 스냅샷 사용
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherEconomyTuning
{
	GENERATED_BODY()

	// PAY_DAY_EFFICIENCY[1일차], 임시값: 2026-11-04 기획 답변 초안(DEV-025), 정식 수치는 경제 담당 결정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="1", ClampMax="100"))
	int32 DayOneEfficiencyPercent = 30;

	// PAY_DAY_EFFICIENCY[2일차], 임시값: 2026-11-04 기획 답변 초안(DEV-025)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="1", ClampMax="100"))
	int32 DayTwoEfficiencyPercent = 50;

	// PAY_DAY_EFFICIENCY[3일차], 임시값: 2026-11-04 기획 답변 초안(DEV-025)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="1", ClampMax="100"))
	int32 DayThreeEfficiencyPercent = 70;

	// PAY_DAY_EFFICIENCY[4일차]. 보드 확정값(DEV-025)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="1", ClampMax="100"))
	int32 DayFourEfficiencyPercent = 100;

	// PAY_TARGET_STEP: 목표 달성마다 더하는 증가량
	// 임시값: 2026-11-04 기획 답변 초안(DEV-022)
	// 첫 목표(PAY_TARGET_FIRST)만 런 스냅샷 InitialQuotaCredits 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="0"))
	int32 TargetIncreaseCredits = 100;

	// 미사용(저장 호환): 폐기된 금액 입력 납부 세션 수명
	// 저장 형식(v3 본문)에 포함되어 필드·검증 유지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="0.0"))
	double DepositSessionLifetimeSeconds = 10.0;

	// PAY_UNIT_AMOUNT: 붓기 한 주기에 지갑에서 빼는 기준액(정수, 1 이상), 잔고 부족 시 잔고 전부
	// 임시값: 기획 미정(경제 담당 확인 대상), 2026-11-05
	// 저장 형식 미포함(불러와도 현재 서버 값 유지)
	// 주기 간격(PAY_TICK_INTERVAL)은 런 스냅샷 FDigetherRunTuning::DepositIntervalSeconds 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="1"))
	int32 PourUnitCredits = 10;

	// PAY_SIGNAL_THRESHOLDS[0]: 목표액 대비 공동 인정액 비율이 이 값 이상이면 주황, 미만이면 빨강
	// 임시값: 기획 시험 기준 0.5(경제 담당 검증 전), 2026-11-05
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="0.0", ClampMax="1.0"))
	double PaymentSignalOrangeRatio = 0.5;

	// PAY_SIGNAL_THRESHOLDS[1]: 목표액 대비 공동 인정액 비율이 이 값 이상이면 초록
	// 임시값: 기획 시험 기준 0.8(경제 담당 검증 전), 2026-11-05
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Economy", meta=(ClampMin="0.0", ClampMax="1.0"))
	double PaymentSignalGreenRatio = 0.8;
};
