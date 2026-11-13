#include "Game/Monster/DigetherMonsterCharacter.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "Game/Monster/DigetherMonsterAIController.h"
#include "Game/Monster/DigetherMonsterDefinition.h"
#include "Game/Monster/DigetherMonsterHealthComponent.h"
#include "Game/Monster/DigetherMonsterLog.h"
#include "Game/Monster/DigetherMonsterSenseSubsystem.h"
#include "Game/Monster/DigetherMonsterSpeciesComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

ADigetherMonsterCharacter::ADigetherMonsterCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	AIControllerClass = ADigetherMonsterAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;

	HealthComponent = CreateDefaultSubobject<UDigetherMonsterHealthComponent>(TEXT("HealthComponent"));

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->bUseControllerDesiredRotation = false;
	}
}

void ADigetherMonsterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// 정의는 불변 에셋, 클라이언트 표시·형상 적용용
	DOREPLIFETIME_CONDITION(ADigetherMonsterCharacter, Definition, COND_InitialOnly);
}

void ADigetherMonsterCharacter::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	// 레벨 직접 배치(시험용) 또는 클라이언트: 디렉터 주입이 없었으면 현재 정의로 형상만 맞춤
	if (!bDefinitionApplied && IsValid(Definition))
	{
		ApplyDefinition(Definition, HasAuthority());
	}
}

void ADigetherMonsterCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (IsValid(HealthComponent))
	{
		HealthComponent->OnDied.AddUniqueDynamic(this, &ADigetherMonsterCharacter::HandleDied);
		// 초기 복제로 이미 사망 상태로 받은 경우
		if (HealthComponent->IsDead())
		{
			HandleDied();
		}
	}
	if (HasAuthority() && !bRemoved)
	{
		if (UDigetherMonsterSenseSubsystem* Sense = GetWorld()->GetSubsystem<UDigetherMonsterSenseSubsystem>())
		{
			Sense->RegisterMonster(this);
		}
	}
}

void ADigetherMonsterCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 디렉터를 거치지 않은 파괴(레벨 언로드 등)도 장부에서 빠지도록 통지
	if (HasAuthority() && !bRemoved)
	{
		NotifyRemoved(EDigetherMonsterRemovalReason::Despawned);
	}
	if (UDigetherMonsterSenseSubsystem* Sense = GetWorld() ? GetWorld()->GetSubsystem<UDigetherMonsterSenseSubsystem>() : nullptr)
	{
		Sense->UnregisterMonster(this);
	}
	Super::EndPlay(EndPlayReason);
}

bool ADigetherMonsterCharacter::InitializeFromDefinition(UDigetherMonsterDefinition* InDefinition, const FGuid& InRunId, const FName InDungeonId)
{
	if (!HasAuthority() || HasActorBegunPlay() || bDefinitionApplied || !IsValid(InDefinition))
	{
		UE_LOG(LogDigetherMonster, Warning, TEXT("%s: 정의 초기화 거절(권한 없음, 이미 시작·적용됨, 또는 정의 없음)."), *GetName());
		return false;
	}
	RunId = InRunId;
	DungeonId = InDungeonId;
	return ApplyDefinition(InDefinition, true);
}

bool ADigetherMonsterCharacter::ApplyDefinition(UDigetherMonsterDefinition* InDefinition, const bool bInjectHealth)
{
	Definition = InDefinition;
	AppliedDefinition = InDefinition;
	bDefinitionApplied = true;

	const FDigetherMonsterMovementParams& Params = InDefinition->Movement;
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCapsuleSize(Params.CapsuleRadius, FMath::Max(Params.CapsuleHalfHeight, Params.CapsuleRadius));
	}
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxStepHeight = Params.MaxStepHeight;
		Movement->MaxWalkSpeed = Params.WalkSpeed;
		Movement->NavAgentProps.AgentRadius = Params.CapsuleRadius;
		Movement->NavAgentProps.AgentHeight = Params.CapsuleHalfHeight * 2.f;
		Movement->NavAgentProps.AgentStepHeight = Params.MaxStepHeight;
	}

	TInlineComponentArray<UDigetherMonsterSpeciesComponent*> SpeciesComponents(this);
	if (!bInjectHealth)
	{
		// 클라이언트 표현용: 종 컴포넌트도 복제된 정의를 읽게 함(체력은 서버 상태 복제)
		for (UDigetherMonsterSpeciesComponent* Species : SpeciesComponents)
		{
			Species->InitializeSpecies(*InDefinition);
		}
		return true;
	}
	bool bOk = IsValid(HealthComponent) && HealthComponent->InitializeDefinition(InDefinition);
	for (UDigetherMonsterSpeciesComponent* Species : SpeciesComponents)
	{
		bOk &= Species->InitializeSpecies(*InDefinition);
	}
	if (!bOk)
	{
		UE_LOG(LogDigetherMonster, Warning, TEXT("%s: 정의 %s 적용 실패."), *GetName(), *GetNameSafe(InDefinition));
	}
	return bOk;
}

void ADigetherMonsterCharacter::RemoveFromRun(const EDigetherMonsterRemovalReason Reason)
{
	if (!HasAuthority() || bRemoved)
	{
		return;
	}
	NotifyRemoved(Reason);
	Destroy();
}

FVector ADigetherMonsterCharacter::GetSenseOrigin() const
{
	return GetActorLocation() + FVector(0.f, 0.f, SenseEyeHeight);
}

void ADigetherMonsterCharacter::HandleDied()
{
	OnMonsterDied();
	if (!HasAuthority() || bRemoved)
	{
		return;
	}
	if (const AAIController* AI = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AI->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Died"));
		}
	}
	GetCharacterMovement()->DisableMovement();
	NotifyRemoved(EDigetherMonsterRemovalReason::Killed);
	if (DeathDestroyDelay > 0.f)
	{
		SetLifeSpan(DeathDestroyDelay);
	}
	else
	{
		Destroy();
	}
}

void ADigetherMonsterCharacter::NotifyRemoved(const EDigetherMonsterRemovalReason Reason)
{
	if (bRemoved)
	{
		return;
	}
	// 방송 전에 기록해 재진입 시 중복 반환 방지
	bRemoved = true;
	if (UDigetherMonsterSenseSubsystem* Sense = GetWorld() ? GetWorld()->GetSubsystem<UDigetherMonsterSenseSubsystem>() : nullptr)
	{
		Sense->UnregisterMonster(this);
	}
	OnRemoved.Broadcast(this, Reason);
}

void ADigetherMonsterCharacter::OnRep_Definition()
{
	// 클라이언트는 생성 시 BP 기본 정의로 형상을 먼저 맞추므로, 디렉터가 주입한 정의가 다르면 다시 적용
	if (!HasAuthority() && IsValid(Definition) && Definition != AppliedDefinition)
	{
		ApplyDefinition(Definition, false);
	}
}
