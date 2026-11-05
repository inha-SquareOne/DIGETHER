#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Run/DigetherGameOverTypes.h"
#include "DigetherGameOverFlowComponent.generated.h"

// 이번 처리 시점에 남아 있는 현재 참가자 한 명이다. 떠난 사람은 넣지 않는다.
struct FDigetherGameOverPresentPlayer
{
	// 공개 PlayerId다.
	int32 PlayerId = 0;
	// 흡입 중심(우물)까지의 거리(cm)다.
	double DistanceToWellCm = 0.0;
};

// 한 번의 Advance가 확정한 사건이다. 연결층(GameMode)이 월드에 적용한다.
struct FDigetherGameOverStep
{
	bool bEnteredFailureEvent = false;
	bool bEnteredCredits = false;
	// 크레딧이 끝나 새 게임을 만들어야 한다. 한 게임 오버에서 한 번만 true다.
	bool bCreditsFinished = false;
	// 이번에 흡입된 공개 PlayerId다.
	TArray<int32> NewlyAbsorbedPlayerIds;
	// 공개 상태가 바뀌어 다시 복제해야 한다.
	bool bPublicStateChanged = false;
};

// 기획 S33 게임 오버 단계를 서버에서 진행한다. GameMode에 부착하며 복제하지 않는다.
// 판정은 DigetherGameOverRules에 위임하고, 월드(Pawn 이동·새 게임 생성) 적용은 GameMode가 한다.
// 세부 단계는 저장하지 않는다. 시각은 GameStateBase::GetServerWorldTimeSeconds 기준을 호출자가 넣는다.
UCLASS(ClassGroup=(Digether))
class DIGETHER_API UDigetherGameOverFlowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherGameOverFlowComponent();

	// None에서만 시작한다. 결과는 서버에만 두고 크레딧 단계에 들어갈 때 공개한다. 중복 호출은 false다.
	bool Begin(double NowSeconds, const FDigetherGameResult& Result, const FDigetherGameOverTuning& Tuning);
	// 정산 화면 시간 경과 → 실패 사건, 흡입 예정 시각 도달·마감 강제 흡입 → 크레딧, 크레딧 끝 → 새 게임 요청.
	FDigetherGameOverStep Advance(double NowSeconds, const TArray<FDigetherGameOverPresentPlayer>& PresentPlayers);
	// 크레딧 중 빨리감기를 켜고 끈다. 상태가 바뀌어 기준점을 다시 정했으면 true다. 호스트 확인은 호출자가 한다.
	bool SetCreditFastForward(bool bFastForward, double NowSeconds);
	// 새 게임 생성이 끝나면 부른다. 크레딧이 끝난 상태에서 한 번만 None으로 돌아간다.
	bool CompleteNewGame(double NowSeconds);
	// 월드 정리·진행 교체 때 남은 단계를 버린다.
	void Cancel();

	bool IsActive() const { return State.Phase != EDigetherGameOverPhase::None; }
	bool IsAwaitingNewGame() const { return State.Phase == EDigetherGameOverPhase::Credits && bNewGameRequested; }
	EDigetherGameOverPhase GetPhase() const { return State.Phase; }
	const FDigetherGameOverPublicState& GetPublicState() const { return State; }
	const FDigetherGameResult& GetPendingResult() const { return PendingResult; }
	bool IsAbsorbed(int32 PlayerId) const { return State.AbsorbedPlayerIds.Contains(PlayerId); }

private:
	bool EnterPhase(EDigetherGameOverPhase Next, double NowSeconds);
	void AdvanceFailureEvent(double NowSeconds, const TArray<FDigetherGameOverPresentPlayer>& PresentPlayers,
		FDigetherGameOverStep& OutStep);

	FDigetherGameOverPublicState State;
	// 크레딧 전에는 공개하지 않는 확정 결과다.
	FDigetherGameResult PendingResult;
	// 공개 PlayerId → 서버가 정한 흡입 시각이다. 흡입 시작 뒤 처음 본 시점의 거리로 정한다.
	TMap<int32, double> AbsorbSchedule;
	// 크레딧 종료 신호가 반복돼도 새 게임을 한 번만 요청하도록 먼저 세운다.
	bool bNewGameRequested = false;
};
