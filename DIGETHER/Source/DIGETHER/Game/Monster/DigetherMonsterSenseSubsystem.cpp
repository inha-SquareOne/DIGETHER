#include "Game/Monster/DigetherMonsterSenseSubsystem.h"

#include "Engine/World.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Monster/DigetherMonsterCharacter.h"
#include "Game/Monster/DigetherMonsterDefinition.h"
#include "Game/Monster/DigetherMonsterLog.h"
#include "Game/Monster/DigetherMonsterSenseRules.h"
#include "Game/Monster/DigetherMonsterSettings.h"
#include "Game/Monster/DigetherMonsterSpeciesComponent.h"
#include "Game/Settings/DigetherGameUserSettings.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "VisualLogger/VisualLogger.h"

namespace
{
	constexpr int32 MaxStepsPerFrame = 3;
	constexpr float DefaultAspectRatio = 16.f / 9.f;

	// 유니티 빌드에서 Director의 같은 이름 익명 함수와 충돌하지 않도록 고유 이름
	bool IsSenseGameWorld(const UWorld* World)
	{
		return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
	}
}

bool UDigetherMonsterSenseSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer) && IsSenseGameWorld(Cast<UWorld>(Outer));
}

void UDigetherMonsterSenseSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Activate();
}

bool UDigetherMonsterSenseSubsystem::Activate()
{
	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		bActive = false;
		return false;
	}
	bActive = true;
	return true;
}

void UDigetherMonsterSenseSubsystem::Deinitialize()
{
	bActive = false;
	Monsters.Reset();
	CameraSamples.Reset();
	PlayerKeyResolver = nullptr;
	Super::Deinitialize();
}

TStatId UDigetherMonsterSenseSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDigetherMonsterSenseSubsystem, STATGROUP_Tickables);
}

void UDigetherMonsterSenseSubsystem::SetPlayerKeyResolver(FPlayerKeyResolver Resolver)
{
	PlayerKeyResolver = MoveTemp(Resolver);
}

FString UDigetherMonsterSenseSubsystem::ResolvePlayerKey(const APlayerState* PlayerState) const
{
	if (!IsValid(PlayerState))
	{
		return FString();
	}
	if (PlayerKeyResolver)
	{
		return PlayerKeyResolver(PlayerState);
	}
	return PlayerState->GetUniqueId().IsValid() ? PlayerState->GetUniqueId().ToString() : PlayerState->GetPlayerName();
}

void UDigetherMonsterSenseSubsystem::Tick(const float DeltaTime)
{
	if (!bActive)
	{
		return;
	}
	const float Interval = FMath::Max(0.02f, UDigetherMonsterSettings::Get().SenseIntervalSeconds);
	Accumulator += FMath::Max(0.f, DeltaTime);
	int32 Steps = 0;
	// 프레임과 무관한 고정 주기, 긴 프레임에서도 한 프레임 최대 3회로 제한
	while (Accumulator >= Interval && Steps < MaxStepsPerFrame)
	{
		Accumulator -= Interval;
		RunStep();
		++Steps;
	}
	if (Steps == MaxStepsPerFrame && Accumulator >= Interval)
	{
		UE_LOG(LogDigetherMonster, Verbose, TEXT("감각 판정 주기 밀림: 남은 누적 %.3f초 폐기"), Accumulator);
		Accumulator = FMath::Fmod(Accumulator, Interval);
	}
}

