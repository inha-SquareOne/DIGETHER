#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "Game/Monster/DigetherMonsterSpawnRules.h"
#include "Game/Monster/DigetherMonsterTypes.h"
#include "DigetherMonsterDirectorSubsystem.generated.h"

class ADigetherMonsterCharacter;
class ADigetherMonsterSpawnPoint;
class UDigetherMonsterDefinition;
class UDigetherMonsterSpawnPolicy;
class UDigetherMonsterSpawnTable;

// 몬스터 생성 실행(설계 MD-1: 월드 서브시스템), 호스트에서만 활성
// 런 수명은 GameMode가 실제 출발·갱도 도착·종료 확정 처리에서 직접 호출(런 조율자 델리게이트는 명령 경로 아님)
// 예약 작업은 런 ID를 들고 있다가 런이 바뀌었으면 폐기
UCLASS()
class DIGETHER_API UDigetherMonsterDirectorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	bool IsActive() const { return bActive; }

	// 테스트·수동 활성용, 클라이언트 넷모드에서는 거절
	bool Activate();

	// 실제 출발 확정 때, 목표 달성 수는 던전 파워 한도 증가용
	bool BeginRun(const UDigetherMonsterSpawnTable* Table, const FGuid& RunId, int32 GoalsAchieved, int32 RandomSeed);

	// 목적지 ID로 프로젝트 설정의 테이블을 찾아 시작, 없으면 몬스터 없이 런만 기록
	bool BeginRunForStage(FName StageId, const FGuid& RunId, int32 GoalsAchieved);

	// 선택된 갱도 맵 로드 완료 때 1회, 서식지형 배치(2026-11-11 결정: 미리 만든 갱도 맵 중 선택)
	void NotifyMapReady(const FGuid& RunId, TOptional<FVector> ElevatorArrivalLocation);

	// 최초 하강 후 갱도 도착 때, 설정 지연(기획 10초) 뒤 스폰 타이머 시작
	void NotifyMineArrival(const FGuid& RunId);

	// 종료 확정 때, 타이머 정지·몬스터 정리, 다른 런 ID면 무시
	void EndRun(const FGuid& RunId);

	// 서버, 주어진 위치에 즉시 생성(디렉터 판정·개발 치트·테스트 공통 경로), 장부 기록 포함
	EDigetherMonsterSpawnResult SpawnMonsterAt(UDigetherMonsterDefinition* Definition, const FTransform& Transform,
		FName DungeonId, ADigetherMonsterCharacter** OutMonster = nullptr);

	// 서버, 몬스터가 만든 탐사 수명 Actor(둥지 등) 등록, 탐사 종료 때 파괴
	void RegisterRunActor(AActor* Actor);

	// 개발 치트: 종 지정 생성(허용되는 지점 우선, 없으면 Fallback 위치)
	EDigetherMonsterSpawnResult DevSpawn(UDigetherMonsterDefinition* Definition, const FTransform& FallbackTransform);
	// 개발 치트: 스폰 타이머를 앞당김
	void DevAdvanceSpawnTimer(float Seconds);
	// 개발 치트: 서식지 확률 무시하고 배치
	void DevForceHabitat();
	// 개발 치트·테스트: 다음 판정을 즉시 실행
	void DevRunEvaluationNow();

#if !UE_BUILD_SHIPPING
	struct FDevSpawnOptions
	{
		// 참이면 Transform 위치 강제, 거짓이면 DevSpawn처럼 허용 스폰 지점 우선
		bool bForceTransform = false;
		// 거짓이면 던전 파워 사용량에 넣지 않음, 제거 때도 반환 안 함
		bool bChargePower = true;
		// 비우면 생성 위치의 던전 구역에서 찾음, 시험용 지정
		TOptional<FName> DungeonId;
	};
	// 개발 치트(F-01): 옵션 지정 생성, 자동 생성 정지 중에도 동작
	EDigetherMonsterSpawnResult DevSpawnWithOptions(UDigetherMonsterDefinition* Definition, const FTransform& Transform,
		const FDevSpawnOptions& Options, ADigetherMonsterCharacter** OutMonster = nullptr);
	// 개발 치트(F-02): 대기 예고 취소(장부 반환) 후 살아 있는 몬스터 전부 RemoveFromRun, 제거 수 반환
	int32 DevClearMonsters(int32* OutCancelledPending = nullptr);
	// 개발 치트(F-02): 주기 판정·대기 예고·서식지 배치 정지, 켤 때 대기 예고 취소, 런이 바뀌어도 유지
	int32 DevSetAutoSpawnPaused(bool bPaused);
	bool IsAutoSpawnPausedForDev() const { return bDevAutoSpawnPaused; }
	int32 GetPendingSpawnCountForDev() const { return PendingSpawns.Num(); }
	// 개발 표시(V-02): 맵의 던전 구역 ID와 장부에 기록된 던전 ID 합집합
	void GetDungeonIdsForDev(TArray<FName>& OutDungeonIds) const;
