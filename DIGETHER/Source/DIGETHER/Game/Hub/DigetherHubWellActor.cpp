#include "Game/Hub/DigetherHubWellActor.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Run/DigetherGameOverPublicStateComponent.h"
#include "TimerManager.h"

namespace
{
	// 클라이언트는 GameState 복제가 우물 BeginPlay보다 늦을 수 있다. 연결될 때까지 이 간격으로 다시 찾는다.
	constexpr float WellBindRetrySeconds = 0.25f;
	// 임시값: 기획 미정(우물 아래 대기 지점 깊이, cm), 2026-11-05. 맵 배치에서 대기 지점 컴포넌트를 옮겨 조정한다.
	const FVector DefaultWaitingPointOffsetCm(0.0, 0.0, -1500.0);
}

ADigetherHubWellActor::ADigetherHubWellActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
	WaitingPoint = CreateDefaultSubobject<USceneComponent>(TEXT("WaitingPoint"));
	WaitingPoint->SetupAttachment(SceneRoot);
	WaitingPoint->SetRelativeLocation(DefaultWaitingPointOffsetCm);
}

void ADigetherHubWellActor::BeginPlay()
{
	Super::BeginPlay();
	TryBindPublicState();
}

void ADigetherHubWellActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryTimer);
		World->GetTimerManager().ClearTimer(SuctionTimer);
	}
	if (BoundPublicState.IsValid())
	{
		BoundPublicState->OnGameOverPhaseChanged.RemoveAll(this);
		BoundPublicState->OnFailureEventStarted.RemoveAll(this);
	}
	BoundPublicState.Reset();
	Super::EndPlay(EndPlayReason);
}

ADigetherHubWellActor* ADigetherHubWellActor::Find(const UWorld* World)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	ADigetherHubWellActor* Found = nullptr;
	for (TActorIterator<ADigetherHubWellActor> Iterator(World); Iterator; ++Iterator)
	{
		if (Found != nullptr)
		{
			return nullptr;
		}
		Found = *Iterator;
	}
	return Found;
}

FVector ADigetherHubWellActor::GetSuctionCenter() const
{
	return GetActorLocation();
}

FTransform ADigetherHubWellActor::GetWaitingTransform(const int32 Index) const
{
	const FTransform Base = IsValid(WaitingPoint) ? WaitingPoint->GetComponentTransform() : GetActorTransform();
	if (Index <= 0 || WaitingSpacingCm <= 0.f)
	{
		return Base;
	}
	// 대기 지점 둘레에 겹치지 않게 놓는다. 위치 판정에 쓰지 않는 표시용 배치다.
	constexpr int32 SeatsPerRing = 6;
	const int32 Ring = 1 + (Index - 1) / SeatsPerRing;
	const double Angle = 2.0 * UE_DOUBLE_PI * static_cast<double>((Index - 1) % SeatsPerRing) / SeatsPerRing;
	const FVector Offset(FMath::Cos(Angle) * WaitingSpacingCm * Ring, FMath::Sin(Angle) * WaitingSpacingCm * Ring, 0.0);
	FTransform Result = Base;
	Result.AddToTranslation(Base.GetRotation().RotateVector(Offset));
	return Result;
}

void ADigetherHubWellActor::TryBindPublicState()
{
	UWorld* World = GetWorld();
	const ADigetherGameplayGameState* State = World != nullptr ? World->GetGameState<ADigetherGameplayGameState>() : nullptr;
	UDigetherGameOverPublicStateComponent* Public = IsValid(State) ? State->GetGameOverPublicStateComponent() : nullptr;
	if (!IsValid(Public))
	{
		if (World != nullptr && !World->bIsTearingDown)
		{
			World->GetTimerManager().SetTimer(BindRetryTimer, this, &ThisClass::TryBindPublicState, WellBindRetrySeconds, false);
		}
		return;
	}
	BoundPublicState = Public;
	Public->OnGameOverPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandlePhaseChanged);
	Public->OnFailureEventStarted.AddUniqueDynamic(this, &ThisClass::HandleFailureEventStarted);
	// 실패 사건 도중 들어온 클라이언트도 남은 연출을 이어 받는다.
	const FDigetherGameOverPublicState Current = Public->GetPublicState();
	if (Current.Phase == EDigetherGameOverPhase::FailureEvent)
	{
		HandleFailureEventStarted(Public->GetSecondsUntilSuction(), Current.Tuning.FailMotionSeconds);
	}
}

void ADigetherHubWellActor::HandlePhaseChanged(const EDigetherGameOverPhase Phase)
{
	if (Phase != EDigetherGameOverPhase::FailureEvent && bFailureEventActive)
	{
		bFailureEventActive = false;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(SuctionTimer);
		}
		BP_OnFailureEventEnd();
	}
}

void ADigetherHubWellActor::HandleFailureEventStarted(const float FloodSeconds, const float FailMotionSeconds)
{
	bFailureEventActive = true;
	BP_OnFloodStart(FloodSeconds, FailMotionSeconds);
	const float Remaining = BoundPublicState.IsValid() ? BoundPublicState->GetSecondsUntilSuction() : FloodSeconds;
	UWorld* World = GetWorld();
	if (Remaining <= 0.f || World == nullptr)
	{
		HandleSuctionStart();
		return;
	}
	World->GetTimerManager().SetTimer(SuctionTimer, this, &ThisClass::HandleSuctionStart, Remaining, false);
}

void ADigetherHubWellActor::HandleSuctionStart()
{
	if (bFailureEventActive)
	{
		BP_OnSuctionStart();
	}
}
