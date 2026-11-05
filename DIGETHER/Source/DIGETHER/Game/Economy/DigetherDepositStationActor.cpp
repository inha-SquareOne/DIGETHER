#include "Game/Economy/DigetherDepositStationActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Game/Economy/DigetherQuotaStateComponent.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 시험 상자 100×60×120cm 기준의 임시 입구·바닥 오프셋
	const FVector DefaultPourInletOffsetCm(50.0, 0.0, 100.0);
	const FVector DefaultCoinDropOffsetCm(100.0, 0.0, 5.0);
	// 클라이언트 GameState 복제 지연에 대비한 재연결 간격
	constexpr float QuotaBindRetrySeconds = 0.25f;
}

ADigetherDepositStationActor::ADigetherDepositStationActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MachineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MachineMesh"));
	MachineMesh->SetupAttachment(SceneRoot);
	MachineMesh->SetRelativeLocation(FVector(0.0, 0.0, 60.0));
	MachineMesh->SetRelativeScale3D(FVector(1.0, 0.6, 1.2));
	MachineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MachineMesh->SetCanEverAffectNavigation(false);

	PourInlet = CreateDefaultSubobject<USceneComponent>(TEXT("PourInlet"));
	PourInlet->SetupAttachment(SceneRoot);
	PourInlet->SetRelativeLocation(DefaultPourInletOffsetCm);

	CoinDropPoint = CreateDefaultSubobject<USceneComponent>(TEXT("CoinDropPoint"));
	CoinDropPoint->SetupAttachment(SceneRoot);
	CoinDropPoint->SetRelativeLocation(DefaultCoinDropOffsetCm);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		MachineMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void ADigetherDepositStationActor::BeginPlay()
{
	Super::BeginPlay();
	TryBindQuota();
}

void ADigetherDepositStationActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 파괴 뒤 재연결·신호 알림 차단
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryTimer);
	}
	if (UDigetherQuotaStateComponent* Quota = BoundQuota.Get())
	{
		Quota->OnQuotaStateChanged.Remove(QuotaChangedHandle);
		Quota->OnQuotaCompleted.Remove(QuotaCompletedHandle);
	}
	BoundQuota.Reset();
	Super::EndPlay(EndPlayReason);
}

bool ADigetherDepositStationActor::IsPawnInRange(const APawn* Pawn, const float InteractionDistanceCm) const
{
	return IsValid(Pawn) && Pawn->GetWorld() == GetWorld() && InteractionDistanceCm > 0.f
		&& FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation())
			<= FMath::Square(InteractionDistanceCm);
}

FTransform ADigetherDepositStationActor::GetPourInletTransform() const
{
	return IsValid(PourInlet) ? PourInlet->GetComponentTransform() : GetActorTransform();
}

FTransform ADigetherDepositStationActor::GetCoinDropTransform() const
{
	return IsValid(CoinDropPoint) ? CoinDropPoint->GetComponentTransform() : GetActorTransform();
}

void ADigetherDepositStationActor::TryBindQuota()
{
	UWorld* World = GetWorld();
	if (World == nullptr || BoundQuota.IsValid())
	{
		return;
	}
	const ADigetherGameplayGameState* State = World->GetGameState<ADigetherGameplayGameState>();
	UDigetherQuotaStateComponent* Quota = IsValid(State) ? State->GetQuotaComponent() : nullptr;
	if (!IsValid(Quota))
	{
		if (!World->GetTimerManager().IsTimerActive(BindRetryTimer))
		{
			World->GetTimerManager().SetTimer(BindRetryTimer, this, &ThisClass::TryBindQuota,
				QuotaBindRetrySeconds, true);
		}
		return;
	}
	World->GetTimerManager().ClearTimer(BindRetryTimer);
	BoundQuota = Quota;
	QuotaChangedHandle = Quota->OnQuotaStateChanged.AddUObject(this, &ThisClass::HandleQuotaStateChanged);
	QuotaCompletedHandle = Quota->OnQuotaCompleted.AddUObject(this, &ThisClass::HandleQuotaCompleted);
	// 첫 연결에서 달성 연출 없이 현재 색만 전달
	PublishSignal(Quota->GetPaymentSignal());
}

void ADigetherDepositStationActor::HandleQuotaStateChanged(const FDigetherQuotaPublicState& State)
{
	if (const UDigetherQuotaStateComponent* Quota = BoundQuota.Get())
	{
		// 누적 인정액 대신 공개 신호기 단계만 전달
		PublishSignal(Quota->GetPaymentSignal());
	}
}

void ADigetherDepositStationActor::HandleQuotaCompleted()
{
	ReceiveQuotaCompleted();
}

void ADigetherDepositStationActor::PublishSignal(const EDigetherPaymentSignal Signal)
{
	if (bSignalPublished && Signal == LastSignal)
	{
		return;
	}
	bSignalPublished = true;
	LastSignal = Signal;
	ReceivePaymentSignalChanged(Signal);
}
