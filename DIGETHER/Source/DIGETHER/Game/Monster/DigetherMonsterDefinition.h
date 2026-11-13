#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Game/Monster/DigetherMonsterTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "DigetherMonsterDefinition.generated.h"

class ADigetherMonsterCharacter;
class UBehaviorTree;

// 종별 고정 정의만 보관, 현재 체력·생명 상태는 런타임 상태 소유
// 레벨과 무관한 값만 둠, 레벨별 값은 UDigetherMonsterSpawnTable
UCLASS(BlueprintType, Const)
class DIGETHER_API UDigetherMonsterDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType PrimaryAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// 체력 규칙 문제만 검사, 체력 컴포넌트 초기화용(스폰 데이터 누락으로 체력 판정을 막지 않음)
	bool CollectProblems(TArray<FText>& OutProblems) const;

	// 스폰·이동·AI까지 포함한 전체 검사, 디렉터 생성 전과 데이터 검사에 사용
	bool CollectSpawnProblems(TArray<FText>& OutProblems) const;

	// 종별 파라미터를 요구 타입으로 조회, 타입이 다르거나 비었으면 null
	template<typename T>
	const T* GetSpeciesParams() const
	{
		return SpeciesParams.GetPtr<T>();
	}

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster")
	FText DisplayName;

	// MON_KILLABLE, 일반 피해로 사망 가능한지(종별 값)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster")
	bool bKillable = false;

	// MON_HP, 종 데이터 에셋의 종별 값, 코드 기본값으로 처치 가능 체력을 추정하지 않음
	// 임시값: 땅거미 100(기본 공격 12.5 가정 약 8회 처치, 2026-11-04 기획 답변 초안 기준)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster", meta=(ClampMin="0"))
	float MaxHealth = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster|Spawn")
	EDigetherMonsterSpawnKind SpawnKind = EDigetherMonsterSpawnKind::Habitat;

	// 던전 종은 던전 밖으로 나가지 않음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster|Spawn")
	EDigetherMonsterZone Zone = EDigetherMonsterZone::None;

	// MON_POWER_LEVEL, 주기+파워형만 사용(1~3)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster|Spawn", meta=(ClampMin="0", ClampMax="3"))
	int32 PowerLevel = 0;

	// MON_RUN_SPAWN_CAP, 탐사당 최대 생성 수(던전 구분 없음), 0은 제한 없음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster|Spawn", meta=(ClampMin="0"))
	int32 RunSpawnCap = 0;

	// 생성 위치 예고 시간(배출구 덜컹거림 등), 0이면 즉시 생성
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster|Spawn", meta=(ClampMin="0", Units="s"))
	float SpawnTelegraphSeconds = 0.f;

	// 살아 있는 플레이어로부터 최소 거리, 서식지형은 사용 안 함(기획: 둥지는 붕괴 구간·엘리베이터 거리만)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster|Spawn", meta=(ClampMin="0", Units="cm"))
	float MinPlayerDistance = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster|Actor")
	TSoftClassPtr<ADigetherMonsterCharacter> MonsterClass;

	// 종별 BT, 테스트 종은 정의 복제 + BT 교체(R07)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster|Actor")
	TSoftObjectPtr<UBehaviorTree> BehaviorTree;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster|Actor")
	FDigetherMonsterMovementParams Movement;

	// 기본 시야, 감각 서브시스템이 0.1초 주기로 판정(AI Perception 미사용, 설계 MD-3)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster|Actor")
	FDigetherMonsterSightParams Sight;

	// 종별 파라미터, 종 컴포넌트가 요구하는 하위 구조체를 선택
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster|Species",
		meta=(BaseStruct="/Script/DIGETHER.DigetherMonsterSpeciesParams", ExcludeBaseStruct))
	FInstancedStruct SpeciesParams;

private:
	void CollectSpawnOnlyProblems(TArray<FText>& OutProblems) const;
};
