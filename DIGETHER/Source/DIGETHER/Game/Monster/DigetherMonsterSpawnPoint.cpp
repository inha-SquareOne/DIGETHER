#include "Game/Monster/DigetherMonsterSpawnPoint.h"

#include "Components/ArrowComponent.h"
#include "Game/Monster/DigetherZoneVolume.h"

ADigetherMonsterSpawnPoint::ADigetherMonsterSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	// 예고 연출 멀티캐스트용, 상태는 복제하지 않음
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(1.f);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

#if WITH_EDITORONLY_DATA
	Arrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	if (Arrow)
	{
		Arrow->SetupAttachment(Root);
		Arrow->bIsScreenSizeScaled = true;
	}
#endif
}

void ADigetherMonsterSpawnPoint::MulticastSpawnTelegraph_Implementation(const float Seconds)
{
	OnSpawnTelegraph(Seconds);
}

EDigetherMonsterZone ADigetherMonsterSpawnPoint::GetZone(FName* OutZoneId) const
{
	return ADigetherZoneVolume::GetZoneKindAt(GetWorld(), GetActorLocation(), OutZoneId);
}