void UDigetherMonsterSenseSubsystem::RunStep()
{
	StepTraceCount = 0;
	Monsters.RemoveAll([](const FMonsterEntry& Entry) { return !Entry.Monster.IsValid() || Entry.Monster->IsRemoved(); });

	TArray<FDigetherMonsterTarget> Targets;
	CollectLivingTargets(Targets);
	const double Now = GetWorld()->GetTimeSeconds();

	// 종 처리기가 몬스터를 생성·제거하면 배열이 바뀌므로 주기 시작 시점 목록으로 순회하고 매번 다시 찾음
	TArray<TWeakObjectPtr<ADigetherMonsterCharacter>, TInlineAllocator<8>> Snapshot;
	for (const FMonsterEntry& Entry : Monsters)
	{
		Snapshot.Add(Entry.Monster);
	}
	for (const TWeakObjectPtr<ADigetherMonsterCharacter>& Weak : Snapshot)
	{
		ADigetherMonsterCharacter* Monster = Weak.Get();
		FMonsterEntry* EntryPtr = IsValid(Monster) && !Monster->IsRemoved() ? FindEntry(Monster) : nullptr;
		if (!EntryPtr)
		{
			continue;
		}
		FMonsterEntry& Entry = *EntryPtr;
		const UDigetherMonsterDefinition* Definition = Monster->GetDefinition();
		if (Entry.bSightEnabled && Definition && Definition->Sight.Range > 0.f && Definition->Sight.FieldOfViewDegrees > 0.f)
		{
			const FVector Origin = Monster->GetSenseOrigin();
			const FVector Forward = Monster->GetActorForwardVector();
			for (const FDigetherMonsterTarget& Target : Targets)
			{
				if (!DigetherMonsterSenseRules::IsInViewCone(Origin, Forward, Target.Center,
					Definition->Sight.FieldOfViewDegrees * 0.5f, Definition->Sight.Range))
				{
					continue;
				}
				const AActor* Ignored[] = { Monster, Target.Pawn.Get() };
				if (!HasLineOfSight(Origin, Target.Center, Ignored))
				{
					continue;
				}
				FDigetherMonsterStimulusEvent Event;
				Event.Cause = EDigetherMonsterStimulus::Sight;
				Event.Location = Target.Center;
				Event.InstigatorKey = Target.PlayerKey;
				Event.ServerTime = Now;
				Entry.PendingStimuli.Add(Event);
				UE_VLOG_SEGMENT(Monster, LogDigetherMonster, Verbose, Origin, Target.Center, FColor::Yellow, TEXT("Sight %s"), *Target.PlayerKey);
			}
		}

		if (Entry.PendingStimuli.IsEmpty())
		{
			continue;
		}
		// 전달 중 배열이 바뀌어도 Entry 참조를 다시 쓰지 않도록 먼저 꺼냄
		TArray<FDigetherMonsterStimulusEvent> Events = MoveTemp(Entry.PendingStimuli);
		Entry.PendingStimuli.Reset();
		EntryPtr = nullptr;
		TInlineComponentArray<UDigetherMonsterSpeciesComponent*> SpeciesComponents(Monster);
		for (UDigetherMonsterSpeciesComponent* Species : SpeciesComponents)
		{
			Species->HandleStimuli(Events);
		}
	}

	LastStepTraceCount = StepTraceCount;
	if (StepTraceCount > UDigetherMonsterSettings::Get().TraceWarningPerStep)
	{
		UE_LOG(LogDigetherMonster, Warning, TEXT("감각 판정 한 주기 트레이스 %d회, 경고 기준 %d 초과(판정은 미루지 않음)"),
			StepTraceCount, UDigetherMonsterSettings::Get().TraceWarningPerStep);
	}
}

UDigetherMonsterSenseSubsystem::FMonsterEntry* UDigetherMonsterSenseSubsystem::FindEntry(const ADigetherMonsterCharacter* Monster)
{
	return Monsters.FindByPredicate([Monster](const FMonsterEntry& Entry) { return Entry.Monster.Get() == Monster; });
}

void UDigetherMonsterSenseSubsystem::RegisterMonster(ADigetherMonsterCharacter* Monster)
{
	if (!bActive || !IsValid(Monster) || FindEntry(Monster))
	{
		return;
	}
	FMonsterEntry& Entry = Monsters.AddDefaulted_GetRef();
	Entry.Monster = Monster;
}

void UDigetherMonsterSenseSubsystem::UnregisterMonster(ADigetherMonsterCharacter* Monster)
{
	Monsters.RemoveAll([Monster](const FMonsterEntry& Entry) { return Entry.Monster.Get() == Monster; });
}

void UDigetherMonsterSenseSubsystem::SetSightEnabled(ADigetherMonsterCharacter* Monster, const bool bEnabled)
{
	if (FMonsterEntry* Entry = FindEntry(Monster))
	{
		Entry->bSightEnabled = bEnabled;
	}
}

void UDigetherMonsterSenseSubsystem::ReportStimulus(ADigetherMonsterCharacter* Monster, const FDigetherMonsterStimulusEvent& Event)
{
	if (!bActive)
	{
		return;
	}
	if (FMonsterEntry* Entry = FindEntry(Monster))
	{
		Entry->PendingStimuli.Add(Event);
	}
}

