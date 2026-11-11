#include "Game/Monster/Spider/DigetherGroundSpiderComponent.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Game/Core/DigetherMonsterTargetInterface.h"
#include "Game/Monster/DigetherMonsterCharacter.h"
#include "Game/Monster/DigetherMonsterDefinition.h"
#include "Game/Monster/DigetherMonsterDirectorSubsystem.h"
#include "Game/Monster/DigetherMonsterHealthComponent.h"
#include "Game/Monster/DigetherMonsterLog.h"
#include "Game/Monster/DigetherMonsterSenseSubsystem.h"
#include "Game/Monster/DigetherMonsterSpawnPoint.h"
#include "Game/Monster/Spider/DigetherGroundSpiderRules.h"
#include "Game/Monster/Spider/DigetherSpiderNestActor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

const FName UDigetherGroundSpiderComponent::BB_SpiderState(TEXT("SpiderState"));
const FName UDigetherGroundSpiderComponent::BB_TargetActor(TEXT("TargetActor"));
const FName UDigetherGroundSpiderComponent::BB_TargetLocation(TEXT("TargetLocation"));
const FName UDigetherGroundSpiderComponent::BB_HomeLocation(TEXT("HomeLocation"));
const FName UDigetherGroundSpiderComponent::BB_InvestigateLocation(TEXT("InvestigateLocation"));

namespace
{
	TAutoConsoleVariable<int32> CVarDigetherMonsterDebug(TEXT("digether.Monster.Debug"), 0,
		TEXT("1이면 호스트에서 몬스터 판정 범위·대상·상태를 그림(R08)"), ECVF_Cheat);

	constexpr float ReachabilityRecheckSeconds = 0.5f;
	// 거미줄 제거 지점 조사 최대 시간(개발 시험값)
	constexpr double InvestigateTimeoutSeconds = 10.0;
	constexpr float NestPointSearchRadius = 300.f;
}

UDigetherGroundSpiderComponent::UDigetherGroundSpiderComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UDigetherGroundSpiderComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDigetherGroundSpiderComponent, PublicState);
}

const UScriptStruct* UDigetherGroundSpiderComponent::GetRequiredParamsStruct() const
{
	return FDigetherGroundSpiderParams::StaticStruct();
}

const FDigetherGroundSpiderParams* UDigetherGroundSpiderComponent::GetParams() const
{
	return Definition ? Definition->GetSpeciesParams<FDigetherGroundSpiderParams>() : nullptr;
}

ADigetherMonsterCharacter* UDigetherGroundSpiderComponent::GetSpider() const
{
	return Cast<ADigetherMonsterCharacter>(GetOwner());
}

UBlackboardComponent* UDigetherGroundSpiderComponent::GetBlackboard() const
{
	const ADigetherMonsterCharacter* Spider = GetSpider();
	AAIController* AI = Spider ? Cast<AAIController>(Spider->GetController()) : nullptr;
	return AI ? AI->GetBlackboardComponent() : nullptr;
}

double UDigetherGroundSpiderComponent::Now() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

double UDigetherGroundSpiderComponent::ServerTimeNow() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : Now();
}

float UDigetherGroundSpiderComponent::GetTrackRadius() const
{
	if (HomeRadius > 0.f)
	{
		return HomeRadius;
	}
	const FDigetherGroundSpiderParams* Params = GetParams();
	return Params ? Params->TrackRadius : 0.f;
}

void UDigetherGroundSpiderComponent::BeginPlay()
{
	Super::BeginPlay();
	ADigetherMonsterCharacter* Spider = GetSpider();
	if (!Spider || !Spider->HasAuthority())
	{
		return;
	}
	if (!GetParams())
	{
		UE_LOG(LogDigetherMonster, Warning, TEXT("%s: 땅거미 파라미터(FDigetherGroundSpiderParams)가 없어 행동하지 않습니다."), *GetNameSafe(Spider));
		return;
	}
	if (UDigetherMonsterHealthComponent* Health = Spider->GetHealthComponent())
	{
		Health->OnDied.AddUniqueDynamic(this, &UDigetherGroundSpiderComponent::HandleDied);
		Health->OnDamagedByServer.AddUObject(this, &UDigetherGroundSpiderComponent::HandleDamagedByServer);
	}
	HomeLocation = Spider->GetActorLocation();
	SetupNest();
	bServerInitialized = true;
	SetComponentTickEnabled(true);
	SetState(EDigetherSpiderState::Idle);
	if (UBlackboardComponent* Blackboard = GetBlackboard())
	{
		Blackboard->SetValueAsVector(BB_HomeLocation, HomeLocation);
	}
}

void UDigetherGroundSpiderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EmergeTimer);
	}
	if (Nest)
	{
		Nest->NotifySpiderGone();
	}
	Super::EndPlay(EndPlayReason);
}

void UDigetherGroundSpiderComponent::SetupNest()
{
	const FDigetherGroundSpiderParams* Params = GetParams();
	UWorld* World = GetWorld();
	if (!Params || !World)
	{
		return;
	}
	// 생성 지점이 둥지 후보면 그 지점의 반지름 우선
	HomeRadius = Params->TrackRadius;
	for (TActorIterator<ADigetherMonsterSpawnPoint> It(World); It; ++It)
	{
		if (It->PointKind == EDigetherMonsterSpawnPointKind::HabitatNest && It->NestRadius > 0.f
			&& FVector::DistSquared(It->GetActorLocation(), HomeLocation) <= FMath::Square(NestPointSearchRadius))
		{
			HomeRadius = It->NestRadius;
			break;
		}
	}
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Nest = World->SpawnActor<ADigetherSpiderNestActor>(HomeLocation, FRotator::ZeroRotator, Spawn);
	if (!Nest)
	{
		return;
	}
	Nest->InitializeNest(GetSpider(), HomeLocation, HomeRadius, *Params);
	if (UDigetherMonsterDirectorSubsystem* Director = World->GetSubsystem<UDigetherMonsterDirectorSubsystem>())
	{
		Director->RegisterRunActor(Nest);
	}
}

void UDigetherGroundSpiderComponent::SetState(const EDigetherSpiderState NewState, const float Duration)
{
	PublicState.State = NewState;
	PublicState.StateStartServerTime = ServerTimeNow();
	PublicState.StateDuration = Duration;
	if (UBlackboardComponent* Blackboard = GetBlackboard())
	{
		Blackboard->SetValueAsEnum(BB_SpiderState, static_cast<uint8>(NewState));
	}
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
	// 서버 대입은 OnRep 미호출, 리슨 호스트도 같은 통지
	OnStateChanged.Broadcast(PublicState);
}

void UDigetherGroundSpiderComponent::OnRep_PublicState()
{
	OnStateChanged.Broadcast(PublicState);
}

APawn* UDigetherGroundSpiderComponent::FindTargetPawn(const FString& Key) const
{
	const UDigetherMonsterSenseSubsystem* Sense = GetWorld() ? GetWorld()->GetSubsystem<UDigetherMonsterSenseSubsystem>() : nullptr;
	if (!Sense || Key.IsEmpty())
	{
		return nullptr;
	}
	TArray<FDigetherMonsterTarget> Targets;
	Sense->CollectLivingTargets(Targets);
	for (const FDigetherMonsterTarget& Target : Targets)
	{
		if (Target.PlayerKey == Key)
		{
			return Target.Pawn.Get();
		}
	}
	return nullptr;
}

bool UDigetherGroundSpiderComponent::IsReachable(const FVector& Location) const
{
	UWorld* World = GetWorld();
	const ADigetherMonsterCharacter* Spider = GetSpider();
	if (!World || !Spider)
	{
		return false;
	}
	// 내비 시스템이 없는 월드(자동 테스트 등)는 판정 불가로 보고 도달 가능 처리
	if (!UNavigationSystemV1::GetCurrent(World))
	{
		return true;
	}
	const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, Spider->GetActorLocation(), Location,
		const_cast<ADigetherMonsterCharacter*>(Spider));
	return Path && Path->IsValid() && !Path->IsPartial();
}

