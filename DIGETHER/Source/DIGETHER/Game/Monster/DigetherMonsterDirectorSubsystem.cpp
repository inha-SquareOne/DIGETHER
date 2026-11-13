#include "Game/Monster/DigetherMonsterDirectorSubsystem.h"

#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Monster/DigetherMonsterCharacter.h"
#include "Game/Monster/DigetherMonsterDefinition.h"
#include "Game/Monster/DigetherMonsterLog.h"
#include "Game/Monster/DigetherMonsterSenseSubsystem.h"
#include "Game/Monster/DigetherMonsterSettings.h"
#include "Game/Monster/DigetherMonsterSpawnPoint.h"
#include "Game/Monster/DigetherMonsterSpawnPolicy.h"
#include "Game/Monster/DigetherMonsterSpawnTable.h"
#include "Game/Monster/DigetherZoneVolume.h"
#include "TimerManager.h"
#include "VisualLogger/VisualLogger.h"

namespace
{
	bool IsGameWorld(const UWorld* World)
	{
		return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
	}
}

bool UDigetherMonsterDirectorSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer) && IsGameWorld(Cast<UWorld>(Outer));
}

void UDigetherMonsterDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Policies.Add(EDigetherMonsterSpawnKind::Habitat, NewObject<UDigetherMonsterHabitatSpawnPolicy>(this));
	Policies.Add(EDigetherMonsterSpawnKind::Periodic, NewObject<UDigetherMonsterPeriodicSpawnPolicy>(this));
	Policies.Add(EDigetherMonsterSpawnKind::PeriodicPower, NewObject<UDigetherMonsterPeriodicPowerSpawnPolicy>(this));
	Random.GenerateNewSeed();
}

void UDigetherMonsterDirectorSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Activate();
}

bool UDigetherMonsterDirectorSubsystem::Activate()
{
	const UWorld* World = GetWorld();
	bActive = World && World->GetNetMode() != NM_Client;
	return bActive;
}

void UDigetherMonsterDirectorSubsystem::Deinitialize()
{
	if (CurrentRunId.IsValid())
	{
		EndRun(CurrentRunId);
	}
	ClearRunState();
	bActive = false;
	Super::Deinitialize();
}

bool UDigetherMonsterDirectorSubsystem::BeginRun(const UDigetherMonsterSpawnTable* InTable, const FGuid& RunId,
	const int32 GoalsAchieved, const int32 RandomSeed)
{
	if (!bActive || !RunId.IsValid())
	{
		return false;
	}
	if (CurrentRunId.IsValid())
	{
		UE_LOG(LogDigetherMonster, Warning, TEXT("이전 런이 끝나지 않은 상태에서 새 런 시작, 이전 런 몬스터를 정리합니다."));
		EndRun(CurrentRunId);
	}
	ClearRunState();
	CurrentRunId = RunId;
	Random.Initialize(RandomSeed);

	TArray<FText> Problems;
	if (InTable && !InTable->CollectProblems(Problems))
	{
		for (const FText& Problem : Problems)
		{
			UE_LOG(LogDigetherMonster, Warning, TEXT("스폰 테이블 %s 오류: %s"), *GetNameSafe(InTable), *Problem.ToString());
		}
		InTable = nullptr;
	}
	Table = InTable;
	if (Table)
	{
		const FDigetherDungeonPowerLimit& Limit = Table->DungeonPowerLimit;
		DungeonPowerLimit = DigetherMonsterSpawnRules::EvaluatePowerLimit(Limit.Base, Limit.PerGoalAchieved, Limit.Max, GoalsAchieved);
	}
	UE_LOG(LogDigetherMonster, Log, TEXT("몬스터 런 시작: 테이블 %s, 던전 파워 한도 %d"), *GetNameSafe(Table), DungeonPowerLimit);
	return true;
}

bool UDigetherMonsterDirectorSubsystem::BeginRunForStage(const FName StageId, const FGuid& RunId, const int32 GoalsAchieved)
{
	const UDigetherMonsterSettings& Settings = UDigetherMonsterSettings::Get();
	const TSoftObjectPtr<UDigetherMonsterSpawnTable>* Found = Settings.SpawnTables.Find(StageId);
	const TSoftObjectPtr<UDigetherMonsterSpawnTable>& Soft = Found ? *Found : Settings.DefaultSpawnTable;
	// 테이블은 작은 정의 에셋, 출발 확정 순간 동기 로드
	const UDigetherMonsterSpawnTable* Loaded = Soft.IsNull() ? nullptr : Soft.LoadSynchronous();
	if (!Loaded)
	{
		UE_LOG(LogDigetherMonster, Log, TEXT("목적지 %s의 몬스터 스폰 테이블이 없어 몬스터 없이 진행합니다."), *StageId.ToString());
	}
	return BeginRun(Loaded, RunId, GoalsAchieved, static_cast<int32>(GetTypeHash(RunId)));
}

