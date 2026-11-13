#include "Game/Monster/Spider/DigetherSpiderNestActor.h"

#include "Engine/World.h"
#include "Game/Monster/DigetherMonsterCharacter.h"
#include "Game/Monster/DigetherMonsterLog.h"
#include "Game/Monster/DigetherMonsterSenseSubsystem.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

ADigetherSpiderNestActor::ADigetherSpiderNestActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ADigetherSpiderNestActor::InitializeNest(ADigetherMonsterCharacter* InSpider, const FVector& InCenter, const float InRadius,
	const FDigetherGroundSpiderParams& Params)
{
	Spider = InSpider;
	Center = InCenter;
	Radius = InRadius;
	BaseCount = Params.WebBaseCount;
	RefillSeconds = Params.WebRefillSeconds;
	SlowMultiplier = Params.WebSlowMultiplier;
	SurfaceTraceDistance = Params.WebSurfaceTraceDistance;
	WebClass = Params.WebClass.LoadSynchronous();
	Random.Initialize(static_cast<int32>(GetTypeHash(InCenter)));
	if (!WebClass || BaseCount <= 0)
	{
		return;
	}
	// 처음에 기준량까지 배치, 이후 부족하면 주기마다 1개(개발 시험값)
	for (int32 Attempt = 0; Attempt < BaseCount * 3 && Webs.Num() < BaseCount; ++Attempt)
	{
		TrySpawnWeb();
	}
	GetWorldTimerManager().SetTimer(RefillTimer, this, &ADigetherSpiderNestActor::Refill, FMath::Max(1.f, RefillSeconds), true);
}

void ADigetherSpiderNestActor::Refill()
{
	Webs.RemoveAll([](const TWeakObjectPtr<ADigetherSpiderWebActor>& Weak) { return !Weak.IsValid(); });
	if (Webs.Num() < BaseCount)
	{
		// 한 번 판정에 실패해도 다음 주기에 다시 시도
		for (int32 Attempt = 0; Attempt < 4 && !TrySpawnWeb(); ++Attempt)
		{
		}
	}
}

bool ADigetherSpiderNestActor::TrySpawnWeb()
{
	UWorld* World = GetWorld();
	if (!World || !WebClass)
	{
		return false;
	}
	// 둥지 영역 안 임의 지점에서 임의 방향으로 면 탐색(통로·모서리·방사·늘어진형 분류는 아트 단계에서 정함)
	const float Angle = Random.FRandRange(0.f, 2.f * UE_PI);
	const float Distance = FMath::Sqrt(Random.FRand()) * Radius * 0.8f;
	const FVector Origin = Center + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 100.f);
	const FVector Direction = Random.VRand();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(DigetherSpiderWeb), false, Spider.Get());
	FHitResult Hit;
	// 채널 트레이스는 Pawn 캡슐에도 막히므로 정적 지형 오브젝트만 대상
	if (!World->LineTraceSingleByObjectType(Hit, Origin, Origin + Direction * SurfaceTraceDistance, FCollisionObjectQueryParams(ECC_WorldStatic), Query))
	{
		return false;
	}
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADigetherSpiderWebActor* Web = World->SpawnActor<ADigetherSpiderWebActor>(WebClass,
		Hit.ImpactPoint + Hit.ImpactNormal * 2.f, Hit.ImpactNormal.Rotation(), Spawn);
	if (!Web)
	{
		return false;
	}
	Web->InitializeWeb(this, SlowMultiplier);
	Webs.Add(Web);
	return true;
}

void ADigetherSpiderNestActor::HandleWebStepped(ADigetherSpiderWebActor* Web, APawn* Pawn)
{
	ReportToSpider(EDigetherMonsterStimulus::WebVibration, Pawn ? Pawn->GetActorLocation() : Web->GetActorLocation(), Pawn);
}

void ADigetherSpiderNestActor::HandleWebRemoved(ADigetherSpiderWebActor* Web, const EDigetherSpiderWebRemoval Reason, AController* ByController)
{
	Webs.RemoveAll([Web](const TWeakObjectPtr<ADigetherSpiderWebActor>& Weak) { return !Weak.IsValid() || Weak.Get() == Web; });
	// 지형 소멸로 사라지면 감지하지 않음(기획)
	if (Reason == EDigetherSpiderWebRemoval::Attacked && Web)
	{
		ReportToSpider(EDigetherMonsterStimulus::WebRemoved, Web->GetActorLocation(), ByController ? ByController->GetPawn() : nullptr);
	}
}

void ADigetherSpiderNestActor::ReportToSpider(const EDigetherMonsterStimulus Cause, const FVector& Location, const APawn* ByPawn)
{
	ADigetherMonsterCharacter* OwningSpider = Spider.Get();
	UDigetherMonsterSenseSubsystem* Sense = GetWorld() ? GetWorld()->GetSubsystem<UDigetherMonsterSenseSubsystem>() : nullptr;
	if (!OwningSpider || !Sense)
	{
		return;
	}
	FDigetherMonsterStimulusEvent Event;
	Event.Cause = Cause;
	Event.Location = Location;
	Event.InstigatorKey = ByPawn ? Sense->ResolvePlayerKey(ByPawn->GetPlayerState()) : FString();
	Event.ServerTime = GetWorld()->GetTimeSeconds();
	Sense->ReportStimulus(OwningSpider, Event);
}

void ADigetherSpiderNestActor::NotifySpiderGone()
{
	GetWorldTimerManager().ClearTimer(RefillTimer);
}

void ADigetherSpiderNestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(RefillTimer);
	TArray<TWeakObjectPtr<ADigetherSpiderWebActor>> ToRemove = Webs;
	Webs.Reset();
	for (const TWeakObjectPtr<ADigetherSpiderWebActor>& Weak : ToRemove)
	{
		if (ADigetherSpiderWebActor* Web = Weak.Get())
		{
			Web->RemoveByRunEnd();
		}
	}
	Super::EndPlay(EndPlayReason);
}
