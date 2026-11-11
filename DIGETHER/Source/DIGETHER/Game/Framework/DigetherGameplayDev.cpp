// 개발용 스모크 자동 운전·크레딧 지급·치트 진입점, Shipping 구현은 상태 불변
#include "Game/Framework/DigetherGameplayGameMode.h"

#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Economy/DigetherWalletComponent.h"
#include "Game/Elevator/DigetherElevatorActor.h"
#include "Game/Elevator/DigetherElevatorComponent.h"
#include "Game/Elevator/DigetherElevatorLeverActor.h"
#include "Game/Elevator/DigetherElevatorLeverComponent.h"
#include "Game/Framework/DigetherFrameworkLog.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Framework/DigetherGameplayRunFlowInternal.h"
#include "Game/Run/DigetherRunCoordinatorComponent.h"

#if !UE_BUILD_SHIPPING
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Game/Economy/DigetherQuotaStateComponent.h"
#include "Game/Framework/DigetherFrameworkDefaults.h"
#include "Game/Framework/DigetherFrameworkRules.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Hub/DigetherHubStateActor.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"

namespace
{
	// 개발 이동 마무리: 소유 클라이언트에 서버 위치 전달, 다른 참가자에게 즉시 복제
	void DevCheatFinishPawnMove(APawn& Pawn, const FRotator& Rotation)
	{
		if (APlayerController* Controller = Cast<APlayerController>(Pawn.GetController()))
		{
			Controller->ClientSetLocation(Pawn.GetActorLocation(), Rotation);
		}
		Pawn.ForceNetUpdate();
	}

	void DevCheatStopPawn(APawn& Pawn)
	{
		if (UPawnMovementComponent* Movement = Pawn.GetMovementComponent())
		{
			Movement->StopMovementImmediately();
		}
	}

	// 거점 침대 휴식 중이면 휴식부터 끝내야 이동 뒤 원래 위치로 되돌려지지 않음
	void DevCheatEndHubRest(const UWorld* World, const ADigetherGameplayPlayerState& Player)
	{
		ADigetherHubStateActor* Hub = ADigetherHubStateActor::Find(World);
		if (ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(Player.GetOwner());
			IsValid(Hub) && IsValid(Controller))
		{
			Hub->EndRest(Controller);
		}
	}
}
#endif

bool ADigetherGameplayGameMode::GrantCredits(ADigetherGameplayPlayerController* Requester, const int32 Amount)
{
#if !UE_BUILD_SHIPPING
	// 기획 S33: 게임 오버 중 개발 지급도 거절(결과 기록 확정 뒤 잔고 불변)
	if (IsValidRequester(Requester) && Amount > 0 && !IsGameOverActive())
	{
		UDigetherWalletComponent* Wallet = Requester->GetPlayerState<ADigetherGameplayPlayerState>()->GetWalletComponent();
		FDigetherWalletState Candidate = Wallet->CaptureState();
		Candidate.BalanceCredits = FDigetherEconomyRules::AddCredits(Candidate.BalanceCredits, Amount);
		const bool bApplied = Wallet->RestoreState(Candidate) == EDigetherEconomyResult::Success;
		UE_LOG(LogDigetherFramework, Log, TEXT("GrantCredits Result=%d (owner wallet only)."), bApplied);
		return bApplied;
	}
#endif
	return false;
}

bool ADigetherGameplayGameMode::AllowCheats(APlayerController* P)
{
#if !UE_BUILD_SHIPPING
	// 엔진 기본은 단독·에디터만 허용, 패키지 리슨 호스트도 자기 화면 치트 사용
	// 원격 참가자는 여기서 거절, 필요하면 자기 프로세스에서 EnableCheats
	if (IsValid(P) && P->IsLocalController())
	{
		return true;
	}
#endif
	return Super::AllowCheats(P);
}

