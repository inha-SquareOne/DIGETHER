#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "Game/Monster/DigetherMonsterTypes.h"
#include "DigetherZoneVolume.generated.h"

// 갱도·던전 구역 표시, 던전 몬스터 이탈 방지와 던전별 파워 장부 키로 사용
// 진입·이탈 통지(던전 출입 1초 유지 입력)는 런 흐름 기획 확정 후 연결
UCLASS()
class DIGETHER_API ADigetherZoneVolume : public AVolume
{
	GENERATED_BODY()

public:
	ADigetherZoneVolume();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Digether|Zone")
	EDigetherMonsterZone Zone = EDigetherMonsterZone::Mine;

	// 던전별 파워 장부 키, 같은 맵 안에서 던전마다 달라야 함
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Digether|Zone")
	FName ZoneId;

	// 위치를 포함하는 구역 볼륨, 겹치면 던전 우선, 없으면 null
	static const ADigetherZoneVolume* FindZoneAt(const UWorld* World, const FVector& Location);

	static EDigetherMonsterZone GetZoneKindAt(const UWorld* World, const FVector& Location, FName* OutZoneId = nullptr);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
