#pragma once

#include "Game/Data/DigetherTuningProfile.h"
#include "Game/Economy/DigetherEconomyTypes.h"
#include "Game/Elevator/DigetherElevatorTypes.h"
#include "Game/Life/DigetherLifeTypes.h"
#include "Game/Run/DigetherRunTypes.h"

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
	// 탑승 볼륨은 바닥에 고정돼 있다. 출발 하강·하단 대기일 때만 겹침을 탑승으로 인정한다.
	// 상승 확정 뒤 볼륨에 들어온 활동 참가자는 갱도 잔류로 판정한다(DEV-062·063).
	DIGETHER_API EDigetherRunPlayerLocation MakeRunPlayerLocation(EDigetherLifeState Life,
		bool bInBoardingVolume, EDigetherElevatorPhase ElevatorPhase);
	// 게임 오버 때 공개할 히든 금액 목록. 키는 서버 GUID가 아닌 공개 식별자다.
	// 두 배열 길이가 다르거나 키가 비었거나 겹치면 false, OutReveals는 보존된다.
	DIGETHER_API bool TryMakeHiddenReveals(const TArray<FString>& PublicKeys,
		const TArray<FDigetherWalletState>& Wallets, TArray<FDigetherHiddenRevealEntry>& OutReveals);
	DIGETHER_API bool IsDepositClosed(const FDigetherRunPublicState& Run,
		EDigetherElevatorPhase ElevatorPhase);
	// 실패 시 OutPlan은 보존된다. 입력 플레이어 순서와 키가 종료 입력과 일치해야 한다.
	DIGETHER_API bool TryPlanReturn(const FDigetherRunTerminationInput& Input,
		const TArray<FDigetherFrameworkPlayerSnapshot>& Players, const FDigetherQuotaState& Quota,
		const FDigetherRunTuning& Tuning, int32 DepartureCount, FDigetherFrameworkReturnPlan& OutPlan);
}
