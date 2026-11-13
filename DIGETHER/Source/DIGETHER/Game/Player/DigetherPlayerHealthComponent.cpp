#include "Game/Player/DigetherPlayerHealthComponent.h"

#include "GameFramework/Actor.h"
#include "Game/Player/DigetherPlayerLog.h"
#include "Net/UnrealNetwork.h"

UDigetherPlayerHealthComponent::UDigetherPlayerHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDigetherPlayerHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UDigetherPlayerHealthComponent, ReplicatedCurrentHealth, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDigetherPlayerHealthComponent, bReplicatedDead, COND_None);
}

void UDigetherPlayerHealthComponent::InitializeFromMaxHealth(const float MaxHealth)
{
	if (bTookDamage)
	{
		return;
	}
	if (!FDigetherPlayerHealthRules::TryMakeFull(MaxHealth, State))
	{
		bReady = false;
		UE_LOG(LogDigetherPlayer, Error, TEXT("%s: 최대 체력 %g가 잘못되어 체력을 쓰지 않는다(피해 무시)"), *GetNameSafe(GetOwner()), MaxHealth);
		return;
	}
	bReady = true;
	ReplicatedCurrentHealth = State.Current;
	bReplicatedDead = false;
}

void UDigetherPlayerHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	AActor* Owner = GetOwner();
	if (IsValid(Owner))
	{
		Owner->OnTakeAnyDamage.AddUniqueDynamic(this, &UDigetherPlayerHealthComponent::HandleTakeAnyDamage);
	}
	// 초기 복제가 BeginPlay보다 먼저 왔으면(이미 죽은 채로 보임) 여기서 한 번 알림
	NotifyLifeIfChanged();
}

void UDigetherPlayerHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AActor* Owner = GetOwner();
	if (IsValid(Owner))
	{
		Owner->OnTakeAnyDamage.RemoveDynamic(this, &UDigetherPlayerHealthComponent::HandleTakeAnyDamage);
	}
	Super::EndPlay(EndPlayReason);
}

void UDigetherPlayerHealthComponent::HandleTakeAnyDamage(AActor* DamagedActor, float Damage,
	const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	AActor* Owner = GetOwner();
	if (!bReady || !IsValid(Owner) || DamagedActor != Owner || !Owner->HasAuthority())
	{
		return;
	}
	// 일반 AActor::TakeDamage 경로는 CanBeDamaged를 보지 않을 수 있어 직접 확인(무적 정책은 소유 Actor 책임)
	if (!Owner->CanBeDamaged())
	{
		return;
	}

	const float PreviousHealth = State.Current;
	const EDigetherPlayerDamageResult Result = FDigetherPlayerHealthRules::ApplyDamage(State, Damage);
	if (Result != EDigetherPlayerDamageResult::Damaged && Result != EDigetherPlayerDamageResult::Killed)
	{
		return;
	}
	bTookDamage = true;
	ReplicatedCurrentHealth = State.Current;
	bReplicatedDead = State.bDead;
	Owner->ForceNetUpdate();

	UE_LOG(LogDigetherPlayer, Log, TEXT("%s 피해 %g (%s): 체력 %g -> %g%s"), *GetNameSafe(Owner), Damage,
		*GetNameSafe(DamageCauser), PreviousHealth, State.Current, State.bDead ? TEXT(" [사망]") : TEXT(""));

	// 서버의 C++ 대입은 OnRep을 부르지 않으므로 같은 통지를 직접 함(리슨 서버 호스트 포함)
	OnHealthChanged.Broadcast(PreviousHealth, State.Current);
	NotifyLifeIfChanged();
}

void UDigetherPlayerHealthComponent::ResetToFull()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || State.Max <= 0.f)
	{
		return;
	}
	const float PreviousHealth = State.Current;
	FDigetherPlayerHealthRules::TryMakeFull(State.Max, State);
	bReady = true;
	bTookDamage = false;
	ReplicatedCurrentHealth = State.Current;
	bReplicatedDead = false;
	Owner->ForceNetUpdate();
	OnHealthChanged.Broadcast(PreviousHealth, State.Current);
	NotifyLifeIfChanged();
}

void UDigetherPlayerHealthComponent::OnRep_ReplicatedCurrentHealth(const float PreviousHealth)
{
	State.Current = ReplicatedCurrentHealth;
	OnHealthChanged.Broadcast(PreviousHealth, State.Current);
}

void UDigetherPlayerHealthComponent::OnRep_ReplicatedDead()
{
	State.bDead = bReplicatedDead;
	if (HasBegunPlay())
	{
		NotifyLifeIfChanged();
	}
}

void UDigetherPlayerHealthComponent::NotifyLifeIfChanged()
{
	if (State.bDead == bLastNotifiedDead)
	{
		return;
	}
	// 수신자가 재진입해도 중복 통지하지 않도록 방송 전에 기록
	bLastNotifiedDead = State.bDead;
	if (State.bDead)
	{
		OnDied.Broadcast();
	}
	else
	{
		OnRevived.Broadcast();
	}
}