void UDigetherMonsterDirectorSubsystem::NotifyMapReady(const FGuid& RunId, const TOptional<FVector> InElevatorArrivalLocation)
{
	if (!bActive || RunId != CurrentRunId || bHabitatsPlaced)
	{
		return;
	}
	ElevatorArrivalLocation = InElevatorArrivalLocation;
	PlaceHabitats(false);
}

void UDigetherMonsterDirectorSubsystem::NotifyMineArrival(const FGuid& RunId)
{
	UWorld* World = GetWorld();
	if (!bActive || RunId != CurrentRunId || bSpawnTimerRunning || !World || World->GetTimerManager().IsTimerActive(StartDelayTimer))
	{
		return;
	}
	const float Delay = UDigetherMonsterSettings::Get().SpawnTimerStartDelaySeconds;
	if (Delay <= 0.f)
	{
		StartSpawnTimer(RunId);
		return;
	}
	World->GetTimerManager().SetTimer(StartDelayTimer,
		FTimerDelegate::CreateUObject(this, &UDigetherMonsterDirectorSubsystem::StartSpawnTimer, RunId), Delay, false);
}

void UDigetherMonsterDirectorSubsystem::StartSpawnTimer(const FGuid RunId)
{
	UWorld* World = GetWorld();
	if (!bActive || RunId != CurrentRunId || bSpawnTimerRunning || !World)
	{
		return;
	}
	bSpawnTimerRunning = true;
	SpawnTimerStartTime = World->GetTimeSeconds();
	SpawnTimerOffset = 0.f;
	UE_LOG(LogDigetherMonster, Log, TEXT("스폰 타이머 시작"));
	if (Table)
	{
		World->GetTimerManager().SetTimer(EvaluationTimer,
			FTimerDelegate::CreateUObject(this, &UDigetherMonsterDirectorSubsystem::RunPeriodicEvaluation, RunId),
			FMath::Max(0.1f, Table->Curve.IntervalSeconds), true);
	}
}

void UDigetherMonsterDirectorSubsystem::EndRun(const FGuid& RunId)
{
	if (!RunId.IsValid() || RunId != CurrentRunId)
	{
		return;
	}
	TArray<TWeakObjectPtr<ADigetherMonsterCharacter>> ToRemove = LiveMonsters;
	for (const TWeakObjectPtr<ADigetherMonsterCharacter>& Weak : ToRemove)
	{
		if (ADigetherMonsterCharacter* Monster = Weak.Get())
		{
			Monster->RemoveFromRun(EDigetherMonsterRemovalReason::RunEnded);
		}
	}
	TArray<TWeakObjectPtr<AActor>> ToDestroy = RunActors;
	for (const TWeakObjectPtr<AActor>& Weak : ToDestroy)
	{
		if (AActor* Actor = Weak.Get())
		{
			Actor->Destroy();
		}
	}
	UE_LOG(LogDigetherMonster, Log, TEXT("몬스터 런 종료: 몬스터 %d, 부속 %d 정리"), ToRemove.Num(), ToDestroy.Num());
	ClearRunState();
}

void UDigetherMonsterDirectorSubsystem::RegisterRunActor(AActor* Actor)
{
	if (!bActive || !CurrentRunId.IsValid() || !IsValid(Actor))
	{
		return;
	}
	RunActors.RemoveAll([](const TWeakObjectPtr<AActor>& Weak) { return !Weak.IsValid(); });
	RunActors.AddUnique(Actor);
}

void UDigetherMonsterDirectorSubsystem::ClearRunState()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& Timers = World->GetTimerManager();
		Timers.ClearTimer(StartDelayTimer);
		Timers.ClearTimer(EvaluationTimer);
		for (TPair<int32, FPendingSpawn>& Pending : PendingSpawns)
		{
			Timers.ClearTimer(Pending.Value.Timer);
		}
	}
	PendingSpawns.Reset();
	LiveMonsters.Reset();
	ChargedPowerByMonster.Reset();
	RunActors.Reset();
	Ledger.Reset();
	Table = nullptr;
	DungeonPowerLimit = 0;
	bSpawnTimerRunning = false;
	SpawnTimerOffset = 0.f;
	ElevatorArrivalLocation.Reset();
	bHabitatsPlaced = false;
	CurrentRunId.Invalidate();
}

float UDigetherMonsterDirectorSubsystem::GetSpawnTimerSeconds() const
{
	const UWorld* World = GetWorld();
	if (!bSpawnTimerRunning || !World)
	{
		return 0.f;
	}
	return static_cast<float>(World->GetTimeSeconds() - SpawnTimerStartTime) + SpawnTimerOffset;
}

