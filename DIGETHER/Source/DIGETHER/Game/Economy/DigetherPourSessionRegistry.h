#pragma once

#include "Game/Economy/DigetherEconomyTypes.h"

// 붓기 중지 이유, 소유 클라이언트 알림·로그 전용(저장·복제 안 함)
enum class EDigetherPourStopReason : uint8
{
	// 거리·투입구 방향·가림·생존·요청자 조건 불일치
	InteractionLost,
	Disconnected,
	// 출발 게이지 완료·마감 등으로 납부 접수 닫힘
	DepositClosed,
	// 목표 달성으로 기간 변경, 새 목표에 내려면 다시 눌러야 함
	PeriodChanged,
	// 불러오기·공통 로딩 실패 복귀로 진행 변경
	Loaded,
	InsufficientFunds,
	CreditTooSmall,
	// 값 계산이 예상하지 못한 이유로 거절
	Rejected
};

// 서버 메모리 전용 값, 공개 상태·저장 데이터·복제 프로퍼티에 넣지 않음
struct DIGETHER_API FDigetherPourSession
{
	FString PlayerKey;
	FName StationId;
	// 시작 순간 기간 세대, 처리 시점 현재 세대와 다르면 그 사람만 중지
	int32 PeriodGeneration = 0;
	// 다음 주기 예정 시각(서버 붓기 시계 초)
	double NextTickServerSeconds = 0.0;
	// 같은 예정 시각의 처리 순서용 시작 순번, 먼저 시작한 사람 우선
	int64 StartSequence = 0;
};

// 이번 처리 시점에 실행할 주기 하나
struct DIGETHER_API FDigetherPourDueTick
{
	FString PlayerKey;
	int64 StartSequence = 0;
	double ScheduledServerSeconds = 0.0;
};

// 서버 조율자가 권한 확인 후 같은 실행 흐름에서만 호출, 월드·타이머·RPC·지갑 비소유
// PlayerKey·StationId 안정성, Pawn·거리·시야 판정, 실제 상태 반영·알림은 호출자 책임
class DIGETHER_API FDigetherPourSessionRegistry
{
public:
	// 첫 주기는 누른 뒤 IntervalSeconds 경과 시각
	// 같은 플레이어·정산기·세대의 중복 시작은 예정 시각·순번 유지(연타로 주기 앞당김 방지)
	// 다른 정산기·세대면 새 세션으로 교체
	EDigetherEconomyResult BeginPour(const FString& PlayerKey, FName StationId, int32 PeriodGeneration,
		double NowServerSeconds, double IntervalSeconds, FDigetherPourSession* OutSession = nullptr);

	// 세션이 있으면 지우고 true, 출력에 지운 세션 저장
	bool StopPour(const FString& PlayerKey, FDigetherPourSession* OutStopped = nullptr);
	TArray<FDigetherPourSession> StopAll();

	// NowServerSeconds 이전 예정 주기를 (예정 시각, 시작 순번) 순으로 최대 MaxTicks개 반환
	// 한 세션이 놓친 여러 주기는 각각 따로 담음, 상한 초과분은 다음 처리 시점으로 이월
	TArray<FDigetherPourDueTick> CollectDueTicks(double NowServerSeconds, double IntervalSeconds, int32 MaxTicks) const;

	// 처리한 주기의 다음 예정 시각 예약, 세션 없음·순번 불일치(그 사이 중지·재시작)면 false
	bool AdvanceAfterTick(const FDigetherPourDueTick& Tick, double IntervalSeconds);

	const FDigetherPourSession* Find(const FString& PlayerKey) const;
	// 서버 내부 조회용 복사본, 소유자 외 전달·공개 복제 금지
	TArray<FDigetherPourSession> CaptureSessions() const { return Sessions; }
	bool IsEmpty() const { return Sessions.IsEmpty(); }

private:
	TArray<FDigetherPourSession> Sessions;
	int64 NextStartSequence = 0;
};
