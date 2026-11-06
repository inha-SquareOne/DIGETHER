#pragma once

#include "CoreMinimal.h"

namespace DigetherSaveTuning
{
	// 런타임 진행 값(FDigetherSaveSnapshot) 스키마 버전, 파일 바이트 형식 버전은 DigetherSaveWire 별도
	inline constexpr int32 SchemaVersion = 3;

	// 슬롯 파일 이름용 버전, 스키마·wire 버전을 올려도 기존 저장을 찾도록 고정
	// 변경 시 기존 슬롯을 못 찾음, 바꿔야 하면 이전 이름을 읽는 이전 경로 함께 추가
	inline constexpr int32 SlotNameVersion = 3;

	// 임시 값: 기획 미정(S30 "슬롯 최대 개수는 미정"), 2026-11-05, 기획 확인 대상
	// 호스트가 방 생성 때 고르는 진행 슬롯 수, 슬롯 0은 슬롯 도입 전과 같은 파일
	inline constexpr int32 MaxProgressSlots = 3;

	// 임시 제한: 기획 확인 대상(세션 전체 참가 기록·정산 기록 보존 상한), 2026-11-04
	// 미접속 이력도 남으므로 동시 참가 4명으로 자르지 않음
	inline constexpr int32 MaxPlayerRecords = 256;
	inline constexpr int32 MaxCompletedRunRecords = 100000;
	inline constexpr int32 MaxStringLength = 1024;
	inline constexpr int32 MaxSaveBytes = 16 * 1024 * 1024;
	// 임시 값(2026-11-05): 바닥 금화 더미 수 상한, 정산기당 하나로 합치므로 손상 파일의 과대 할당 방지용
	inline constexpr int32 MaxFloorCoins = 64;
	// 임시 값: 기획 미정(S33 '이전 게임 결과는 따로 보관' 보관 개수), 2026-11-05, 기획 확인 대상
	// 슬롯당 이전 게임 결과 보관 수, 넘으면 가장 오래된 결과부터 폐기
	inline constexpr int32 MaxArchivedGameResults = 10;

	// 임시 값: 기획 미정(S30 '재시도 간격·표시 위치·문구는 미정'), 2026-11-05
	// 플레이 중 자동 저장 실패 시 같은 저장 시점 스냅샷을 다시 쓰는 간격
	inline constexpr float AutosaveRetrySeconds = 10.f;
}
