#include "DigetherAftershockLab.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "Algo/RandomShuffle.h"
#include "DigetherCeilingProbe.h"
#include "DigetherCollapseLab.h"
#include "Game/Excavation/Dain/DigetherDiggableSoil.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogDigetherAftershockLab, Log, All);

ADigetherAftershockLab::ADigetherAftershockLab()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = .1f;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ADigetherAftershockLab::BeginPlay()
{
	Super::BeginPlay();
	for (const TPair<FName, float>& Entry : InitialSegments)
	{
		if (!RegisterSegment(Entry.Key, Entry.Value) && HasAuthority())
		{
			UE_LOG(LogDigetherAftershockLab, Warning, TEXT("Invalid lab segment: %s"), *Entry.Key.ToString());
		}
	}
#if !UE_BUILD_SHIPPING
	if (HasAuthority() && !CollapseTargets.IsEmpty())
	{
		if (APlayerController* Controller = GetWorld()->GetFirstPlayerController())
		{
			EnableInput(Controller);
			InputComponent->BindKey(EKeys::C, IE_Pressed, this, &ADigetherAftershockLab::TriggerPreviewAftershock);
		}
	}
#endif
	RestartSchedule();
}

void ADigetherAftershockLab::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(AftershockTimer);
	for (FDigetherAftershockLabSegment& Segment : Segments)
	{
		GetWorldTimerManager().ClearTimer(Segment.ExecutionTimer);
	}
	Super::EndPlay(EndPlayReason);
}

bool ADigetherAftershockLab::RegisterSegment(FName SegmentId, float DamageThreshold)
{
	if (!HasAuthority() || IsActorBeingDestroyed() || SegmentId.IsNone() ||
		!FMath::IsFinite(DamageThreshold) || DamageThreshold <= 0.f ||
		Segments.ContainsByPredicate([SegmentId](const FDigetherAftershockLabSegment& Segment)
		{
			return Segment.SegmentId == SegmentId;
		}))
	{
		return false;
	}
	FDigetherAftershockLabSegment& Segment = Segments.AddDefaulted_GetRef();
	Segment.SegmentId = SegmentId;
	Segment.DamageThreshold = DamageThreshold;
	return true;
}

namespace
{
float QuakeInterval(double Remaining)
{
	return Remaining >= 600 ? 300.f : Remaining >= 300 ? 180.f : Remaining >= 120 ? 90.f : 45.f;
}
}

void ADigetherAftershockLab::ReserveQuake(float MaximumDelay)
{
	auto& Timers = GetWorldTimerManager();
	Timers.ClearTimer(AftershockTimer);
	const double Remaining = (ClockEndTime - GetWorld()->GetTimeSeconds()) * ActiveClockRate;
	ReservedCollapseCount = 0;
	ReservedIntervalSeconds = 0;
	if (Remaining <= 0)
	{
		return;
	}
	ReservedIntervalSeconds = QuakeInterval(Remaining);
	const int32 MaxCount = Remaining >= 600 ? 1 : Remaining >= 120 ? 2 : 3;
	ReservedCollapseCount = FMath::RandRange(0, MaxCount);
	Timers.SetTimer(AftershockTimer, this, &ADigetherAftershockLab::TriggerAftershock,
		FMath::Min(MaximumDelay, ReservedIntervalSeconds / ActiveClockRate), false);
}

void ADigetherAftershockLab::DeductExpeditionSeconds(float Seconds)
{
	if (!HasAuthority() || !HasActorBegunPlay() || !bUseExpeditionClock || !FMath::IsFinite(Seconds) || Seconds <= 0)
	{
		return;
	}
	const double Remaining = (ClockEndTime - GetWorld()->GetTimeSeconds()) * ActiveClockRate;
	ClockEndTime -= Seconds / ActiveClockRate;
	if (Remaining <= Seconds || QuakeInterval(Remaining) != QuakeInterval(Remaining - Seconds))
	{
		const float Wait = GetWorldTimerManager().GetTimerRemaining(AftershockTimer);
		ReserveQuake(Wait > 0 ? Wait : MAX_flt);
	}
}

void ADigetherAftershockLab::RestartSchedule()
{
	if (!HasActorBegunPlay() || IsActorBeingDestroyed())
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(AftershockTimer);
	if (HasAuthority() && bUseExpeditionClock)
	{
		ActiveClockRate = FMath::Clamp(LabClockRate, 1.f, 60.f);
		ClockEndTime = GetWorld()->GetTimeSeconds() + double(InitialRemainingSeconds) / ActiveClockRate;
		ReserveQuake();
		return;
	}
	if (HasAuthority() && FMath::IsFinite(IntervalSeconds) && IntervalSeconds >= .1f)
	{
		GetWorldTimerManager().SetTimer(AftershockTimer, this,
			&ADigetherAftershockLab::TriggerAftershock, IntervalSeconds, true);
	}
}

