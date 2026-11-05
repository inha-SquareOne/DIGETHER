#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Economy/DigetherEconomyTuning.h"
#include "Game/Economy/DigetherEconomyTypes.h"
#include "DigetherQuotaStateComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherQuotaStateChanged, const FDigetherQuotaPublicState&);
// 목표 달성 연출 시점이다. 이어하기·늦은 참가로 처음 받은 값에서는 부르지 않는다.
DECLARE_MULTICAST_DELEGATE(FOnDigetherQuotaCompleted);

// GameState에 부착한다. 목표 달성·일차 전환·게임 오버 판정은 조율자 책임이다.
UCLASS(ClassGroup=(Digether), meta=(BlueprintSpawnableComponent))
class DIGETHER_API UDigetherQuotaStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherQuotaStateComponent();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버는 누적 인정액을 포함한 판정·저장 원본을, 클라이언트는 누적 인정액이 0인 복제 값을 돌려준다.
	UFUNCTION(BlueprintPure, Category="Digether|Economy")
	FDigetherQuotaPublicState CaptureState() const;

	// 정산기 신호기 단계(PAY_SIGNAL_THRESHOLDS)다. 서버·클라이언트 모두 같은 값을 돌려준다.
	UFUNCTION(BlueprintPure, Category="Digether|Economy")
	EDigetherPaymentSignal GetPaymentSignal() const;

	// 기간 일차(현재 할당량 기준 1~4, 마감 다음 날 5)다. 서버·클라이언트 모두 같은 값이다. 초기화 전에는 0이다.
	UFUNCTION(BlueprintPure, Category="Digether|Economy")
	int32 GetPeriodDay() const { return PublicState.Quota.Day; }

	// 전체 일차(게임 시작부터 센 날)다. 서버·클라이언트 모두 같은 값이다. 초기화 전에는 0이다.
	UFUNCTION(BlueprintPure, Category="Digether|Economy")
	int32 GetTotalDay() const { return PublicState.Quota.TotalDay; }

	// 모든 도메인 후보 검사 후 bNotify=false로 적용하고 마지막에 통지한다.
	// 여기의 유효성 검사는 값의 형태만 검사하며 게임 진행을 판정하지 않는다.
	EDigetherEconomyResult RestoreState(const FDigetherQuotaPublicState& State, bool bNotify = true);
	EDigetherEconomyResult SetQuotaState(const FDigetherQuotaState& State, bool bNotify = true);

	// 게임 오버 공개를 확정한 조율자만 호출한다. 그 전에는 빈 배열을 유지한다.
	// 이 컴포넌트는 게임 오버 여부나 플레이어 식별자 발급을 결정하지 않는다.
	EDigetherEconomyResult SetHiddenReveals(const TArray<FDigetherHiddenRevealEntry>& HiddenReveals, bool bNotify = true);
	EDigetherEconomyResult NotifyStateChanged();

	// 신호기 경계를 GameMode의 경제 튜닝 원본과 맞춘다. 서버 복제 값(신호기 단계)도 다시 만든다.
	EDigetherEconomyResult SetSignalTuning(const FDigetherEconomyTuning& Tuning);

	// 붓기로 목표를 달성한 순간 조율자가 한 번 부른다. 저장하지 않는 연출 카운터만 올린다.
	EDigetherEconomyResult MarkQuotaCompleted();

	FOnDigetherQuotaStateChanged OnQuotaStateChanged;
	FOnDigetherQuotaCompleted OnQuotaCompleted;

private:
	UFUNCTION()
	void OnRep_ReplicatedState();

	UFUNCTION()
	void OnRep_QuotaCompletionCue();

	// 서버 판정·저장 원본이다. 정확한 누적 인정액을 담으므로 복제하지 않는다.
	// 클라이언트에서는 ReplicatedState로 다시 만든 조회 값(누적 인정액 0)을 담는다.
	UPROPERTY(Transient)
	FDigetherQuotaPublicState PublicState;

	// 같은 스냅샷으로 전달해 목표·일차·신호기와 공개 목록 사이의 별도 OnRep 순서를 피한다.
	UPROPERTY(ReplicatedUsing=OnRep_ReplicatedState)
	FDigetherQuotaReplicatedState ReplicatedState;

	// 신호기 경계는 GameMode의 경제 튜닝 원본을 SetSignalTuning으로 받는다. 받기 전에는 같은 중앙 기본값이다.
	FDigetherEconomyTuning SignalTuning;

	// 달성 연출용 카운터다. 저장하지 않고 누적 인정액도 담지 않는다. 서버는 1부터 시작해 기본값(0)과 달라지므로
	// 첫 복제가 반드시 도착하고, 클라이언트는 처음 받은 값을 연출 없이 기준으로만 삼는다(이어하기·늦은 참가).
	UPROPERTY(ReplicatedUsing=OnRep_QuotaCompletionCue)
	int32 QuotaCompletionCue = 0;

	bool bQuotaCompletionCueReceived = false;
};
