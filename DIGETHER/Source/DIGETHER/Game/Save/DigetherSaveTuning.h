#pragma once

#include "CoreMinimal.h"

namespace DigetherSaveTuning
{
	// 런타임 진행 값(FDigetherSaveSnapshot)의 스키마 버전이다. 파일 바이트 형식은 DigetherSaveWire가 따로 버전을 갖는다.
	inline constexpr int32 SchemaVersion = 3;

	// 슬롯 파일 이름에 넣는 버전이다. 스키마·wire 버전을 올려도 기존 저장을 계속 찾도록 이 값은 고정한다.
	// 이 값을 바꾸면 기존 슬롯을 못 찾으므로, 바꿔야 한다면 이전 이름을 읽는 이전 경로를 함께 만든다.
	inline constexpr int32 SlotNameVersion = 3;

	// 임시 값: 기획 미정(S30 "슬롯 최대 개수는 미정"), 2026-11-05. 기획 확인 대상.
	// 호스트가 방을 만들 때 고르는 진행 슬롯 수다. 슬롯 0은 슬롯 도입 전 이름과 같은 파일을 쓴다.
	inline constexpr int32 MaxProgressSlots = 3;

	// 임시 제한: 기획 확인 대상(세션 전체 참가 기록·정산 기록 보존 상한), 2026-11-04.
	// 동시 참가 4명과 달리 미접속 이력도 남으므로 레코드 수를 4로 자르지 않는다.
	inline constexpr int32 MaxPlayerRecords = 256;
	inline constexpr int32 MaxCompletedRunRecords = 100000;
	inline constexpr int32 MaxStringLength = 1024;
	inline constexpr int32 MaxSaveBytes = 16 * 1024 * 1024;
	// 바닥 금화 더미 수 상한이다. 정산기당 하나로 합치므로 손상 파일의 과대 할당만 막는 값이다(임시, 2026-11-05).
	inline constexpr int32 MaxFloorCoins = 64;
	// 임시 값: 기획 미정(S33 '이전 게임 결과는 따로 보관' 보관 개수), 2026-11-05. 기획 확인 대상.
	// 슬롯 하나가 보관하는 이전 게임 결과 수다. 넘으면 가장 오래된 결과부터 버린다.
	inline constexpr int32 MaxArchivedGameResults = 10;

	// 임시 값: 기획 미정(S30 '재시도 간격·표시 위치·문구는 미정'), 2026-11-05.
	// 플레이 중 자동 저장이 실패하면 이 간격으로 같은 저장 시점의 스냅샷을 다시 쓴다.
	inline constexpr float AutosaveRetrySeconds = 10.f;
}
