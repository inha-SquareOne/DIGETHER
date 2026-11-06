#pragma once

#include "CoreMinimal.h"
#include "DigetherGameOverTypes.generated.h"

// 기획 S33 게임 오버 세부 단계, 저장 안 함(저장 플래그는 FDigetherRunPublicState::bGameOver 하나)
// 시작 시 게임 오버 확정 저장을 불러오면 이 단계에 들어가지 않고 결과만 보관한 뒤 같은 슬롯 새 게임 생성
UENUM(BlueprintType)
enum class EDigetherGameOverPhase : uint8
{
	None,
	// 5일차 귀환의 검은 개인 정산 화면, 일반 귀환과 같은 결과 화면(소유자 전용) 사용
	SettlementScreen,
	// 우물 범람 → 거점 침수 → 흡입, 각자 이동·시점 조작 유지(공통 컷신 없음)
	FailureEvent,
	// 함께 보는 결과 크레딧, 종료 시 현재 슬롯 새 게임을 1회 만들고 None으로 복귀
	Credits
};

// 기획 S33·15 GO_* 연출 튜닝, 저장 형식 미포함, 서버가 단계 시작 때 공개 상태에 복사해 모두 같은 값 사용
// 아래 기본값은 모두 임시값, 최종 값은 D15 연출 시안·화면 검수 뒤 튜닝 파라미터(GO_*)로 결정
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherGameOverTuning
{
	GENERATED_BODY()

	// 임시값: 기획 미정(ID 없음, TMP_GO_SETTLEMENT_SCREEN_TIME), 2026-11-05
	// 검은 개인 정산 화면 표시 후 실패 사건까지의 시간(초), 일반 귀환은 미사용
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	float SettlementScreenSeconds = 5.f;

	// 임시값: GO_FAIL_MOTION_TIME 미정, 2026-11-05
	// 범람·침수·흡입 전체 길이(초)이자 흡입 마감 시각
	// 마감까지 흡입되지 않은 참가자는 강제 흡입(연출 오류로 크레딧이 막히지 않게 하는 복구 경로)
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	float FailMotionSeconds = 12.f;

	// 임시값: 기획 미정(ID 없음, TMP_GO_FLOOD_RATIO), 2026-11-05
	// 실패 사건 중 범람·침수 비율(0~1), 이 시점부터 흡입 시작
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	float FloodRatio = 0.5f;

	// 임시값: 기획 미정(ID 없음, TMP_GO_SUCTION_SPEED), 2026-11-05
	// 흡입 속도(cm/초), 흡입 시작 뒤 우물에서 먼 사람일수록 늦게 흡입, 가까운 사람이 먼저 흡입돼 대기
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	float SuctionSpeedCmPerSecond = 1500.f;

	// 임시값: GO_CREDIT_START_DELAY 미정, 2026-11-05, 크레딧 화면 진입 뒤 스크롤 시작까지(초)
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	float CreditStartDelaySeconds = 2.f;

	// 임시값: GO_CREDIT_SCROLL_SPEED 미정, 2026-11-05, 문자 블록 상승 속도(화면 높이/초)
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	float CreditScrollSpeed = 0.08f;

	// 임시값: GO_CREDIT_TILT 미정, 2026-11-05, 문자 평면이 뒤로 기운 각도(도), 원근 표현은 BP 담당
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	float CreditTiltDegrees = 25.f;

	// 임시값: GO_CREDIT_BLOCK_GAP 미정, 2026-11-05, 항목별 블록 간격(화면 높이 비율)
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	float CreditBlockGap = 0.15f;

	// 임시값: GO_CREDIT_FF_MULTIPLIER 미정, 2026-11-05, 호스트 빨리감기 중 재생 속도 배율
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	float CreditFastForwardMultiplier = 4.f;

	// 임시값: 기획 미정(ID 없음, TMP_GO_CREDIT_LINE_HEIGHT), 2026-11-05
	// 크레딧 한 줄 높이(화면 높이 비율), 서버의 결정적 전체 재생 길이 계산 전용
	// 실제 글꼴·줄바꿈 검수(S33 243행) 뒤 조정
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	float CreditLineHeight = 0.06f;
};