void UDigetherGroundSpiderComponent::HandleStimuli(const TConstArrayView<FDigetherMonsterStimulusEvent> Events)
{
	const FDigetherGroundSpiderParams* Params = GetParams();
	const ADigetherMonsterCharacter* Spider = GetSpider();
	if (!bServerInitialized || !Params || !Spider)
	{
		return;
	}
	const EDigetherSpiderState State = PublicState.State;
	if (State == EDigetherSpiderState::Dead || State == EDigetherSpiderState::Buried)
	{
		// 매몰 중 감지·공격 없음(기획)
		return;
	}
	const double Time = Now();
	const float TrackRadius = GetTrackRadius();
	TSet<FString> OutsideAttackers;
	bool bWebRemoved = false;
	FVector WebRemovedLocation = FVector::ZeroVector;
	// 감지 원인별 대상 위치(플레이어 위치), 거미줄 제거는 대상 후보가 아니라 조사 지점(기획: 제거 지점 조사)
	TMap<FString, FVector> SensedLocations;
	for (const FDigetherMonsterStimulusEvent& Event : Events)
	{
		if (Event.Cause == EDigetherMonsterStimulus::WebRemoved)
		{
			bWebRemoved = true;
			WebRemovedLocation = Event.Location;
			continue;
		}
		if (Event.InstigatorKey.IsEmpty())
		{
			continue;
		}
		// 감지 원인 모두 '본 것'으로 기록, 포기 판정의 재감지 시각
		LastSeenTime.Add(Event.InstigatorKey, Time);
		const bool bOutside = FVector::DistSquared(Event.Location, HomeLocation) > FMath::Square(TrackRadius);
		if (Event.Cause == EDigetherMonsterStimulus::Damaged && bOutside)
		{
			OutsideAttackers.Add(Event.InstigatorKey);
		}
		SensedLocations.Add(Event.InstigatorKey, Event.Location);
	}

	const bool bBusy = State == EDigetherSpiderState::Tracking || State == EDigetherSpiderState::LeapWindup
		|| State == EDigetherSpiderState::LeapFlight || State == EDigetherSpiderState::LeapLanding;
	if (bBusy)
	{
		// 추적 중 목표 유지(경로 탐색 생략), 영역 밖에서 목표에게 공격받으면 영역 밖까지 추적
		if (OutsideAttackers.Contains(TargetKey))
		{
			bChaseOutsideArea = true;
		}
		return;
	}
	TArray<DigetherGroundSpiderRules::FTargetCandidate> Candidates;
	for (const TPair<FString, FVector>& Sensed : SensedLocations)
	{
		// 영역 밖 대상은 추적하자마자 포기하므로 후보에서 제외(영역 밖에서 공격한 사람만 예외)
		if (!DigetherGroundSpiderRules::IsTrackableCandidate(FVector::Dist(Sensed.Value, HomeLocation), TrackRadius,
			OutsideAttackers.Contains(Sensed.Key)))
		{
			continue;
		}
		DigetherGroundSpiderRules::FTargetCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.PlayerKey = Sensed.Key;
		Candidate.DistanceSquared = FVector::DistSquared(Sensed.Value, Spider->GetActorLocation());
		Candidate.bReachable = IsReachableCached(Sensed.Key, Sensed.Value);
	}
	const int32 Pick = DigetherGroundSpiderRules::SelectNearestReachable(Candidates);
	if (Pick != INDEX_NONE)
	{
		BeginTracking(Candidates[Pick].PlayerKey, OutsideAttackers.Contains(Candidates[Pick].PlayerKey));
		return;
	}
	if (bWebRemoved)
	{
		InvestigateLocation = WebRemovedLocation;
		if (UBlackboardComponent* Blackboard = GetBlackboard())
		{
			Blackboard->SetValueAsVector(BB_InvestigateLocation, InvestigateLocation);
		}
		InvestigateStartTime = Time;
		SetState(EDigetherSpiderState::Investigating);
	}
}

bool UDigetherGroundSpiderComponent::IsReachableCached(const FString& Key, const FVector& Location)
{
	// 감지 자극은 0.1초마다 오므로 대상별 경로 탐색은 0.5초마다만
	const double Time = Now();
	if (const TPair<double, bool>* Cached = ReachabilityCache.Find(Key))
	{
		if (Time - Cached->Key < ReachabilityRecheckSeconds)
		{
			return Cached->Value;
		}
	}
	const bool bReachable = IsReachable(Location);
	ReachabilityCache.Add(Key, TPair<double, bool>(Time, bReachable));
	return bReachable;
}

