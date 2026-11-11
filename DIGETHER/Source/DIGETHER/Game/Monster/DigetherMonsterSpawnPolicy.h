#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Game/Monster/DigetherMonsterTypes.h"
#include "DigetherMonsterSpawnPolicy.generated.h"

class ADigetherMonsterSpawnPoint;
class UDigetherMonsterDefinition;

// 위치 필터 입력, 디렉터가 판정 시점에 채움
struct FDigetherSpawnPointQuery
{
	const UDigetherMonsterDefinition* Definition = nullptr;
	// 던전형만, 이 던전의 배출구만 허용(None이면 구역 무관)
	FName DungeonId;
	TArray<FVector> LivingPlayerLocations;
	// 서식지형만, 엘리베이터 도착 지점(없으면 거리 필터 생략)
	TOptional<FVector> ElevatorArrivalLocation;
	float MinNestDistanceFromElevator = 0.f;
};

// 스폰 방식별로 다른 부분(쓰는 지점 종류·위치 필터), 새 방식은 enum 값 + 하위 클래스 + 디렉터 선택 경로 추가
UCLASS(Abstract)
class DIGETHER_API UDigetherMonsterSpawnPolicy : public UObject
{
	GENERATED_BODY()

public:
	virtual EDigetherMonsterSpawnPointKind GetPointKind() const PURE_VIRTUAL(UDigetherMonsterSpawnPolicy::GetPointKind, return EDigetherMonsterSpawnPointKind::DungeonVent;);

	// 지점 종류·구역 확인 뒤 방식별 조건
	virtual bool IsPointAllowed(const ADigetherMonsterSpawnPoint& Point, const FDigetherSpawnPointQuery& Query) const;

protected:
	static bool IsFarFromPlayers(const FVector& Location, const FDigetherSpawnPointQuery& Query);
};

// 갱도 서식지형: 붕괴 구간 제외, 엘리베이터 도착 지점 최소 거리, 생존자 거리 필터 없음(기획)
UCLASS()
class DIGETHER_API UDigetherMonsterHabitatSpawnPolicy : public UDigetherMonsterSpawnPolicy
{
	GENERATED_BODY()

public:
	virtual EDigetherMonsterSpawnPointKind GetPointKind() const override { return EDigetherMonsterSpawnPointKind::HabitatNest; }
	virtual bool IsPointAllowed(const ADigetherMonsterSpawnPoint& Point, const FDigetherSpawnPointQuery& Query) const override;
};

// 갱도 주기형: 지정 스폰 포인트, 생존자 최소 거리(개발 시험값)
UCLASS()
class DIGETHER_API UDigetherMonsterPeriodicSpawnPolicy : public UDigetherMonsterSpawnPolicy
{
	GENERATED_BODY()

public:
	virtual EDigetherMonsterSpawnPointKind GetPointKind() const override { return EDigetherMonsterSpawnPointKind::MinePeriodic; }
	virtual bool IsPointAllowed(const ADigetherMonsterSpawnPoint& Point, const FDigetherSpawnPointQuery& Query) const override;
};

// 던전 주기+파워형: 해당 던전 배출구, 생존자 최소 거리
UCLASS()
class DIGETHER_API UDigetherMonsterPeriodicPowerSpawnPolicy : public UDigetherMonsterSpawnPolicy
{
	GENERATED_BODY()

public:
	virtual EDigetherMonsterSpawnPointKind GetPointKind() const override { return EDigetherMonsterSpawnPointKind::DungeonVent; }
	virtual bool IsPointAllowed(const ADigetherMonsterSpawnPoint& Point, const FDigetherSpawnPointQuery& Query) const override;
};