float UDigetherMonsterDirectorSubsystem::GetCurrentChance() const
{
	if (!Table || !bSpawnTimerRunning)
	{
		return 0.f;
	}
	const FDigetherMonsterSpawnCurve& Curve = Table->Curve;
	const float Seconds = GetSpawnTimerSeconds();
	if (Curve.OverrideCurve)
	{
		return FMath::Clamp(Curve.OverrideCurve->GetFloatValue(Seconds), 0.f, 1.f);
	}
	DigetherMonsterSpawnRules::FCurveParams Params;
	Params.GraceSeconds = Curve.GraceSeconds;
	Params.StartChance = Curve.StartChance;
	Params.MaxChance = Curve.MaxChance;
	Params.MaxChanceAtSeconds = Curve.MaxChanceAtSeconds;
	return DigetherMonsterSpawnRules::EvaluateChance(Params, Seconds);
}

void UDigetherMonsterDirectorSubsystem::RunPeriodicEvaluation(const FGuid RunId)
{
	if (!bActive || RunId != CurrentRunId || !Table)
	{
		return;
	}
#if !UE_BUILD_SHIPPING
	// 개발 자동 생성 정지, 타이머 경과는 그대로 흐름
	if (bDevAutoSpawnPaused)
	{
		UE_LOG(LogDigetherMonster, Verbose, TEXT("자동 생성 정지 중, 스폰 판정 건너뜀 t=%.1f"), GetSpawnTimerSeconds());
		return;
	}
#endif
	const float Chance = GetCurrentChance();
	// 갱도·던전은 같은 곡선을 쓰되 각각 판정(기획: 갱도·던전 각각)
	const float MineRoll = Random.FRand();
	const bool bMine = MineRoll < Chance && TrySpawnMinePeriodic();
	const float DungeonRoll = Random.FRand();
	const bool bDungeon = DungeonRoll < Chance && TrySpawnDungeon();
	UE_LOG(LogDigetherMonster, Verbose, TEXT("스폰 판정 t=%.1f 확률 %.3f 갱도 %.3f(%d) 던전 %.3f(%d)"),
		GetSpawnTimerSeconds(), Chance, MineRoll, bMine, DungeonRoll, bDungeon);
}

void UDigetherMonsterDirectorSubsystem::DevRunEvaluationNow()
{
	if (bActive && CurrentRunId.IsValid())
	{
		RunPeriodicEvaluation(CurrentRunId);
	}
}

bool UDigetherMonsterDirectorSubsystem::TrySpawnMinePeriodic()
{
	TArray<DigetherMonsterSpawnRules::FCandidate> Candidates;
	for (int32 Index = 0; Index < Table->MinePeriodicEntries.Num(); ++Index)
	{
		const FDigetherMonsterPeriodicEntry& Entry = Table->MinePeriodicEntries[Index];
		const FName Species = GetSpeciesId(Entry.Definition);
		DigetherMonsterSpawnRules::FCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.SourceIndex = Index;
		Candidate.Weight = Entry.Weight;
		Candidate.RunSpawned = Ledger.GetRunSpawned(Species);
		Candidate.RunSpawnCap = Entry.Definition ? Entry.Definition->RunSpawnCap : 0;
		Candidate.Alive = Ledger.GetAlive(Species);
		Candidate.MaxAlive = Entry.MaxAlive;
	}
	const TArray<DigetherMonsterSpawnRules::FCandidate> Filtered = DigetherMonsterSpawnRules::FilterCandidates(Candidates, -1);
	// 후보가 남으면 확률 성공은 반드시 생성 시도, 없으면 이 판정 종료(기획)
	const int32 Pick = DigetherMonsterSpawnRules::PickWeighted(Filtered, Random);
	if (Pick == INDEX_NONE)
	{
		return false;
	}
	UDigetherMonsterDefinition* Definition = Table->MinePeriodicEntries[Filtered[Pick].SourceIndex].Definition;
	ADigetherMonsterSpawnPoint* Point = PickPoint(Definition, NAME_None);
	if (!Point)
	{
		UE_LOG(LogDigetherMonster, Log, TEXT("갱도 주기형 %s: 허용되는 스폰 포인트가 없어 생성하지 않습니다."), *GetNameSafe(Definition));
		return false;
	}
	return ScheduleSpawn(Definition, Point, NAME_None);
}