void UDigetherGroundSpiderComponent::BeginTracking(const FString& Key, const bool bChaseOutside)
{
	TargetKey = Key;
	bChaseOutsideArea = bChaseOutside;
	ReachabilityCheckTime = -1.0;
	bLastReachable = true;
	SetState(EDigetherSpiderState::Tracking);
	UE_LOG(LogDigetherMonster, Log, TEXT("%s: 추적 시작 %s(영역 밖 추적 %d)"), *GetNameSafe(GetOwner()), *Key, bChaseOutside);
}

void UDigetherGroundSpiderComponent::GiveUp(const EDigetherSpiderGiveUpReason Reason)
{
	UE_LOG(LogDigetherMonster, Log, TEXT("%s: 추적 포기 %s (%s)"), *GetNameSafe(GetOwner()), *TargetKey, *UEnum::GetValueAsString(Reason));
	TargetKey.Reset();
	bChaseOutsideArea = false;
	if (UBlackboardComponent* Blackboard = GetBlackboard())
	{
		Blackboard->ClearValue(BB_TargetActor);
	}
	SetState(EDigetherSpiderState::Returning);
}

void UDigetherGroundSpiderComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const ADigetherMonsterCharacter* Spider = GetSpider();
	const FDigetherGroundSpiderParams* Params = GetParams();
	if (!bServerInitialized || !Spider || !Params)
	{
		return;
	}
	switch (PublicState.State)
	{
	case EDigetherSpiderState::Tracking:
		TickTracking();
		break;
	case EDigetherSpiderState::LeapWindup:
	case EDigetherSpiderState::LeapFlight:
	case EDigetherSpiderState::LeapLanding:
		TickLeap(DeltaTime);
		break;
	case EDigetherSpiderState::Returning:
		if (FVector::DistSquared2D(Spider->GetActorLocation(), HomeLocation) <= FMath::Square(Params->HomeArriveRadius))
		{
			SetState(EDigetherSpiderState::Idle);
		}
		break;
	case EDigetherSpiderState::Investigating:
		// 도착하거나 이동이 막혀 시간이 지나면 복귀(BT 이동 실패로 멈추지 않게)
		if (FVector::DistSquared2D(Spider->GetActorLocation(), InvestigateLocation) <= FMath::Square(Params->HomeArriveRadius)
			|| Now() - InvestigateStartTime > InvestigateTimeoutSeconds)
		{
			SetState(EDigetherSpiderState::Returning);
		}
		break;
	default:
		break;
	}
	if (PublicState.State != EDigetherSpiderState::Dead && PublicState.State != EDigetherSpiderState::Buried)
	{
		TickContact();
	}
	DrawDebug();
}

void UDigetherGroundSpiderComponent::TickTracking()
{
	const FDigetherGroundSpiderParams* Params = GetParams();
	APawn* Target = FindTargetPawn(TargetKey);
	DigetherGroundSpiderRules::FTrackingInput Input;
	Input.bTargetValid = Target != nullptr;
	Input.TrackRadius = GetTrackRadius();
	Input.bChaseOutsideArea = bChaseOutsideArea;
	Input.ReacquireSeconds = Params->ReacquireSeconds;
	const double* Seen = LastSeenTime.Find(TargetKey);
	Input.SecondsSinceSeen = Seen ? static_cast<float>(Now() - *Seen) : TNumericLimits<float>::Max();
	if (Target)
	{
		// 경로 탐색은 0.5초마다만
		if (ReachabilityCheckTime < 0.0 || Now() - ReachabilityCheckTime >= ReachabilityRecheckSeconds)
		{
			ReachabilityCheckTime = Now();
			bLastReachable = IsReachable(Target->GetActorLocation());
		}
		Input.bReachable = bLastReachable;
		Input.TargetDistanceFromHome = FVector::Dist(Target->GetActorLocation(), HomeLocation);
	}
	const EDigetherSpiderGiveUpReason Reason = DigetherGroundSpiderRules::EvaluateGiveUp(Input);
	if (Reason != EDigetherSpiderGiveUpReason::None)
	{
		GiveUp(Reason);
		return;
	}
	if (UBlackboardComponent* Blackboard = GetBlackboard())
	{
		Blackboard->SetValueAsObject(BB_TargetActor, Target);
		Blackboard->SetValueAsVector(BB_TargetLocation, Target->GetActorLocation());
	}
}

