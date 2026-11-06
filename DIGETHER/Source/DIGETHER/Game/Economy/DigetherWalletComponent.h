#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Economy/DigetherEconomyTypes.h"
#include "DigetherWalletComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherWalletChanged, const FDigetherWalletState&);

// PlayerState에 부착, 소유 Actor의 복제·소유 연결 설정 필요
UCLASS(ClassGroup=(Digether), meta=(BlueprintSpawnableComponent))
class DIGETHER_API UDigetherWalletComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherWalletComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Digether|Economy")
	FDigetherWalletState CaptureState() const;

	// 조율자가 모든 후보 검사 후 bNotify=false로 일괄 적용
	// 전부 성공 후 NotifyStateChanged 호출, 수신자는 표현만 갱신
	EDigetherEconomyResult RestoreState(const FDigetherWalletState& State, bool bNotify = true);
	EDigetherEconomyResult ApplyUpdate(const FDigetherWalletUpdateResult& Update, bool bNotify = true);
	EDigetherEconomyResult NotifyStateChanged();

	FOnDigetherWalletChanged OnWalletChanged;

private:
	UFUNCTION()
	void OnRep_PrivateState();

	// 공개 복사본 없음(COND_OwnerOnly), 게임 오버 히든 공개는 조율자가 QuotaStateComponent에 별도 확정
	UPROPERTY(ReplicatedUsing=OnRep_PrivateState)
	FDigetherWalletState PrivateState;
};