bool ADigetherGameplayGameMode::StartRunSmoke(ADigetherGameplayPlayerController* Requester)
{
#if !UE_BUILD_SHIPPING
	if (!IsValidRequester(Requester) || bSmokeActive || !ElevatorActor.IsValid()
		|| RunCoordinatorComponent->CapturePublicState().bGameOver
		|| ElevatorActor->GetElevatorComponent()->GetPublicState().Phase != EDigetherElevatorPhase::HubWaiting)
	{
		return false;
	}
	SmokeController = Requester;
	bSmokeActive = true;
	LogRunStep(TEXT("SmokeStarted"));
	return true;
#else
	return false;
#endif
}

#if !UE_BUILD_SHIPPING
void ADigetherGameplayGameMode::AdvanceSmoke()
{
	if (!bSmokeActive)
	{
		return;
	}
	ADigetherGameplayPlayerController* Controller = SmokeController.Get();
	if (!IsValidRequester(Controller))
	{
		bSmokeActive = false;
		SmokeController.Reset();
		UE_LOG(LogDigetherFramework, Warning, TEXT("SmokeAborted: requesting player disconnected."));
		return;
	}
	FDigetherElevatorPullContext Context;
	if (!TryMakePullContext(Context))
	{
		return;
	}
	if (Context.ElevatorPhase != EDigetherElevatorPhase::HubWaiting
		&& Context.ElevatorPhase != EDigetherElevatorPhase::BottomWaiting)
	{
		return;
	}
	const EDigetherElevatorLeverKind Kind = Context.ElevatorPhase == EDigetherElevatorPhase::HubWaiting
		? EDigetherElevatorLeverKind::Departure : EDigetherElevatorLeverKind::Ascent;
	for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Lever : LeverActors)
	{
		if (Lever.IsValid() && Lever->GetLeverKind() == Kind)
		{
			// 개발 자동 운전, 첫 편 탈출 뒤에도 남은 빈 편을 구동해 최종 도착까지 검사
			// 안·밖 상승 레버 중 점유된 레버가 있으면 다른 레버 미시작(BeginLeverPull과 같은 규칙)
			FDigetherElevatorPullContext LeverContext = Context;
			LeverContext.bOtherLeverOccupied = DigetherGameplayRunFlowInternal::FindOccupiedSiblingLever(LeverActors, Lever.Get()) != nullptr;
			const EDigetherElevatorLeverResult Result = Lever->GetLeverComponent()->TryBeginPull(
				Controller->GetPlayerState<ADigetherGameplayPlayerState>()->GetGameplayPlayerId(), LeverContext);
			if (Result == EDigetherElevatorLeverResult::Started)
			{
				if (Kind == EDigetherElevatorLeverKind::Departure)
				{
					DepartureRequester = Controller;
				}
				LogRunStep(Kind == EDigetherElevatorLeverKind::Departure ? TEXT("DeparturePullStarted") : TEXT("AscentPullStarted"));
				break;
			}
		}
	}
}

bool ADigetherGameplayGameMode::CheckDevCheatReady(FString& OutMessage) const
{
	if (!HasAuthority() || !bRunReady || bRestoreRejected || !IsValid(GetGameState<ADigetherGameplayGameState>()))
	{
		OutMessage = TEXT("게임 진행이 준비되지 않았습니다(호스트 게임 맵 시작 뒤 사용).");
		return false;
	}
	if (IsGameOverActive())
	{
		OutMessage = TEXT("게임 오버(정산·실패 사건·크레딧) 중에는 쓸 수 없습니다.");
		return false;
	}
	return true;
}

bool ADigetherGameplayGameMode::IsDevTargetPlayer(const ADigetherGameplayPlayerState* Player) const
{
	return IsValid(Player) && Player->HasAuthority() && Player->GetWorld() == GetWorld()
		&& Player->IsGameplayInitialized() && Player->GetGameplayPlayerId().IsValid()
		&& IsValid(Player->GetWalletComponent()) && IsValid(Player->GetLifeComponent());
}