bool UDigetherGroundSpiderComponent::CanLeapNow() const
{
	const FDigetherGroundSpiderParams* Params = GetParams();
	const ADigetherMonsterCharacter* Spider = GetSpider();
	if (!Params || !Spider || PublicState.State != EDigetherSpiderState::Tracking)
	{
		return false;
	}
	const APawn* Target = FindTargetPawn(TargetKey);
	if (!Target || FVector::DistSquared(Target->GetActorLocation(), Spider->GetActorLocation()) > FMath::Square(Params->LeapMaxDistance))
	{
		return false;
	}
	const UDigetherMonsterSenseSubsystem* Sense = GetWorld()->GetSubsystem<UDigetherMonsterSenseSubsystem>();
	const AActor* Ignored[] = { Spider, Target };
	return Sense && Sense->HasLineOfSight(Spider->GetSenseOrigin(), Target->GetActorLocation(), Ignored);
}

bool UDigetherGroundSpiderComponent::StartLeap()
{
	const FDigetherGroundSpiderParams* Params = GetParams();
	ADigetherMonsterCharacter* Spider = GetSpider();
	if (!CanLeapNow() || !Params || !Spider)
	{
		return false;
	}
	if (AAIController* AI = Cast<AAIController>(Spider->GetController()))
	{
		AI->StopMovement();
	}
	Leap = FLeapRuntime();
	Leap.LeapId = NextLeapId++;
	Leap.PhaseEndTime = Now() + Params->LeapWindupSeconds;
	SetState(EDigetherSpiderState::LeapWindup, Params->LeapWindupSeconds);
	return true;
}

bool UDigetherGroundSpiderComponent::IsLeaping() const
{
	const EDigetherSpiderState State = PublicState.State;
	return State == EDigetherSpiderState::LeapWindup || State == EDigetherSpiderState::LeapFlight || State == EDigetherSpiderState::LeapLanding;
}

void UDigetherGroundSpiderComponent::CancelLeap()
{
	if (!IsLeaping())
	{
		return;
	}
	if (PublicState.State == EDigetherSpiderState::LeapFlight)
	{
		FinishLeapFlight();
	}
	SetState(TargetKey.IsEmpty() ? EDigetherSpiderState::Returning : EDigetherSpiderState::Tracking);
}

