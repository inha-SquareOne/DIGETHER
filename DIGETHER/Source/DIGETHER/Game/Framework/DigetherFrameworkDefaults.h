#pragma once

#include "CoreMinimal.h"

namespace DigetherFrameworkDefaults
{
	// 임시값: 기획 미정(시작 지급액), 2026-11-04
	inline constexpr int32 StartingBalanceCredits = 0;

	// 임시값: 기획 미정(납부 요청 종류별 최소 간격), 2026-11-04
	inline constexpr double DepositRequestIntervalSeconds = 0.2;

	// 임시값: 기획 미정(시험 엘리베이터·레버 표시 크기와 탑승 공간, cm), 2026-11-04
	inline const FVector ElevatorBoardingExtentCm(250.0, 250.0, 100.0);
	inline const FVector ElevatorBoardingOffsetCm(0.0, 0.0, 100.0);
	inline const FVector ElevatorFloorMeshScale(5.0, 5.0, 0.2);
	inline const FVector ElevatorFloorMeshOffsetCm(0.0, 0.0, -10.0);
	inline const FVector ElevatorLeverMeshScale(0.2, 0.2, 1.0);
	inline const FVector ElevatorLeverMeshOffsetCm(0.0, 0.0, 50.0);

	// 임시값: 기획 미정(시험 Pawn의 탑승·귀환 배치 간격과 관전 전환 시간), 2026-11-04
	inline constexpr double PlayerSpacingCm = 100.0;
	inline constexpr float SpectateBlendSeconds = 0.2f;

	// 임시값: 기획 미정(결과 크레딧 화면 연결 전), 2026-11-04
	inline constexpr bool bAnyoneWatchingResultCredits = false;
}