bool ADigetherGameplayGameMode::DevSetDay(const int32 TotalDay, const int32 PeriodDay, FString& OutMessage)
{
	if (!CheckDevCheatReady(OutMessage))
	{
		return false;
	}
	// 기간 일차 5는 4일차 탐사 뒤 귀환 판정 안에서만 생기는 값, 5일차 실패는 4일차 출발·귀환으로 재현
	if (PeriodDay < 1 || PeriodDay > 4 || TotalDay < 1)
	{
		OutMessage = TEXT("기간 일차는 1~4, 전체 일차는 1 이상이어야 합니다.");
		return false;
	}
	// 출발 레버 이후·탐사 중 일차 변경은 마감·귀환 판정 입력을 바꾸므로 거점 대기에서만
	if (!ElevatorActor.IsValid() || RunCoordinatorComponent->CapturePublicState().Phase == EDigetherRunPhase::InProgress
		|| ElevatorActor->GetElevatorComponent()->GetPublicState().Phase != EDigetherElevatorPhase::HubWaiting)
	{
		OutMessage = TEXT("거점 대기(엘리베이터 출발 전)에서만 일차를 바꿀 수 있습니다.");
		return false;
	}
	UDigetherQuotaStateComponent* Quota = GetGameState<ADigetherGameplayGameState>()->GetQuotaComponent();
	FDigetherQuotaState Candidate = Quota->CaptureState().Quota;
	Candidate.Day = PeriodDay;
	Candidate.TotalDay = TotalDay;
	if (Quota->SetQuotaState(Candidate) != EDigetherEconomyResult::Success)
	{
		OutMessage = TEXT("할당량 상태가 값 검사를 통과하지 못했습니다.");
		return false;
	}
	LogRunStep(TEXT("DevSetDay"));
	OutMessage = FString::Printf(TEXT("전체 %d일차, 기간 %d일차로 설정했습니다(목표·납부액 유지). 다음 자동 저장에 반영됩니다."),
		TotalDay, PeriodDay);
	return true;
}

bool ADigetherGameplayGameMode::DevSetQuotaTarget(const int32 TargetCredits, FString& OutMessage)
{
	if (!CheckDevCheatReady(OutMessage))
	{
		return false;
	}
	UDigetherQuotaStateComponent* Quota = GetGameState<ADigetherGameplayGameState>()->GetQuotaComponent();
	FDigetherQuotaState Candidate = Quota->CaptureState().Quota;
	// 목표 이하 인정액은 달성 처리(새 기간·자동 저장) 없이 충족 상태가 되므로 거절, 달성은 DigetherCompleteQuota
	if (TargetCredits < 1 || TargetCredits <= Candidate.DepositedCredits)
	{
		OutMessage = FString::Printf(TEXT("목표는 현재 납부 인정액(%d)보다 커야 합니다. 즉시 달성은 DigetherCompleteQuota를 쓰세요."),
			Candidate.DepositedCredits);
		return false;
	}
	Candidate.TargetCredits = TargetCredits;
	if (Quota->SetQuotaState(Candidate) != EDigetherEconomyResult::Success)
	{
		OutMessage = TEXT("할당량 상태가 값 검사를 통과하지 못했습니다.");
		return false;
	}
	LogRunStep(TEXT("DevSetQuotaTarget"));
	OutMessage = FString::Printf(TEXT("현재 목표를 %d로 설정했습니다(인정액 %d 유지)."), TargetCredits, Candidate.DepositedCredits);
	return true;
}

