#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "DigetherMonsterSettings.generated.h"

class UDigetherMonsterSpawnTable;

// 몬스터 공통 설정(프로젝트 설정 > DIGETHER Monster), 레벨별 값은 스폰 테이블
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "DIGETHER Monster"))
class DIGETHER_API UDigetherMonsterSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UDigetherMonsterSettings& Get();

	// 목적지 ID → 레벨 스폰 테이블, 없으면 DefaultSpawnTable
	UPROPERTY(Config, EditAnywhere, Category="Spawn")
	TMap<FName, TSoftObjectPtr<UDigetherMonsterSpawnTable>> SpawnTables;

	UPROPERTY(Config, EditAnywhere, Category="Spawn")
	TSoftObjectPtr<UDigetherMonsterSpawnTable> DefaultSpawnTable;

	// 기획 확정: 최초 하강 후 갱도 도착 10초 뒤 스폰 타이머 시작
	UPROPERTY(Config, EditAnywhere, Category="Spawn", meta=(ClampMin="0", Units="s"))
	float SpawnTimerStartDelaySeconds = 10.f;

	// 둥지 후보와 엘리베이터 도착 지점 최소 거리(개발 시험값 30m)
	UPROPERTY(Config, EditAnywhere, Category="Spawn", meta=(ClampMin="0", Units="cm"))
	float MinNestDistanceFromElevator = 3000.f;

	// 감각 판정 고정 주기(기획: 호스트 0.1초)
	UPROPERTY(Config, EditAnywhere, Category="Sense", meta=(ClampMin="0.02", Units="s"))
	float SenseIntervalSeconds = 0.1f;

	// 클라이언트 카메라 표본 전송 주기
	UPROPERTY(Config, EditAnywhere, Category="Sense", meta=(ClampMin="0.02", Units="s"))
	float CameraSampleIntervalSeconds = 0.1f;

	// 이 각도 이상 회전하면 주기를 기다리지 않고 즉시 전송(시험값)
	UPROPERTY(Config, EditAnywhere, Category="Sense", meta=(ClampMin="0", Units="deg"))
	float CameraSampleResendDegrees = 5.f;

	// 표본 유효 기간, 넘으면 '보지 않음'으로 처리(시험값)
	UPROPERTY(Config, EditAnywhere, Category="Sense", meta=(ClampMin="0.1", Units="s"))
	float CameraSampleMaxAgeSeconds = 0.5f;

	// 서버 검증용 최대 화면비(32:9)
	UPROPERTY(Config, EditAnywhere, Category="Sense", meta=(ClampMin="1"))
	float MaxAspectRatio = 3.6f;

	// 주기당 트레이스 경고 기준, 판정은 미루지 않음(설계 3.6)
	UPROPERTY(Config, EditAnywhere, Category="Sense", meta=(ClampMin="1"))
	int32 TraceWarningPerStep = 64;

	// 시야 가림 판정 채널
	UPROPERTY(Config, EditAnywhere, Category="Sense")
	TEnumAsByte<ECollisionChannel> SightTraceChannel = ECC_Visibility;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
