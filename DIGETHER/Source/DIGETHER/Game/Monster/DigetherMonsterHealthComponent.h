#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Monster/DigetherMonsterHealthRules.h"
#include "DigetherMonsterHealthComponent.generated.h"

class AController;
class UDamageType;
class UDigetherMonsterDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDigetherMonsterDamaged, float, PreviousHealth, float, CurrentHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDigetherMonsterDied);

// 체력 판정과 공개 상태만 담당한다. 파괴·드롭·래그돌·AI 정지는 통지 수신자의 책임이다.
UCLASS(ClassGroup=(Digether), meta=(BlueprintSpawnableComponent))
class DIGETHER_API UDigetherMonsterHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherMonsterHealthComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 종별 정의는 클래스 기본값에서 연결한다. BeginPlay 이후 교체로 회복·부활하지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster")
	TObjectPtr<UDigetherMonsterDefinition> Definition;

	// 서버는 적용한 피해마다, 클라이언트는 수신한 체력 감소마다 통지한다.
	// 복제 사이의 여러 피해는 합쳐질 수 있으므로 클라이언트의 타격 횟수 판정에 쓰지 않는다.
	UPROPERTY(BlueprintAssignable, Category="Digether|Monster")
	FOnDigetherMonsterDamaged OnDamaged;

	UPROPERTY(BlueprintAssignable, Category="Digether|Monster")
	FOnDigetherMonsterDied OnDied;

	UFUNCTION(BlueprintPure, Category="Digether|Monster")
	bool IsKillable() const;

	UFUNCTION(BlueprintPure, Category="Digether|Monster")
	bool IsDead() const;

	UFUNCTION(BlueprintPure, Category="Digether|Monster")
	float GetCurrentHealth() const;

	UFUNCTION(BlueprintPure, Category="Digether|Monster")
	float GetMaxHealth() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void InitializeHealthState();
	void NotifyHealthState(float PreviousHealth);

	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
		AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION()
	void OnRep_HealthState(const FDigetherMonsterHealthState& PreviousState);

	// 관련성 있는 모든 클라이언트에 공개한다(COND_None). 소유자가 없는 몬스터도 수신해야 한다.
	// 생명 상태와 체력을 같은 RepNotify에서 읽고, 클라이언트의 정의 로드 여부에 의존하지 않는다.
	// 소유 Actor의 bReplicates와 관련성 설정은 몬스터 Actor 담당이 연결해야 한다.
	UPROPERTY(ReplicatedUsing=OnRep_HealthState)
	FDigetherMonsterHealthState HealthState;

	// 스트리밍 등으로 같은 객체가 다시 BeginPlay해도 체력을 채우거나 부활시키지 않는다.
	bool bHasInitializedHealth = false;

	// 로컬 통지 이력이며 게임 생명 상태의 원본은 HealthState.Life 하나다.
	// EndPlay에서 초기화하지 않아 같은 컴포넌트의 중복 수신·재진입에도 사망을 다시 알리지 않는다.
	EDigetherMonsterLife LastNotifiedLife = EDigetherMonsterLife::Alive;
};
