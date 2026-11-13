#include "Game/Monster/Spider/DigetherSpiderWebActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Game/Core/DigetherMonsterTargetInterface.h"
#include "Game/Monster/DigetherMonsterLog.h"
#include "Game/Monster/Spider/DigetherSpiderNestActor.h"
#include "GameFramework/Pawn.h"

ADigetherSpiderWebActor::ADigetherSpiderWebActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetCanBeDamaged(true);

	// 판(두께 방향 X) 주변 상자, 크기는 BP에서 메시에 맞춤
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetBoxExtent(FVector(30.f, 100.f, 100.f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	// 시야 채널은 막지 않음(몬스터 시야·도약 판정을 가리지 않게). 공격 판정은 공격 쪽이 WorldDynamic 겹침·피해로 처리
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetGenerateOverlapEvents(true);
	SetRootComponent(Trigger);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Trigger);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADigetherSpiderWebActor::InitializeWeb(ADigetherSpiderNestActor* InNest, const float InSlowMultiplier)
{
	if (!HasAuthority())
	{
		return;
	}
	Nest = InNest;
	SlowMultiplier = FMath::Clamp(InSlowMultiplier, 0.05f, 1.f);
	Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &ADigetherSpiderWebActor::HandleBeginOverlap);
	Trigger->OnComponentEndOverlap.AddUniqueDynamic(this, &ADigetherSpiderWebActor::HandleEndOverlap);
	// 생성 순간 이미 서 있던 플레이어는 시작 겹침 이벤트를 바인딩 전에 놓치므로 직접 처리
	TArray<AActor*> Overlapping;
	Trigger->GetOverlappingActors(Overlapping, APawn::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		HandlePawnEntered(Cast<APawn>(Actor));
	}
}

void ADigetherSpiderWebActor::HandleBeginOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	HandlePawnEntered(Cast<APawn>(OtherActor));
}

void ADigetherSpiderWebActor::HandlePawnEntered(APawn* Pawn)
{
	// 몬스터는 감속·진동 대상 아님, 한 Pawn의 여러 컴포넌트 겹침은 한 번만
	if (bRemoved || !Pawn || !Pawn->IsPlayerControlled() || SlowedPawns.Contains(Pawn))
	{
		return;
	}
	SlowedPawns.Add(Pawn);
	if (IDigetherMonsterTargetInterface* Target = Cast<IDigetherMonsterTargetInterface>(Pawn))
	{
		Target->AddMoveSpeedMultiplier(this, SlowMultiplier);
	}
	if (ADigetherSpiderNestActor* OwningNest = Nest.Get())
	{
		OwningNest->HandleWebStepped(this, Pawn);
	}
}

void ADigetherSpiderWebActor::HandleEndOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	// 부속 컴포넌트만 빠져나간 경우 감속 유지
	if (!Pawn || Trigger->IsOverlappingActor(Pawn))
	{
		return;
	}
	if (IDigetherMonsterTargetInterface* Target = Cast<IDigetherMonsterTargetInterface>(Pawn))
	{
		Target->RemoveMoveSpeedMultiplier(this);
	}
	SlowedPawns.Remove(Pawn);
}

float ADigetherSpiderWebActor::TakeDamage(const float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	// 기본 공격 1회로 제거(개발 시험값), 몬스터 자신의 피해는 무시
	if (HasAuthority() && DamageAmount > 0.f && EventInstigator && EventInstigator->IsPlayerController())
	{
		Remove(EDigetherSpiderWebRemoval::Attacked, EventInstigator);
	}
	return Applied;
}

void ADigetherSpiderWebActor::RemoveByTerrain()
{
	Remove(EDigetherSpiderWebRemoval::Terrain, nullptr);
}

void ADigetherSpiderWebActor::RemoveByRunEnd()
{
	Remove(EDigetherSpiderWebRemoval::RunEnded, nullptr);
}

void ADigetherSpiderWebActor::Remove(const EDigetherSpiderWebRemoval Reason, AController* ByController)
{
	if (!HasAuthority() || bRemoved)
	{
		return;
	}
	bRemoved = true;
	ReleaseAllSlowed();
	if (ADigetherSpiderNestActor* OwningNest = Nest.Get())
	{
		OwningNest->HandleWebRemoved(this, Reason, ByController);
	}
	MulticastWebRemoved(Reason);
	SetLifeSpan(0.5f);
	SetActorEnableCollision(false);
}

void ADigetherSpiderWebActor::MulticastWebRemoved_Implementation(const EDigetherSpiderWebRemoval Reason)
{
	OnWebRemoved(Reason);
}

void ADigetherSpiderWebActor::ReleaseAllSlowed()
{
	for (const TWeakObjectPtr<APawn>& Weak : SlowedPawns)
	{
		if (IDigetherMonsterTargetInterface* Target = Cast<IDigetherMonsterTargetInterface>(Weak.Get()))
		{
			Target->RemoveMoveSpeedMultiplier(this);
		}
	}
	SlowedPawns.Reset();
}

void ADigetherSpiderWebActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		ReleaseAllSlowed();
		if (!bRemoved)
		{
			if (ADigetherSpiderNestActor* OwningNest = Nest.Get())
			{
				OwningNest->HandleWebRemoved(this, EDigetherSpiderWebRemoval::RunEnded, nullptr);
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}