void UDigetherGroundSpiderComponent::TickLeap(const float DeltaTime)
{
	const FDigetherGroundSpiderParams* Params = GetParams();
	ADigetherMonsterCharacter* Spider = GetSpider();
	UWorld* World = GetWorld();
	APawn* Target = FindTargetPawn(TargetKey);
	if (PublicState.State == EDigetherSpiderState::LeapWindup)
	{
		if (Target)
		{
			const FVector ToTarget = (Target->GetActorLocation() - Spider->GetActorLocation()).GetSafeNormal2D();
			if (!ToTarget.IsNearlyZero())
			{
				Spider->SetActorRotation(ToTarget.Rotation());
			}
		}
		if (Now() < Leap.PhaseEndTime)
		{
			return;
		}
		if (!Target)
		{
			SetState(EDigetherSpiderState::Returning);
			return;
		}
		// 준비가 끝난 순간의 대상 위치로 고정(돌진 중 방향 전환 없음)
		Leap.Start = Spider->GetActorLocation();
		// 대상 몸 앞에서 멈춤(착지 직후 겹쳐 접촉 피해가 이어지지 않게), 경로 위 플레이어 판정은 그대로
		float SpiderRadius = 0.f;
		float SpiderHalfHeight = 0.f;
		float TargetRadius = 0.f;
		float TargetHalfHeight = 0.f;
		Spider->GetSimpleCollisionCylinder(SpiderRadius, SpiderHalfHeight);
		Target->GetSimpleCollisionCylinder(TargetRadius, TargetHalfHeight);
		const FVector TargetFlat(Target->GetActorLocation().X, Target->GetActorLocation().Y, Leap.Start.Z);
		const FVector Approach = (TargetFlat - Leap.Start).GetSafeNormal();
		const FVector Aim = TargetFlat - Approach * (SpiderRadius + TargetRadius);
		Leap.Destination = DigetherGroundSpiderRules::ComputeLeapDestination(Leap.Start, Aim, Params->LeapMaxDistance);
		const float Flight = DigetherGroundSpiderRules::ComputeLeapFlightSeconds(Leap.Start, Leap.Destination, Params->LeapSpeed);
		// 비행 중 플레이어를 통과하며 피해, 벽·지형에는 막힘, BP 충돌 프로필 값은 보관 후 복원
		SavedPawnResponse = Spider->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn);
		bPawnResponseOverridden = true;
		Spider->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Spider->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		Leap.PhaseEndTime = Now() + Flight;
		SetState(EDigetherSpiderState::LeapFlight, Flight);
		return;
	}
	if (PublicState.State == EDigetherSpiderState::LeapFlight)
	{
		const FVector Current = Spider->GetActorLocation();
		const FVector ToDest = Leap.Destination - Current;
		const float Remaining = ToDest.Size();
		const float Step = Params->LeapSpeed * DeltaTime;
		const FVector Next = Remaining <= Step ? Leap.Destination : Current + ToDest / FMath::Max(Remaining, KINDA_SMALL_NUMBER) * Step;

		// 경로 위 모든 플레이어(동료 포함) 판정, 도약 1회당 같은 대상 1회
		TArray<FHitResult> Hits;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(DigetherSpiderLeap), false, Spider);
		const UCapsuleComponent* Capsule = Spider->GetCapsuleComponent();
		World->SweepMultiByObjectType(Hits, Current, Next, Spider->GetActorQuat(), FCollisionObjectQueryParams(ECC_Pawn),
			FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Query);
		const UDigetherMonsterSenseSubsystem* Sense = World->GetSubsystem<UDigetherMonsterSenseSubsystem>();
		TArray<FDigetherMonsterTarget> Living;
		if (Sense)
		{
			Sense->CollectLivingTargets(Living);
		}
		for (const FHitResult& Hit : Hits)
		{
			APawn* Victim = Cast<APawn>(Hit.GetActor());
			const FDigetherMonsterTarget* Found = Living.FindByPredicate([Victim](const FDigetherMonsterTarget& T) { return T.Pawn.Get() == Victim; });
			if (!Victim || !Found || Leap.HitKeys.Contains(Found->PlayerKey))
			{
				continue;
			}
			Leap.HitKeys.Add(Found->PlayerKey);
			LeapDamageFrame.Add(Found->PlayerKey, GFrameCounter);
			// 도약 피해를 같은 순간의 접촉으로 보고 접촉 간격을 다시 셈(도약 직후 접촉 피해 중복 방지)
			LastContactDamageTime.Add(Found->PlayerKey, Now());
			UGameplayStatics::ApplyDamage(Victim, Params->LeapDamage, Spider->GetController(), Spider, UDamageType::StaticClass());
			UE_LOG(LogDigetherMonster, Log, TEXT("%s: 도약 %d 피해 %.0f → %s"), *Spider->GetName(), Leap.LeapId, Params->LeapDamage, *Found->PlayerKey);
		}
		FHitResult Block;
		Spider->SetActorLocation(Next, true, &Block);
		if (Block.bBlockingHit || Next.Equals(Leap.Destination, 1.f) || Now() >= Leap.PhaseEndTime + 0.5)
		{
			FinishLeapFlight();
			Leap.PhaseEndTime = Now() + Params->LeapLandingSeconds;
			// 착지 범위 피해 없음(기획)
			SetState(EDigetherSpiderState::LeapLanding, Params->LeapLandingSeconds);
		}
		return;
	}
	if (Now() >= Leap.PhaseEndTime)
	{
		SetState(TargetKey.IsEmpty() || !Target ? EDigetherSpiderState::Returning : EDigetherSpiderState::Tracking);
	}
}

