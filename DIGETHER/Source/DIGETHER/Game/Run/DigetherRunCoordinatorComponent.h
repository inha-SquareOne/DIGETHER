#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Run/DigetherRunCoordinatorState.h"
#include "Templates/Function.h"
#include "DigetherRunCoordinatorComponent.generated.h"

class UDigetherRunPublicStateComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherRunStarted, const FDigetherRunPublicState&);
DECLARE_MULTICAST_DELEGATE(FOnDigetherRunTimeExpired);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherRunEnded, const FDigetherRunTerminationResult&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDigetherRunQuotaEvaluated, const FDigetherRunQuotaResult&);

// GameMode에 부착한다. 결과 적용(생존·사출·인벤토리·경제)은 호출자의 책임이다.
// 델리게이트는 표현용 확정 통지이며 다른 도메인을 변경하는 명령 경로가 아니다.
UCLASS(ClassGroup=(Digether), meta=(BlueprintSpawnableComponent))
class DIGETHER_API UDigetherRunCoordinatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherRunCoordinatorComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// 서버 연결층이 다른 도메인까지 적용한 뒤 표현 통지를 내보내는 중첩 가능한 범위다.
	// 시간 만료는 조율 입력이므로 배치 중에도 즉시 알린다.
	void BeginNotificationBatch();
	void EndNotificationBatch();

	// 약참조이므로 GameState 교체 때 새 공개 컴포넌트를 다시 연결한다. nullptr는 연결 해제다.
	EDigetherRunResult SetPublicStateComponent(UDigetherRunPublicStateComponent* Component);
	// Tick과 주입 시각의 원점이 다르면 단조성 검사에 막히므로 같은 월드의 GetTimeSeconds를 쓴다.
	// bStartClock이 거짓이면 제한 시간은 StartClock(갱도 첫 도착) 전까지 흐르지 않는다.
	EDigetherRunResult StartRun(const FDigetherRunTuning& Tuning,
		const FDigetherRunRuleTuning& RuleTuning, double ServerSeconds, bool bStartClock = true);
	// 기획: 제한 시간은 최초 하강을 마치고 갱도에 도착한 순간부터 흐른다.
	EDigetherRunResult StartClock(double ServerSeconds);
	bool IsClockStarted() const;
	// P3가 확정한 상승 사건당 한 번 호출한다. 운행 한도·사건 중복 판정은 P3/호출자가 맡는다.
	EDigetherRunResult RecordConfirmedAscent();
	EDigetherRunResult UpdateTime(double ServerSeconds);
	// TimeExpired/FinalAscentCompleted 계기의 선택과 최신 엘리베이터 스냅샷은 호출자가 공급한다.
	FDigetherRunTerminationResult CompleteRun(const FDigetherRunTerminationInput& Input);

	// 레버 완료 시점의 현재 일차를 받는다. 경제 상태를 변경하지 않고 마감 값만 확정한다.
	FDigetherRunQuotaResult HandleDepartureLeverCompleted(const FDigetherQuotaState& Quota);
	// 경제의 귀환 처리 후 스냅샷(미달은 다음 일차, 달성은 새 기간 1일차)을 받는다.
	FDigetherRunQuotaResult HandleBaseReturn(const FDigetherQuotaState& Quota);

	// 서버 전용 관찰 값이며 공개 상태나 복제 프로퍼티로 옮기지 않는다.
	double GetRemainingSeconds() const;
	int32 GetConfirmedAscentCount() const;
	FDigetherRunPublicState CapturePublicState() const;

	FOnDigetherRunStarted OnRunStarted;
	FOnDigetherRunTimeExpired OnTimeExpired;
	FOnDigetherRunEnded OnRunEnded;
	FOnDigetherRunQuotaEvaluated OnQuotaEvaluated;

private:
	bool HasServerAuthority() const;
	EDigetherRunResult PublishCandidate(const FDigetherRunPublicState& Candidate, bool& bOutChanged);
	void NotifyPublishedState(bool bChanged);
	void DispatchOrDefer(TFunction<void()> Notification);
	EDigetherRunResult ApplyAdvance(const FDigetherRunCoordinatorState& Candidate,
		const FDigetherRunAdvanceResult& Advance);

	UPROPERTY(Transient)
	TWeakObjectPtr<UDigetherRunPublicStateComponent> PublicStateComponent;

	FDigetherRunCoordinatorState ServerState;
	FDigetherRunPublicState ConfirmedPublicState;
	int32 NotificationBatchDepth = 0;
	bool bPendingPublicStateChanged = false;
	TArray<TFunction<void()>> PendingNotifications;
};
