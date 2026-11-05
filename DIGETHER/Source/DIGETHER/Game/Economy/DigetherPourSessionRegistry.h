#pragma once

#include "Game/Economy/DigetherEconomyTypes.h"

// 붓기를 멈춘 이유다. 소유 클라이언트 알림과 로그에만 쓰며 저장·복제하지 않는다.
enum class EDigetherPourStopReason : uint8
{
	// 우클릭을 뗐다(Server_EndPour).
	Released,
	// 거리·투입구 방향·가림·생존·요청자 조건이 어긋났다.
	InteractionLost,
	Disconnected,
	// 출발 게이지 완료·마감 등으로 납부 접수가 닫혔다.
	DepositClosed,
	// 목표 달성으로 기간이 바뀌었다. 새 목표에 내려면 다시 눌러야 한다.
	PeriodChanged,
	// 불러오기·공통 로딩 실패 복귀로 진행이 바뀌었다.
	Loaded,
	InsufficientFunds,
	CreditTooSmall,
	// 값 계산이 예상하지 못한 이유로 거절됐다.
	Rejected
};

// 서버 메모리 전용 값이다. 공개 상태·저장 데이터·복제 프로퍼티에 넣지 않는다.
struct DIGETHER_API FDigetherPourSession
{
	FString PlayerKey;
	FName StationId;
	// 시작 순간의 기간 세대다. 처리 시점에 현재 세대와 다르면 그 사람만 멈춘다.
	int32 PeriodGeneration = 0;
	// 다음 주기 예정 시각(서버 붓기 시계 초)이다.
	double NextTickServerSeconds = 0.0;
	// 같은 예정 시각의 처리 순서를 정하는 시작 순번이다. 먼저 시작한 사람이 먼저 반영된다.
	int64 StartSequence = 0;
};

// 이번 처리 시점에 실행할 주기 하나다.
struct DIGETHER_API FDigetherPourDueTick
{
	FString PlayerKey;
	int64 StartSequence = 0;
	double ScheduledServerSeconds = 0.0;
};

// 서버 조율자가 권한을 확인한 뒤 같은 실행 흐름에서만 호출한다. 월드·타이머·RPC·지갑은 소유하지 않는다.
// PlayerKey와 StationId의 안정성, Pawn·거리·시야 판정과 실제 상태 반영·알림은 호출자 책임이다.
class DIGETHER_API FDigetherPourSessionRegistry
{
public:
	// 첫 주기는 누른 뒤 IntervalSeconds가 지난 시각이다. 같은 플레이어·같은 정산기·같은 세대의 중복 시작은
	// 예정 시각과 순번을 그대로 둔다(연타로 주기를 앞당기지 못하게 한다). 다른 정산기·세대면 새 세션으로 바꾼다.
	EDigetherEconomyResult BeginPour(const FString& PlayerKey, FName StationId, int32 PeriodGeneration,
		double NowServerSeconds, double IntervalSeconds, FDigetherPourSession* OutSession = nullptr);

	// 세션이 있으면 지우고 true다. 출력에는 지운 세션을 담는다.
	bool StopPour(const FString& PlayerKey, FDigetherPourSession* OutStopped = nullptr);
	TArray<FDigetherPourSession> StopAll();

	// NowServerSeconds 이전에 예정된 주기를 (예정 시각, 시작 순번) 순으로 최대 MaxTicks개 돌려준다.
	// 한 세션이 이번 시점까지 여러 주기를 놓쳤으면 각각 따로 담는다. 상한을 넘은 주기는 다음 처리 시점에 남는다.
	TArray<FDigetherPourDueTick> CollectDueTicks(double NowServerSeconds, double IntervalSeconds, int32 MaxTicks) const;

	// 처리한 주기 다음 예정 시각을 예약한다. 세션이 없거나 순번이 다르면(그 사이 멈춤·재시작) false다.
	bool AdvanceAfterTick(const FDigetherPourDueTick& Tick, double IntervalSeconds);

	const FDigetherPourSession* Find(const FString& PlayerKey) const;
	// 서버 내부 조회용 복사본이다. 소유자 외 전달이나 공개 복제에 사용하지 않는다.
	TArray<FDigetherPourSession> CaptureSessions() const { return Sessions; }
	bool IsEmpty() const { return Sessions.IsEmpty(); }

private:
	TArray<FDigetherPourSession> Sessions;
	int64 NextStartSequence = 0;
};