void UDigetherGroundSpiderComponent::FinishLeapFlight()
{
	ADigetherMonsterCharacter* Spider = GetSpider();
	if (!Spider)
	{
		return;
	}
	if (bPawnResponseOverridden)
	{
		Spider->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, SavedPawnResponse);
		bPawnResponseOverridden = false;
	}
	Spider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void UDigetherGroundSpiderComponent::TickContact()
{
	const FDigetherGroundSpiderParams* Params = GetParams();
	ADigetherMonsterCharacter* Spider = GetSpider();
	const UDigetherMonsterSenseSubsystem* Sense = GetWorld()->GetSubsystem<UDigetherMonsterSenseSubsystem>();
	if (!Sense)
	{
		return;
	}
	float SpiderRadius = 0.f;
	float SpiderHalfHeight = 0.f;
	Spider->GetSimpleCollisionCylinder(SpiderRadius, SpiderHalfHeight);
	const FVector SpiderLocation = Spider->GetActorLocation();
	TArray<FDigetherMonsterTarget> Living;
	Sense->CollectLivingTargets(Living);
	for (const FDigetherMonsterTarget& Target : Living)
	{
		APawn* Pawn = Target.Pawn.Get();
		if (!Pawn)
		{
			continue;
		}
		float Radius = 0.f;
		float HalfHeight = 0.f;
		Pawn->GetSimpleCollisionCylinder(Radius, HalfHeight);
		const FVector Delta = Pawn->GetActorLocation() - SpiderLocation;
		const bool bTouching = FVector2D(Delta).Size() <= SpiderRadius + Radius + Params->ContactTolerance
			&& FMath::Abs(Delta.Z) <= SpiderHalfHeight + HalfHeight + Params->ContactTolerance;
		if (!bTouching)
		{
			continue;
		}
		const IDigetherMonsterTargetInterface* TargetInterface = Cast<IDigetherMonsterTargetInterface>(Pawn);
		DigetherGroundSpiderRules::FContactInput Input;
		Input.Now = Now();
		const double* Last = LastContactDamageTime.Find(Target.PlayerKey);
		Input.LastContactDamageTime = Last ? *Last : -1.0;
		Input.IntervalSeconds = Params->ContactIntervalSeconds;
		const uint64* LeapFrame = LeapDamageFrame.Find(Target.PlayerKey);
		Input.bLeapDamagedThisFrame = LeapFrame && *LeapFrame == GFrameCounter;
		Input.bTargetBuried = TargetInterface && TargetInterface->IsBuriedForMonster();
		if (!DigetherGroundSpiderRules::ShouldApplyContactDamage(Input))
		{
			continue;
		}
		LastContactDamageTime.Add(Target.PlayerKey, Input.Now);
		UGameplayStatics::ApplyDamage(Pawn, Params->ContactDamage, Spider->GetController(), Spider, UDamageType::StaticClass());
		// 몸 접촉도 감지 원인
		LastSeenTime.Add(Target.PlayerKey, Input.Now);
		if (PublicState.State == EDigetherSpiderState::Idle || PublicState.State == EDigetherSpiderState::Returning
			|| PublicState.State == EDigetherSpiderState::Investigating)
		{
			BeginTracking(Target.PlayerKey, false);
		}
	}
}

void UDigetherGroundSpiderComponent::HandleDamagedByServer(AController* InstigatedBy, AActor* DamageCauser)
{
	ADigetherMonsterCharacter* Spider = GetSpider();
	UDigetherMonsterSenseSubsystem* Sense = GetWorld() ? GetWorld()->GetSubsystem<UDigetherMonsterSenseSubsystem>() : nullptr;
	const APawn* Attacker = InstigatedBy ? InstigatedBy->GetPawn<APawn>() : Cast<APawn>(DamageCauser);
	if (!Spider || !Sense || !Attacker || !Attacker->GetPlayerState())
	{
		return;
	}
	// 피격 모션은 BP(체력 OnDamaged)에서, 행동은 끊지 않음(기획)
	FDigetherMonsterStimulusEvent Event;
	Event.Cause = EDigetherMonsterStimulus::Damaged;
	Event.Location = Attacker->GetActorLocation();
	Event.InstigatorKey = Sense->ResolvePlayerKey(Attacker->GetPlayerState());
	Event.ServerTime = Now();
	Sense->ReportStimulus(Spider, Event);
}

