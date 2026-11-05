#pragma once

#include "CoreMinimal.h"
#include "DigetherSaveSlotTypes.generated.h"

// 방 생성 때 고른 진행 슬롯의 시작 방식(기획 S01 2행·S30 205행)
UENUM(BlueprintType)
enum class EDigetherSaveStartMode : uint8
{
	// 슬롯의 저장 진행 이어 가기, 빈 슬롯은 새 진행으로 시작하고 첫 자동 저장이 이 슬롯에 씀
	// (슬롯 도입 전 기본 동작과 같음), 손상·미래 버전 저장은 새 게임으로 덮지 않고 거절
	Continue,
	// 이 슬롯만 새 게임으로 초기화, 다른 슬롯은 읽기·쓰기 없음
	NewGame
};

// 슬롯 목록 한 칸의 상태, 화면은 Ready만 이어하기 선택 허용
UENUM(BlueprintType)
enum class EDigetherSaveSlotStatus : uint8
{
	// 주·백업 모두 없음
	Empty,
	// 읽을 수 있는 진행 있음, 요약 값 채움
	Ready,
	// 이 실행 파일보다 새 형식, 이어하기·새 게임 모두 거절, 파일 보존
	FutureVersion,
	// 손상·다른 호스트 계정·읽기 실패, 이어하기 거절, 새 게임만 선택 가능
	Unreadable,
	// 호스트 계정(온라인 ID) 미확인으로 슬롯 파일 이름 결정 불가
	IdentityUnavailable
};

// 호스트가 방 생성 때 고른 진행 슬롯, 저장 서브시스템이 게임 맵까지 보관하고 GameMode 시작이 사용
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherSaveSlotSelection
{
	GENERATED_BODY()

	// 0..DigetherSaveTuning::MaxProgressSlots-1, 슬롯 0은 슬롯 도입 전 저장 파일
	UPROPERTY(BlueprintReadWrite, Category="Digether|Save")
	int32 SlotIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category="Digether|Save")
	EDigetherSaveStartMode StartMode = EDigetherSaveStartMode::Continue;
};

// 슬롯 목록 표시용 요약, 모든 참가자가 게임 안에서 볼 수 있는 공개 값만
// (개인 잔고·히든 누적액·정확한 공동 인정액·인증 ID 제외)
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherSaveSlotSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Save")
	int32 SlotIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Save")
	EDigetherSaveSlotStatus Status = EDigetherSaveSlotStatus::Empty;

	// 아래 값은 Status가 Ready일 때만 유효
	UPROPERTY(BlueprintReadOnly, Category="Digether|Save")
	int32 TotalDay = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Save")
	int32 PeriodDay = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Save")
	int32 TargetCredits = 0;

	// 달성한 할당량 기간 수(0이면 첫 기간)
	UPROPERTY(BlueprintReadOnly, Category="Digether|Save")
	int32 CompletedPeriods = 0;

	// 게임 오버 확정 저장 여부, 이어하기는 같은 슬롯 새 게임으로 시작(S30 206행)
	UPROPERTY(BlueprintReadOnly, Category="Digether|Save")
	bool bGameOver = false;

	// 진행에 기록된 참가자 수(미접속 이력 포함)
	UPROPERTY(BlueprintReadOnly, Category="Digether|Save")
	int32 PlayerRecordCount = 0;

	// 저장 시각을 아는지, wire v5 이전 파일은 거짓
	UPROPERTY(BlueprintReadOnly, Category="Digether|Save")
	bool bHasSavedAt = false;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Save")
	FDateTime SavedAtUtc;
};