void ADigetherAftershockLab::TriggerPreviewAftershock()
{
	const int32 PreviousCount = CollapseRequestCount;
	RunAftershock(NextPreviewStage);
	const bool bWarned = CollapseRequestCount > PreviousCount;
	if (GEngine)
	{
		const FString Message = bWarned
			? FString::Printf(TEXT("시험 여진: %d단계 붕괴 예고"), NextPreviewStage)
			: FString::Printf(TEXT("%d단계 대상이 없습니다. 천장을 굴착하거나 잔해를 치운 뒤 다시 시도하세요."), NextPreviewStage);
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Orange, Message);
	}
	if (bWarned) NextPreviewStage = NextPreviewStage % 3 + 1;
}

void ADigetherAftershockLab::TriggerAftershock()
{
	RunAftershock(0);
}

void ADigetherAftershockLab::RunAftershock(int32 PreviewStage)
{
	if (!HasActorBegunPlay() || !HasAuthority() || IsActorBeingDestroyed() ||
		!FMath::IsFinite(DamagePerAftershock) || DamagePerAftershock <= 0.f)
	{
		return;
	}
	if (bUseExpeditionClock && GetWorld()->GetTimeSeconds() >= ClockEndTime)
	{
		ReserveQuake();
		return;
	}
	++AftershockCount;
	UE_LOG(LogDigetherAftershockLab, Log, TEXT("Aftershock %d (lab only)"), AftershockCount);
	TArray<int32> Order;
	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		Order.Add(Index);
	}
	const APlayerController* Controller = PreviewStage > 0 ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (Controller && Controller->GetPawn())
	{
		const FVector PlayerLocation = Controller->GetPawn()->GetActorLocation();
		const auto DistanceToSite = [&](int32 Index)
		{
			const ADigetherCollapseLab* Target = CollapseTargets.FindRef(Segments[Index].SegmentId);
			return IsValid(Target) ? FVector::DistSquared2D(PlayerLocation, Target->Deposit->GetComponentLocation()) : MAX_dbl;
		};
		Order.StableSort([&](int32 A, int32 B) { return DistanceToSite(A) < DistanceToSite(B); });
	}
	else if (bUseExpeditionClock)
	{
		Algo::RandomShuffle(Order);
	}
	TArray<TPair<FVector, FName>> SelectedSites;
	for (int32 Index : Order)
	{
		FDigetherAftershockLabSegment& Segment = Segments[Index];
		ADigetherCeilingProbe* Probe = CeilingRegions.FindRef(Segment.SegmentId);
		// 여진 직전에 재검사해 표시 Tick 사이에 천장이 사라져도 오래된 후보를 사용하지 않음
		// 초기화된 흙(Revision 0)은 이전 기준 표면·표시 상태와 무관하게 제외
		ADigetherCollapseLab* Target = CollapseTargets.FindRef(Segment.SegmentId);
		const int32 Stage = PreviewStage > 0 ? PreviewStage : FMath::RandRange(1, 3);
		TArray<FVector> Surface;
		if (bUseExpeditionClock || PreviewStage > 0)
		{
			Segment.bDamageActive = false;
			if (!IsValid(Target) || SelectedSites.Num() >= (PreviewStage > 0 ? 1 : ReservedCollapseCount)) continue;
			const FVector Site = Target->CeilingRemoval->GetComponentLocation();
			const FName Group = ConnectedCeilingGroups.FindRef(Segment.SegmentId);
			if (SelectedSites.ContainsByPredicate([&](const TPair<FVector, FName>& Previous)
			{
				const float Separation = !Group.IsNone() && Group == Previous.Value ? 1000.f : 500.f;
				return FVector::DistSquared(Site, Previous.Key) < FMath::Square(Separation);
			})) continue;
		}
		if (!CollapseTargets.IsEmpty())
		{
			Segment.bDamageActive = IsValid(Target) && IsValid(Probe) && Target->CeilingProbe == Probe &&
				(!Segment.bCollapseRequested || Segment.bExecutionRequested) && Target->GetWarningSurface(Surface, Stage);
			if (Segment.bExecutionRequested && Segment.bDamageActive)
			{
				// 퇴적 공간이 다시 열린 다음 여진에만 새 요청을 허용한다.
				Segment.bCollapseRequested = false;
				Segment.bExecutionRequested = false;
				Segment.Damage = 0.f;
			}
		}
		else
		{
			Segment.bDamageActive = !bRequireCeiling ||
			(IsValid(Probe) && !Probe->IsActorBeingDestroyed() && IsValid(Probe->Soil) &&
			 !Probe->Soil->IsActorBeingDestroyed() && Probe->Soil->GetRevision() > 0 &&
			 Probe->RefreshCandidates() > 0);
		}
		if (Segment.bDamageActive && !Segment.bCollapseRequested)
		{
			// 유한한 큰 입력도 더하기 전 잔여 손상으로 제한해 오버플로 방지
			Segment.Damage += FMath::Min(DamagePerAftershock, Segment.DamageThreshold - Segment.Damage);
			if (PreviewStage > 0 || bUseExpeditionClock || Segment.Damage >= Segment.DamageThreshold)
			{
				if (bUseExpeditionClock || PreviewStage > 0)
				{
					SelectedSites.Emplace(Target->CeilingRemoval->GetComponentLocation(), ConnectedCeilingGroups.FindRef(Segment.SegmentId));
				}
				Segment.bCollapseRequested = true;
				++CollapseRequestCount;
				Segment.WarnedTarget = Target;
				Segment.WarningStage = Stage;
				Segment.WarningDelaySeconds = FMath::FRandRange(3.f, IsValid(Target) ? 10.f - FMath::Clamp(Target->FallSeconds, .1f, 5.f) : 10.f);
				if (IsValid(Target))
				{
					// 재굴착 후 원래 천장 표본이 이미 제거된 경우 같은 구간의 예고 위치를 재사용한다.
					if (!Surface.IsEmpty())
					{
						Segment.WarningVertices = MoveTemp(Surface);
					}
				}
				else if (IsValid(Probe))
				{
					Probe->CopyCandidateVertices(Segment.WarningVertices);
				}
				GetWorldTimerManager().SetTimer(Segment.ExecutionTimer,
					FTimerDelegate::CreateUObject(this, &ADigetherAftershockLab::RequestCollapseExecution, Segment.SegmentId),
					Segment.WarningDelaySeconds, false);
				UE_LOG(LogDigetherAftershockLab, Log, TEXT("Collapse warning: %s delay=%.2fs (no terrain change)"),
					*Segment.SegmentId.ToString(), Segment.WarningDelaySeconds);
			}
		}
		UE_LOG(LogDigetherAftershockLab, Log, TEXT("Segment=%s Active=%d Damage=%.2f/%.2f Requested=%d"),
			*Segment.SegmentId.ToString(), Segment.bDamageActive, Segment.Damage, Segment.DamageThreshold,
			Segment.bCollapseRequested);
	}
	if (bUseExpeditionClock && PreviewStage == 0)
	{
		ReserveQuake();
	}
}

