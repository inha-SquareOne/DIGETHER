#include "Game/Monster/DigetherMonsterHealthComponent.h"

#include "Game/Monster/DigetherMonsterDefinition.h"
#include "Game/Monster/DigetherMonsterLog.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UDigetherMonsterHealthComponent::UDigetherMonsterHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDigetherMonsterHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UDigetherMonsterHealthComponent, HealthState, COND_None);
}

void UDigetherMonsterHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		UE_LOG(LogDigetherMonster, Warning, TEXT("%s: 소유 Actor가 없어 몬스터 피해를 처리하지 않습니다."), *GetNameSafe(this));
		return;
	}

	if (Owner->HasAuthority())
	{
		InitializeHealthState();
	}

	Owner->OnTakeAnyDamage.AddUniqueDynamic(this, &UDigetherMonsterHealthComponent::HandleTakeAnyDamage);

	// 초기 복제가 BeginPlay보다 먼저 도착할 수 있다. 이미 사망한 상태도 로컬에서 한 번 알린다.
	// 후입 시 과거 피해는 재생하지 않는다. 이 시점 이후 구독자는 IsDead()로 현재 상태를 조회한다.
	NotifyHealthState(HealthState.CurrentHealth);
}

void UDigetherMonsterHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AActor* Owner = GetOwner();
	if (IsValid(Owner))
	{
		Owner->OnTakeAnyDamage.RemoveDynamic(this, &UDigetherMonsterHealthComponent::HandleTakeAnyDamage);
	}

	Super::EndPlay(EndPlayReason);
}

void UDigetherMonsterHealthComponent::InitializeHealthState()
{
	if (bHasInitializedHealth)
	{
		return;
	}
	bHasInitializedHealth = true;

	// 정의 누락·오류 시 기본 불사 상태를 유지하며 임의의 체력 수치를 채우지 않는다.
	HealthState = FDigetherMonsterHealthState();
	if (!IsValid(Definition))
	{
		UE_LOG(LogDigetherMonster, Warning, TEXT("%s: 몬스터 정의가 없어 피해를 무시합니다."), *GetNameSafe(GetOwner()));
		return;
	}

	TArray<FText> Problems;
	if (!Definition->CollectProblems(Problems))
	{
		for (const FText& Problem : Problems)
		{
			UE_LOG(LogDigetherMonster, Warning, TEXT("%s: 몬스터 정의 %s 오류: %s. 피해를 무시합니다."),
				*GetNameSafe(GetOwner()), *GetNameSafe(Definition), *Problem.ToString());
		}
		return;
	}

	if (!DigetherMonsterHealthRules::TryCreateState(Definition->bKillable, Definition->MaxHealth, HealthState))
	{
		UE_LOG(LogDigetherMonster, Warning, TEXT("%s: 몬스터 체력을 초기화하지 못해 피해를 무시합니다."), *GetNameSafe(GetOwner()));
		return;
	}

	// 무적·피해 허용 여부는 소유 Actor의 정책이다. 컴포넌트가 SetCanBeDamaged로 덮어쓰지 않는다.
	if (HealthState.bKillable && !GetOwner()->CanBeDamaged())
	{
		UE_LOG(LogDigetherMonster, Warning, TEXT("%s: 처치 가능한 정의이지만 CanBeDamaged=false이므로 피해를 무시합니다."), *GetNameSafe(GetOwner()));
	}
}

void UDigetherMonsterHealthComponent::HandleTakeAnyDamage(AActor* DamagedActor, float Damage,
	const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || DamagedActor != Owner || !Owner->HasAuthority() || !HasBegunPlay())
	{
		return;
	}

	// 일반 AActor의 TakeDamage는 CanBeDamaged를 검사하지 않으므로 표준 입력에서도 직접 존중한다.
	if (!Owner->CanBeDamaged())
	{
		return;
	}

	const float PreviousHealth = HealthState.CurrentHealth;
	// 휴면 중인 Actor는 상태를 바꾸기 전에 깨워야 기존 상태가 복제 기준으로 남는다.
	// 무시할 입력에서는 휴면을 깨우지 않도록 순수 판정을 복사본에 먼저 적용한다.
	FDigetherMonsterHealthState NextState = HealthState;
	const EDigetherMonsterDamageResult Result = DigetherMonsterHealthRules::ApplyDamage(NextState, Damage);
	if (Result != EDigetherMonsterDamageResult::Damaged && Result != EDigetherMonsterDamageResult::Killed)
	{
		return;
	}

	Owner->FlushNetDormancy();
	HealthState = NextState;
	Owner->ForceNetUpdate();

	// C++ 서버 대입은 OnRep를 호출하지 않는다. 리슨 호스트도 같은 통지 경로를 직접 거친다.
	NotifyHealthState(PreviousHealth);
}

void UDigetherMonsterHealthComponent::OnRep_HealthState(const FDigetherMonsterHealthState& PreviousState)
{
	if (HasBegunPlay())
	{
		NotifyHealthState(PreviousState.CurrentHealth);
	}
}

void UDigetherMonsterHealthComponent::NotifyHealthState(float PreviousHealth)
{
	const float CurrentHealth = HealthState.CurrentHealth;
	const bool bNotifyDeath = HealthState.Life == EDigetherMonsterLife::Dead && LastNotifiedLife != EDigetherMonsterLife::Dead;
	if (bNotifyDeath)
	{
		// 외부 델리게이트가 재진입해도 사망을 중복 통지하지 않도록 방송 전에 기록한다.
		LastNotifiedLife = EDigetherMonsterLife::Dead;
	}

	if (CurrentHealth < PreviousHealth)
	{
		OnDamaged.Broadcast(PreviousHealth, CurrentHealth);
	}
	if (bNotifyDeath)
	{
		OnDied.Broadcast();
	}
}

bool UDigetherMonsterHealthComponent::IsKillable() const
{
	return HealthState.bKillable;
}

bool UDigetherMonsterHealthComponent::IsDead() const
{
	return HealthState.Life == EDigetherMonsterLife::Dead;
}

float UDigetherMonsterHealthComponent::GetCurrentHealth() const
{
	return HealthState.CurrentHealth;
}

float UDigetherMonsterHealthComponent::GetMaxHealth() const
{
	return HealthState.MaxHealth;
}
