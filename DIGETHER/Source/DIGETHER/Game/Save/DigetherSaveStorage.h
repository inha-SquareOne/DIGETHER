#pragma once

#include "Game/Save/DigetherSaveTypes.h"

struct DIGETHER_API FDigetherSaveSlotNames
{
	FString Primary;
	FString Backup;
};

// 주·백업 슬롯을 한 번 읽은 결과다. 같은 게임 스레드 저장 안에서 선택과 쓰기 판정에 재사용한다.
struct DIGETHER_API FDigetherSaveSlotReads
{
	EDigetherSaveResult PrimaryResult = EDigetherSaveResult::NotFound;
	EDigetherSaveResult BackupResult = EDigetherSaveResult::NotFound;
	FDigetherSaveSlotData Primary;
	FDigetherSaveSlotData Backup;
};

// 디스크 접근은 엔진 SaveGame API로 한정한다. 슬롯 선택과 순서 판정은 SaveRules가 담당한다.
namespace DigetherSaveStorage
{
	// 호스트 계정 해시와 진행 슬롯 번호로 주·백업 파일 이름을 만든다. 슬롯 0은 슬롯 도입 전 이름과 같다.
	// 슬롯 번호가 범위를 벗어나면 빈 이름을 돌려주며, 빈 이름은 읽기·쓰기에서 InvalidState로 거절된다.
	DIGETHER_API FDigetherSaveSlotNames MakeSlotNames(const FString& HostKey, int32 SlotIndex = 0);
	DIGETHER_API EDigetherSaveResult ReadSlot(const FString& SlotName, const FString& HostKey,
		FDigetherSaveSlotData& OutSlot);
	// 주 슬롯이 유효해도 백업의 미래 버전 보호 여부를 확인하려고 두 슬롯을 모두 읽는다.
	DIGETHER_API FDigetherSaveSlotReads ReadSlots(const FDigetherSaveSlotNames& Names, const FString& HostKey);
	DIGETHER_API EDigetherSaveResult Read(const FDigetherSaveSlotNames& Names, const FString& HostKey,
		FDigetherSaveSlotData& OutSlot, bool& bOutRecoveredFromBackup);
	// ReadSlots 결과에서 읽을 슬롯을 고른다. 실패 시 출력 인자를 유지한다.
	DIGETHER_API EDigetherSaveResult Read(const FDigetherSaveSlotReads& Reads,
		FDigetherSaveSlotData& OutSlot, bool& bOutRecoveredFromBackup);
	DIGETHER_API EDigetherSaveResult Write(const FDigetherSaveSlotNames& Names, const FString& HostKey,
		const FDigetherSaveSlotData& Slot, bool bExplicitNewGame);
	// Reads는 같은 Names·HostKey로 쓰기 직전에 읽은 결과여야 한다. 디스크를 다시 읽지 않고 쓰기 허용·순서를 정한다.
	DIGETHER_API EDigetherSaveResult Write(const FDigetherSaveSlotNames& Names, const FString& HostKey,
		const FDigetherSaveSlotData& Slot, bool bExplicitNewGame, const FDigetherSaveSlotReads& Reads);
}