bool UDigetherMonsterDirectorSubsystem::TrySpawnDungeon()
{
	TArray<FName> DungeonIds;
	CollectDungeonIds(DungeonIds);
	if (DungeonIds.IsEmpty())
	{
		return false;
	}
	// 던전 판정은 한 번, 종을 먼저 뽑고 그 종이 들어갈 수 있는 던전 중 하나를 고름(파워는 던전별)
	// 2026-11-11 결정은 '던전마다 파워 한도'만 정함, 한 판정에서 던전 선택 방식은 개발 임시 판단
	TArray<DigetherMonsterSpawnRules::FCandidate> Candidates;
	for (int32 Index = 0; Index < Table->DungeonEntries.Num(); ++Index)
	{
		const FDigetherMonsterPeriodicEntry& Entry = Table->DungeonEntries[Index];
		if (!Entry.Definition)
		{
			continue;
		}
		const FName Species = GetSpeciesId(Entry.Definition);
		DigetherMonsterSpawnRules::FCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.SourceIndex = Index;
		Candidate.Weight = Entry.Weight;
		Candidate.PowerCost = Entry.Definition->PowerLevel;
		Candidate.RunSpawned = Ledger.GetRunSpawned(Species);
		Candidate.RunSpawnCap = Entry.Definition->RunSpawnCap;
		Candidate.Alive = Ledger.GetAlive(Species);
	}
	// 남은 파워가 가장 큰 던전 기준으로 거름, 실제 던전은 아래에서 다시 확인
	int32 BestRemaining = 0;
	for (const FName DungeonId : DungeonIds)
	{
		BestRemaining = FMath::Max(BestRemaining, DungeonPowerLimit - Ledger.GetPowerUsed(DungeonId));
	}
	const TArray<DigetherMonsterSpawnRules::FCandidate> Filtered = DigetherMonsterSpawnRules::FilterCandidates(Candidates, BestRemaining);
	const int32 Pick = DigetherMonsterSpawnRules::PickWeighted(Filtered, Random);
	if (Pick == INDEX_NONE)
	{
		return false;
	}
	UDigetherMonsterDefinition* Definition = Table->DungeonEntries[Filtered[Pick].SourceIndex].Definition;
	TArray<TPair<FName, ADigetherMonsterSpawnPoint*>> Options;
	for (const FName DungeonId : DungeonIds)
	{
		if (DungeonPowerLimit - Ledger.GetPowerUsed(DungeonId) < Definition->PowerLevel)
		{
			continue;
		}
		if (ADigetherMonsterSpawnPoint* Point = PickPoint(Definition, DungeonId))
		{
			Options.Emplace(DungeonId, Point);
		}
	}
	if (Options.IsEmpty())
	{
		UE_LOG(LogDigetherMonster, Log, TEXT("던전 %s: 파워·거리 조건을 만족하는 배출구가 없어 생성하지 않습니다."), *GetNameSafe(Definition));
		return false;
	}
	const TPair<FName, ADigetherMonsterSpawnPoint*>& Chosen = Options[Random.RandRange(0, Options.Num() - 1)];
	return ScheduleSpawn(Definition, Chosen.Value, Chosen.Key);
}

void UDigetherMonsterDirectorSubsystem::PlaceHabitats(const bool bIgnoreChance)
{
	bHabitatsPlaced = true;
	if (!Table)
	{
		return;
	}
#if !UE_BUILD_SHIPPING
	// 맵 준비 통지는 런당 1회라 정지 중 건너뛴 배치는 재개해도 다시 하지 않음
	if (bDevAutoSpawnPaused)
	{
		UE_LOG(LogDigetherMonster, Log, TEXT("자동 생성 정지 중, 서식지 배치 건너뜀"));
		return;
	}
#endif
	TSet<const ADigetherMonsterSpawnPoint*> Used;
	for (const FDigetherMonsterHabitatEntry& Entry : Table->MineHabitatEntries)
	{
		if (!Entry.Definition)
		{
			continue;
		}
		const float Roll = Random.FRand();
		if (!bIgnoreChance && Roll >= Entry.Chance)
		{
			UE_LOG(LogDigetherMonster, Log, TEXT("서식지 %s 미등장(확률 %.2f, 굴림 %.3f)"), *GetNameSafe(Entry.Definition), Entry.Chance, Roll);
			continue;
		}
		const UDigetherMonsterSpawnPolicy* Policy = GetPolicy(EDigetherMonsterSpawnKind::Habitat);
		FDigetherSpawnPointQuery Query;
		Query.Definition = Entry.Definition;
		Query.ElevatorArrivalLocation = ElevatorArrivalLocation;
		Query.MinNestDistanceFromElevator = UDigetherMonsterSettings::Get().MinNestDistanceFromElevator;
		TArray<ADigetherMonsterSpawnPoint*> Allowed;
		for (TActorIterator<ADigetherMonsterSpawnPoint> It(GetWorld()); It; ++It)
		{
			if (!Used.Contains(*It) && Policy && Policy->IsPointAllowed(**It, Query))
			{
				Allowed.Add(*It);
			}
		}
		if (Allowed.IsEmpty())
		{
			UE_LOG(LogDigetherMonster, Warning, TEXT("서식지 %s: 허용되는 둥지 후보가 없습니다."), *GetNameSafe(Entry.Definition));
			continue;
		}
		ADigetherMonsterSpawnPoint* Point = Allowed[Random.RandRange(0, Allowed.Num() - 1)];
		Used.Add(Point);
		ScheduleSpawn(Entry.Definition, Point, NAME_None);
	}
}