bool ADigetherGameplayGameMode::DevCompleteQuota(FString& OutMessage)
{
	if (!CheckDevCheatReady(OutMessage))
	{
		return false;
	}
	// 붓기 달성과 같은 조건: 납부 접수가 열린 동안만(출발 레버 완료·탐사 중 달성 없음)
	if (IsDepositClosed())
	{
		OutMessage = TEXT("납부 접수가 닫혀 있습니다(거점 대기·출발 게이지 완료 전에만 달성할 수 있음).");
		return false;
	}
	UDigetherQuotaStateComponent* Quota = GetGameState<ADigetherGameplayGameState>()->GetQuotaComponent();
	const FDigetherQuotaState Previous = Quota->CaptureState().Quota;
	FDigetherQuotaState Completed;
	if (Previous.Day < 1 || !FDigetherEconomyRules::TryMakeCompletedQuotaState(Previous, EconomyTuning, Completed))
	{
		OutMessage = TEXT("현재 할당량 상태로는 달성 처리를 할 수 없습니다.");
		return false;
	}
	if (Quota->SetQuotaState(Completed, false) != EDigetherEconomyResult::Success)
	{
		OutMessage = TEXT("새 기간 상태가 값 검사를 통과하지 못했습니다.");
		return false;
	}
	CommitQuotaCompletion(*Quota, [Quota]()
	{
		Quota->NotifyStateChanged();
	});
	OutMessage = FString::Printf(TEXT("할당량 달성 처리: 다음 목표 %d, 기간 1일차(전체 %d일차 유지), 달성 연출·자동 저장 실행."),
		Completed.TargetCredits, Completed.TotalDay);
	return true;
}

bool ADigetherGameplayGameMode::DevSetRunRemainingSeconds(const double RemainingSeconds, FString& OutMessage)
{
	if (!CheckDevCheatReady(OutMessage))
	{
		return false;
	}
	UDigetherRunCoordinatorComponent* Run = RunCoordinatorComponent;
	if (Run->CapturePublicState().Phase != EDigetherRunPhase::InProgress || bApplyingRunEnd)
	{
		OutMessage = TEXT("탐사 중에만 쓸 수 있습니다.");
		return false;
	}
	if (!Run->IsClockStarted())
	{
		OutMessage = TEXT("제한 시간이 아직 시작되지 않았습니다(최초 하강 뒤 갱도 도착부터 흐름).");
		return false;
	}
	if (Run->HasTimeExpired() || bTimeExpiredPending)
	{
		OutMessage = TEXT("이미 시간 종료 처리 중입니다.");
		return false;
	}
	const double MaxRemainingSeconds = Run->DevGetMaxRemainingSeconds();
	if (!FMath::IsFinite(RemainingSeconds) || RemainingSeconds < 0.0 || RemainingSeconds > MaxRemainingSeconds)
	{
		OutMessage = FString::Printf(TEXT("남은 시간은 0~%.1f초여야 합니다(탐사 시간 - 확정 상승 페널티)."), MaxRemainingSeconds);
		return false;
	}
	const EDigetherRunResult Result = Run->DevSetRemainingSeconds(RemainingSeconds, GetWorld()->GetTimeSeconds());
	if (Result != EDigetherRunResult::Success)
	{
		OutMessage = FString::Printf(TEXT("남은 시간 설정 거절: %s"), *UEnum::GetValueAsString(Result));
		return false;
	}
	LogRunStep(TEXT("DevRunTimeSet"));
	OutMessage = Run->HasTimeExpired()
		? FString(TEXT("남은 시간 0: 시간 종료 경로로 귀환합니다."))
		: FString::Printf(TEXT("남은 시간 %.1f초, 여진 단계 %d%s"), Run->GetRemainingSeconds(),
			Run->CapturePublicState().AftershockStage, Run->IsDevClockPaused() ? TEXT(" (정지 중)") : TEXT(""));
	return true;
}

