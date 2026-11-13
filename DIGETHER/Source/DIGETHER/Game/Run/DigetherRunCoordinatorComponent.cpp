#include "Game/Run/DigetherRunCoordinatorComponent.h"

#include "Engine/World.h"
#include "Game/Run/DigetherRunLog.h"
#include "Game/Run/DigetherRunPublicStateComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	bool ArePublicStatesEqual(const FDigetherRunPublicState& Left, const FDigetherRunPublicState& Right)
	{
		return Left.Phase == Right.Phase && Left.AftershockStage == Right.AftershockStage
			&& Left.bGameOver == Right.bGameOver && Left.bPaymentDeadline == Right.bPaymentDeadline;
	}
}

UDigetherRunCoordinatorComponent::UDigetherRunCoordinatorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(false);
}

bool UDigetherRunCoordinatorComponent::HasServerAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return IsValid(OwnerActor) && OwnerActor->HasAuthority();
}

void UDigetherRunCoordinatorComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (HasServerAuthority() && ServerState.Phase == EDigetherRunPhase::InProgress)
	{
		if (const UWorld* World = GetWorld())
		{
			UpdateTime(World->GetTimeSeconds());
		}
	}
}

void UDigetherRunCoordinatorComponent::BeginNotificationBatch()
{
	if (!HasServerAuthority())
	{
		return;
	}
	++NotificationBatchDepth;
}

void UDigetherRunCoordinatorComponent::EndNotificationBatch()
{
	if (!HasServerAuthority())
	{
		return;
	}
	if (!ensureMsgf(NotificationBatchDepth > 0, TEXT("Run notification batch ended without a matching begin.")))
	{
		return;
	}
	--NotificationBatchDepth;
	if (NotificationBatchDepth != 0)
	{
		return;
	}

	// 외부 콜백이 새 배치를 시작해도 소비할 통지와 새 통지가 섞이지 않도록 먼저 비움
	const bool bNotifyPublicState = bPendingPublicStateChanged;
	bPendingPublicStateChanged = false;
	TArray<TFunction<void()>> Notifications = MoveTemp(PendingNotifications);
	PendingNotifications.Reset();
	NotifyPublishedState(bNotifyPublicState);
	for (TFunction<void()>& Notification : Notifications)
	{
		Notification();
	}
}

EDigetherRunResult UDigetherRunCoordinatorComponent::SetPublicStateComponent(
	UDigetherRunPublicStateComponent* Component)
{
	if (!HasServerAuthority())
	{
		return EDigetherRunResult::NotAuthority;
	}
	if (Component != nullptr)
	{
		if (!IsValid(Component))
		{
			return EDigetherRunResult::InvalidInput;
		}
		const EDigetherRunResult Result = Component->RestoreState(ConfirmedPublicState, false);
		if (Result != EDigetherRunResult::Success)
		{
			return Result;
		}
	}
	PublicStateComponent = Component;
	NotifyPublishedState(Component != nullptr);
	return EDigetherRunResult::Success;
}

EDigetherRunResult UDigetherRunCoordinatorComponent::PublishCandidate(
	const FDigetherRunPublicState& Candidate, bool& bOutChanged)
{
	bOutChanged = !ArePublicStatesEqual(Candidate, ConfirmedPublicState);
	if (UDigetherRunPublicStateComponent* Component = PublicStateComponent.Get())
	{
		// 외부에서 복원된 공개 값도 다음 확정 시점에 조율자 원본과 동기화
		bOutChanged |= !ArePublicStatesEqual(Candidate, Component->CaptureState());
		const EDigetherRunResult Result = Component->RestoreState(Candidate, false);
		if (Result != EDigetherRunResult::Success)
		{
			return Result;
		}
	}
	ConfirmedPublicState = Candidate;
	return EDigetherRunResult::Success;
}

void UDigetherRunCoordinatorComponent::NotifyPublishedState(const bool bChanged)
{
	if (bChanged)
	{
		if (NotificationBatchDepth > 0)
		{
			bPendingPublicStateChanged = true;
			return;
		}
		if (UDigetherRunPublicStateComponent* Component = PublicStateComponent.Get())
		{
			Component->NotifyStateChanged();
		}
	}
}

void UDigetherRunCoordinatorComponent::DispatchOrDefer(TFunction<void()> Notification)
{
	if (NotificationBatchDepth > 0)
	{
		PendingNotifications.Add(MoveTemp(Notification));
		return;
	}
	Notification();
}