// 결과 크레딧 블록의 플레이어 한 줄, 인증 ID 미포함(서버 전용 키 비복제)
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherGameResultPlayer
{
	GENERATED_BODY()

	// 결과 확정 시점의 표시 이름, 중간에 떠난 사람은 떠날 때의 이름
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	FString DisplayName;

	// 방 참가 순서(1부터), 블록 안 표시 순서이며 성과 순위 아님, 0은 알 수 없음(구 저장)
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	int32 JoinOrder = 0;

	// 실제 지급받은 일반 몫 + 히든 수익 누적
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	int32 EarnedCredits = 0;

	// 위 획득액 중 히든 수익만, 획득액에 이미 포함되므로 다시 더하지 않음
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	int32 HiddenCredits = 0;

	// 사망으로 실제 빠진 금액 - 시체 회수로 실제 돌려받은 금액(순손실)
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	int32 DeathLossCredits = 0;
};

// 기획 S33 한 게임(새 게임부터 이번 게임 오버까지) 결과 기록, 호스트가 마지막 정산 뒤 1회 확정
// 크레딧 재생은 이 값 표시만, 돈 재지급 없음
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherGameResult
{
	GENERATED_BODY()

	// 결과 기록 ID, "런(탐사 1회)" ID와 별도 범위
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	FGuid ResultId;

	// 끝난 게임의 ID, 이어지는 새 게임은 다른 ID 사용
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	FGuid GameId;

	// 합산 기간: 게임 오버 귀환 시점의 전체 일차(새 게임 1일차부터 계산)
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	int32 FinalTotalDay = 0;

	// 합산 기간 중 달성한 할당량 기간 수
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	int32 CompletedPeriods = 0;

	// 플레이어 전체 획득 총액 = 개인 획득액 합(히든 재가산 없음)
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	int32 TotalEarnedCredits = 0;

	// 참가 순서대로 정렬, 0원 항목도 생략하지 않음
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	TArray<FDigetherGameResultPlayer> Players;

	bool IsValid() const { return ResultId.IsValid() && GameId.IsValid(); }
};

// 크레딧 재생 기준점, 클라이언트 진행 = clamp(Anchor + (서버 시각 - AnchorServerSeconds) × Rate, 0, Total)
// 빨리감기 변경 시에만 서버가 기준점 재설정, 끝 판정은 서버 전용
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherCreditPlayback
{
	GENERATED_BODY()

	// GameStateBase::GetServerWorldTimeSeconds 기준 서버 시각
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	double AnchorServerSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	float AnchorProgressSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	float PlaybackRate = 1.f;

	// 시작 대기 + 전체 블록이 화면을 지나가는 시간(재생 속도 1 기준 초)
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	float TotalSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	bool bFastForward = false;
};

// GameState 복제용 게임 오버 공개 상태(COND_None), 결과는 크레딧 단계 진입 때만 채움
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherGameOverPublicState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	EDigetherGameOverPhase Phase = EDigetherGameOverPhase::None;

	// 단계 변경마다 1씩 증가, 같은 단계 중복 신호와 새 단계 구분
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	int32 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	double PhaseStartServerSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	FDigetherGameOverTuning Tuning;

	// 흡입이 끝난 참가자의 공개 PlayerId, 서버 GUID·인증 ID 미사용
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	TArray<int32> AbsorbedPlayerIds;

	// 아직 흡입되지 않은 현재 참가자 수, 떠난 사람 제외
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	int32 WaitingPlayerCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	FDigetherCreditPlayback Playback;

	// 크레딧 단계에서만 채움, 탐사 중 비공개였던 히든 수익·사망 손실을 이때 모두에게 공개(S33 241행)
	UPROPERTY(BlueprintReadOnly, Category="Digether|Game Over")
	FDigetherGameResult PublishedResult;
};