void UDigetherGroundSpiderComponent::HandleDied()
{
	if (!bServerInitialized)
	{
		return;
	}
	CancelLeap();
	GetWorld()->GetTimerManager().ClearTimer(EmergeTimer);
	if (Nest)
	{
		// 거미줄은 처치 후에도 남음(기획)
		Nest->NotifySpiderGone();
	}
	TargetKey.Reset();
	SetState(EDigetherSpiderState::Dead);
	SetComponentTickEnabled(false);
}

void UDigetherGroundSpiderComponent::NotifyBuried()
{
	const FDigetherGroundSpiderParams* Params = GetParams();
	ADigetherMonsterCharacter* Spider = GetSpider();
	if (!bServerInitialized || !Params || !Spider || PublicState.State == EDigetherSpiderState::Dead
		|| PublicState.State == EDigetherSpiderState::Buried)
	{
		return;
	}
	CancelLeap();
	if (AAIController* AI = Cast<AAIController>(Spider->GetController()))
	{
		AI->StopMovement();
	}
	Spider->GetCharacterMovement()->DisableMovement();
	SetState(EDigetherSpiderState::Buried, Params->BuriedEmergeSeconds);
	GetWorld()->GetTimerManager().SetTimer(EmergeTimer, this, &UDigetherGroundSpiderComponent::Emerge,
		FMath::Max(0.01f, Params->BuriedEmergeSeconds), false);
}

void UDigetherGroundSpiderComponent::Emerge()
{
	ADigetherMonsterCharacter* Spider = GetSpider();
	if (!Spider || PublicState.State != EDigetherSpiderState::Buried)
	{
		return;
	}
	// 1단계: 가장 가까운 빈 공간(내비 투영 지점)에 나타남, 2단계 B에서 스스로 파고 나오기로 교체
	FVector Location = Spider->GetActorLocation();
	if (const UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld()))
	{
		FNavLocation Projected;
		if (Nav->ProjectPointToNavigation(Location, Projected, FVector(500.f, 500.f, 500.f)))
		{
			Location = Projected.Location + FVector(0.f, 0.f, Spider->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		}
	}
	Spider->TeleportTo(Location, Spider->GetActorRotation(), false, true);
	Spider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	SetState(TargetKey.IsEmpty() ? EDigetherSpiderState::Returning : EDigetherSpiderState::Tracking);
}

void UDigetherGroundSpiderComponent::DrawDebug() const
{
#if ENABLE_DRAW_DEBUG
	if (CVarDigetherMonsterDebug.GetValueOnGameThread() == 0)
	{
		return;
	}
	const ADigetherMonsterCharacter* Spider = GetSpider();
	const UWorld* World = GetWorld();
	const UDigetherMonsterDefinition* Def = Spider ? Spider->GetDefinition() : nullptr;
	if (!Spider || !World || !Def)
	{
		return;
	}
	DrawDebugCircle(World, HomeLocation, GetTrackRadius(), 48, FColor::Green, false, -1.f, 0, 4.f, FVector::ForwardVector, FVector::RightVector, false);
	const float HalfAngle = FMath::DegreesToRadians(Def->Sight.FieldOfViewDegrees * 0.5f);
	DrawDebugCone(World, Spider->GetSenseOrigin(), Spider->GetActorForwardVector(), Def->Sight.Range, HalfAngle, HalfAngle, 12, FColor::Yellow);
	if (const APawn* Target = FindTargetPawn(TargetKey))
	{
		DrawDebugLine(World, Spider->GetSenseOrigin(), Target->GetActorLocation(), FColor::Red, false, -1.f, 0, 3.f);
	}
	DrawDebugString(World, Spider->GetActorLocation() + FVector(0.f, 0.f, 150.f),
		FString::Printf(TEXT("%s %s"), *UEnum::GetValueAsString(PublicState.State), *TargetKey), nullptr, FColor::White, 0.f, true);
#endif
}
