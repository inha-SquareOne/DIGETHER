#pragma once

#include "CoreMinimal.h"
#include "DigetherSaveSlotTypes.generated.h"

// 방을 만들 때 고른 진행 슬롯의 시작 방식이다(기획 S01 2행·S30 205행).
UENUM(BlueprintType)
enum class EDigetherSaveStartMode : uint8
{
	// 슬롯의 저장 진행을 이어 간다. 슬롯이 비어 있으면 새 진행으로 시작해 첫 자동 저장이 이 슬롯에 쓴다
	// (슬롯 도입 전 기본 동작과 같다). 손상·미래 버전 저장은 새 게임으로 덮지 않고 거절한다.
	Continue,
	// 이 슬롯만 새 게임으로 초기화한다. 다른 슬롯은 읽거나 쓰지 않는다.
	NewGame
};

// 슬롯 목록 한 칸의 상태다. 화면은 Ready만 이어하기로 고를 수 있게 한다.
UENUM(BlueprintType)
enum class EDigetherSaveSlotStatus : uint8
{
	// 주·백업 모두 없다.
	Empty,
	// 읽을 수 있는 진행이 있다. 요약 값이 채워진다.
	Ready,
	// 이 실행 파일보다 새 형식이다. 이어하기·새 게임 모두 거절되며 파일은 보존된다.
	FutureVersion,
	// 손상·다른 호스트 계정·읽기 실패다. 이어하기는 거절되고 새 게임만 고를 수 있다.
	Unreadable,
	// 호스트 계정(온라인 ID)을 아직 알 수 없어 슬롯 파일 이름을 정하지 못했다.
	IdentityUnavailable
};

// 호스트가 방 생성 때 고른 진행 슬롯이다. 저장 서브시스템이 게임 맵까지 보관하고 GameMode 시작이 사용한다.
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherSaveSlotSelection
{
	GENERATED_BODY()

	// 0..DigetherSaveTuning::MaxProgressSlots-1. 슬롯 0은 슬롯 도입 전 저장 파일과 같다.
	UPROPERTY(BlueprintReadWrite, Category="Digether|Save")
	int32 SlotIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category="Digether|Save")
	EDigetherSaveStartMode StartMode = EDigetherSaveStartMode::Continue;
};

// 슬롯 목록 표시용 요약이다. 모든 참가자가 게임 안에서 볼 수 있는 공개 값만 담는다
// (개인 잔고·히든 누적액·정확한 공동 인정액·인증 ID는 넣지 않는다).
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherSaveSlotSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Save")
	int32 SlotIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Save")
	EDigetherSaveSlotStatus Status = EDigetherSaveSlotStatus::Empty;

	// 아래 값은 Status가 Ready일 때만 의미가 있다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Save")
	int32 TotalDay = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Save")
	int32 PeriodDay = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Save")
	int32 TargetCredits = 0;

	// 달성한 할당량 기간 수다(0이면 첫 기간).
	UPROPERTY(BlueprintReadOnly, Category="Digether|Save")
	int32 CompletedPeriods = 0;

	// 게임 오버 확정이 저장된 진행이면 참이다. 이어하기는 같은 슬롯의 새 게임으로 시작한다(S30 206행).
	UPROPERTY(BlueprintReadOnly, Category="Digether|Save")
	bool bGameOver = false;

	// 진행에 기록된 참가자 수(미접속 이력 포함)다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Save")
	int32 PlayerRecordCount = 0;

	// 저장 시각을 아는지다. wire v5 이전 파일은 거짓이다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Save")
	bool bHasSavedAt = false;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Save")
	FDateTime SavedAtUtc;
};