bool ADigetherGameplayGameMode::DevAdvanceRunTime(const double Seconds, FString& OutMessage)
{
	if (!CheckDevCheatReady(OutMessage))
	{
		return false;
	}
	if (!FMath::IsFinite(Seconds) || Seconds <= 0.0)
	{
		OutMessage = TEXT("진행할 시간은 0보다 커야 합니다.");
		return false;
	}
	if (RunCoordinatorComponent->CapturePublicState().Phase != EDigetherRunPhase::InProgress)
	{
		OutMessage = TEXT("탐사 중에만 쓸 수 있습니다.");
		return false;
	}
	// 지금까지 흐른 시간을 먼저 반영한 남은 시간 기준
	RunCoordinatorComponent->UpdateTime(GetWorld()->GetTimeSeconds());
	const double Target = FMath::Max(0.0, RunCoordinatorComponent->GetRemainingSeconds() - Seconds);
	return DevSetRunRemainingSeconds(Target, OutMessage);
}

bool ADigetherGameplayGameMode::DevSetRunTimePaused(const bool bPaused, FString& OutMessage)
{
	if (!CheckDevCheatReady(OutMessage))
	{
		return false;
	}
	if (RunCoordinatorComponent->CapturePublicState().Phase != EDigetherRunPhase::InProgress || bApplyingRunEnd)
	{
		OutMessage = TEXT("탐사 중에만 쓸 수 있습니다(정지는 이번 탐사에만 적용).");
		return false;
	}
	const EDigetherRunResult Result = RunCoordinatorComponent->DevSetClockPaused(bPaused, GetWorld()->GetTimeSeconds());
	if (Result != EDigetherRunResult::Success)
	{
		OutMessage = FString::Printf(TEXT("시간 정지 전환 거절: %s"), *UEnum::GetValueAsString(Result));
		return false;
	}
	LogRunStep(bPaused ? TEXT("DevRunTimePaused") : TEXT("DevRunTimeResumed"));
	OutMessage = FString::Printf(TEXT("탐사 시간 %s, 남은 시간 %.1f초%s"), bPaused ? TEXT("정지") : TEXT("재개"),
		RunCoordinatorComponent->GetRemainingSeconds(),
		RunCoordinatorComponent->IsClockStarted() ? TEXT("") : TEXT(" (시계는 갱도 도착 뒤 시작)"));
	return true;
}

bool ADigetherGameplayGameMode::DevEndRunTimeout(FString& OutMessage)
{
	return DevSetRunRemainingSeconds(0.0, OutMessage);
}

