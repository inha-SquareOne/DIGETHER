#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Economy/DigetherEconomyTypes.h"
#include "DigetherQuotaStateComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherQuotaStateChanged, const FDigetherQuotaPublicState&);

// GameState에 부착한다. 목표 달성·일차 전환·게임 오버 판정은 조율자 책임이다.
UCLASS(ClassGroup=(Digether), meta=(BlueprintSpawnableComponent))
class DIGETHER_API UDigetherQuotaStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherQuotaStateComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Digether|Economy")
	FDigetherQuotaPublicState CaptureState() const;

	// 모든 도메인 후보 검사 후 bNotify=false로 적용하고 마지막에 통지한다.
	// 여기의 유효성 검사는 값의 형태만 검사하며 게임 진행을 판정하지 않는다.
	EDigetherEconomyResult RestoreState(const FDigetherQuotaPublicState& State, bool bNotify = true);
	EDigetherEconomyResult SetQuotaState(const FDigetherQuotaState& State, bool bNotify = true);

	// 게임 오버 공개를 확정한 조율자만 호출한다. 그 전에는 빈 배열을 유지한다.
	// 이 컴포넌트는 게임 오버 여부나 플레이어 식별자 발급을 결정하지 않는다.
	EDigetherEconomyResult SetHiddenReveals(const TArray<FDigetherHiddenRevealEntry>& HiddenReveals, bool bNotify = true);
	EDigetherEconomyResult NotifyStateChanged();

	FOnDigetherQuotaStateChanged OnQuotaStateChanged;

private:
	UFUNCTION()
	void OnRep_PublicState();

	// 같은 스냅샷으로 전달해 목표·일차와 공개 목록 사이의 별도 OnRep 순서를 피한다.
	UPROPERTY(ReplicatedUsing=OnRep_PublicState)
	FDigetherQuotaPublicState PublicState;
};
