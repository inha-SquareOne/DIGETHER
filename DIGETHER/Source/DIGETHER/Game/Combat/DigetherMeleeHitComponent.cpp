#include "Game/Combat/DigetherMeleeHitComponent.h"

#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Game/Combat/DigetherCombatLog.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

namespace
{
#if !UE_BUILD_SHIPPING
	// 서버 월드에 히트박스를 그린다(리슨 서버 호스트 화면에서 보인다), 0 끔 / 1 켬
	int32 GDigetherMeleeDebugDraw = 0;
	FAutoConsoleVariableRef CVarDigetherMeleeDebugDraw(
		TEXT("Digether.Melee.DebugDraw"),
		GDigetherMeleeDebugDraw,
		TEXT("근접 판정 히트박스 표시(서버 월드). 0 끔, 1 켬"));
#endif
}

UDigetherMeleeHitComponent::UDigetherMeleeHitComponent()
{
	// 판정은 애니메이션 노티파이가 진행시키므로 컴포넌트 자체 틱은 쓰지 않는다
	PrimaryComponentTick.bCanEverTick = false;
}

void UDigetherMeleeHitComponent::SetHitSource(const FDigetherMeleeHitParams& InParams, AActor* InDamageCauser)
{
	Params = InParams;
	DamageCauser = InDamageCauser;
	TArray<FString> Problems;
	bHasUsableParams = DigetherMeleeRules::IsUsable(Params, &Problems);
	if (!bHasUsableParams && CanRunOnThisMachine())
	{
		for (const FString& Problem : Problems)
		{
			UE_LOG(LogDigetherCombat, Warning, TEXT("%s 근접 판정 값 오류(%s): %s"),
				*GetNameSafe(GetOwner()), *GetNameSafe(InDamageCauser), *Problem);
		}
	}
}

void UDigetherMeleeHitComponent::ClearHitSource()
{
	Params = FDigetherMeleeHitParams();
	DamageCauser.Reset();
	bHasUsableParams = false;
}

void UDigetherMeleeHitComponent::BeginHitWindow(float DamageScale)
{
	if (!CanRunOnThisMachine())
	{
		return;
	}
	if (ActiveWindowCount == 0)
	{
		HitActorsThisWindow.Reset();
	}
	++ActiveWindowCount;
	ActiveDamageScale = DamageScale;
	// 구간이 한 프레임보다 짧아도 최소 한 번은 검사한다
	PerformHitCheck();
}

void UDigetherMeleeHitComponent::TickHitWindow()
{
	if (!CanRunOnThisMachine() || ActiveWindowCount == 0)
	{
		return;
	}
	PerformHitCheck();
}

void UDigetherMeleeHitComponent::EndHitWindow()
{
	if (!CanRunOnThisMachine() || ActiveWindowCount == 0)
	{
		return;
	}
	--ActiveWindowCount;
	if (ActiveWindowCount == 0)
	{
		HitActorsThisWindow.Reset();
		ActiveDamageScale = 1.f;
	}
}

void UDigetherMeleeHitComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ActiveWindowCount = 0;
	HitActorsThisWindow.Reset();
	Super::EndPlay(EndPlayReason);
}

bool UDigetherMeleeHitComponent::CanRunOnThisMachine() const
{
	const AActor* Owner = GetOwner();
	return Owner != nullptr && Owner->HasAuthority() && GetWorld() != nullptr;
}

void UDigetherMeleeHitComponent::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	const AActor* Owner = GetOwner();
	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		// 서버의 조준 회전은 이동 RPC로 받은 컨트롤 회전이다(카메라 매니저 값은 서버에서 늦을 수 있어 쓰지 않는다)
		OutLocation = Pawn->GetPawnViewLocation();
		OutRotation = Pawn->GetBaseAimRotation();
		return;
	}
	OutLocation = Owner->GetActorLocation();
	OutRotation = Owner->GetActorRotation();
}

void UDigetherMeleeHitComponent::PerformHitCheck()
{
	if (!bHasUsableParams)
	{
		return;
	}
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();

	FVector ViewLocation;
	FRotator ViewRotation;
	GetViewPoint(ViewLocation, ViewRotation);
	const FDigetherMeleeHitBox Box = DigetherMeleeRules::ComputeHitBox(ViewLocation, ViewRotation, Params);

#if !UE_BUILD_SHIPPING
	if (GDigetherMeleeDebugDraw != 0)
	{
		DrawDebugBox(World, Box.Center, Box.HalfExtent, Box.Rotation, FColor::Red, false, 0.5f, 0, 1.f);
	}
#endif

	FCollisionObjectQueryParams ObjectParams;
	for (const TEnumAsByte<ECollisionChannel> Channel : Params.TargetObjectTypes)
	{
		ObjectParams.AddObjectTypesToQuery(Channel);
	}
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DigetherMeleeHit), false, Owner);
	// 들고 있는 장비 등 소유자에게 붙은 액터는 맞지 않는다
	TArray<AActor*> AttachedActors;
	Owner->GetAttachedActors(AttachedActors, true, true);
	QueryParams.AddIgnoredActors(AttachedActors);
	if (AActor* Causer = DamageCauser.Get())
	{
		QueryParams.AddIgnoredActor(Causer);
	}

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Box.Center, Box.Rotation, ObjectParams,
		FCollisionShape::MakeBox(Box.HalfExtent), QueryParams);
	if (Overlaps.IsEmpty())
	{
		return;
	}

	const float Damage = DigetherMeleeRules::ComputeDamage(Params, ActiveDamageScale);
	const APawn* OwnerPawn = Cast<APawn>(Owner);
	AController* InstigatorController = OwnerPawn != nullptr ? OwnerPawn->GetController() : nullptr;
	AActor* Causer = DamageCauser.IsValid() ? DamageCauser.Get() : Owner;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (Target == nullptr || Target == Owner || HitActorsThisWindow.Contains(Target))
		{
			continue;
		}
		const UPrimitiveComponent* TargetComponent = Overlap.GetComponent();
		const FVector TargetPoint = TargetComponent != nullptr ? TargetComponent->Bounds.Origin : Target->GetActorLocation();
		if (Params.bRequireLineOfSight && !HasLineOfSight(ViewLocation, Target, TargetPoint))
		{
			continue;
		}
		HitActorsThisWindow.Add(Target);
		if (Damage > 0.f)
		{
			UGameplayStatics::ApplyDamage(Target, Damage, InstigatorController, Causer, UDamageType::StaticClass());
		}
		UE_LOG(LogDigetherCombat, Verbose, TEXT("%s 근접 적중: %s, 피해 %g"), *GetNameSafe(Owner), *GetNameSafe(Target), Damage);
		OnMeleeHit.Broadcast(Target, Damage, TargetPoint);
		// 피해 처리 중 소유자가 사라졌으면 나머지 대상은 처리하지 않는다
		if (!IsValid(Owner) || ActiveWindowCount == 0)
		{
			return;
		}
	}
}

bool UDigetherMeleeHitComponent::HasLineOfSight(const FVector& From, const AActor* Target, const FVector& TargetPoint) const
{
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(DigetherMeleeSight), false, GetOwner());
	TArray<AActor*> AttachedActors;
	GetOwner()->GetAttachedActors(AttachedActors, true, true);
	TraceParams.AddIgnoredActors(AttachedActors);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, From, TargetPoint, ECC_Visibility, TraceParams))
	{
		return true;
	}
	return Hit.GetActor() == Target;
}
