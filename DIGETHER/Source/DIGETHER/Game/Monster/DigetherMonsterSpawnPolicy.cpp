#include "Game/Monster/DigetherMonsterSpawnPolicy.h"

#include "Game/Monster/DigetherMonsterDefinition.h"
#include "Game/Monster/DigetherMonsterSpawnPoint.h"

bool UDigetherMonsterSpawnPolicy::IsPointAllowed(const ADigetherMonsterSpawnPoint& Point, const FDigetherSpawnPointQuery& Query) const
{
	if (Point.PointKind != GetPointKind() || !Query.Definition)
	{
		return false;
	}
	FName ZoneId;
	const EDigetherMonsterZone Zone = Point.GetZone(&ZoneId);
	// 구역 볼륨이 없는 시험 맵은 갱도로 간주, 던전 지점은 반드시 던전 볼륨 안
	const EDigetherMonsterZone EffectiveZone = Zone == EDigetherMonsterZone::None ? EDigetherMonsterZone::Mine : Zone;
	if (EffectiveZone != Query.Definition->Zone)
	{
		return false;
	}
	return Query.DungeonId.IsNone() || ZoneId == Query.DungeonId;
}

bool UDigetherMonsterSpawnPolicy::IsFarFromPlayers(const FVector& Location, const FDigetherSpawnPointQuery& Query)
{
	const float MinDistance = Query.Definition ? Query.Definition->MinPlayerDistance : 0.f;
	if (MinDistance <= 0.f || Query.bIgnorePlayerDistance)
	{
		return true;
	}
	for (const FVector& Player : Query.LivingPlayerLocations)
	{
		if (FVector::DistSquared(Player, Location) < FMath::Square(MinDistance))
		{
			return false;
		}
	}
	return true;
}

bool UDigetherMonsterHabitatSpawnPolicy::IsPointAllowed(const ADigetherMonsterSpawnPoint& Point, const FDigetherSpawnPointQuery& Query) const
{
	if (!Super::IsPointAllowed(Point, Query) || Point.bInCollapseArea)
	{
		return false;
	}
	return !Query.ElevatorArrivalLocation.IsSet()
		|| FVector::DistSquared(Point.GetActorLocation(), Query.ElevatorArrivalLocation.GetValue()) >= FMath::Square(Query.MinNestDistanceFromElevator);
}

bool UDigetherMonsterPeriodicSpawnPolicy::IsPointAllowed(const ADigetherMonsterSpawnPoint& Point, const FDigetherSpawnPointQuery& Query) const
{
	return Super::IsPointAllowed(Point, Query) && IsFarFromPlayers(Point.GetActorLocation(), Query);
}

bool UDigetherMonsterPeriodicPowerSpawnPolicy::IsPointAllowed(const ADigetherMonsterSpawnPoint& Point, const FDigetherSpawnPointQuery& Query) const
{
	return Super::IsPointAllowed(Point, Query) && IsFarFromPlayers(Point.GetActorLocation(), Query);
}
