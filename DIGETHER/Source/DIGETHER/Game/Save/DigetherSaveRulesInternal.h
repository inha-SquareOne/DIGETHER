#pragma once

#include "Game/Save/DigetherSaveTypes.h"

// DigetherSaveRules 구현 파일(Rules·Codec·Validation·SlotPolicy) 사이에서만 공유한다.
// 모듈 밖이나 다른 도메인에서 포함하지 않는다.
namespace DigetherSaveRulesInternal
{
	bool IsBoundedString(const FString& Value);
	bool IsValidTransformParts(const FVector& Location, const FQuat& Rotation, const FVector& Scale);
	bool IsValidPlayerRecord(const FDigetherSavePlayerRecord& Player);
	bool IsValidPlayers(const TArray<FDigetherSavePlayerRecord>& Players);
	EDigetherSaveResult ValidateSlot(const FDigetherSaveSlotData& Slot);
	// 슬롯 헤더 없이 스냅샷 하나만 v3 codec으로 쓴다. Archive 오류면 false다.
	bool TryEncodeSnapshot(const FDigetherSaveSnapshot& Snapshot, TArray<uint8>& OutBytes);
}