void UDigetherMonsterDirectorSubsystem::DevForceHabitat()
{
	if (bActive && CurrentRunId.IsValid())
	{
		PlaceHabitats(true);
	}
}

void UDigetherMonsterDirectorSubsystem::DevAdvanceSpawnTimer(const float Seconds)
{
	if (bSpawnTimerRunning && FMath::IsFinite(Seconds) && Seconds > 0.f)
	{
		SpawnTimerOffset += Seconds;
	}
}

EDigetherMonsterSpawnResult UDigetherMonsterDirectorSubsystem::DevSpawn(UDigetherMonsterDefinition* Definition, const FTransform& FallbackTransform)
{
	if (!bActive)
	{
		return EDigetherMonsterSpawnResult::NotAuthority;
	}
	if (!CurrentRunId.IsValid())
	{
		// 시험 맵 직접 실행용 임시 런
		BeginRun(nullptr, FGuid::NewGuid(), 0, 0);
	}
	FName DungeonId;
	if (Definition && Definition->Zone == EDigetherMonsterZone::Dungeon)
	{
		ADigetherZoneVolume::GetZoneKindAt(GetWorld(), FallbackTransform.GetLocation(), &DungeonId);
	}
	if (ADigetherMonsterSpawnPoint* Point = PickPoint(Definition, NAME_None))
	{
		Point->GetZone(&DungeonId);
		return SpawnMonsterAt(Definition, Point->GetActorTransform(), DungeonId);
	}
	return SpawnMonsterAt(Definition, FallbackTransform, DungeonId);
}

#if !UE_BUILD_SHIPPING
EDigetherMonsterSpawnResult UDigetherMonsterDirectorSubsystem::DevSpawnWithOptions(UDigetherMonsterDefinition* Definition,
	const FTransform& Transform, const FDevSpawnOptions& Options, ADigetherMonsterCharacter** OutMonster)
{
	if (OutMonster)
	{
		*OutMonster = nullptr;
	}
	if (!bActive)
	{
		return EDigetherMonsterSpawnResult::NotAuthority;
	}
	if (!IsValid(Definition))
	{
		return EDigetherMonsterSpawnResult::InvalidDefinition;
	}
	if (!CurrentRunId.IsValid())
	{
		// DevSpawn과 같은 시험 맵 직접 실행용 임시 런
		BeginRun(nullptr, FGuid::NewGuid(), 0, 0);
	}
	FTransform SpawnTransform = Transform;
	FName DungeonId;
	ADigetherMonsterSpawnPoint* Point = Options.bForceTransform ? nullptr : PickPoint(Definition, NAME_None);
	if (Point)
	{
		SpawnTransform = Point->GetActorTransform();
		Point->GetZone(&DungeonId);
	}
	else if (Definition->Zone == EDigetherMonsterZone::Dungeon)
	{
		ADigetherZoneVolume::GetZoneKindAt(GetWorld(), SpawnTransform.GetLocation(), &DungeonId);
	}
	if (Options.DungeonId.IsSet())
	{
		DungeonId = Options.DungeonId.GetValue();
	}
	const int32 ChargedPower = Options.bChargePower ? Definition->PowerLevel : 0;
	return SpawnMonsterCharged(Definition, SpawnTransform, DungeonId, ChargedPower, OutMonster);
}

int32 UDigetherMonsterDirectorSubsystem::CancelPendingSpawnsForDev()
{
	UWorld* World = GetWorld();
	int32 Cancelled = 0;
	for (TPair<int32, FPendingSpawn>& Entry : PendingSpawns)
	{
		FPendingSpawn& Pending = Entry.Value;
		if (World)
		{
			World->GetTimerManager().ClearTimer(Pending.Timer);
		}
		// 이전 런 예약은 새 런 장부에 없으므로 현재 런 것만 반환
		const UDigetherMonsterDefinition* Definition = Pending.Definition.Get();
		if (Pending.RunId == CurrentRunId && Definition)
		{
			Ledger.RecordCancelledSpawn(GetSpeciesId(Definition), Pending.DungeonId, Pending.ChargedPower);
		}
		++Cancelled;
	}
	PendingSpawns.Reset();
	return Cancelled;
}

