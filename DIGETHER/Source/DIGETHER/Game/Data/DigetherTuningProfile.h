#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DigetherTuningProfile.generated.h"

// 런 하나에 적용하는 튜닝 값, 런 시작 때 프로필에서 복사해 고정, 진행 중 변경 없음
// 기획 파라미터 ID 대응은 각 필드 주석(R03), 코드 기본값 없음, 값 원본은 DA 하나
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherRunTuning
{
	GENERATED_BODY()

	// 값 변경 시 증가, 런 시작 로그에 남겨 진행 값 재현용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="1"))
	int32 ProfileVersion = 0;

	// PAY_TARGET_FIRST, 첫 목표액, 증가량은 FDigetherEconomyTuning::TargetIncreaseCredits(PAY_TARGET_STEP)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="1"))
	int32 InitialQuotaCredits = 0;

	// RUN_TIME_LIMIT
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="1", Units="s"))
	float RunSeconds = 0.f;

	// PLY_INTERACT_RANGE, 시트 단위 m, 이 필드는 cm(1m = 100cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="1", Units="cm"))
	float InteractionDistanceCm = 0.f;

	// ELV_WEIGHT_LIMIT, 기획 수치 미정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="1", Units="kg"))
	float ElevatorCapacityKg = 0.f;

	// ELV_ASCENT_LIMIT, 일반 상승 최대 2회 확정(최초 하강 제외)이라 2만 허용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="2", ClampMax="2"))
	int32 ElevatorTripLimit = 0;

	// ELV_RUN_TIME_PENALTY, 상승 확정 1회당 남은 시간 차감값, 첫 하강 제외, 남은 시간 하한 0
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="0", Units="s"))
	float AscentPenaltySeconds = 0.f;

	// ELV_TRAVEL_TIME, 기획은 구간별 값이나 현재 전 구간 단일 값
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="0.1", Units="s"))
	float ElevatorTravelSeconds = 0.f;

	// PAY_TICK_INTERVAL, 정산기 우클릭 유지 중 붓기 주기(기획 S24 동시 붓기)
	// 첫 차감은 누른 뒤 이 간격 경과 시점, 주기당 금액은 FDigetherEconomyTuning::PourUnitCredits(PAY_UNIT_AMOUNT)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="0.1", Units="s"))
	float DepositIntervalSeconds = 0.f;

	// EMG_WEIGHT_LIMIT, 긴급 탈출 무게 한도, 회수품 반입 불가
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="0", Units="kg"))
	float EmergencyWeightLimitKg = 0.f;

	// PLY_DEATH_LOSS_RATIO, 탐사 사망 중 시체 미회수(미스터리 박스 포식 포함) 손실률, 정수 백분율·손실액 올림
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="0", ClampMax="100"))
	int32 DeathLossPercent = 0;

	// 사용 중단(런 플로우 v1.1.0 9.4 시체 복구금 폐지): 프로토타입 코드와 DA 이관 원본 전용, Game 코드 미사용
	// 이름 유지 이유: Prototype/ 수정 금지(2026-11-04 사용자 결정)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="0", ClampMax="100"))
	int32 CorpseRefundPercent = 0;

	// 탐사 사망 중 시체 유효 회수 손실률(시험값 50, 기획 확정 전), 기준은 사망 직전 잔고·손실액 올림
	// 코드 기본값 없음(미설정 INDEX_NONE), 필드 도입 전 DA는 PostLoad가 DeathLossPercent - CorpseRefundPercent로 이관
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="0", ClampMax="100"))
	int32 RecoveredDeathLossPercent = INDEX_NONE;

	// 스냅샷 출처 프로필, 프로필 자산 안에서는 빈 값
	UPROPERTY(BlueprintReadOnly, Category="Digether|Tuning")
	FPrimaryAssetId SourceProfile;

	// 값 범위 검사, 미설정·범위 밖이면 문제를 모두 담고 false
	bool CollectProblems(TArray<FText>& OutProblems) const;

	// 시체 복구금 시절 값(손실 D%·복구 R%)의 순 손실률 max(0, D - R)로 회수 손실률 이관, 이미 설정됐으면 그대로
	void MigrateLegacyCorpseRefund();
};

// 기획자가 재빌드 없이 고치는 튜닝 원본(R01), 다음 런부터 적용
UCLASS(BlueprintType, Const)
class DIGETHER_API UDigetherTuningProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Asset Manager 등록(DefaultGame.ini) 이름과 일치 필요
	static const FPrimaryAssetType PrimaryAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// 런에 고정할 복사본, 출처 프로필 ID 포함
	FDigetherRunTuning MakeSnapshot() const;

	// RecoveredDeathLossPercent 도입 전 저장된 DA 값 이관, 다시 저장하면 새 필드로 기록
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ShowOnlyInnerProperties))
	FDigetherRunTuning Values;
};
