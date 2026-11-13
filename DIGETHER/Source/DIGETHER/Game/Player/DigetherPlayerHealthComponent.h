#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Player/DigetherPlayerHealthRules.h"
#include "DigetherPlayerHealthComponent.generated.h"

class AController;
class UDamageType;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDigetherPlayerHealthChanged, float /*PreviousHealth*/, float /*CurrentHealth*/);
DECLARE_MULTICAST_DELEGATE(FOnDigetherPlayerLifeChanged);

// 플레이어 체력(S03), 플레이어 캐릭터에 붙음, 피해는 엔진 ApplyDamage(OnTakeAnyDamage)로만 받음
// 서버가 원본, 체력 수치는 소유자에게만, 사망 여부는 모두에게 복제함(D2·D3 체력 표시 미정 → 남의 체력은 보내지 않음)
// 캐릭터에 두는 이유: 탐사 중 잃은 체력은 거점 귀환 때 초기화되고(답변 15) 탐사 중 끊겨도 캐릭터가 유지되므로 저장·재접속 보관이 필요 없음
// 사망 확정(탈출과 선착순)·드롭·관전은 생존 상태(UDigetherLifeStateComponent)와 런 흐름 책임, 이 컴포넌트는 체력 0을 알릴 뿐
UCLASS(ClassGroup=(Digether), meta=(BlueprintSpawnableComponent))
class DIGETHER_API UDigetherPlayerHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherPlayerHealthComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 표의 최대 체력으로 가득 채움, 모든 머신이 같은 표로 호출해 클라이언트도 최대치를 암
	// 서버에서 이미 피해를 받은 뒤에는 다시 채우지 않음(ResetToFull로만 초기화)
	void InitializeFromMaxHealth(float MaxHealth);

	// 서버 전용: 가득 채우고 사망을 품(부활·거점 귀환·개발 명령)
	void ResetToFull();

	bool IsReady() const { return bReady; }
	bool IsDead() const { return State.bDead; }
	float GetCurrentHealth() const { return State.Current; }
	float GetMaxHealth() const { return State.Max; }
	float GetHealthFraction() const { return State.Max > 0.f ? FMath::Clamp(State.Current / State.Max, 0.f, 1.f) : 0.f; }

	// 서버는 적용한 피해마다, 소유 클라이언트는 받은 값이 바뀔 때마다, 복제 사이 피해가 합쳐질 수 있어 횟수 판정 금지
	FOnDigetherPlayerHealthChanged OnHealthChanged;

	// 사망·부활 전이, 서버와 모든 클라이언트에서 한 번씩
	FOnDigetherPlayerLifeChanged OnDied;
	FOnDigetherPlayerLifeChanged OnRevived;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
		AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION()
	void OnRep_ReplicatedCurrentHealth(float PreviousHealth);

	UFUNCTION()
	void OnRep_ReplicatedDead();

	// 사망 여부 변화를 한 번만 알림
	void NotifyLifeIfChanged();

	// 소유자 전용 복제
	UPROPERTY(ReplicatedUsing=OnRep_ReplicatedCurrentHealth)
	float ReplicatedCurrentHealth = 0.f;

	// 모두에게 복제(다른 사람 화면의 사망 표현용)
	UPROPERTY(ReplicatedUsing=OnRep_ReplicatedDead)
	bool bReplicatedDead = false;

	FDigetherPlayerHealthState State;
	bool bReady = false;
	bool bTookDamage = false;
	bool bLastNotifiedDead = false;
};
