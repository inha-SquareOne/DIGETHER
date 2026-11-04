#pragma once

#include "Game/Economy/DigetherEconomyTypes.h"

struct FDigetherEconomyTuning;

enum class EDigetherDepositSessionCloseReason : uint8
{
	ExplicitCancel,
	ScreenClosed,
	InteractionLost,
	Death,
	Departure,
	Disconnected,
	Loaded,
	Expired,
	PeriodChanged
};

// 서버 메모리 전용 값이다. 공개 상태·저장 데이터·복제 프로퍼티에 넣지 않는다.
struct DIGETHER_API FDigetherDepositSession
{
	FString PlayerKey;
	FGuid SessionId;
	FName StationId;
	int32 PeriodGeneration = 0;
	double ExpiresAtServerSeconds = 0.0;
};

struct DIGETHER_API FDigetherDepositSessionOpenResult
{
	EDigetherEconomyResult Result = EDigetherEconomyResult::InvalidInput;
	FDigetherDepositSession Session;
};

struct DIGETHER_API FDigetherDepositSubmission
{
	FString PlayerKey;
	FGuid SessionId;
	FName StationId;
	int32 PeriodGeneration = 0;
	int32 AmountCredits = 0;
	double NowServerSeconds = 0.0;
	bool bDepositClosed = false;
};

struct DIGETHER_API FDigetherDepositSessionCloseResult
{
	EDigetherEconomyResult Result = EDigetherEconomyResult::InvalidInput;
	EDigetherDepositSessionCloseReason Reason = EDigetherDepositSessionCloseReason::ExplicitCancel;

	// 성공한 닫기만 담는다. 호출자가 소유 클라이언트에 종료를 전달할 때 사용한다.
	TArray<FDigetherDepositSession> ClosedSessions;
};

// 서버 조율자가 권한을 확인한 뒤 같은 실행 흐름에서만 호출한다. 월드·타이머·RPC는 소유하지 않는다.
// PlayerKey와 StationId의 안정성, Pawn·거리·시야 판정 및 실제 상태 반영·알림은 호출자 책임이다.
// PlayerKey는 호출 연결에서 확인하고, 지갑·목표와 현재 시각은 현재 서버 값만 전달한다.
class DIGETHER_API FDigetherDepositSessionRegistry
{
public:
	// 호출자는 중앙 기본값 FDigetherEconomyTuning::DepositSessionLifetimeSeconds를 초 단위로 전달한다.
	// 같은 플레이어·같은 납부대는 ID·기억한 세대를 유지하고 만료만 갱신한다.
	FDigetherDepositSessionOpenResult OpenSession(const FString& PlayerKey, FName StationId,
		int32 PeriodGeneration, double NowServerSeconds, double LifetimeSeconds);

	// 모든 검사 후 값 지갑·목표와 세션 소비를 함께 확정한다. 실패는 셋 모두 보존한다.
	// 튜닝은 요청 데이터 대신 서버 보관 값을 별도로 전달하며 현재 목표 일차의 효율을 사용한다.
	// 반환된 값들을 실제 컴포넌트에 적용하는 조율자는 전체 적용 후에만 알린다.
	FDigetherDepositResult SubmitDeposit(const FDigetherDepositSubmission& Submission,
		FDigetherWalletState& WalletState, FDigetherQuotaState& QuotaState, const FDigetherEconomyTuning& Tuning);

	// 화면의 취소는 ID까지 검사한다. 서버의 사망·이탈 등은 플레이어 단위로 닫을 수 있다.
	FDigetherDepositSessionCloseResult CloseSession(const FString& PlayerKey, const FGuid& SessionId,
		EDigetherDepositSessionCloseReason Reason);
	FDigetherDepositSessionCloseResult ClosePlayerSession(const FString& PlayerKey,
		EDigetherDepositSessionCloseReason Reason);
	FDigetherDepositSessionCloseResult CloseAllSessions(EDigetherDepositSessionCloseReason Reason);
	FDigetherDepositSessionCloseResult ExpireSessions(double NowServerSeconds);

	// 서버 내부 조회용 복사본이다. 소유자 외 전달이나 공개 복제에 사용하지 않는다.
	TArray<FDigetherDepositSession> CaptureSessions() const;

private:
	TArray<FDigetherDepositSession> Sessions;
};