void ADigetherAftershockLab::RequestCollapseExecution(FName SegmentId)
{
	if (!HasAuthority() || IsActorBeingDestroyed() || !HasActorBegunPlay())
	{
		return;
	}
	FDigetherAftershockLabSegment* Segment = Segments.FindByPredicate(
		[SegmentId](const FDigetherAftershockLabSegment& Value) { return Value.SegmentId == SegmentId; });
	if (!Segment || !Segment->bCollapseRequested || Segment->bExecutionRequested)
	{
		return;
	}
	// 천장 후보·보강 상태를 다시 검사해 취소하지 않음. 구독 콜백보다 먼저 확정
	Segment->bExecutionRequested = true;
	++ExecutionRequestCount;
	if (ADigetherCollapseLab* Target = Segment->WarnedTarget.Get())
	{
		const bool bStarted = Target->CeilingProbe == CeilingRegions.FindRef(SegmentId) && Target->StartCollapse(Segment->WarningStage);
		UE_LOG(LogDigetherAftershockLab, Log, TEXT("Collapse execution: %s started=%d"), *SegmentId.ToString(), bStarted);
	}
	OnCollapseExecutionRequested.Broadcast(SegmentId);
}

void ADigetherAftershockLab::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	for (const FDigetherAftershockLabSegment& Segment : Segments)
	{
		if (!Segment.bCollapseRequested || Segment.bExecutionRequested)
		{
			continue;
		}
		for (int32 Index = 0; Index + 2 < Segment.WarningVertices.Num(); Index += 3)
		{
			for (int32 Edge = 0; Edge < 3; ++Edge)
			{
				DrawDebugLine(GetWorld(), Segment.WarningVertices[Index + Edge], Segment.WarningVertices[Index + (Edge + 1) % 3],
					FColor::Orange, false, .15f, 0, 4.f);
			}
		}
	}
}
