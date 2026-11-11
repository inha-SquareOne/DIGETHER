#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Game/Monster/DigetherMonsterTypes.h"
#include "DigetherMonsterCharacter.generated.h"

class ADigetherMonsterCharacter;
class UDigetherMonsterDefinition;
class UDigetherMonsterHealthComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDigetherMonsterRemoved, ADigetherMonsterCharacter* /*Monster*/, EDigetherMonsterRemovalReason);

// 몬스터 공통 기반, 종별 규칙은 UDigetherMonsterSpeciesComponent 하위 컴포넌트로 추가(상속 확장 안 함)
// 외형·애님·예고·사망 연출은 BP 하위 클래스 담당, 자동 테스트는 이 클래스를 직접 생성
UCLASS()
class DIGETHER_API ADigetherMonsterCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADigetherMonsterCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 서버, SpawnActorDeferred 뒤 FinishSpawning 전 1회
	// 정의를 체력·종 컴포넌트·이동에 일관 적용, 실패하면 호출자가 개체를 파괴
	bool InitializeFromDefinition(UDigetherMonsterDefinition* InDefinition, const FGuid& InRunId, FName InDungeonId);

	// 서버, 처치 외 사유로 탐사에서 제거(도주 소멸·탐사 종료), 통지 후 파괴
	void RemoveFromRun(EDigetherMonsterRemovalReason Reason);

	UFUNCTION(BlueprintPure, Category="Digether|Monster")
	UDigetherMonsterDefinition* GetDefinition() const { return Definition; }

	UFUNCTION(BlueprintPure, Category="Digether|Monster")
	UDigetherMonsterHealthComponent* GetHealthComponent() const { return HealthComponent; }

	const FGuid& GetRunId() const { return RunId; }
	FName GetDungeonId() const { return DungeonId; }
	bool IsRemoved() const { return bRemoved; }

	// 감각 서브시스템의 몬스터 시야 기준점(눈 위치)
	FVector GetSenseOrigin() const;

	// 서버 통지, 디렉터 장부·감각 등록 해제용
	FOnDigetherMonsterRemoved OnRemoved;

protected:
	// 사망 연출, 클라이언트 포함 체력 컴포넌트 사망 통지 때 호출
	UFUNCTION(BlueprintImplementableEvent, Category="Digether|Monster")
	void OnMonsterDied();

	// 처치 후 파괴까지 시간(연출용), 0이면 즉시
	UPROPERTY(EditDefaultsOnly, Category="Digether|Monster", meta=(ClampMin="0", Units="s"))
	float DeathDestroyDelay = 5.f;

	// 눈 높이(캡슐 중심 기준), 시야 판정 기준점
	UPROPERTY(EditDefaultsOnly, Category="Digether|Monster", meta=(Units="cm"))
	float SenseEyeHeight = 50.f;

private:
	bool ApplyDefinition(UDigetherMonsterDefinition* InDefinition, bool bInjectHealth);

	UFUNCTION()
	void HandleDied();

	void NotifyRemoved(EDigetherMonsterRemovalReason Reason);

	// 클라이언트에서 생성 시점보다 늦게 받은 경우 형상만 적용
	UFUNCTION()
	void OnRep_Definition();

	UPROPERTY(VisibleAnywhere, Category="Digether|Monster")
	TObjectPtr<UDigetherMonsterHealthComponent> HealthComponent;

	// 레벨 직접 배치 시험용 기본값, 디렉터 생성은 주입 값 사용
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_Definition, Category="Digether|Monster")
	TObjectPtr<UDigetherMonsterDefinition> Definition;

	// 형상·종 컴포넌트에 마지막으로 적용한 정의, 클라이언트 재적용 판단용
	UPROPERTY(Transient)
	TObjectPtr<UDigetherMonsterDefinition> AppliedDefinition;

	FGuid RunId;
	FName DungeonId;
	// 서버 주입 잠금, 클라이언트는 AppliedDefinition으로 재적용 판단
	bool bDefinitionApplied = false;
	bool bRemoved = false;
};
