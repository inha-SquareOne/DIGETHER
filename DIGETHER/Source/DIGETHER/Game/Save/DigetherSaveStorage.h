#pragma once

#include "Game/Save/DigetherSaveTypes.h"

struct DIGETHER_API FDigetherSaveSlotNames
{
	FString Primary;
	FString Backup;
};

// 주·백업 슬롯을 한 번 읽은 결과, 같은 게임 스레드 저장 안에서 선택·쓰기 판정에 재사용
struct DIGETHER_API FDigetherSaveSlotReads
{
	EDigetherSaveResult PrimaryResult = EDigetherSaveResult::NotFound;
	EDigetherSaveResult BackupResult = EDigetherSaveResult::NotFound;
	FDigetherSaveSlotData Primary;
	FDigetherSaveSlotData Backup;
};

// 디스크 접근은 엔진 SaveGame API로 한정, 슬롯 선택·순서 판정은 SaveRules 담당
namespace DigetherSaveStorage
{
	// 호스트 계정 해시와 진행 슬롯 번호로 주·백업 파일 이름 생성, 슬롯 0은 슬롯 도입 전 이름
	// 범위 밖 슬롯 번호는 빈 이름, 빈 이름은 읽기·쓰기에서 InvalidState
	DIGETHER_API FDigetherSaveSlotNames MakeSlotNames(const FString& HostKey, int32 SlotIndex = 0);
	DIGETHER_API EDigetherSaveResult ReadSlot(const FString& SlotName, const FString& HostKey,
		FDigetherSaveSlotData& OutSlot);
	// 백업의 미래 버전 보호 확인을 위해 주 슬롯이 유효해도 두 슬롯 모두 읽음
	DIGETHER_API FDigetherSaveSlotReads ReadSlots(const FDigetherSaveSlotNames& Names, const FString& HostKey);
	DIGETHER_API EDigetherSaveResult Read(const FDigetherSaveSlotNames& Names, const FString& HostKey,
		FDigetherSaveSlotData& OutSlot, bool& bOutRecoveredFromBackup);
	// ReadSlots 결과에서 읽을 슬롯 선택, 실패 시 출력 인자 유지
	DIGETHER_API EDigetherSaveResult Read(const FDigetherSaveSlotReads& Reads,
		FDigetherSaveSlotData& OutSlot, bool& bOutRecoveredFromBackup);
	DIGETHER_API EDigetherSaveResult Write(const FDigetherSaveSlotNames& Names, const FString& HostKey,
		const FDigetherSaveSlotData& Slot, bool bExplicitNewGame);
	// Reads는 같은 Names·HostKey로 쓰기 직전에 읽은 결과여야 함, 디스크 재읽기 없이 쓰기 허용·순서 결정
	DIGETHER_API EDigetherSaveResult Write(const FDigetherSaveSlotNames& Names, const FString& HostKey,
		const FDigetherSaveSlotData& Slot, bool bExplicitNewGame, const FDigetherSaveSlotReads& Reads);
}
