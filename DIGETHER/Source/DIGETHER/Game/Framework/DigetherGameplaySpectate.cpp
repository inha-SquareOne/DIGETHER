// 사망·탈출 관전 대상 전환, S01 관전 합류
#include "Game/Framework/DigetherGameplayGameMode.h"

#include "Engine/World.h"
#include "Game/Elevator/DigetherElevatorActor.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Framework/DigetherGameplayRunFlowInternal.h"
#include "Game/Life/DigetherLifeRules.h"
#include "Game/Life/DigetherLifeStateComponent.h"

void ADigetherGameplayGameMode::CycleSpectateTarget(ADigetherGameplayPlayerController* Requester, const bool bNext)
{
	if (IsJoinSpectator(Requester))
	{
		CycleJoinSpectateTarget(Requester, bNext);
		return;
	}
	if (!IsValidRequester(Requester))
	{
		return;
	}
	ADigetherGameplayPlayerState* Self = Requester->GetPlayerState<ADigetherGameplayPlayerState>();
	if (!Self->GetLifeComponent()->CaptureState().PublicState.bSpectating)
	{
		return;
	}
	TArray<ADigetherGameplayPlayerState*> Players;
	CollectPlayers(Players);
	TArray<FDigetherSpectateCandidate> Candidates;
	for (const ADigetherGameplayPlayerState* Player : Players)
	{
		if (IsValid(Player->GetGameplayPawn()))
		{
			FDigetherSpectateCandidate Candidate;
			// 관전 대상 키는 공개 PlayerId만, 런 매니페스트 서버 GUID 미전달
			Candidate.PlayerKey = FString::FromInt(Player->GetPlayerId());
			Candidate.Life = Player->GetLifeComponent()->CaptureState().PublicState.Life;
			Candidates.Add(Candidate);
		}
	}
	if (Self->GetLifeComponent()->SelectSpectateTarget(Candidates, FString::FromInt(Self->GetPlayerId()), bNext
		? EDigetherSpectateDirection::Next : EDigetherSpectateDirection::Previous) != EDigetherLifeResult::Success)
	{
		return;
	}
	const FString TargetKey = Self->GetLifeComponent()->CaptureState().PrivateState.SpectateTargetPlayerKey;
	AActor* Target = ElevatorActor.Get();
	for (ADigetherGameplayPlayerState* Player : Players)
	{
		if (FString::FromInt(Player->GetPlayerId()) == TargetKey && IsValid(Player->GetGameplayPawn()))
		{
			Target = Player->GetGameplayPawn();
			break;
		}
	}
	if (IsValid(Target))
	{
		DigetherGameplayRunFlowInternal::SetSpectateViewTarget(Requester, Target);
	}
}

void ADigetherGameplayGameMode::EnterJoinSpectate(APlayerController* Controller)
{
	ADigetherGameplayPlayerController* Gameplay = Cast<ADigetherGameplayPlayerController>(Controller);
	if (!HasAuthority() || !IsValid(Gameplay) || Gameplay->GetWorld() != GetWorld())
	{
		return;
	}
	// 몸이 없어 생존 컴포넌트 관전 상태(사망·탈출 전용 규칙)는 불변, 입력 잠금·카메라만 사용
	// TODO(UI): "탐사가 끝나면 거점에서 합류합니다" 안내 문구는 HUD 작업에서 추가
	Gameplay->Client_SetLifeSpectating(true);
	CycleJoinSpectateTarget(Gameplay, true);
}

void ADigetherGameplayGameMode::CycleJoinSpectateTarget(ADigetherGameplayPlayerController* Requester, const bool bNext)
{
	FDigetherJoinSpectator* Spectator = JoinSpectators.FindByPredicate([Requester](const FDigetherJoinSpectator& Entry)
	{
		return Entry.Controller.Get() == Requester;
	});
	if (!HasAuthority() || Spectator == nullptr || !IsValid(Requester) || Requester->GetWorld() != GetWorld()
		|| !IsValid(Requester->PlayerState))
	{
		return;
	}
	const APlayerState* Self = Requester->PlayerState;
	TArray<ADigetherGameplayPlayerState*> Players;
	CollectPlayers(Players);
	TArray<FDigetherSpectateCandidate> Candidates;
	for (const ADigetherGameplayPlayerState* Player : Players)
	{
		if (Player != Self && IsValid(Player->GetGameplayPawn()))
		{
			FDigetherSpectateCandidate Candidate;
			// 일반 관전과 같이 공개 PlayerId만 키로 사용
			Candidate.PlayerKey = FString::FromInt(Player->GetPlayerId());
			Candidate.Life = Player->GetLifeComponent()->CaptureState().PublicState.Life;
			Candidates.Add(Candidate);
		}
	}
	const FDigetherSpectateSelectionResult Selection = FDigetherLifeRules::SelectSpectateTarget(Candidates,
		FString::FromInt(Self->GetPlayerId()), Spectator->TargetPlayerKey,
		bNext ? EDigetherSpectateDirection::Next : EDigetherSpectateDirection::Previous);
	AActor* Target = ElevatorActor.Get();
	if (Selection.Result == EDigetherLifeResult::Success)
	{
		Spectator->TargetPlayerKey = Selection.TargetPlayerKey;
		for (ADigetherGameplayPlayerState* Player : Players)
		{
			if (!Selection.TargetPlayerKey.IsEmpty() && FString::FromInt(Player->GetPlayerId()) == Selection.TargetPlayerKey
				&& IsValid(Player->GetGameplayPawn()))
			{
				Target = Player->GetGameplayPawn();
				break;
			}
		}
	}
	if (IsValid(Target))
	{
		DigetherGameplayRunFlowInternal::SetSpectateViewTarget(Requester, Target);
	}
}

void ADigetherGameplayGameMode::ReleaseJoinSpectators()
{
	TArray<FDigetherJoinSpectator> Released = MoveTemp(JoinSpectators);
	JoinSpectators.Reset();
	for (const FDigetherJoinSpectator& Spectator : Released)
	{
		APlayerController* Controller = Spectator.Controller.Get();
		if (IsValid(Controller) && Controller->GetWorld() == GetWorld() && !IsValid(Controller->GetPawn()))
		{
			// 신규 참가자는 여기서 캐릭터 생성·시작 지급 한 번, 기존 참가자는 저장 장부로 지급 없이 합류
			HandleStartingNewPlayer(Controller);
		}
	}
}
