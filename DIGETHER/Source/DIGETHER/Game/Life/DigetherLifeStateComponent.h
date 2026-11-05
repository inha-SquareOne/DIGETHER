#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Life/DigetherLifeTypes.h"
#include "DigetherLifeStateComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherLifePublicStateChanged, const FDigetherLifePublicState&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherLifePrivateStateChanged, const FDigetherLifePrivateState&);

// PlayerState에 부착, 소유 액터 복제·소유 연결과 카메라 적용은 연결 계층 책임
UCLASS(ClassGroup=(Digether), meta=(BlueprintSpawnableComponent))
class DIGETHER_API UDigetherLifeStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherLifeStateComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Digether|Life")
	FDigetherLifeComponentState CaptureState() const;

	// 조율자는 모든 도메인 검사 후 bNotify=false로 적용하고 마지막에 통지
	// 드롭·잔고 손실·탈출 장치 소비를 다른 도메인에 명령하지 않음
	EDigetherLifeResult RestoreState(const FDigetherLifeComponentState& State, bool bNotify = true);
	EDigetherLifeResult ApplyUpdate(const FDigetherLifeUpdateResult& Update, bool bNotify = true);
	EDigetherLifeResult ApplyDeath(EDigetherDeathCause Cause, bool bNotify = true);
	EDigetherLifeResult ApplyEscape(EDigetherEscapeKind Kind, bool bNotify = true);
	EDigetherLifeResult Revive(bool bNotify = true);

	// 서버 보유 최신 후보 목록과 자기 식별자를 전달, 키만 보관하며 카메라 미조작
	EDigetherLifeResult SelectSpectateTarget(const TArray<FDigetherSpectateCandidate>& Candidates,
		const FString& SelfPlayerKey, EDigetherSpectateDirection Direction, bool bNotify = true);
	EDigetherLifeResult NotifyStateChanged();

	// 수신자는 표현만 갱신, 공개 알림에 개인 상세 미포함
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
