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
	// 개인 정산 화면의 사망자·실종자 이름 표시 전용(런 플로우 9.5)
	FString DisplayName;
	FDigetherLifeComponentState Life;
	FDigetherWalletState Wallet;
	// 이번 탐사 시체가 지상에 유효하게 도착했는지, 사망자만 true 허용(시체 회수 미구현이라 현재 호출자는 false)
	bool bCorpseRecovered = false;
};

struct FDigetherFrameworkPlayerReturn
{
	FDigetherLifeComponentState LifeAtEnd;
	// 사망 손실까지 반영한 정산 직전 지갑
	FDigetherWalletState WalletAtEnd;
	FDigetherWalletState SettledWallet;
	FDigetherLifeComponentState RevivedLife;
	FDigetherPlayerSettlement Settlement;
	// 정산 전 본인 잔고(사망자는 사망 직전 잔고와 같음)
	int32 PreviousBalanceCredits = 0;
	int32 DeathLossCredits = 0;
	// 사망자만 의미, 적용한 손실률과 시체 유효 회수 여부
	int32 DeathLossPercent = 0;
	bool bCorpseRecovered = false;
	EDigetherRunReturnOutcome ReturnOutcome = EDigetherRunReturnOutcome::Returned;
};

struct FDigetherFrameworkReturnPlan
{
	FDigetherRunTerminationResult Termination;
	TArray<FDigetherFrameworkPlayerReturn> Players;
	FDigetherQuotaState ReturnedQuota;
	// 이번 탐사 파밍액(유효 일반 회수 총액)과 생존자 몫 지급 뒤 제외된 금액
	int32 GeneralCargoCredits = 0;
	int32 ExcludedGeneralCredits = 0;
	// 입력 플레이어 순서, 다른 사람 금액은 담지 않음
	TArray<FString> DeadPlayerNames;
	TArray<FString> MissingPlayerNames;
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
	// 본인 금액과 탐사 공통 값(파밍액·제외액·사망자·실종자 이름)만 담음, 범위 밖 순번이면 무효 결과
	DIGETHER_API FDigetherRunResult MakeRunResult(FGuid RunId, int32 CompletedDay,
		const FDigetherFrameworkReturnPlan& Plan, int32 PlayerIndex);
	// 거점 사슬 소집 착지 위치를 탑승 볼륨 로컬 좌표로 정한다. 고정 착지 지점 없이 볼륨 바닥 안쪽 격자에서
	// 이미 서 있는 직접 탑승자·먼저 정한 소집자와 수평 SpacingCm 이상 떨어진 칸을 볼륨 중심에 가까운 순서로 고른다.
	// 칸이 모자라거나 입력이 잘못되면 false이고 OutLocations는 보존된다.
	DIGETHER_API bool TryPlanChainSummon(const FVector& BoxExtent, double SpacingCm,
		const TArray<FVector>& OccupiedLocal, int32 SummonCount, TArray<FVector>& OutLocations);
	// 실패 시 OutPlan은 보존된다. 입력 플레이어 순서와 키가 종료 입력과 일치해야 한다.
	// 사망 손실은 여기서 사망자마다 한 번만 적용(시체 회수 여부로 비율 확정, D25 개정), 탐사 중 사망 시점에는 차감 없음
	DIGETHER_API bool TryPlanReturn(const FDigetherRunTerminationInput& Input,
		const TArray<FDigetherFrameworkPlayerSnapshot>& Players, const FDigetherQuotaState& Quota,
		const FDigetherRunTuning& Tuning, int32 DepartureCount, int32 GeneralCargoCredits,
		FDigetherFrameworkReturnPlan& OutPlan);
	// 기획 런 플로우 5.2: 문구 판정용 출발 경고 종류, 4일차 마감 확정(게이지 완료) 뒤 출발 전까지 DeadlineConfirmed
	// 공통 로딩 실패로 거점에 돌아와도 마감 기록이 남아 있으면 같은 경고, 게임 오버 중은 레버 값 그대로
	DIGETHER_API EDigetherDepartureWarningKind ResolveDepartureWarning(EDigetherDepartureWarningKind LeverWarning,
		const FDigetherRunPublicState& Run);
}
