#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Economy/DigetherEconomyTypes.h"
#include "DigetherWalletComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherWalletChanged, const FDigetherWalletState&);

// PlayerState에 부착하고 소유 Actor의 복제와 소유 연결을 설정해야 한다.
UCLASS(ClassGroup=(Digether), meta=(BlueprintSpawnableComponent))
class DIGETHER_API UDigetherWalletComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherWalletComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Digether|Economy")
	FDigetherWalletState CaptureState() const;

	// 조율자가 모든 후보를 검사한 뒤 bNotify=false로 일괄 적용하고,
	// 전부 성공한 후 NotifyStateChanged를 호출한다. 수신자는 표현만 갱신한다.
	EDigetherEconomyResult RestoreState(const FDigetherWalletState& State, bool bNotify = true);
	EDigetherEconomyResult ApplyUpdate(const FDigetherWalletUpdateResult& Update, bool bNotify = true);
	EDigetherEconomyResult NotifyStateChanged();

	FOnDigetherWalletChanged OnWalletChanged;

private:
	UFUNCTION()
	void OnRep_PrivateState();

	// 지갑에는 공개 복사본을 두지 않는다. 게임 오버의 히든 공개는 조율자가
	// QuotaStateComponent에 별도로 확정하므로 개인 지갑의 복제 조건을 바꾸지 않는다.
	UPROPERTY(ReplicatedUsing=OnRep_PrivateState)
	FDigetherWalletState PrivateState;
};
