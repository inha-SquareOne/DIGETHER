#pragma once

#include "CoreMinimal.h"
#include "Game/Economy/DigetherEconomyTypes.h"
#include "Game/Run/DigetherGameOverTypes.h"

// 결과 기록 한 줄 입력, 월드 객체 없이 현재 참가자와 떠난 참가자 장부를 같은 규칙으로 계산
struct FDigetherGameResultInput
{
	FString DisplayName;
	int32 JoinOrder = 0;
	FDigetherWalletState Wallet;
};

// 기획 S33 게임 오버 순서·결과 크레딧 순수 규칙, 월드·시각·UObject 비참조, 시각은 호출자 입력
namespace DigetherGameOverRules
{
	// 크레딧 블록 수(합산 기간·전체 획득 / 획득 금액 / 히든 슬롯으로 얻은 금액 / 사망으로 잃은 금액)(S33 235·236행)
	inline constexpr int32 CreditBlockCount = 4;
	// 첫 블록은 합산 기간 줄과 전체 획득 줄
	inline constexpr int32 FirstCreditBlockLines = 2;

	// 시간·속도·배율·비율 범위 검사(정산 화면·크레딧 대기·블록 간격은 0 허용, 빨리감기 배율 1 이상, 비율 0~1)
	DIGETHER_API bool IsValidTuning(const FDigetherGameOverTuning& Tuning);
	// 허용 전이: None→SettlementScreen→FailureEvent→Credits→None, 같은 단계 반복·역방향 거절
	DIGETHER_API bool CanTransition(EDigetherGameOverPhase From, EDigetherGameOverPhase To);

	DIGETHER_API double GetSettlementScreenEndSeconds(double SettlementStartSeconds, const FDigetherGameOverTuning& Tuning);
	DIGETHER_API double GetSuctionStartSeconds(double FailureStartSeconds, const FDigetherGameOverTuning& Tuning);
	// 흡입 마감 시각, 이때까지 남은 사람은 강제 흡입
	DIGETHER_API double GetFailureDeadlineSeconds(double FailureStartSeconds, const FDigetherGameOverTuning& Tuning);
	// 흡입 시각 = min(마감, 흡입 시작 + 우물까지 거리 / 흡입 속도), 음수·비정상 거리는 0
	DIGETHER_API double CalculateAbsorbSeconds(double FailureStartSeconds, double DistanceCm,
		const FDigetherGameOverTuning& Tuning);
	// 범람·침수 진행률(0~1), 흡입 시작 시각에 1, 표현(BP) 전용
	DIGETHER_API float CalculateFloodAlpha(double FailureStartSeconds, double NowSeconds,
		const FDigetherGameOverTuning& Tuning);
	// 기다릴 현재 참가자가 없거나(전원 흡입·퇴장) 마감이 지나면 크레딧 시작
	DIGETHER_API bool ShouldStartCredits(int32 WaitingPlayerCount, double FailureStartSeconds, double NowSeconds,
		const FDigetherGameOverTuning& Tuning);

	// 크레딧 전체 줄 수 = 첫 블록 2줄 + 항목 블록 3개 × (제목 1줄 + 플레이어 수)
	DIGETHER_API int32 CountCreditLines(int32 PlayerCount);
	// 시작 대기 + (줄 수 × 줄 높이 + 블록 간격 × 3 + 화면 높이 1) / 스크롤 속도, 같은 입력이면 같은 값
	DIGETHER_API float CalculateCreditTotalSeconds(int32 PlayerCount, const FDigetherGameOverTuning& Tuning);
	DIGETHER_API FDigetherCreditPlayback MakeCreditPlayback(double NowSeconds, int32 PlayerCount,
		const FDigetherGameOverTuning& Tuning);
	// 진행 = clamp(기준 진행 + max(0, 지금 - 기준 시각) × 배속, 0, 전체), 시계 오차로 역행하지 않음
	DIGETHER_API float CalculateCreditProgressSeconds(const FDigetherCreditPlayback& Playback, double NowSeconds);
	DIGETHER_API bool IsCreditFinished(const FDigetherCreditPlayback& Playback, double NowSeconds);
	// 빨리감기 상태가 바뀌면 지금 진행으로 기준점 재설정 후 true, 같은 상태면 false이고 출력 보존
	DIGETHER_API bool TrySetFastForward(const FDigetherCreditPlayback& Current, bool bFastForward, double NowSeconds,
		const FDigetherGameOverTuning& Tuning, FDigetherCreditPlayback& OutPlayback);

	// 결과 통계 사망 손실 = 실제 차감액(런 플로우 v1.1.0 10.4), 새 규칙은 TotalDeathLossCredits 그대로
	// 복구금 폐지 전 구 저장은 복구 전 차감액이라 구 복구 누적(LifetimeCorpseRefundCredits)을 빼서 순액 유지, 음수면 0
	DIGETHER_API int32 CalculateNetDeathLoss(const FDigetherWalletState& Wallet);
	// 플레이어별 금액·전체 획득 계산 후 참가 순서로 정렬(0은 알 수 없음, 뒤에 입력 순서대로)
	// ID 없음·플레이어 0명·음수 일차/기간 수면 false, OutResult 보존
	DIGETHER_API bool TryMakeGameResult(const FGuid& ResultId, const FGuid& GameId, int32 FinalTotalDay,
		int32 CompletedPeriods, const TArray<FDigetherGameResultInput>& Players, FDigetherGameResult& OutResult);
	// 값 형태만 검사, 이름 길이·문자 제한은 저장 검증 담당
	DIGETHER_API bool IsValidGameResult(const FDigetherGameResult& Result, int32 MaxPlayers);
	// 같은 결과 ID는 1회만 보관, 상한 초과 시 가장 오래된 결과부터 제거
	DIGETHER_API void ArchiveGameResult(TArray<FDigetherGameResult>& Archive, const FDigetherGameResult& Result,
		int32 MaxResults);
}
