// DIGETHER 연결 상태 패널의 순수 값 모델. UI와 분리해 Automation Test에서 직접 검증한다.
#pragma once

#include "CoreMinimal.h"

/**
 * 'DIGETHER 연결 상태' 패널의 한 행.
 * 실게임 item/economy/spawn/room/runtime provider의 연결 여부를 그대로 보여주는 합성 모델이며,
 * 이 구조체 자체가 실제 provider 등록을 수행하지 않는다.
 */
struct FDigetherConnectionStatusRow
{
	/** 도메인 식별자. 중복되지 않아야 한다. */
	FName DomainId;

	/** 한국어 도메인 표시 이름. */
	FText DomainLabel;

	/** 한국어 상태 표시 문구 (예: "미연결", "미등록"). */
	FText StatusLabel;

	/** 왜 미연결/미등록인지와 관련 결정 대기 항목을 설명하는 한국어 문구. */
	FText DetailText;

	/** 실게임 provider가 실제로 등록되어 있으면 true. 현재 구현에서는 항상 false다. */
	bool bConnected = false;
};

/**
 * 아이템/경제/스폰/룸/런타임 준비 신호와 기획 승인 개정 등록 상태를 조회한다.
 * 이 함수는 실게임 데이터를 읽지 않으며, provider가 등록되지 않은 현재 상태를 고정 값으로 반환한다.
 * 실제 provider가 추가되면 이 함수가 그 연결 여부를 반영하도록 확장해야 한다 (README.md 참조).
 */
TArray<FDigetherConnectionStatusRow> BuildDigetherConnectionStatusRows();
