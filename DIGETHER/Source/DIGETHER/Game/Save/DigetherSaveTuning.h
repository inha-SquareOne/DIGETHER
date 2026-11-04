#pragma once

#include "CoreMinimal.h"

namespace DigetherSaveTuning
{
	inline constexpr int32 SchemaVersion = 3;

	// 임시 제한: 기획 확인 대상(세션 전체 참가 기록·정산 기록 보존 상한), 2026-11-04.
	// 동시 참가 4명과 달리 미접속 이력도 남으므로 레코드 수를 4로 자르지 않는다.
	inline constexpr int32 MaxPlayerRecords = 256;
	inline constexpr int32 MaxCompletedRunRecords = 100000;
	inline constexpr int32 MaxStringLength = 1024;
	inline constexpr int32 MaxSaveBytes = 16 * 1024 * 1024;
}