EDigetherRunResult UDigetherRunCoordinatorComponent::ApplyAdvance(
	const FDigetherRunCoordinatorState& Candidate, const FDigetherRunAdvanceResult& Advance)
{
	if (Advance.Result != EDigetherRunResult::Success)
	{
		return Advance.Result;
	}
	const FDigetherRunPublicState PublicCandidate = FDigetherRunCoordinatorRules::MakePublicState(
		Candidate, ConfirmedPublicState.bGameOver, ConfirmedPublicState.bPaymentDeadline);
	bool bPublicChanged = false;
	const EDigetherRunResult Result = PublishCandidate(PublicCandidate, bPublicChanged);
	if (Result != EDigetherRunResult::Success)
	{
		return Result;
	}
	ServerState = Candidate;
	NotifyPublishedState(bPublicChanged);
	if (Advance.bTimeExpiredNow)
	{
		UE_LOG(LogDigetherRun, Log, TEXT("Run time expired."));
		OnTimeExpired.Broadcast();
	}
	return EDigetherRunResult::Success;
}

EDigetherRunResult UDigetherRunCoordinatorComponent::StartRun(const FDigetherRunTuning& Tuning,
	const FDigetherRunRuleTuning& RuleTuning, const double ServerSeconds, const bool bStartClock)
{
	if (!HasServerAuthority())
	{
		return EDigetherRunResult::NotAuthority;
	}
	FDigetherRunCoordinatorState Candidate = ServerState;
	const FDigetherRunAdvanceResult Advance = FDigetherRunCoordinatorRules::StartRun(
		Candidate, Tuning, RuleTuning, ServerSeconds, bStartClock);
	const EDigetherRunResult Result = ApplyAdvance(Candidate, Advance);
	if (Result == EDigetherRunResult::Success)
	{
		UE_LOG(LogDigetherRun, Log, TEXT("Run started."));
		const FDigetherRunPublicState State = ConfirmedPublicState;
		DispatchOrDefer([this, State]()
		{
			OnRunStarted.Broadcast(State);
		});
	}
	return Result;
}

EDigetherRunResult UDigetherRunCoordinatorComponent::StartClock(const double ServerSeconds)
{
	if (!HasServerAuthority())
	{
		return EDigetherRunResult::NotAuthority;
	}
	FDigetherRunCoordinatorState Candidate = ServerState;
	const FDigetherRunAdvanceResult Advance = FDigetherRunCoordinatorRules::StartClock(Candidate, ServerSeconds);
	const EDigetherRunResult Result = ApplyAdvance(Candidate, Advance);
	if (Result == EDigetherRunResult::Success)
	{
		UE_LOG(LogDigetherRun, Log, TEXT("Run clock started."));
	}
	return Result;
}

bool UDigetherRunCoordinatorComponent::IsClockStarted() const
{
	return ServerState.Phase == EDigetherRunPhase::InProgress && ServerState.bClockStarted;
}

EDigetherRunResult UDigetherRunCoordinatorComponent::RecordConfirmedAscent()
{
	if (!HasServerAuthority())
	{
		return EDigetherRunResult::NotAuthority;
	}
	FDigetherRunCoordinatorState Candidate = ServerState;
	const FDigetherRunAdvanceResult Advance = FDigetherRunCoordinatorRules::RecordConfirmedAscent(Candidate);
	return ApplyAdvance(Candidate, Advance);
}

EDigetherRunResult UDigetherRunCoordinatorComponent::UpdateTime(const double ServerSeconds)
{
	if (!HasServerAuthority())
	{
		return EDigetherRunResult::NotAuthority;
	}
	FDigetherRunCoordinatorState Candidate = ServerState;
	const FDigetherRunAdvanceResult Advance = FDigetherRunCoordinatorRules::UpdateTime(Candidate, ServerSeconds);
	return ApplyAdvance(Candidate, Advance);
}

FDigetherRunTerminationResult UDigetherRunCoordinatorComponent::CompleteRun(const FDigetherRunTerminationInput& Input)
{
	FDigetherRunTerminationResult Result;
	if (!HasServerAuthority())
	{
		Result.Result = EDigetherRunResult::NotAuthority;
		return Result;
	}
	FDigetherRunCoordinatorState Candidate = ServerState;
	Result = FDigetherRunCoordinatorRules::CompleteRun(Candidate, Input);
	if (Result.Result != EDigetherRunResult::Success)
	{
		return Result;
	}
	const FDigetherRunPublicState PublicCandidate = FDigetherRunCoordinatorRules::MakePublicState(
		Candidate, ConfirmedPublicState.bGameOver, ConfirmedPublicState.bPaymentDeadline);
	bool bPublicChanged = false;
	Result.Result = PublishCandidate(PublicCandidate, bPublicChanged);
	if (Result.Result != EDigetherRunResult::Success)
	{
		return Result;
	}
	ServerState = Candidate;
	NotifyPublishedState(bPublicChanged);
	UE_LOG(LogDigetherRun, Log, TEXT("Run ended."));
	DispatchOrDefer([this, Result]()
	{
		OnRunEnded.Broadcast(Result);
	});
	return Result;
}

