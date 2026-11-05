#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Run/DigetherGameOverTypes.h"
#include "DigetherGameOverFlowComponent.generated.h"

// 이번 처리 시점에 남아 있는 현재 참가자 한 명, 떠난 사람 제외
struct FDigetherGameOverPresentPlayer
{
	// 공개 PlayerId
	int32 PlayerId = 0;
	// 흡입 중심(우물)까지 거리(cm)
	double DistanceToWellCm = 0.0;
};

// 한 번의 Advance가 확정한 사건, 연결층(GameMode)이 월드에 적용
struct FDigetherGameOverStep
{
	bool bEnteredFailureEvent = false;
	bool bEnteredCredits = false;
	// 크레딧 종료로 새 게임 생성 필요, 한 게임 오버에서 1회만 true
	bool bCreditsFinished = false;
	// 이번에 흡입된 공개 PlayerId
	TArray<int32> NewlyAbsorbedPlayerIds;
	// 공개 상태 변경으로 재복제 필요
	bool bPublicStateChanged = false;
};

// 기획 S33 게임 오버 단계를 서버에서 진행, GameMode에 부착하며 복제 안 함
// 판정은 DigetherGameOverRules에 위임, 월드 적용(Pawn 이동·새 게임 생성)은 GameMode 담당
// 세부 단계 저장 안 함, 시각은 호출자가 GameStateBase::GetServerWorldTimeSeconds 기준으로 입력
UCLASS(ClassGroup=(Digether))
class DIGETHER_API UDigetherGameOverFlowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherGameOverFlowComponent();

	// None에서만 시작, 결과는 서버 보관 후 크레딧 단계 진입 때 공개, 중복 호출은 false
	bool Begin(double NowSeconds, const FDigetherGameResult& Result, const FDigetherGameOverTuning& Tuning);
	// 정산 화면 시간 경과 → 실패 사건, 흡입 예정 시각 도달·마감 강제 흡입 → 크레딧, 크레딧 끝 → 새 게임 요청
	FDigetherGameOverStep Advance(double NowSeconds, const TArray<FDigetherGameOverPresentPlayer>& PresentPlayers);
	// 크레딧 중 빨리감기 전환, 기준점을 재설정했으면 true, 호스트 확인은 호출자 담당
	bool SetCreditFastForward(bool bFastForward, double NowSeconds);
	// 새 게임 생성 완료 후 호출, 크레딧이 끝난 상태에서 1회만 None으로 복귀
	bool CompleteNewGame(double NowSeconds);
	// 월드 정리·진행 교체 때 남은 단계 폐기
	void Cancel();

	bool IsActive() const { return State.Phase != EDigetherGameOverPhase::None; }
	bool IsAwaitingNewGame() const { return State.Phase == EDigetherGameOverPhase::Credits && bNewGameRequested; }
	EDigetherGameOverPhase GetPhase() const { return State.Phase; }
	const FDigetherGameOverPublicState& GetPublicState() const { return State; }

private:
	bool EnterPhase(EDigetherGameOverPhase Next, double NowSeconds);
	void AdvanceFailureEvent(double NowSeconds, const TArray<FDigetherGameOverPresentPlayer>& PresentPlayers,
		FDigetherGameOverStep& OutStep);

	FDigetherGameOverPublicState State;
	// 크레딧 전에는 비공개인 확정 결과
	FDigetherGameResult PendingResult;
	// 공개 PlayerId → 서버가 정한 흡입 시각, 흡입 시작 뒤 처음 본 시점의 거리 기준
	TMap<int32, double> AbsorbSchedule;
	// 크레딧 종료 신호가 반복돼도 새 게임을 1회만 요청하도록 먼저 설정
	bool bNewGameRequested = false;
};
