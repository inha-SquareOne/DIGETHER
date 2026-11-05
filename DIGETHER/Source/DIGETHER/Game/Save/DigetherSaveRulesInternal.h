#pragma once

#include "Game/Save/DigetherSaveTypes.h"

// DigetherSaveRules 구현 파일(Rules·Codec·Validation·SlotPolicy) 사이 전용
// 모듈 밖·다른 도메인에서 포함 금지
namespace DigetherSaveRulesInternal
{
	bool IsBoundedString(const FString& Value);
	bool IsValidTransformParts(const FVector& Location, const FQuat& Rotation, const FVector& Scale);
	bool IsValidPlayerRecord(const FDigetherSavePlayerRecord& Player);
	bool IsValidPlayers(const TArray<FDigetherSavePlayerRecord>& Players);
	EDigetherSaveResult ValidateSlot(const FDigetherSaveSlotData& Slot);
}
