#pragma once

#include "Game/Data/DigetherTuningProfile.h"
#include "Game/Economy/DigetherEconomyTypes.h"
#include "Game/Elevator/DigetherElevatorTypes.h"
#include "Game/Life/DigetherLifeTypes.h"
#include "Game/Run/DigetherRunTypes.h"
#include "Game/Run/DigetherRunResult.h"

struct FDigetherFrameworkPlayerSnapshot
{
	FString PlayerKey;
	FDigetherLifeComponentState Life;
	FDigetherWalletState Wallet;
};

struct FDigetherFrameworkPlayerReturn
{
	FDigetherLifeComponentState LifeAtEnd;
	FDigetherWalletState WalletAtEnd;
	FDigetherWalletState SettledWallet;
	FDigetherLifeComponentState RevivedLife;
	FDigetherPlayerSettlement Settlement;
	int32 DeathLossCredits = 0;
	EDigetherRunReturnOutcome ReturnOutcome = EDigetherRunReturnOutcome::Returned;
};

struct FDigetherFrameworkReturnPlan
{
	FDigetherRunTerminationResult Termination;
	TArray<FDigetherFrameworkPlayerReturn> Players;
	FDigetherQuotaState ReturnedQuota;
	bool bGameOver = false;
};

// 연결층의 값 변환·일괄 사전 검사만 맡는다. 도메인 규칙은 P1/P2/P3에 위임한다.
namespace DigetherFrameworkRules
{
	DIGETHER_API bool TryMakePlayerLoad(const TArray<FGuid>& PassengerIds,
		TArray<FDigetherElevatorLoadEntry>& OutEntries);
	DIGETHER_API EDigetherRunElevatorState MakeRunElevatorState(EDigetherElevatorPhase Phase);
	// 탑승 볼륨은 바닥에 고정돼 있다. 출발 하강·하단 대기·문 닫힘일 때만 겹침을 탑승으로 인정한다.
	// 상승 확정 뒤 볼륨에 들어온 활동 참가자는 갱도 잔류로 판정한다(DEV-062·063).
	DIGETHER_API EDigetherRunPlayerLocation MakeRunPlayerLocation(EDigetherLifeState Life,
		bool bInBoardingVolume, EDigetherElevatorPhase ElevatorPhase);
	// 게임 오버 때 공개할 히든 금액 목록. 키는 서버 GUID가 아닌 공개 식별자다.
	// 두 배열 길이가 다르거나 키가 비었거나 겹치면 false, OutReveals는 보존된다.
	DIGETHER_API bool TryMakeHiddenReveals(const TArray<FString>& PublicKeys,
		const TArray<FDigetherWalletState>& Wallets, TArray<FDigetherHiddenRevealEntry>& OutReveals);
	DIGETHER_API bool IsDepositClosed(const FDigetherRunPublicState& Run,
		EDigetherElevatorPhase ElevatorPhase);
	// 새 참가 잠금 판정이다. 재접속 예외는 참가 규칙(DigetherJoinRules)이 따로 다룬다.
	DIGETHER_API bool IsNewJoinLocked(const FDigetherRunPublicState& Run,
		EDigetherElevatorPhase ElevatorPhase);
	DIGETHER_API FDigetherRunResult MakeRunResult(FGuid RunId, int32 CompletedDay,
		const FDigetherFrameworkPlayerReturn& Returned);
	// 거점 사슬 소집 착지 위치를 탑승 볼륨 로컬 좌표로 정한다. 고정 착지 지점 없이 볼륨 바닥 안쪽 격자에서
	// 이미 서 있는 직접 탑승자·먼저 정한 소집자와 수평 SpacingCm 이상 떨어진 칸을 볼륨 중심에 가까운 순서로 고른다.
	// 칸이 모자라거나 입력이 잘못되면 false이고 OutLocations는 보존된다.
	DIGETHER_API bool TryPlanChainSummon(const FVector& BoxExtent, double SpacingCm,
		const TArray<FVector>& OccupiedLocal, int32 SummonCount, TArray<FVector>& OutLocations);
	// 실패 시 OutPlan은 보존된다. 입력 플레이어 순서와 키가 종료 입력과 일치해야 한다.
	DIGETHER_API bool TryPlanReturn(const FDigetherRunTerminationInput& Input,
		const TArray<FDigetherFrameworkPlayerSnapshot>& Players, const FDigetherQuotaState& Quota,
		const FDigetherRunTuning& Tuning, int32 DepartureCount, FDigetherFrameworkReturnPlan& OutPlan);
}
