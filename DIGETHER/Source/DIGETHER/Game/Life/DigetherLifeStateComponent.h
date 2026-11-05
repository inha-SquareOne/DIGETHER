#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Life/DigetherLifeTypes.h"
#include "DigetherLifeStateComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherLifePublicStateChanged, const FDigetherLifePublicState&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherLifePrivateStateChanged, const FDigetherLifePrivateState&);

// PlayerState에 부착한다. 소유 액터의 복제·소유 연결과 카메라 적용은 연결 계층의 책임이다.
UCLASS(ClassGroup=(Digether), meta=(BlueprintSpawnableComponent))
class DIGETHER_API UDigetherLifeStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherLifeStateComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Digether|Life")
	FDigetherLifeComponentState CaptureState() const;

	// 조율자는 모든 도메인을 검사한 뒤 bNotify=false로 적용하고 마지막에 통지한다.
	// 이 컴포넌트는 드롭·잔고 손실·탈출 장치 소비를 다른 도메인에 명령하지 않는다.
	EDigetherLifeResult RestoreState(const FDigetherLifeComponentState& State, bool bNotify = true);
	EDigetherLifeResult ApplyUpdate(const FDigetherLifeUpdateResult& Update, bool bNotify = true);
	EDigetherLifeResult ApplyDeath(EDigetherDeathCause Cause, bool bNotify = true);
	EDigetherLifeResult ApplyEscape(EDigetherEscapeKind Kind, bool bNotify = true);
	EDigetherLifeResult Revive(bool bNotify = true);

	// 서버가 보유한 최신 후보 목록과 자기 식별자를 전달한다. 키만 보관하며 카메라는 건드리지 않는다.
	EDigetherLifeResult SelectSpectateTarget(const TArray<FDigetherSpectateCandidate>& Candidates,
		const FString& SelfPlayerKey, EDigetherSpectateDirection Direction, bool bNotify = true);
	EDigetherLifeResult NotifyStateChanged();

	// 수신자는 표현만 갱신한다. 공개 알림에 개인 상세를 섞지 않는다.
	FOnDigetherLifePublicStateChanged OnPublicStateChanged;
	FOnDigetherLifePrivateStateChanged OnPrivateStateChanged;

private:
	UFUNCTION()
	void OnRep_PublicState();

	UFUNCTION()
	void OnRep_PrivateState();

	UPROPERTY(ReplicatedUsing=OnRep_PublicState)
	FDigetherLifePublicState PublicState;

	UPROPERTY(ReplicatedUsing=OnRep_PrivateState)
	FDigetherLifePrivateState PrivateState;
};