int32 UDigetherMonsterDirectorSubsystem::DevClearMonsters(int32* OutCancelledPending)
{
	const int32 Cancelled = bActive ? CancelPendingSpawnsForDev() : 0;
	if (OutCancelledPending)
	{
		*OutCancelledPending = Cancelled;
	}
	if (!bActive)
	{
		return 0;
	}
	// 제거 통지가 LiveMonsters를 바꾸므로 사본 순회
	const TArray<TWeakObjectPtr<ADigetherMonsterCharacter>> ToRemove = LiveMonsters;
	int32 Removed = 0;
	for (const TWeakObjectPtr<ADigetherMonsterCharacter>& Weak : ToRemove)
	{
		ADigetherMonsterCharacter* Monster = Weak.Get();
		if (Monster && !Monster->IsRemoved())
		{
			Monster->RemoveFromRun(EDigetherMonsterRemovalReason::Despawned);
			++Removed;
		}
	}
	UE_LOG(LogDigetherMonster, Log, TEXT("개발 전부 제거: 몬스터 %d, 대기 예고 %d 취소"), Removed, Cancelled);
	return Removed;
}

int32 UDigetherMonsterDirectorSubsystem::DevSetAutoSpawnPaused(const bool bPaused)
{
	bDevAutoSpawnPaused = bPaused;
	const int32 Cancelled = bPaused && bActive ? CancelPendingSpawnsForDev() : 0;
	UE_LOG(LogDigetherMonster, Log, TEXT("개발 자동 생성 %s, 대기 예고 %d 취소"), bPaused ? TEXT("정지") : TEXT("재개"), Cancelled);
	return Cancelled;
}

void UDigetherMonsterDirectorSubsystem::GetDungeonIdsForDev(TArray<FName>& OutDungeonIds) const
{
	CollectDungeonIds(OutDungeonIds);
	for (const TPair<FName, int32>& Used : Ledger.DungeonPowerUsed)
	{
		OutDungeonIds.AddUnique(Used.Key);
	}
	OutDungeonIds.Sort(FNameLexicalLess());
}
#endif

bool UDigetherMonsterDirectorSubsystem::ScheduleSpawn(UDigetherMonsterDefinition* Definition, ADigetherMonsterSpawnPoint* Point, const FName DungeonId)
{
	if (!Definition || !Point)
	{
		return false;
	}
	const float Telegraph = Definition->SpawnTelegraphSeconds;
	if (Telegraph <= 0.f)
	{
		return SpawnMonsterAt(Definition, Point->GetActorTransform(), DungeonId) == EDigetherMonsterSpawnResult::Spawned;
	}
	// 예고 중에도 파워·상한을 차지하도록 먼저 예약
	const FName Species = GetSpeciesId(Definition);
	Ledger.RecordSpawn(Species, DungeonId, Definition->PowerLevel);
	const int32 PendingId = NextPendingId++;
	FPendingSpawn& Pending = PendingSpawns.Add(PendingId);
	Pending.RunId = CurrentRunId;
	Pending.Definition = Definition;
	Pending.Point = Point;
	Pending.DungeonId = DungeonId;
	Pending.ChargedPower = Definition->PowerLevel;
	Point->MulticastSpawnTelegraph(Telegraph);
	GetWorld()->GetTimerManager().SetTimer(Pending.Timer,
		FTimerDelegate::CreateUObject(this, &UDigetherMonsterDirectorSubsystem::FinishPendingSpawn, PendingId), Telegraph, false);
	UE_VLOG_LOCATION(this, LogDigetherMonster, Log, Point->GetActorLocation(), 50.f, FColor::Orange, TEXT("Telegraph %s"), *Species.ToString());
	return true;
}

void UDigetherMonsterDirectorSubsystem::FinishPendingSpawn(const int32 PendingId)
{
	FPendingSpawn Pending;
	if (!PendingSpawns.RemoveAndCopyValue(PendingId, Pending))
	{
		return;
	}
	UDigetherMonsterDefinition* Definition = Pending.Definition.Get();
	if (Pending.RunId != CurrentRunId || !Definition)
	{
		return;
	}
	const FName Species = GetSpeciesId(Definition);
#if !UE_BUILD_SHIPPING
	// 정지를 켤 때 대기 예고는 모두 취소되지만 같은 프레임 순서 차이 대비
	if (bDevAutoSpawnPaused)
	{
		Ledger.RecordCancelledSpawn(Species, Pending.DungeonId, Pending.ChargedPower);
		UE_LOG(LogDigetherMonster, Log, TEXT("자동 생성 정지 중, 예고된 %s 생성 취소"), *Species.ToString());
		return;
	}
#endif
	EDigetherMonsterSpawnResult Result = EDigetherMonsterSpawnResult::NoSpawnPoint;
	ADigetherMonsterCharacter* Monster = nullptr;
	if (const ADigetherMonsterSpawnPoint* Point = Pending.Point.Get())
	{
		Monster = SpawnActorFor(Definition, Point->GetActorTransform(), Pending.DungeonId, Result);
	}
	if (!Monster)
	{
		Ledger.RecordCancelledSpawn(Species, Pending.DungeonId, Pending.ChargedPower);
		UE_LOG(LogDigetherMonster, Warning, TEXT("예고 후 %s 생성 실패(%s), 예약 반환"), *Species.ToString(), *UEnum::GetValueAsString(Result));
		return;
	}
	TrackMonster(Monster, Pending.ChargedPower);
}

