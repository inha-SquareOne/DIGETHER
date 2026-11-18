#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "DigetherAftershockLab.generated.h"

class ADigetherCeilingProbe;
class ADigetherCollapseLab;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDigetherCollapseExecutionRequested, FName, SegmentId);

// 청크·흙 Actor와 독립된 개인 시험 상태, 값은 게임 밸런스 확정값이 아님
USTRUCT(BlueprintType)
struct FDigetherAftershockLabSegment
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aftershock Lab")
	FName SegmentId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aftershock Lab")
	float DamageThreshold = 100.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aftershock Lab")
	float Damage = 0.f;
	// 마지막 여진에서의 대상 판정, 매 프레임 상태나 붕괴 완료 표시가 아님
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aftershock Lab")
	bool bDamageActive = false;
	// 임계치 최초 도달 시 고정, 실제 지형 붕괴 완료 상태가 아님
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aftershock Lab")
	bool bCollapseRequested = false;
	// 예고 시작 시 한 번 결정, 플레이어에게 숫자 카운트다운으로 표시하지 않음
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aftershock Lab")
	float WarningDelaySeconds = 0.f;
	// 예고 확정 때 한 번 결정해 같은 구간의 착지 대상 전원에게 전달한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aftershock Lab")
	int32 WarningStage = 0;
	// 실행 요청을 보냈다는 뜻, 실제 붕괴 성공·완료와 구분
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aftershock Lab")
	bool bExecutionRequested = false;
	// 월드 좌표 삼각형 스냅샷, UObject 참조 없음. 천장 소실 이후에도 예고 위치 유지
	TArray<FVector> WarningVertices;
	TWeakObjectPtr<ADigetherCollapseLab> WarnedTarget;
	FTimerHandle ExecutionTimer;
};

// 개인 여진·손상 시험, 천장 후보만 조회하고 지형 변경·정식 런 시계·복제는 제외
UCLASS()
class ADigetherAftershockLab : public AActor
{
	GENERATED_BODY()
public:
	ADigetherAftershockLab();
	virtual void Tick(float DeltaSeconds) override;
	// 실행 중 변경한 주기는 RestartSchedule 버튼으로 적용, 남은 대기는 새 주기로 재시작
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aftershock Lab",
		meta = (ClampMin = "0.1", Units = "s"))
	float IntervalSeconds = 5.f;
	UPROPERTY(EditInstanceOnly, Category = "Aftershock Lab|Schedule")
	bool bUseExpeditionClock = false;
	UPROPERTY(EditAnywhere, Category = "Aftershock Lab|Schedule", meta = (ClampMin = "0", Units = "s"))
	float InitialRemainingSeconds = 900.f;
	// 탐사 시계와 여진 예약만 가속한다. 예고와 실제 낙하 시간은 월드 초 단위다.
	UPROPERTY(EditAnywhere, Category = "Aftershock Lab|Schedule", meta = (ClampMin = "1", ClampMax = "60"))
	float LabClockRate = 1.f;
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Aftershock Lab|Schedule")
	int32 ReservedCollapseCount = 0;
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Aftershock Lab|Schedule")
	float ReservedIntervalSeconds = 0.f;
	UFUNCTION(BlueprintCallable, Category = "Aftershock Lab|Schedule")
	void DeductExpeditionSeconds(float Seconds);
	// 레벨에서 이어진 천장에 같은 그룹 이름을 부여한다. 그룹 간에도 붕괴 범위는 겹치지 않는다.
	UPROPERTY(EditInstanceOnly, Category = "Aftershock Lab")
	TMap<FName, FName> ConnectedCeilingGroups;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aftershock Lab", meta = (ClampMin = "0.01"))
	float DamagePerAftershock = 25.f;
	// 시작 시 ID→임계치 등록. 빈 목록도 허용하며 지형 위치나 청크 번호를 뜻하지 않음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aftershock Lab")
	TMap<FName, float> InitialSegments;
	// 기존 수동 등록 시험은 false, 갱도 연결 시험에서는 누락된 참조도 안전하게 제외
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aftershock Lab")
	bool bRequireCeiling = false;
	// 같은 구간 이름의 검사 영역 하나를 손상 단위로 사용, 삼각형·청크별 손상 아님
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Aftershock Lab")
	TMap<FName, TObjectPtr<ADigetherCeilingProbe>> CeilingRegions;
	// 연결된 맵에서는 실제 낙하 위치가 있는 구간만 예고한다. 빈 목록은 기존 이벤트 단독 시험용.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Aftershock Lab")
	TMap<FName, TObjectPtr<ADigetherCollapseLab>> CollapseTargets;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Aftershock Lab")
	TArray<FDigetherAftershockLabSegment> Segments;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Aftershock Lab")
	int32 AftershockCount = 0;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Aftershock Lab")
	int32 CollapseRequestCount = 0;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Aftershock Lab")
	int32 ExecutionRequestCount = 0;
	// 서버 권한의 로컬 이벤트, RPC·Replication이나 지형 변경은 구현하지 않음
	UPROPERTY(BlueprintAssignable, Category = "Aftershock Lab")
	FOnDigetherCollapseExecutionRequested OnCollapseExecutionRequested;

	// 유효하고 중복되지 않은 ID/양수 임계치만 등록, 기존 구간 상태는 보존
	UFUNCTION(BlueprintCallable, Category = "Aftershock Lab")
	bool RegisterSegment(FName SegmentId, float DamageThreshold);
	// PIE에서 사용. 무효 주기는 타이머를 중지하며 손상·요청 상태는 초기화하지 않음
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Aftershock Lab")
	void RestartSchedule();
	// 수동 시험도 같은 누적 경로 사용, 자동 여진의 예약 시각은 바꾸지 않음
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Aftershock Lab")
	void TriggerAftershock();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 구간 ID로 다시 조회해 배열 재할당·콜백 중 참조 무효화 방지
	void RequestCollapseExecution(FName SegmentId);
	// 여진 예약과 구간별 예고 예약은 별개, RestartSchedule은 예고를 취소하지 않음
	void ReserveQuake(float MaximumDelay = MAX_flt);
	void RunAftershock(int32 PreviewStage);
	void TriggerPreviewAftershock();
	int32 NextPreviewStage = 1;
	double ClockEndTime = 0;
	float ActiveClockRate = 1.f;
	FTimerHandle AftershockTimer;
};