bool ADigetherGameplayGameMode::DevEndRunSurvive(ADigetherGameplayPlayerController* Preferred, FString& OutMessage)
{
	if (!CheckDevCheatReady(OutMessage))
	{
		return false;
	}
	if (!ElevatorActor.IsValid() || RunCoordinatorComponent->CapturePublicState().Phase != EDigetherRunPhase::InProgress
		|| bApplyingRunEnd || bTimeExpiredPending)
	{
		OutMessage = TEXT("탐사 중에만 쓸 수 있습니다.");
		return false;
	}
	// 이동 중 단계에서는 엘리베이터 안 생존자도 갱도 잔류로 분류되므로 상승 확정을 거치는 바닥 대기로 한정
	if (ElevatorActor->GetElevatorComponent()->GetPublicState().Phase != EDigetherElevatorPhase::BottomWaiting)
	{
		OutMessage = TEXT("엘리베이터가 갱도 바닥에 문을 열고 대기 중일 때만 쓸 수 있습니다.");
		return false;
	}
	if (bSmokeActive)
	{
		OutMessage = TEXT("이미 자동 운전 중입니다.");
		return false;
	}
	TArray<ADigetherGameplayPlayerState*> Participants;
	CollectRunParticipants(Participants);
	TArray<ADigetherGameplayPlayerState*> Survivors;
	int32 DeadCount = 0;
	for (ADigetherGameplayPlayerState* Player : Participants)
	{
		const EDigetherLifeState Life = Player->GetLifeComponent()->CaptureState().PublicState.Life;
		if (Life == EDigetherLifeState::Active && IsValid(Player->GetGameplayPawn()))
		{
			Survivors.Add(Player);
		}
		DeadCount += Life == EDigetherLifeState::Dead ? 1 : 0;
	}
	// 레버 운전자: 생존자 또는 탈출자(스모크 규칙상 탈출자도 남은 빈 편 구동 가능)
	auto CanOperate = [this, &Participants](ADigetherGameplayPlayerController* Controller)
	{
		const ADigetherGameplayPlayerState* Player = IsValidRequester(Controller)
			? Controller->GetPlayerState<ADigetherGameplayPlayerState>() : nullptr;
		if (!IsValid(Player) || !Participants.Contains(Player) || !IsValid(Controller->GetPawn()) || IsJoinSpectator(Controller))
		{
			return false;
		}
		const EDigetherLifeState Life = Player->GetLifeComponent()->CaptureState().PublicState.Life;
		return Life == EDigetherLifeState::Active || Life == EDigetherLifeState::Escaped;
	};
	ADigetherGameplayPlayerController* Operator = CanOperate(Preferred) ? Preferred : nullptr;
	for (int32 Index = 0; Operator == nullptr && Index < Participants.Num(); ++Index)
	{
		ADigetherGameplayPlayerController* Candidate = Cast<ADigetherGameplayPlayerController>(Participants[Index]->GetOwner());
		Operator = CanOperate(Candidate) ? Candidate : nullptr;
	}
	if (Operator == nullptr)
	{
		OutMessage = TEXT("레버를 운전할 생존자·탈출자가 없습니다(전원 사망이면 DigetherEndRun Timeout 사용).");
		return false;
	}
	if (!Survivors.IsEmpty() && !DevMovePlayersIntoElevator(Survivors, OutMessage))
	{
		return false;
	}
	// 탑승 판정·탈출 반영은 상승 확정(ProcessAscent)에서, 귀환은 최종 도착(FinishRunAndReturn)에서 실제 경로로 처리
	SmokeController = Operator;
	bSmokeActive = true;
	LogRunStep(TEXT("DevSurviveStarted"));
	const int32 RemainingTrips = FMath::Max(0, RunTuning.ElevatorTripLimit
		- ElevatorActor->GetElevatorComponent()->GetPublicState().CompletedAscents);
	OutMessage = FString::Printf(
		TEXT("생존자 %d명을 엘리베이터에 태우고 정상 상승을 자동 운전합니다(남은 운행 %d회, 실제 운행 시간 소요). 사망자 %d명은 그대로입니다."),
		Survivors.Num(), RemainingTrips, DeadCount);
	return true;
}

bool ADigetherGameplayGameMode::DevSetBalance(ADigetherGameplayPlayerState* Player, const int32 BalanceCredits,
	FString& OutMessage)
{
	if (!CheckDevCheatReady(OutMessage))
	{
		return false;
	}
	if (!IsDevTargetPlayer(Player) || BalanceCredits < 0)
	{
		OutMessage = TEXT("대상 플레이어 또는 금액(0 이상)이 잘못됐습니다.");
		return false;
	}
	UDigetherWalletComponent* Wallet = Player->GetWalletComponent();
	FDigetherWalletState Candidate = Wallet->CaptureState();
	const int32 Previous = Candidate.BalanceCredits;
	Candidate.BalanceCredits = BalanceCredits;
	if (Wallet->RestoreState(Candidate) != EDigetherEconomyResult::Success)
	{
		OutMessage = TEXT("지갑 상태가 값 검사를 통과하지 못했습니다.");
		return false;
	}
	UE_LOG(LogDigetherFramework, Log, TEXT("DevSetBalance Player=%d %d -> %d (owner wallet only)."),
		Player->GetPlayerId(), Previous, BalanceCredits);
	OutMessage = FString::Printf(TEXT("플레이어 %d 잔고 %d → %d"), Player->GetPlayerId(), Previous, BalanceCredits);
	return true;
}