EDigetherMonsterSpawnResult UDigetherMonsterDirectorSubsystem::SpawnMonsterAt(UDigetherMonsterDefinition* Definition,
	const FTransform& Transform, const FName DungeonId, ADigetherMonsterCharacter** OutMonster)
{
	return SpawnMonsterCharged(Definition, Transform, DungeonId, IsValid(Definition) ? Definition->PowerLevel : 0, OutMonster);
}

EDigetherMonsterSpawnResult UDigetherMonsterDirectorSubsystem::SpawnMonsterCharged(UDigetherMonsterDefinition* Definition,
	const FTransform& Transform, const FName DungeonId, const int32 ChargedPower, ADigetherMonsterCharacter** OutMonster)
{
	if (OutMonster)
	{
		*OutMonster = nullptr;
	}
	if (!bActive)
	{
		return EDigetherMonsterSpawnResult::NotAuthority;
	}
	if (!CurrentRunId.IsValid())
	{
		return EDigetherMonsterSpawnResult::NoActiveRun;
	}
	EDigetherMonsterSpawnResult Result = EDigetherMonsterSpawnResult::SpawnFailed;
	ADigetherMonsterCharacter* Monster = SpawnActorFor(Definition, Transform, DungeonId, Result);
	if (!Monster)
	{
		return Result;
	}
	Ledger.RecordSpawn(GetSpeciesId(Definition), DungeonId, ChargedPower);
	TrackMonster(Monster, ChargedPower);
	if (OutMonster)
	{
		*OutMonster = Monster;
	}
	return EDigetherMonsterSpawnResult::Spawned;
}

ADigetherMonsterCharacter* UDigetherMonsterDirectorSubsystem::SpawnActorFor(UDigetherMonsterDefinition* Definition,
	const FTransform& Transform, const FName DungeonId, EDigetherMonsterSpawnResult& OutResult)
{
	TArray<FText> Problems;
	if (!IsValid(Definition) || !Definition->CollectSpawnProblems(Problems))
	{
		for (const FText& Problem : Problems)
		{
			UE_LOG(LogDigetherMonster, Warning, TEXT("정의 %s 생성 거절: %s"), *GetNameSafe(Definition), *Problem.ToString());
		}
		OutResult = EDigetherMonsterSpawnResult::InvalidDefinition;
		return nullptr;
	}
	UClass* Class = Definition->MonsterClass.LoadSynchronous();
	UWorld* World = GetWorld();
	if (!Class || !World)
	{
		OutResult = EDigetherMonsterSpawnResult::InvalidDefinition;
		return nullptr;
	}
	ADigetherMonsterCharacter* Monster = World->SpawnActorDeferred<ADigetherMonsterCharacter>(Class, Transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Monster)
	{
		OutResult = EDigetherMonsterSpawnResult::SpawnFailed;
		return nullptr;
	}
	if (!Monster->InitializeFromDefinition(Definition, CurrentRunId, DungeonId))
	{
		Monster->Destroy();
		OutResult = EDigetherMonsterSpawnResult::InvalidDefinition;
		return nullptr;
	}
	Monster->FinishSpawning(Transform);
	if (!IsValid(Monster))
	{
		OutResult = EDigetherMonsterSpawnResult::SpawnFailed;
		return nullptr;
	}
	OutResult = EDigetherMonsterSpawnResult::Spawned;
	UE_LOG(LogDigetherMonster, Log, TEXT("몬스터 생성: %s at %s 던전 %s"), *Definition->GetName(), *Transform.GetLocation().ToString(), *DungeonId.ToString());
	return Monster;
}

void UDigetherMonsterDirectorSubsystem::TrackMonster(ADigetherMonsterCharacter* Monster, const int32 ChargedPower)
{
	LiveMonsters.Add(Monster);
	ChargedPowerByMonster.Add(Monster, ChargedPower);
	Monster->OnRemoved.AddUObject(this, &UDigetherMonsterDirectorSubsystem::HandleMonsterRemoved);
}