bool UDigetherMonsterSenseSubsystem::SubmitCameraSample(APlayerController* Controller, const FDigetherCameraSample& Sample)
{
	if (!bActive || !IsValid(Controller))
	{
		return false;
	}
	FDigetherReceivedCameraSample& Stored = CameraSamples.FindOrAdd(Controller);
	// 비신뢰 전송이라 순서가 뒤바뀔 수 있음, 이전보다 큰 순번만 수용(재접속 시 Controller가 바뀌어 0부터 다시 시작)
	if (Stored.bValid && Sample.Sequence <= Stored.Sample.Sequence)
	{
		return false;
	}
	FDigetherCameraSample Clean = Sample;
	const bool bWasClean = DigetherMonsterSenseRules::SanitizeSample(Clean,
		UDigetherGameUserSettings::MinFieldOfView, UDigetherGameUserSettings::MaxFieldOfView,
		UDigetherMonsterSettings::Get().MaxAspectRatio, UDigetherGameUserSettings::DefaultFieldOfView, DefaultAspectRatio);
	if (!bWasClean)
	{
		UE_LOG(LogDigetherMonster, Verbose, TEXT("%s: 카메라 표본 값 보정"), *GetNameSafe(Controller));
	}
	Stored.Sample = Clean;
	Stored.ServerReceiveTime = GetWorld()->GetTimeSeconds();
	Stored.bValid = true;
	Stored.bWarnedStale = false;
	return true;
}

bool UDigetherMonsterSenseSubsystem::GetCameraSample(const APlayerController* Controller, FDigetherCameraSample& OutSample) const
{
	const FDigetherReceivedCameraSample* Stored = CameraSamples.Find(Controller);
	if (!Stored || !Stored->bValid)
	{
		return false;
	}
	const double Age = GetWorld()->GetTimeSeconds() - Stored->ServerReceiveTime;
	if (Age > UDigetherMonsterSettings::Get().CameraSampleMaxAgeSeconds)
	{
		// 끊김 구간마다 한 번만 경고, 새 표본을 받으면 다시 경고 가능
		if (!Stored->bWarnedStale)
		{
			Stored->bWarnedStale = true;
			UE_LOG(LogDigetherMonster, Warning, TEXT("%s: 카메라 표본이 %.2f초 지나 '보지 않음'으로 처리합니다."), *GetNameSafe(Controller), Age);
		}
		return false;
	}
	// 위치는 서버 Pawn 기준(조작 방지), Pawn이 없으면 클라이언트 위치를 믿지 않고 판정하지 않음
	const APawn* Pawn = Controller->GetPawn();
	if (!Pawn)
	{
		return false;
	}
	OutSample = Stored->Sample;
	OutSample.Location = Pawn->GetPawnViewLocation();
	return true;
}

void UDigetherMonsterSenseSubsystem::CollectLivingTargets(TArray<FDigetherMonsterTarget>& OutTargets) const
{
	OutTargets.Reset();
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		APawn* Pawn = IsValid(PlayerState) ? PlayerState->GetPawn() : nullptr;
		if (!IsValid(Pawn) || PlayerState->IsOnlyASpectator())
		{
			continue;
		}
		if (const UDigetherLifeStateComponent* Life = PlayerState->FindComponentByClass<UDigetherLifeStateComponent>())
		{
			const FDigetherLifePublicState Public = Life->CaptureState().PublicState;
			if (Public.Life != EDigetherLifeState::Active || Public.bSpectating)
			{
				continue;
			}
		}
		FDigetherMonsterTarget& Target = OutTargets.AddDefaulted_GetRef();
		Target.Pawn = Pawn;
		Target.Controller = Cast<APlayerController>(Pawn->GetController());
		Target.PlayerKey = ResolvePlayerKey(PlayerState);
		Target.Center = Pawn->GetActorLocation();
	}
}

bool UDigetherMonsterSenseSubsystem::HasLineOfSight(const FVector& From, const FVector& To, const TConstArrayView<const AActor*> IgnoredActors) const
{
	++StepTraceCount;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DigetherMonsterSight), false);
	for (const AActor* Actor : IgnoredActors)
	{
		if (Actor)
		{
			Params.AddIgnoredActor(Actor);
		}
	}
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, From, To, UDigetherMonsterSettings::Get().SightTraceChannel, Params);
}

bool UDigetherMonsterSenseSubsystem::IsPointVisibleToPlayer(const APlayerController* Controller, const FVector& Point,
	const float MaxDistance, const TConstArrayView<const AActor*> IgnoredActors) const
{
	FDigetherCameraSample Sample;
	if (!GetCameraSample(Controller, Sample) || !DigetherMonsterSenseRules::IsPointOnScreen(Sample, Point, MaxDistance))
	{
		return false;
	}
	TArray<const AActor*, TInlineAllocator<4>> Ignored(IgnoredActors);
	Ignored.Add(Controller->GetPawn());
	return HasLineOfSight(Sample.Location, Point, Ignored);
}

float UDigetherMonsterSenseSubsystem::GetAngleFromPlayerView(const APlayerController* Controller, const FVector& Point) const
{
	FDigetherCameraSample Sample;
	return GetCameraSample(Controller, Sample) ? DigetherMonsterSenseRules::AngleFromViewCenter(Sample, Point) : 180.f;
}
