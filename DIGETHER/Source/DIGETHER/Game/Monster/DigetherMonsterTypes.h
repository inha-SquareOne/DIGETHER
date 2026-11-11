#pragma once

#include "CoreMinimal.h"
#include "DigetherMonsterTypes.generated.h"

class APawn;
class APlayerController;

// 스폰 방식은 구역이 아니라 몬스터 속성(2026-11-08 시스템 기획서 확정)
UENUM(BlueprintType)
enum class EDigetherMonsterSpawnKind : uint8
{
	// 맵 로드 때 확률로 둥지와 함께 배치, 탐사 중 추가 생성 없음
	Habitat,
	// 갱도 주기 판정, 종별 최대 수로 제한
	Periodic,
	// 던전 배출구 주기 판정, 던전별 파워 한도로 제한
	PeriodicPower
};

UENUM(BlueprintType)
enum class EDigetherMonsterZone : uint8
{
	None,
	Mine,
	Dungeon
};

// 레벨 배치 스폰 지점 종류, 방식별 위치 필터가 다름
UENUM(BlueprintType)
enum class EDigetherMonsterSpawnPointKind : uint8
{
	DungeonVent,
	MinePeriodic,
	HabitatNest
};

UENUM(BlueprintType)
enum class EDigetherMonsterRemovalReason : uint8
{
	Killed,
	Despawned,
	RunEnded
};

UENUM(BlueprintType)
enum class EDigetherMonsterSpawnResult : uint8
{
	Spawned,
	NotAuthority,
	NoActiveRun,
	StaleRun,
	InvalidDefinition,
	NoCandidate,
	NoSpawnPoint,
	SpawnFailed
};

// 감지 원인, 소리·목소리는 최초 2종 범위 밖(PQ-13)
UENUM(BlueprintType)
enum class EDigetherMonsterStimulus : uint8
{
	Sight,
	WebVibration,
	Damaged,
	WebRemoved,
	BodyContact
};

// 종별 파라미터 기본 구조체, 종 컴포넌트가 하위 구조체를 요구
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherMonsterSpeciesParams
{
	GENERATED_BODY()

	virtual ~FDigetherMonsterSpeciesParams() = default;

	// 호출자가 모은 문제는 보존, 이 구조체 문제만 뒤에 추가
	virtual bool CollectProblems(TArray<FText>& OutProblems) const { return true; }
};

// 종별 이동·형상, 내비 에이전트 설정과 일치해야 함(단차 100cm 조건)
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherMonsterMovementParams
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster", meta=(ClampMin="1", Units="cm"))
	float CapsuleRadius = 40.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster", meta=(ClampMin="1", Units="cm"))
	float CapsuleHalfHeight = 90.f;

	// 엔진 기본 45cm, 기획 기준 100cm 단차는 내비 AgentMaxStepHeight도 같이 맞춰야 함
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster", meta=(ClampMin="0", Units="cm"))
	float MaxStepHeight = 45.f;

	// 종 기본 이동 속도, 상태별 속도는 종 파라미터
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster", meta=(ClampMin="0", Units="cm/s"))
	float WalkSpeed = 300.f;
};

// 몬스터 시야, 종 컴포넌트가 상태에 따라 켜고 끔(예: 박스는 닫힌 상자일 때만)
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherMonsterSightParams
{
	GENERATED_BODY()

	// 전체 시야각(도), 땅거미 120·닫힌 상자 70(기획 시험값)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster", meta=(ClampMin="0", ClampMax="360", Units="deg"))
	float FieldOfViewDegrees = 0.f;

	// 0이면 시야 없음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster", meta=(ClampMin="0", Units="cm"))
	float Range = 0.f;
};

// 감지 대상 플레이어 한 명, 감각 서브시스템이 매 주기 수집
struct FDigetherMonsterTarget
{
	TWeakObjectPtr<APawn> Pawn;
	TWeakObjectPtr<APlayerController> Controller;
	FString PlayerKey;
	// 몸 중심(기획: 땅거미 판정 지점)
	FVector Center = FVector::ZeroVector;
};

// 클라이언트 카메라 표본, 서버 시선 판정 입력
USTRUCT()
struct DIGETHER_API FDigetherCameraSample
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 Sequence = 0;

	UPROPERTY()
	FVector_NetQuantize10 Location = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	// 실제 투영 기준 유효 수평·수직 시야각(도), 화면비·축 유지 설정 반영
	UPROPERTY()
	float HorizontalFov = 90.f;

	UPROPERTY()
	float VerticalFov = 59.f;
};

// 서버 수신 표본, 수신 시각으로 유효 기간 판정
struct FDigetherReceivedCameraSample
{
	FDigetherCameraSample Sample;
	double ServerReceiveTime = 0.0;
	bool bValid = false;
	// 조회는 const 경로라 경고 표시만 mutable
	mutable bool bWarnedStale = false;
};

// 감지 자극 한 건, 감각 서브시스템이 다음 주기에 몬스터로 전달
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherMonsterStimulusEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Monster")
	EDigetherMonsterStimulus Cause = EDigetherMonsterStimulus::Sight;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Monster")
	FVector Location = FVector::ZeroVector;

	// 원인 플레이어, 지형 원인 등은 빈 값
	UPROPERTY(BlueprintReadOnly, Category="Digether|Monster")
	FString InstigatorKey;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Monster")
	double ServerTime = 0.0;
};
