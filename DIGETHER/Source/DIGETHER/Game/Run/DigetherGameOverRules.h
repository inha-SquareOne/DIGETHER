#pragma once

#include "CoreMinimal.h"
#include "Game/Economy/DigetherEconomyTypes.h"
#include "Game/Run/DigetherGameOverTypes.h"

// 결과 기록 한 줄의 입력이다. 월드 객체 없이 현재 참가자와 떠난 참가자의 장부를 같은 규칙으로 계산한다.
struct FDigetherGameResultInput
{
	FString DisplayName;
	int32 JoinOrder = 0;
	FDigetherWalletState Wallet;
};

// 기획 S33 게임 오버 순서·결과 크레딧의 순수 규칙이다. 월드·시각·UObject를 읽지 않으며 시각은 호출자가 넣는다.
namespace DigetherGameOverRules
{
	// 크레딧 블록 수(합산 기간·전체 획득 / 획득 금액 / 히든 슬롯으로 얻은 금액 / 사망으로 잃은 금액)다(S33 235·236행).
	inline constexpr int32 CreditBlockCount = 4;
	// 첫 블록은 합산 기간 줄과 전체 획득 줄이다.
	inline constexpr int32 FirstCreditBlockLines = 2;

	// 모든 시간·속도·배율이 유한한 양수(빨리감기 배율 1 이상)이고 비율이 0~1이면 true다.
	DIGETHER_API bool IsValidTuning(const FDigetherGameOverTuning& Tuning);
	// 허용 전이: None→SettlementScreen→FailureEvent→Credits→None. 같은 단계 반복과 역방향은 거절한다.
	DIGETHER_API bool CanTransition(EDigetherGameOverPhase From, EDigetherGameOverPhase To);

	DIGETHER_API double GetSettlementScreenEndSeconds(double SettlementStartSeconds, const FDigetherGameOverTuning& Tuning);
	DIGETHER_API double GetSuctionStartSeconds(double FailureStartSeconds, const FDigetherGameOverTuning& Tuning);
	// 흡입 마감 시각이다. 이때까지 남은 사람은 강제로 흡입한다.
	DIGETHER_API double GetFailureDeadlineSeconds(double FailureStartSeconds, const FDigetherGameOverTuning& Tuning);
	// 흡입 시각 = min(마감, 흡입 시작 + 우물까지 거리 / 흡입 속도). 음수·비정상 거리는 0으로 본다.
	DIGETHER_API double CalculateAbsorbSeconds(double FailureStartSeconds, double DistanceCm,
		const FDigetherGameOverTuning& Tuning);
	// 범람·침수 진행률(0~1)이다. 흡입 시작 시각에 1이 된다. 표현(BP)에만 쓴다.
	DIGETHER_API float CalculateFloodAlpha(double FailureStartSeconds, double NowSeconds,
		const FDigetherGameOverTuning& Tuning);
	// 기다릴 현재 참가자가 없거나(전원 흡입·퇴장) 마감이 지나면 크레딧을 시작한다.
	DIGETHER_API bool ShouldStartCredits(int32 WaitingPlayerCount, double FailureStartSeconds, double NowSeconds,
		const FDigetherGameOverTuning& Tuning);

	// 크레딧 전체 줄 수다. 첫 블록 2줄 + 항목 블록 3개 × (제목 1줄 + 플레이어 수).
	DIGETHER_API int32 CountCreditLines(int32 PlayerCount);
	// 시작 대기 + (줄 수 × 줄 높이 + 블록 간격 × 3 + 화면 높이 1) / 스크롤 속도. 같은 입력이면 모두 같은 값이다.
	DIGETHER_API float CalculateCreditTotalSeconds(int32 PlayerCount, const FDigetherGameOverTuning& Tuning);
	DIGETHER_API FDigetherCreditPlayback MakeCreditPlayback(double NowSeconds, int32 PlayerCount,
		const FDigetherGameOverTuning& Tuning);
	// 진행 = clamp(기준 진행 + max(0, 지금 - 기준 시각) × 배속, 0, 전체). 시계 오차로 뒤로 가지 않는다.
	DIGETHER_API float CalculateCreditProgressSeconds(const FDigetherCreditPlayback& Playback, double NowSeconds);
	DIGETHER_API bool IsCreditFinished(const FDigetherCreditPlayback& Playback, double NowSeconds);
	// 빨리감기 상태가 바뀌면 지금 진행으로 기준점을 다시 정하고 true다. 같은 상태이면 false이고 출력은 보존된다.
	DIGETHER_API bool TrySetFastForward(const FDigetherCreditPlayback& Current, bool bFastForward, double NowSeconds,
		const FDigetherGameOverTuning& Tuning, FDigetherCreditPlayback& OutPlayback);

	// 사망 손실 순액 = 사망으로 빠진 금액의 합 - 시체 회수 복구액의 합이다(D25 확정 산식). 복구가 손실보다 크면 0이다.
	DIGETHER_API int32 CalculateNetDeathLoss(const FDigetherWalletState& Wallet);
	// 플레이어별 금액·전체 획득을 계산하고 참가 순서로 정렬한다(0은 알 수 없음이며 뒤에 입력 순서대로).
	// ID가 없거나 플레이어가 0명이면 false이고 OutResult는 보존된다.
	DIGETHER_API bool TryMakeGameResult(const FGuid& ResultId, const FGuid& GameId, int32 FinalTotalDay,
		int32 CompletedPeriods, const TArray<FDigetherGameResultInput>& Players, FDigetherGameResult& OutResult);
	// 값의 형태만 본다. 이름 길이·문자 제한은 저장 검증이 따로 본다.
	DIGETHER_API bool IsValidGameResult(const FDigetherGameResult& Result, int32 MaxPlayers);
	// 같은 결과 ID는 한 번만 보관한다. 상한을 넘으면 가장 오래된 결과부터 버린다.
	DIGETHER_API void ArchiveGameResult(TArray<FDigetherGameResult>& Archive, const FDigetherGameResult& Result,
		int32 MaxResults);
}
