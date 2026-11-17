#pragma once

#include "CoreMinimal.h"
#include "Game/Inventory/DigetherInventoryTypes.h"

enum class EDigetherInventoryResult : uint8
{
	Success,
	InvalidState,
	InvalidItem,
	DuplicateInstance,
	Full,
	InvalidSlot,
	EmptySlot,
};

// 인벤토리 슬롯 규칙(월드 의존 없는 순수 함수), 서버가 줍기·버리기를 확정할 때 씀
// 실패하면 상태를 바꾸지 않음
struct DIGETHER_API FDigetherInventoryRules
{
	// 일반 슬롯 수(기획 고정 규칙: 일반 4칸)
	static constexpr int32 GeneralSlotCount = 4;

	static FDigetherInventoryState MakeEmpty();

	// 슬롯 수·선택 슬롯 범위가 맞고 같은 인스턴스가 두 번 들어 있지 않은지 확인함
	static bool IsValidState(const FDigetherInventoryState& State);

	// PreferredSlot이 비어 있으면 거기에, 아니면 가장 앞의 빈 슬롯에 넣음
	static EDigetherInventoryResult TryAdd(FDigetherInventoryState& State, const FDigetherItemInstance& Item, int32& OutSlotIndex, int32 PreferredSlot = INDEX_NONE);

	// 손에 들 슬롯을 고름(빈 슬롯도 허용)
	static EDigetherInventoryResult TrySelect(FDigetherInventoryState& State, int32 SlotIndex);

	// 슬롯을 비우고 꺼낸 아이템을 돌려줌
	static EDigetherInventoryResult TryRemove(FDigetherInventoryState& State, int32 SlotIndex, FDigetherItemInstance& OutItem);

	static bool ContainsInstance(const FDigetherInventoryState& State, const FGuid& InstanceId);
};
