#pragma once

#include "CoreMinimal.h"
#include "Game/Monster/DigetherMonsterTypes.h"
#include "DigetherGroundSpiderTypes.generated.h"

class ADigetherSpiderWebActor;

// 땅거미 상태, Blackboard 키 SpiderState(Enum)로 BT 분기에 사용
UENUM(BlueprintType)
enum class EDigetherSpiderState : uint8
{
	Idle,
	Tracking,
	Investigating,
	LeapWindup,
	LeapFlight,
	LeapLanding,
	Returning,
	Buried,
	Dead
};

// 추적 포기 사유, 로그·시험용
UENUM(BlueprintType)
enum class EDigetherSpiderGiveUpReason : uint8
{
	None,
	TargetLost,
	Unreachable,
	OutOfArea,
	NotReacquired
};

// 땅거미 종 파라미터, 값은 2026-11-08 우선제작 기획서 시험값과 개발 시험값(기획 확정 아님)
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherGroundSpiderParams : public FDigetherMonsterSpeciesParams
{
	GENERATED_BODY()

	virtual bool CollectProblems(TArray<FText>& OutProblems) const override;

	// 추적 가능 영역(둥지 중심 반지름), 개발 시험값 25m
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Spider", meta=(ClampMin="1", Units="cm"))
	float TrackRadius = 2500.f;

	// 시체 회수 영역, 2단계 A에서 사용, 개발 시험값 30m
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Spider", meta=(ClampMin="1", Units="cm"))
	float CorpseRadius = 3000.f;

	// 시야 밖에서 재감지를 기다리는 시간, 기획서에 값 없음(개발 시험값, 기획 확인 대상)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Spider", meta=(ClampMin="0", Units="s"))
	float ReacquireSeconds = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Spider", meta=(ClampMin="0", Units="s"))
	float LeapWindupSeconds = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Spider", meta=(ClampMin="0", Units="cm"))
	float LeapMaxDistance = 800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Spider", meta=(ClampMin="1", Units="cm/s"))
	float LeapSpeed = 1500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Spider", meta=(ClampMin="0", Units="s"))
	float LeapLandingSeconds = 0.8f;

	// 도약 경로 위 모든 플레이어(동료 포함)에게 1회
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Spider", meta=(ClampMin="0"))
	float LeapDamage = 90.f;

	// 몸에 닿아 있으면 간격마다, 매몰자 제외, 같은 순간 도약 피해와 중복 없음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Spider", meta=(ClampMin="0"))
	float ContactDamage = 40.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Spider", meta=(ClampMin="0.1", Units="s"))
	float ContactIntervalSeconds = 1.f;

	// 접촉으로 보는 캡슐 표면 사이 여유
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Spider", meta=(ClampMin="0", Units="cm"))
	float ContactTolerance = 15.f;

	// 1단계: 매몰되면 이 시간 뒤 가장 가까운 빈 공간에 나타남(개발 시험값 5초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Spider", meta=(ClampMin="0", Units="s"))
	float BuriedEmergeSeconds = 5.f;

	// 둥지 복귀 완료 거리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Spider", meta=(ClampMin="1", Units="cm"))
	float HomeArriveRadius = 200.f;

	// 거미줄, 개발 시험값: 기준량 6, 부족하면 20초마다 1개, 감속 50%
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Spider|Web")
	TSoftClassPtr<ADigetherSpiderWebActor> WebClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Spider|Web", meta=(ClampMin="0"))
	int32 WebBaseCount = 6;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Spider|Web", meta=(ClampMin="1", Units="s"))
	float WebRefillSeconds = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Spider|Web", meta=(ClampMin="0.05", ClampMax="1"))
	float WebSlowMultiplier = 0.5f;

	// 거미줄을 붙일 면 탐색 거리(기획: 최대 3m 면 검사)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Spider|Web", meta=(ClampMin="1", Units="cm"))
	float WebSurfaceTraceDistance = 300.f;
};

// 클라이언트 표현용 공개 상태, 늦은 합류자가 남은 연출 시간을 계산
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherSpiderPublicState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Spider")
	EDigetherSpiderState State = EDigetherSpiderState::Idle;

	// GameState 서버 시각 기준 상태 시작
	UPROPERTY(BlueprintReadOnly, Category="Digether|Spider")
	double StateStartServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Spider")
	float StateDuration = 0.f;
};
