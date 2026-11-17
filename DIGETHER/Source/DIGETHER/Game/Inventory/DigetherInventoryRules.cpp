#include "Game/Inventory/DigetherInventoryRules.h"

FDigetherInventoryState FDigetherInventoryRules::MakeEmpty()
{
	FDigetherInventoryState State;
	State.GeneralSlots.SetNum(GeneralSlotCount);
	return State;
}

bool FDigetherInventoryRules::IsValidState(const FDigetherInventoryState& State)
{
	if (State.GeneralSlots.Num() != GeneralSlotCount || !State.GeneralSlots.IsValidIndex(State.SelectedSlot))
	{
		return false;
	}
	TSet<FGuid> Seen;
	for (const FDigetherItemInstance& Slot : State.GeneralSlots)
	{
		if (!Slot.IsSet())
		{
			continue;
		}
		bool bAlreadyInSet = false;
		Seen.Add(Slot.InstanceId, &bAlreadyInSet);
		if (bAlreadyInSet)
		{
			return false;
		}
	}
	return true;
}

EDigetherInventoryResult FDigetherInventoryRules::TryAdd(FDigetherInventoryState& State, const FDigetherItemInstance& Item, int32& OutSlotIndex, const int32 PreferredSlot)
{
	OutSlotIndex = INDEX_NONE;
	if (!IsValidState(State))
	{
		return EDigetherInventoryResult::InvalidState;
	}
	if (!Item.IsSet())
	{
		return EDigetherInventoryResult::InvalidItem;
	}
	if (ContainsInstance(State, Item.InstanceId))
	{
		return EDigetherInventoryResult::DuplicateInstance;
	}
	if (State.GeneralSlots.IsValidIndex(PreferredSlot) && !State.GeneralSlots[PreferredSlot].IsSet())
	{
		State.GeneralSlots[PreferredSlot] = Item;
		OutSlotIndex = PreferredSlot;
		return EDigetherInventoryResult::Success;
	}
	for (int32 Index = 0; Index < State.GeneralSlots.Num(); ++Index)
	{
		if (!State.GeneralSlots[Index].IsSet())
		{
			State.GeneralSlots[Index] = Item;
			OutSlotIndex = Index;
			return EDigetherInventoryResult::Success;
		}
	}
	return EDigetherInventoryResult::Full;
}

EDigetherInventoryResult FDigetherInventoryRules::TryRemove(FDigetherInventoryState& State, const int32 SlotIndex, FDigetherItemInstance& OutItem)
{
	OutItem = FDigetherItemInstance();
	if (!IsValidState(State))
	{
		return EDigetherInventoryResult::InvalidState;
	}
	if (!State.GeneralSlots.IsValidIndex(SlotIndex))
	{
		return EDigetherInventoryResult::InvalidSlot;
	}
	if (!State.GeneralSlots[SlotIndex].IsSet())
	{
		return EDigetherInventoryResult::EmptySlot;
	}
	OutItem = State.GeneralSlots[SlotIndex];
	State.GeneralSlots[SlotIndex] = FDigetherItemInstance();
	return EDigetherInventoryResult::Success;
}

EDigetherInventoryResult FDigetherInventoryRules::TrySelect(FDigetherInventoryState& State, const int32 SlotIndex)
{
	if (!IsValidState(State))
	{
		return EDigetherInventoryResult::InvalidState;
	}
	if (!State.GeneralSlots.IsValidIndex(SlotIndex))
	{
		return EDigetherInventoryResult::InvalidSlot;
	}
	State.SelectedSlot = SlotIndex;
	return EDigetherInventoryResult::Success;
}

bool FDigetherInventoryRules::ContainsInstance(const FDigetherInventoryState& State, const FGuid& InstanceId)
{
	if (!InstanceId.IsValid())
	{
		return false;
	}
	for (const FDigetherItemInstance& Slot : State.GeneralSlots)
	{
		if (Slot.IsSet() && Slot.InstanceId == InstanceId)
		{
			return true;
		}
	}
	return false;
}
