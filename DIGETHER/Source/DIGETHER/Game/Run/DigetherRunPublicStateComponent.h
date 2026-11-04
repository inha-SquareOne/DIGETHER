#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Run/DigetherRunTypes.h"
#include "DigetherRunPublicStateComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherRunPublicStateChanged, const FDigetherRunPublicState&);

// GameState에 부착한다. 숫자 시간은 노출하지 않고 조율자가 확정한 값만 전달한다.
UCLASS(ClassGroup=(Digether), meta=(BlueprintSpawnableComponent))
class DIGETHER_API UDigetherRunPublicStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherRunPublicStateComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Digether|Run")
	FDigetherRunPublicState CaptureState() const;

	// 전체 후보를 검사한 뒤 알림을 미뤄 여러 도메인의 중간 상태가 관찰되지 않게 한다.
	EDigetherRunResult RestoreState(const FDigetherRunPublicState& State, bool bNotify = true);
	EDigetherRunResult NotifyStateChanged();
	FOnDigetherRunPublicStateChanged OnRunPublicStateChanged;

private:
	UFUNCTION()
	void OnRep_PublicState();

	UPROPERTY(ReplicatedUsing=OnRep_PublicState)
	FDigetherRunPublicState PublicState;
};