#endif

	bool HasActiveRun() const { return CurrentRunId.IsValid(); }
	const FGuid& GetCurrentRunId() const { return CurrentRunId; }
	bool IsSpawnTimerRunning() const { return bSpawnTimerRunning; }
	float GetSpawnTimerSeconds() const;
	float GetCurrentChance() const;
	int32 GetDungeonPowerLimit() const { return DungeonPowerLimit; }
	int32 GetDungeonPowerUsed(FName DungeonId) const { return Ledger.GetPowerUsed(DungeonId); }
	int32 GetAliveCount(const UDigetherMonsterDefinition* Definition) const;
	int32 GetRunSpawnedCount(const UDigetherMonsterDefinition* Definition) const;
	void GetLiveMonsters(TArray<ADigetherMonsterCharacter*>& OutMonsters) const;

	// 장부 키, 정의 에셋 이름
	static FName GetSpeciesId(const UDigetherMonsterDefinition* Definition);

	// 테스트용 난수 시드 재지정
	void SetRandomSeedForTest(int32 Seed) { Random.Initialize(Seed); }

private:
	struct FPendingSpawn
	{
		FGuid RunId;
		TWeakObjectPtr<UDigetherMonsterDefinition> Definition;
		TWeakObjectPtr<ADigetherMonsterSpawnPoint> Point;
		FName DungeonId;
		// 예약 때 장부에 넣은 파워, 생성 성공 시 개체 기록으로 넘기고 취소 시 같은 값 반환
		int32 ChargedPower = 0;
		FTimerHandle Timer;
	};

	void StartSpawnTimer(FGuid RunId);
	void RunPeriodicEvaluation(FGuid RunId);
	bool TrySpawnMinePeriodic();
	bool TrySpawnDungeon();
	void PlaceHabitats(bool bIgnoreChance);

	// 예고가 있으면 예약 후 지연 생성, 없으면 즉시
	bool ScheduleSpawn(UDigetherMonsterDefinition* Definition, ADigetherMonsterSpawnPoint* Point, FName DungeonId);
	void FinishPendingSpawn(int32 PendingId);

	ADigetherMonsterSpawnPoint* PickPoint(const UDigetherMonsterDefinition* Definition, FName DungeonId) const;
	void CollectLivingPlayerLocations(TArray<FVector>& OutLocations) const;
	void CollectDungeonIds(TArray<FName>& OutDungeonIds) const;
	const UDigetherMonsterSpawnPolicy* GetPolicy(EDigetherMonsterSpawnKind Kind) const;

	// SpawnMonsterAt 본체, ChargedPower를 장부와 개체 기록에 같은 값으로 사용
	EDigetherMonsterSpawnResult SpawnMonsterCharged(UDigetherMonsterDefinition* Definition, const FTransform& Transform,
		FName DungeonId, int32 ChargedPower, ADigetherMonsterCharacter** OutMonster);
	// 장부 기록 없이 개체만 생성
	ADigetherMonsterCharacter* SpawnActorFor(UDigetherMonsterDefinition* Definition, const FTransform& Transform, FName DungeonId,
		EDigetherMonsterSpawnResult& OutResult);
	void TrackMonster(ADigetherMonsterCharacter* Monster, int32 ChargedPower);
	void HandleMonsterRemoved(ADigetherMonsterCharacter* Monster, EDigetherMonsterRemovalReason Reason);
	void ClearRunState();

	bool bActive = false;
	FGuid CurrentRunId;

	UPROPERTY(Transient)
	TObjectPtr<const UDigetherMonsterSpawnTable> Table;

	UPROPERTY(Transient)
	TMap<EDigetherMonsterSpawnKind, TObjectPtr<UDigetherMonsterSpawnPolicy>> Policies;

	DigetherMonsterSpawnRules::FLedger Ledger;
	int32 DungeonPowerLimit = 0;
	FRandomStream Random;

	bool bSpawnTimerRunning = false;
	double SpawnTimerStartTime = 0.0;
	float SpawnTimerOffset = 0.f;
	FTimerHandle StartDelayTimer;
	FTimerHandle EvaluationTimer;

	TOptional<FVector> ElevatorArrivalLocation;
	bool bHabitatsPlaced = false;

	int32 NextPendingId = 1;
	TMap<int32, FPendingSpawn> PendingSpawns;
	TArray<TWeakObjectPtr<ADigetherMonsterCharacter>> LiveMonsters;
	// 개체별 실제 부과 파워, 제거 때 정의의 PowerLevel 대신 이 값 반환
	TMap<TObjectKey<ADigetherMonsterCharacter>, int32> ChargedPowerByMonster;
	TArray<TWeakObjectPtr<AActor>> RunActors;

#if !UE_BUILD_SHIPPING
	// 대기 예고를 취소하고 예약 장부 반환, 취소 수 반환
	int32 CancelPendingSpawnsForDev();
	bool bDevAutoSpawnPaused = false;
#endif
};
