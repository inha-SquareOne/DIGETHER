#pragma once

#include "CoreMinimal.h"
#include "DigetherElevatorTuning.generated.h"

// 보드 확정값(DEV-061), 판정 원본이며 아래 튜닝 필드 기본값도 이 값 사용
namespace DigetherElevatorBoardValues
{
	inline constexpr int32 WarningDayNumber = 4;
	inline constexpr float WarningDayHoldMultiplier = 2.f;
}

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherElevatorTuning
{
	GENERATED_BODY()

	// 임시값: 기획 미정(DEV-019), 2026-11-04
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Elevator", meta=(ClampMin="0", ClampMax="1"))
	float LightMaxCapacityRatio = 0.3f;

	// 임시값: 기획 미정(DEV-019), 2026-11-04
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Elevator", meta=(ClampMin="0", ClampMax="1"))
	float MediumMaxCapacityRatio = 0.7f;

	// ELV_AUTO_RETURN_DELAY(추정 대응: 1차 위층 회수 뒤 자동 재하강까지의 위층 정차 시간)
	// 임시값: 기획 미정(DEV-034), 2026-11-04
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Elevator", meta=(ClampMin="0", Units="s"))
	float UpperStopSeconds = 3.f;

	// ELV_ASCEND_HOLD_TIME, 임시값: 기획 미정(DEV-066), 2026-11-04
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Elevator", meta=(ClampMin="0", Units="s"))
	float AscentHoldSeconds = 3.f;

	// PLY_LEVER_HOLD_TIME(1~3일차 T, 4일차는 확정 배수 2T), 임시값: 기획 시험 기준 약 3초(DEV-061), 2026-11-04
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Elevator", meta=(ClampMin="0", Units="s"))
	float DepartureHoldSeconds = 3.f;

	// LOBBY_DOOR_CLOSE_TIME: 거점 출발 게이지 완료 뒤 문이 닫히는 고정 시간, 그동안 걸어서 탑승 가능
	// 임시값: 기획 시험 값·수치 미정, 2026-11-05
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Elevator", meta=(ClampMin="0", Units="s"))
	float HubDoorCloseSeconds = 3.f;

	// ELV_DOOR_CLOSE_TIME: 갱도 상승 게이지 완료 뒤 문 닫힘 시간, 완전히 닫힌 순간 편 목록 고정
	// 임시값: 기획 시험 값·수치 미정, 2026-11-05
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Elevator", meta=(ClampMin="0", Units="s"))
	float MineDoorCloseSeconds = 3.f;

	// LOBBY_CHAIN_SUMMON_TIME: 거점 문 닫힘 완료 때 사슬 소집 연출(암전) 길이
	// 서버 이동은 이 값과 무관하게 문 닫힘 완료 순간 순간이동 1회, 클라이언트 BP가 소집 이벤트로 이 길이를 받아 연출
	// 임시값: MVP 즉시 소집 0초(기획 수치 미정), 2026-11-05
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Elevator", meta=(ClampMin="0", Units="s"))
	float ChainSummonSeconds = 0.f;

	// 보드 확정값(DEV-061), BP 노출용 필드 유지, 다른 값이면 IsValidTuning이 거절
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="Digether|Elevator")
	int32 WarningDayNumber = DigetherElevatorBoardValues::WarningDayNumber;

	// 보드 확정값(DEV-061), BP 노출용 필드 유지, 다른 값이면 IsValidTuning이 거절
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="Digether|Elevator")
	float WarningDayHoldMultiplier = DigetherElevatorBoardValues::WarningDayHoldMultiplier;
};
