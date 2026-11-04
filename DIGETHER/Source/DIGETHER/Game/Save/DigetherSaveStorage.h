#pragma once

#include "Game/Save/DigetherSaveTypes.h"

struct DIGETHER_API FDigetherSaveSlotNames
{
	FString Primary;
	FString Backup;
};

// 디스크 접근은 엔진 SaveGame API로 한정한다. 슬롯 선택과 순서 판정은 SaveRules가 담당한다.
namespace DigetherSaveStorage
{
	DIGETHER_API FDigetherSaveSlotNames MakeSlotNames(const FString& HostKey);
	DIGETHER_API EDigetherSaveResult ReadSlot(const FString& SlotName, const FString& HostKey,
		FDigetherSaveSlotData& OutSlot);
	DIGETHER_API EDigetherSaveResult Read(const FDigetherSaveSlotNames& Names, const FString& HostKey,
		FDigetherSaveSlotData& OutSlot, bool& bOutRecoveredFromBackup);
	DIGETHER_API EDigetherSaveResult Write(const FDigetherSaveSlotNames& Names, const FString& HostKey,
		const FDigetherSaveSlotData& Slot, bool bExplicitNewGame);
}