bool ADigetherGameplayGameMode::DevKillPlayer(ADigetherGameplayPlayerState* Player, FString& OutMessage)
{
	if (!CheckDevCheatReady(OutMessage))
	{
		return false;
	}
	if (!IsDevTargetPlayer(Player))
	{
		OutMessage = TEXT("대상 플레이어가 잘못됐습니다.");
		return false;
	}
	// 거점 사망은 출발 판정(전원 생존 요구)을 막고 귀환 부활도 없으므로 탐사 중 참가자만
	TArray<ADigetherGameplayPlayerState*> Participants;
	CollectRunParticipants(Participants);
	if (RunCoordinatorComponent->CapturePublicState().Phase != EDigetherRunPhase::InProgress || bApplyingRunEnd
		|| !Participants.Contains(Player))
	{
		OutMessage = FString::Printf(TEXT("플레이어 %d: 탐사에 출발한 참가자만 탐사 중에 사망시킬 수 있습니다."), Player->GetPlayerId());
		return false;
	}
	if (Player->GetLifeComponent()->ApplyDeath(EDigetherDeathCause::Killed) != EDigetherLifeResult::Success)
	{
		OutMessage = FString::Printf(TEXT("플레이어 %d: 생존 상태가 아니라 사망 처리하지 않았습니다."), Player->GetPlayerId());
		return false;
	}
	if (APawn* Pawn = Player->GetGameplayPawn())
	{
		DevCheatStopPawn(*Pawn);
	}
	// 관전 전환은 상승 탈출과 같은 순서(관전 표시 → 첫 대상 선택)
	if (ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(Player->GetOwner()))
	{
		Controller->Client_SetLifeSpectating(true);
		CycleSpectateTarget(Controller, true);
	}
	LogRunStep(TEXT("DevPlayerKilled"));
	OutMessage = FString::Printf(TEXT("플레이어 %d 사망(관전 전환). 사망 손실·드롭·시체는 재현되지 않습니다."), Player->GetPlayerId());
	return true;
}

bool ADigetherGameplayGameMode::DevRevivePlayer(ADigetherGameplayPlayerState* Player, FString& OutMessage)
{
	if (!CheckDevCheatReady(OutMessage))
	{
		return false;
	}
	if (!IsDevTargetPlayer(Player))
	{
		OutMessage = TEXT("대상 플레이어가 잘못됐습니다.");
		return false;
	}
	// 탈출자는 이미 상승 편에 확정된 사람이라 되살리면 갱도 잔류로 바뀌므로 사망자만
	if (Player->GetLifeComponent()->CaptureState().PublicState.Life != EDigetherLifeState::Dead)
	{
		OutMessage = FString::Printf(TEXT("플레이어 %d: 사망 상태가 아닙니다(탈출자는 부활 대상 아님)."), Player->GetPlayerId());
		return false;
	}
	if (Player->GetLifeComponent()->Revive() != EDigetherLifeResult::Success)
	{
		OutMessage = FString::Printf(TEXT("플레이어 %d: 부활 처리 거절"), Player->GetPlayerId());
		return false;
	}
	// 귀환 부활과 같은 순서(관전 해제 → 자기 몸 시점)
	if (ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(Player->GetOwner()))
	{
		Controller->Client_SetLifeSpectating(false);
		if (IsValid(Controller->GetPawn()))
		{
			DigetherGameplayRunFlowInternal::SetSpectateViewTarget(Controller, Controller->GetPawn());
		}
	}
	LogRunStep(TEXT("DevPlayerRevived"));
	OutMessage = FString::Printf(TEXT("플레이어 %d 부활(생존 상태·시점 복귀)"), Player->GetPlayerId());
	return true;
}

