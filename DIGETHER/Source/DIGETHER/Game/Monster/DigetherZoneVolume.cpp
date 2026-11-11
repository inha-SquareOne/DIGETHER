#include "Game/Monster/DigetherZoneVolume.h"

#include "Components/BrushComponent.h"
#include "EngineUtils.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "DigetherZoneVolume"

ADigetherZoneVolume::ADigetherZoneVolume()
{
	// 판정은 EncompassesPoint(물리 본체 거리 조회)만, 본체 생성 위해 질의 전용 + 모든 채널 무시
	GetBrushComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GetBrushComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
	GetBrushComponent()->SetGenerateOverlapEvents(false);
	bReplicates = false;
}

const ADigetherZoneVolume* ADigetherZoneVolume::FindZoneAt(const UWorld* World, const FVector& Location)
{
	if (!IsValid(World))
	{
		return nullptr;
	}
	const ADigetherZoneVolume* Found = nullptr;
	for (TActorIterator<ADigetherZoneVolume> It(World); It; ++It)
	{
		const ADigetherZoneVolume* Volume = *It;
		if (!IsValid(Volume) || Volume->Zone == EDigetherMonsterZone::None || !Volume->EncompassesPoint(Location))
		{
			continue;
		}
		if (!Found || (Volume->Zone == EDigetherMonsterZone::Dungeon && Found->Zone != EDigetherMonsterZone::Dungeon))
		{
			Found = Volume;
		}
	}
	return Found;
}

EDigetherMonsterZone ADigetherZoneVolume::GetZoneKindAt(const UWorld* World, const FVector& Location, FName* OutZoneId)
{
	const ADigetherZoneVolume* Volume = FindZoneAt(World, Location);
	if (OutZoneId)
	{
		*OutZoneId = Volume ? Volume->ZoneId : NAME_None;
	}
	return Volume ? Volume->Zone : EDigetherMonsterZone::None;
}

#if WITH_EDITOR
EDataValidationResult ADigetherZoneVolume::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Zone == EDigetherMonsterZone::Dungeon && ZoneId.IsNone())
	{
		Context.AddError(LOCTEXT("DungeonZoneIdRequired", "던전 구역은 파워 장부용 ZoneId가 필요합니다."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