void UDigetherMonsterDirectorSubsystem::HandleMonsterRemoved(ADigetherMonsterCharacter* Monster, const EDigetherMonsterRemovalReason Reason)
{
	const int32 Removed = LiveMonsters.RemoveAll([Monster](const TWeakObjectPtr<ADigetherMonsterCharacter>& Weak) { return Weak.Get() == Monster; });
	int32 ChargedPower = 0;
	ChargedPowerByMonster.RemoveAndCopyValue(Monster, ChargedPower);
	if (Removed == 0 || !Monster || Monster->GetRunId() != CurrentRunId)
	{
		return;
	}
	const UDigetherMonsterDefinition* Definition = Monster->GetDefinition();
	// 처치·도주 소멸 시 생성 때 실제 부과한 파워 반환(기획), 탐사 종료 정리는 장부를 곧 비움
	Ledger.RecordRemoval(GetSpeciesId(Definition), Monster->GetDungeonId(), ChargedPower);
	UE_LOG(LogDigetherMonster, Log, TEXT("몬스터 제거: %s (%s)"), *GetNameSafe(Monster), *UEnum::GetValueAsString(Reason));
}

ADigetherMonsterSpawnPoint* UDigetherMonsterDirectorSubsystem::PickPoint(const UDigetherMonsterDefinition* Definition, const FName DungeonId) const
{
	const UDigetherMonsterSpawnPolicy* Policy = Definition ? GetPolicy(Definition->SpawnKind) : nullptr;
	if (!Policy)
	{
		return nullptr;
	}
	FDigetherSpawnPointQuery Query;
	Query.Definition = Definition;
	Query.DungeonId = DungeonId;
	Query.ElevatorArrivalLocation = ElevatorArrivalLocation;
	Query.MinNestDistanceFromElevator = UDigetherMonsterSettings::Get().MinNestDistanceFromElevator;
	CollectLivingPlayerLocations(Query.LivingPlayerLocations);
	TArray<ADigetherMonsterSpawnPoint*> Allowed;
	for (TActorIterator<ADigetherMonsterSpawnPoint> It(GetWorld()); It; ++It)
	{
		if (Policy->IsPointAllowed(**It, Query))
		{
			Allowed.Add(*It);
		}
	}
	return Allowed.IsEmpty() ? nullptr : Allowed[Random.RandRange(0, Allowed.Num() - 1)];
}

void UDigetherMonsterDirectorSubsystem::CollectLivingPlayerLocations(TArray<FVector>& OutLocations) const
{
	OutLocations.Reset();
	if (const UDigetherMonsterSenseSubsystem* Sense = GetWorld()->GetSubsystem<UDigetherMonsterSenseSubsystem>())
	{
		TArray<FDigetherMonsterTarget> Targets;
		Sense->CollectLivingTargets(Targets);
		for (const FDigetherMonsterTarget& Target : Targets)
		{
			OutLocations.Add(Target.Center);
		}
	}
}

void UDigetherMonsterDirectorSubsystem::CollectDungeonIds(TArray<FName>& OutDungeonIds) const
{
	OutDungeonIds.Reset();
	for (TActorIterator<ADigetherZoneVolume> It(GetWorld()); It; ++It)
	{
		if (It->Zone == EDigetherMonsterZone::Dungeon && !It->ZoneId.IsNone())
		{
			OutDungeonIds.AddUnique(It->ZoneId);
		}
	}
}

const UDigetherMonsterSpawnPolicy* UDigetherMonsterDirectorSubsystem::GetPolicy(const EDigetherMonsterSpawnKind Kind) const
{
	const TObjectPtr<UDigetherMonsterSpawnPolicy>* Found = Policies.Find(Kind);
	return Found ? Found->Get() : nullptr;
}

int32 UDigetherMonsterDirectorSubsystem::GetAliveCount(const UDigetherMonsterDefinition* Definition) const
{
	return Ledger.GetAlive(GetSpeciesId(Definition));
}

int32 UDigetherMonsterDirectorSubsystem::GetRunSpawnedCount(const UDigetherMonsterDefinition* Definition) const
{
	return Ledger.GetRunSpawned(GetSpeciesId(Definition));
}

void UDigetherMonsterDirectorSubsystem::GetLiveMonsters(TArray<ADigetherMonsterCharacter*>& OutMonsters) const
{
	OutMonsters.Reset();
	for (const TWeakObjectPtr<ADigetherMonsterCharacter>& Weak : LiveMonsters)
	{
		if (ADigetherMonsterCharacter* Monster = Weak.Get())
		{
			OutMonsters.Add(Monster);
		}
	}
}

FName UDigetherMonsterDirectorSubsystem::GetSpeciesId(const UDigetherMonsterDefinition* Definition)
{
	return Definition ? Definition->GetFName() : NAME_None;
}