bool ADigetherGameplayGameMode::DevTeleportPlayer(ADigetherGameplayPlayerState* Player, const FVector& Location,
	const FRotator& Rotation, FString& OutMessage)
{
	if (!CheckDevCheatReady(OutMessage))
	{
		return false;
	}
	APawn* Pawn = IsDevTargetPlayer(Player) ? Player->GetGameplayPawn() : nullptr;
	if (!IsValid(Pawn) || Location.ContainsNaN() || Rotation.ContainsNaN())
	{
		OutMessage = TEXT("대상 플레이어의 몸이 없거나 위치가 잘못됐습니다.");
		return false;
	}
	DevCheatEndHubRest(GetWorld(), *Player);
	DevCheatStopPawn(*Pawn);
	// 개발 정산기 이동과 같은 방식: TeleportTo가 겹침을 피해 근처 빈 자리로 보정
	if (!Pawn->TeleportTo(Location, Rotation))
	{
		OutMessage = FString::Printf(TEXT("플레이어 %d: 막힌 위치라 이동하지 못했습니다(%s)."), Player->GetPlayerId(), *Location.ToCompactString());
		return false;
	}
	DevCheatFinishPawnMove(*Pawn, Rotation);
	OutMessage = FString::Printf(TEXT("플레이어 %d 이동: %s"), Player->GetPlayerId(), *Pawn->GetActorLocation().ToCompactString());
	return true;
}

bool ADigetherGameplayGameMode::DevMovePlayersIntoElevator(const TArray<ADigetherGameplayPlayerState*>& Players,
	FString& OutMessage)
{
	if (!CheckDevCheatReady(OutMessage))
	{
		return false;
	}
	const UBoxComponent* Volume = ElevatorActor.IsValid() ? ElevatorActor->GetBoardingVolume() : nullptr;
	if (!IsValid(Volume))
	{
		OutMessage = TEXT("엘리베이터 탑승 영역이 없습니다.");
		return false;
	}
	const FTransform VolumeTransform = Volume->GetComponentTransform();
	TArray<APawn*> BoardedPawns;
	ElevatorActor->GetOverlappingPawns(BoardedPawns);
	TArray<FVector> OccupiedLocal;
	for (const APawn* Boarded : BoardedPawns)
	{
		OccupiedLocal.Add(VolumeTransform.InverseTransformPosition(Boarded->GetActorLocation()));
	}
	TArray<ADigetherGameplayPlayerState*> ToMove;
	for (ADigetherGameplayPlayerState* Player : Players)
	{
		APawn* Pawn = IsDevTargetPlayer(Player) ? Player->GetGameplayPawn() : nullptr;
		if (!IsValid(Pawn))
		{
			OutMessage = TEXT("대상 플레이어의 몸이 없습니다.");
			return false;
		}
		if (!BoardedPawns.Contains(Pawn))
		{
			ToMove.AddUnique(Player);
		}
	}
	// 출발 사슬 소집과 같은 빈 자리 계산(볼륨 바닥 안쪽, 탑승자와 간격 유지)
	TArray<FVector> Destinations;
	if (!DigetherFrameworkRules::TryPlanChainSummon(Volume->GetUnscaledBoxExtent(), DigetherFrameworkDefaults::PlayerSpacingCm,
		OccupiedLocal, ToMove.Num(), Destinations))
	{
		OutMessage = FString::Printf(TEXT("엘리베이터 안에 %d명이 설 빈 자리가 없습니다."), ToMove.Num());
		return false;
	}
	for (int32 Index = 0; Index < ToMove.Num(); ++Index)
	{
		APawn* Pawn = ToMove[Index]->GetGameplayPawn();
		DevCheatEndHubRest(GetWorld(), *ToMove[Index]);
		DevCheatStopPawn(*Pawn);
		// 탑승 영역 안 계산 위치를 그대로 쓰도록 출발 소집처럼 보정 없는 순간이동
		Pawn->SetActorLocation(VolumeTransform.TransformPosition(Destinations[Index]), false, nullptr, ETeleportType::TeleportPhysics);
		DevCheatFinishPawnMove(*Pawn, Pawn->GetActorRotation());
	}
	OutMessage = FString::Printf(TEXT("엘리베이터 안으로 %d명 이동(이미 안에 있던 %d명 유지)"), ToMove.Num(), Players.Num() - ToMove.Num());
	return true;
}
#endif
