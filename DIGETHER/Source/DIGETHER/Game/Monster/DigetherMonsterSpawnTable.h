#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DigetherMonsterSpawnTable.generated.h"

class UCurveFloat;
class UDigetherMonsterDefinition;

// 스폰 곡선, 갱도·던전이 같은 곡선 공유(기획 확정), 값은 레벨별
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherMonsterSpawnCurve
{
	GENERATED_BODY()

	// MON_SPAWN_GRACE, 스폰 타이머 시작 후 확률 0 구간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster", meta=(ClampMin="0", Units="s"))
	float GraceSeconds = 0.f;

	// MON_SPAWN_INTERVAL, 판정 간격
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster", meta=(ClampMin="0.1", Units="s"))
	float IntervalSeconds = 30.f;

	// MON_SPAWN_CHANCE_START, 유예 직후 판정당 확률(0~1)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster", meta=(ClampMin="0", ClampMax="1"))
	float StartChance = 0.f;

	// MON_SPAWN_CHANCE_MAX, 도달 시각 이후 판정당 확률(0~1)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster", meta=(ClampMin="0", ClampMax="1"))
	float MaxChance = 0.f;

	// MON_SPAWN_CURVE_EXP, 최종 확률 도달 시각(스폰 타이머 기준), 지수형 상승
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster", meta=(ClampMin="0", Units="s"))
	float MaxChanceAtSeconds = 0.f;

	// 지정 시 위 지수식 대신 사용(가로: 스폰 타이머 초, 세로: 확률 0~1, R11)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster")
	TObjectPtr<UCurveFloat> OverrideCurve;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherMonsterPeriodicEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster")
	TObjectPtr<UDigetherMonsterDefinition> Definition;

	// MON_SPAWN_WEIGHT
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster", meta=(ClampMin="0"))
	float Weight = 1.f;

	// MON_MAX_ALIVE, 갱도 주기형 종별 최대 생존 수(0은 제한 없음), 던전 항목은 사용 안 함
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster", meta=(ClampMin="0"))
	int32 MaxAlive = 0;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherMonsterHabitatEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster")
	TObjectPtr<UDigetherMonsterDefinition> Definition;

	// MON_HABITAT_CHANCE(땅거미는 MON_SPIDER_NEST_CHANCE와 같은 값), 맵 로드 때 1회 판정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster", meta=(ClampMin="0", ClampMax="1"))
	float Chance = 0.f;
};

// 던전 파워 한도, 던전마다 따로 적용(2026-11-11 사용자 결정), 수치는 미정·개발 시험값
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherDungeonPowerLimit
{
	GENERATED_BODY()

	// DUN_POWER_LIMIT 기본값
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster", meta=(ClampMin="0"))
	int32 Base = 0;

	// 달성한 납부 목표 하나당 증가량(증가 방식은 경제 담당 결정 전 시험값)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster", meta=(ClampMin="0"))
	int32 PerGoalAchieved = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster", meta=(ClampMin="0"))
	int32 Max = 0;
};

// 레벨(해금 갱도)별 몬스터 테이블, 목적지 ID로 선택
UCLASS(BlueprintType, Const)
class DIGETHER_API UDigetherMonsterSpawnTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType PrimaryAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	bool CollectProblems(TArray<FText>& OutProblems) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	// 거점 목적지 ID(UDigetherHubStateActor::GetSelectedStageId)와 같은 값
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster")
	FName StageId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster")
	FDigetherMonsterSpawnCurve Curve;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster")
	TArray<FDigetherMonsterPeriodicEntry> DungeonEntries;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster")
	FDigetherDungeonPowerLimit DungeonPowerLimit;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster")
	TArray<FDigetherMonsterPeriodicEntry> MinePeriodicEntries;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster")
	TArray<FDigetherMonsterHabitatEntry> MineHabitatEntries;
};
