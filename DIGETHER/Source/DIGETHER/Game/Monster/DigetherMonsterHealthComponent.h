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

// 체력 판정과 공개 상태만 담당, 파괴·드롭·래그돌·AI 정지는 통지 수신자 책임
UCLASS(ClassGroup=(Digether), meta=(BlueprintSpawnableComponent))
class DIGETHER_API UDigetherMonsterHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherMonsterHealthComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 종별 정의는 클래스 기본값에서 연결, BeginPlay 이후 교체해도 회복·부활 없음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Monster")
	TObjectPtr<UDigetherMonsterDefinition> Definition;

	// 서버는 적용한 피해마다, 클라이언트는 수신한 체력 감소마다 통지
	// 복제 사이 여러 피해가 합쳐질 수 있어 클라이언트 타격 횟수 판정에 사용 금지
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

	// 관련성 있는 모든 클라이언트에 공개(COND_None), 소유자 없는 몬스터도 수신 필요
	// 생명 상태·체력을 같은 RepNotify에서 읽음, 클라이언트 정의 로드 여부와 무관
	// 소유 Actor의 bReplicates·관련성 설정은 몬스터 Actor 담당이 연결
	UPROPERTY(ReplicatedUsing=OnRep_HealthState)
	FDigetherMonsterHealthState HealthState;

	// 스트리밍 등으로 다시 BeginPlay해도 체력 재충전·부활 방지
	bool bHasInitializedHealth = false;

	// 로컬 통지 이력, 생명 상태 원본은 HealthState.Life
	// EndPlay에서 초기화하지 않음, 중복 수신·재진입에도 사망 재통지 방지
	EDigetherMonsterLife LastNotifiedLife = EDigetherMonsterLife::Alive;
};