FDigetherRunQuotaResult UDigetherRunCoordinatorComponent::HandleDepartureLeverCompleted(const FDigetherQuotaState& Quota)
{
	FDigetherRunQuotaResult Result;
	if (!HasServerAuthority())
	{
		Result.Result = EDigetherRunResult::NotAuthority;
		return Result;
	}
	Result = FDigetherRunRules::EvaluateQuota(Quota);
	if (Result.Result != EDigetherRunResult::Success)
	{
		return Result;
	}
	FDigetherRunPublicState Candidate = ConfirmedPublicState;
	Candidate.bPaymentDeadline = Result.bPaymentDeadline;
	Result.bGameOver = Candidate.bGameOver;
	bool bPublicChanged = false;
	Result.Result = PublishCandidate(Candidate, bPublicChanged);
	if (Result.Result == EDigetherRunResult::Success)
	{
		NotifyPublishedState(bPublicChanged);
		DispatchOrDefer([this, Result]()
		{
			OnQuotaEvaluated.Broadcast(Result);
		});
	}
	return Result;
}

FDigetherRunQuotaResult UDigetherRunCoordinatorComponent::HandleBaseReturn(const FDigetherQuotaState& Quota)
{
	FDigetherRunQuotaResult Result;
	if (!HasServerAuthority())
	{
		Result.Result = EDigetherRunResult::NotAuthority;
		return Result;
	}
	Result = FDigetherRunRules::EvaluateQuota(Quota);
	if (Result.Result != EDigetherRunResult::Success)
	{
		return Result;
	}
	FDigetherRunPublicState Candidate = ConfirmedPublicState;
	Candidate.bGameOver = Result.bGameOver;
	// 미달 게임 오버에서는 이미 닫힌 납부를 다시 열지 않음
	// 다음 탐사를 허용하는 귀환에서는 아직 다음 출발 레버 미완료
	Candidate.bPaymentDeadline = Result.bGameOver && ConfirmedPublicState.bPaymentDeadline;
	Result.bPaymentDeadline = Candidate.bPaymentDeadline;
	bool bPublicChanged = false;
	Result.Result = PublishCandidate(Candidate, bPublicChanged);
	if (Result.Result == EDigetherRunResult::Success)
	{
		NotifyPublishedState(bPublicChanged);
		DispatchOrDefer([this, Result]()
		{
			OnQuotaEvaluated.Broadcast(Result);
		});
	}
	return Result;
}

double UDigetherRunCoordinatorComponent::GetRemainingSeconds() const
{
	return ServerState.RemainingSeconds;
}

int32 UDigetherRunCoordinatorComponent::GetConfirmedAscentCount() const
{
	return ServerState.ConfirmedAscentCount;
}

FDigetherRunPublicState UDigetherRunCoordinatorComponent::CapturePublicState() const
{
	return ConfirmedPublicState;
}

#if !UE_BUILD_SHIPPING
EDigetherRunResult UDigetherRunCoordinatorComponent::DevSetRemainingSeconds(const double RemainingSeconds,
	const double ServerSeconds)
{
	if (!HasServerAuthority())
	{
		return EDigetherRunResult::NotAuthority;
	}
	FDigetherRunCoordinatorState Candidate = ServerState;
	const FDigetherRunAdvanceResult Advance = FDigetherRunCoordinatorRules::DevSetRemainingSeconds(
		Candidate, RemainingSeconds, ServerSeconds);
	const EDigetherRunResult Result = ApplyAdvance(Candidate, Advance);
	if (Result == EDigetherRunResult::Success)
	{
		UE_LOG(LogDigetherRun, Log, TEXT("Dev remaining time set: %.2f"), ServerState.RemainingSeconds);
	}
	return Result;
}

EDigetherRunResult UDigetherRunCoordinatorComponent::DevSetClockPaused(const bool bPaused, const double ServerSeconds)
{
	// 정지·재개 직전까지 흐른 시간을 먼저 반영해 전환 시점 기준 남은 시간 고정
	const EDigetherRunResult Updated = UpdateTime(ServerSeconds);
	if (Updated != EDigetherRunResult::Success)
	{
		return Updated;
	}
	ServerState.bDevClockPaused = bPaused;
	UE_LOG(LogDigetherRun, Log, TEXT("Dev run clock paused: %d"), bPaused);
	return EDigetherRunResult::Success;
}

bool UDigetherRunCoordinatorComponent::IsDevClockPaused() const
{
	return ServerState.Phase == EDigetherRunPhase::InProgress && ServerState.bDevClockPaused;
}

double UDigetherRunCoordinatorComponent::DevGetMaxRemainingSeconds() const
{
	return FDigetherRunCoordinatorRules::DevGetMaxRemainingSeconds(ServerState);
}
#endif
