#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DigetherTuningProfile.generated.h"

// 런 하나에 적용하는 튜닝 값. 런 시작 때 프로필에서 복사해 고정하고, 진행 중에는 바꾸지 않는다.
// 기획 파라미터 ID와의 대응은 각 필드 주석에 적는다(R03). 기본값은 두지 않는다. 값의 원본은 DA 하나다.
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherRunTuning
{
	GENERATED_BODY()

	// 값을 바꾸면 올린다. 런 시작 로그에 함께 남겨 어떤 값으로 진행했는지 재현한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="1"))
	int32 ProfileVersion = 0;

	// 첫 목표액이자 목표 달성 때마다 더하는 증가량.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="1"))
	int32 InitialQuotaCredits = 0;

	// RUN_TIME_LIMIT
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="1", Units="s"))
	float RunSeconds = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="1", Units="cm"))
	float InteractionDistanceCm = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="1", Units="kg"))
	float ElevatorCapacityKg = 0.f;

	// ELV_ASCENT_LIMIT. D24 비교 대상인 1회·2회만 허용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="1", ClampMax="2"))
	int32 ElevatorTripLimit = 0;

	// 상승 확정 1회당 남은 시간에서 빼는 값.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="0", Units="s"))
	float AscentPenaltySeconds = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="0.1", Units="s"))
	float ElevatorTravelSeconds = 0.f;

	// 납부 입력 한 단위의 간격.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="0.1", Units="s"))
	float DepositIntervalSeconds = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="0", Units="kg"))
	float EmergencyWeightLimitKg = 0.f;

	// PLY_DEATH_LOSS_RATIO. 정수 백분율로 둬 실수 반올림 오차 없이 내림 계산한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="0", ClampMax="100"))
	int32 DeathLossPercent = 0;

	// CORPSE_REFUND_RATIO. 기준은 사망 직전 잔고다(PQ-14 확인 전까지 현재 산식 유지).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ClampMin="0", ClampMax="100"))
	int32 CorpseRefundPercent = 0;

	// 스냅샷을 만든 프로필. 프로필 자산 안에서는 비어 있다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Tuning")
	FPrimaryAssetId SourceProfile;

	// 값 범위 검사. 비어 있거나 범위를 벗어나면 문제를 모두 담고 false를 돌려준다.
	bool CollectProblems(TArray<FText>& OutProblems) const;
};

// 기획자가 재빌드 없이 고치는 튜닝 원본(R01). 다음 런부터 적용된다.
UCLASS(BlueprintType, Const)
class DIGETHER_API UDigetherTuningProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Asset Manager 등록(DefaultGame.ini)과 같은 이름이어야 한다.
	static const FPrimaryAssetType PrimaryAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// 런에 고정할 복사본. 출처 프로필 ID를 채운다.
	FDigetherRunTuning MakeSnapshot() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Tuning", meta=(ShowOnlyInnerProperties))
	FDigetherRunTuning Values;
};
