#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/Monster/DigetherMonsterTypes.h"
#include "DigetherMonsterSpawnPoint.generated.h"

class UArrowComponent;

// 레벨 배치 스폰 지점(던전 배출구·갱도 주기 포인트·둥지 후보), 판정은 디렉터
UCLASS()
class DIGETHER_API ADigetherMonsterSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	ADigetherMonsterSpawnPoint();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Digether|Monster")
	EDigetherMonsterSpawnPointKind PointKind = EDigetherMonsterSpawnPointKind::DungeonVent;

	// 둥지 후보 제외 조건(기획: 붕괴 가능 구간 제외), 레벨 디자이너가 표시
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Digether|Monster")
	bool bInCollapseArea = false;

	// 둥지 영역 반지름(추적 가능 영역), 서식지형 종이 읽음, 0이면 종 기본값
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Digether|Monster", meta=(ClampMin="0", Units="cm"))
	float NestRadius = 0.f;

	// 서버 판정 뒤 예고 연출, 배출구 덜컹거림 등은 BP에서 구현(모든 클라이언트 재생)
	UFUNCTION(BlueprintImplementableEvent, Category="Digether|Monster")
	void OnSpawnTelegraph(float Seconds);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSpawnTelegraph(float Seconds);

	// 배치된 구역, 비교 시점마다 볼륨 조회
	EDigetherMonsterZone GetZone(FName* OutZoneId = nullptr) const;

private:
	UPROPERTY(VisibleAnywhere, Category="Digether|Monster")
	TObjectPtr<USceneComponent> Root;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UArrowComponent> Arrow;
#endif
};
