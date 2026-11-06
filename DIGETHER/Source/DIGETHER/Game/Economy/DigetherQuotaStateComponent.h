#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Economy/DigetherEconomyTuning.h"
#include "Game/Economy/DigetherEconomyTypes.h"
#include "DigetherQuotaStateComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherQuotaStateChanged, const FDigetherQuotaPublicState&);
// 목표 달성 연출 시점, 이어하기·늦은 참가로 처음 받은 값에서는 호출 안 함
DECLARE_MULTICAST_DELEGATE(FOnDigetherQuotaCompleted);

// GameState에 부착, 목표 달성·일차 전환·게임 오버 판정은 조율자 책임
UCLASS(ClassGroup=(Digether), meta=(BlueprintSpawnableComponent))
class DIGETHER_API UDigetherQuotaStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherQuotaStateComponent();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버는 누적 인정액 포함 판정·저장 원본, 클라이언트는 누적 인정액 0인 복제 값 반환
	UFUNCTION(BlueprintPure, Category="Digether|Economy")
	FDigetherQuotaPublicState CaptureState() const;

	// 정산기 신호기 단계(PAY_SIGNAL_THRESHOLDS), 서버·클라이언트 동일 값
	UFUNCTION(BlueprintPure, Category="Digether|Economy")
	EDigetherPaymentSignal GetPaymentSignal() const;

	// 기간 일차(현재 할당량 기준 1~4, 마감 다음 날 5), 서버·클라이언트 동일, 초기화 전 0
	UFUNCTION(BlueprintPure, Category="Digether|Economy")
	int32 GetPeriodDay() const { return PublicState.Quota.Day; }

	// 전체 일차(게임 시작부터 센 날), 서버·클라이언트 동일, 초기화 전 0
	UFUNCTION(BlueprintPure, Category="Digether|Economy")
	int32 GetTotalDay() const { return PublicState.Quota.TotalDay; }

	// 조율자는 모든 도메인 후보 검사 후 bNotify=false로 적용하고 마지막에 통지
	// 유효성 검사는 값 형태만 확인, 게임 진행 판정 안 함
	EDigetherEconomyResult RestoreState(const FDigetherQuotaPublicState& State, bool bNotify = true);
	EDigetherEconomyResult SetQuotaState(const FDigetherQuotaState& State, bool bNotify = true);

	// 게임 오버 공개를 확정한 조율자만 호출, 그 전에는 빈 배열 유지
	// 게임 오버 여부·플레이어 식별자 발급은 이 컴포넌트가 결정하지 않음
	EDigetherEconomyResult SetHiddenReveals(const TArray<FDigetherHiddenRevealEntry>& HiddenReveals, bool bNotify = true);
	EDigetherEconomyResult NotifyStateChanged();

	// 신호기 경계를 GameMode 경제 튜닝 원본과 동기화, 복제 값(신호기 단계)도 재생성
	EDigetherEconomyResult SetSignalTuning(const FDigetherEconomyTuning& Tuning, bool bNotify = true);

	// 붓기로 목표를 달성한 순간 조율자가 1회 호출, 저장하지 않는 연출 카운터만 증가
	EDigetherEconomyResult MarkQuotaCompleted();

	FOnDigetherQuotaStateChanged OnQuotaStateChanged;
	FOnDigetherQuotaCompleted OnQuotaCompleted;

private:
	UFUNCTION()
	void OnRep_ReplicatedState();

	UFUNCTION()
	void OnRep_QuotaCompletionCue();

	// 서버 판정·저장 원본, 정확한 누적 인정액 포함이라 복제 안 함
	// 클라이언트에서는 ReplicatedState로 재구성한 조회 값(누적 인정액 0)
	UPROPERTY(Transient)
	FDigetherQuotaPublicState PublicState;

	// 목표·일차·신호기·공개 목록을 한 스냅샷으로 전달해 별도 OnRep 순서 문제 회피
	UPROPERTY(ReplicatedUsing=OnRep_ReplicatedState)
	FDigetherQuotaReplicatedState ReplicatedState;

	// 신호기 경계는 SetSignalTuning으로 GameMode 경제 튜닝 원본을 받음, 받기 전에는 같은 중앙 기본값
	FDigetherEconomyTuning SignalTuning;

	// 달성 연출용 카운터, 저장 안 함·누적 인정액 미포함
	// 서버는 1부터 시작해 기본값(0)과 달라 첫 복제 보장
	// 클라이언트는 처음 받은 값을 연출 없이 기준으로만 사용(이어하기·늦은 참가)
	UPROPERTY(ReplicatedUsing=OnRep_QuotaCompletionCue)
	int32 QuotaCompletionCue = 0;

	bool bQuotaCompletionCueReceived = false;
};
