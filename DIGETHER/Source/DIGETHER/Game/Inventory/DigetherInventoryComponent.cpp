#include "Game/Inventory/DigetherInventoryComponent.h"

#include "Game/Inventory/DigetherInventoryLog.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UDigetherInventoryComponent::UDigetherInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	State = FDigetherInventoryRules::MakeEmpty();
}

void UDigetherInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// 소지품 내용은 본인만 알면 됨
	DOREPLIFETIME_CONDITION(UDigetherInventoryComponent, State, COND_OwnerOnly);
}

bool UDigetherInventoryComponent::IsFull() const
{
	for (const FDigetherItemInstance& Slot : State.GeneralSlots)
	{
		if (!Slot.IsSet())
		{
			return false;
		}
	}
	return true;
}

EDigetherInventoryResult UDigetherInventoryComponent::TryAddItem(const FDigetherItemInstance& Item, int32& OutSlotIndex)
{
	OutSlotIndex = INDEX_NONE;
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return EDigetherInventoryResult::InvalidState;
	}
	const EDigetherInventoryResult Result = FDigetherInventoryRules::TryAdd(State, Item, OutSlotIndex, State.SelectedSlot);
	if (Result == EDigetherInventoryResult::Success)
	{
		// 서버의 C++ 대입은 OnRep을 부르지 않으므로 직접 통지함
		OnInventoryChanged.Broadcast();
	}
	return Result;
}

EDigetherInventoryResult UDigetherInventoryComponent::TrySelectSlot(const int32 SlotIndex)
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return EDigetherInventoryResult::InvalidState;
	}
	if (SlotIndex == State.SelectedSlot)
	{
		return EDigetherInventoryResult::Success;
	}
	const EDigetherInventoryResult Result = FDigetherInventoryRules::TrySelect(State, SlotIndex);
	if (Result == EDigetherInventoryResult::Success)
	{
		OnInventoryChanged.Broadcast();
	}
	return Result;
}

void UDigetherInventoryComponent::GrantStartingItemsOnce(const TArray<FName>& DefinitionIds)
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || State.bStartingItemsGranted)
	{
		return;
	}
	State.bStartingItemsGranted = true;
	for (const FName DefinitionId : DefinitionIds)
	{
		int32 SlotIndex = INDEX_NONE;
		const EDigetherInventoryResult Result = FDigetherInventoryRules::TryAdd(State, FDigetherItemInstance::MakeNew(DefinitionId), SlotIndex);
		if (Result != EDigetherInventoryResult::Success)
		{
			UE_LOG(LogDigetherInventory, Warning, TEXT("시작 장비 %s를 넣지 못했다: 결과 %d"), *DefinitionId.ToString(), static_cast<int32>(Result));
		}
	}
	OnInventoryChanged.Broadcast();
}

EDigetherInventoryResult UDigetherInventoryComponent::TryRemoveSlot(const int32 SlotIndex, FDigetherItemInstance& OutItem)
{
	OutItem = FDigetherItemInstance();
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return EDigetherInventoryResult::InvalidState;
	}
	const EDigetherInventoryResult Result = FDigetherInventoryRules::TryRemove(State, SlotIndex, OutItem);
	if (Result == EDigetherInventoryResult::Success)
	{
		OnInventoryChanged.Broadcast();
	}
	return Result;
}

bool UDigetherInventoryComponent::RestoreState(const FDigetherInventoryState& NewState, const bool bNotify)
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !FDigetherInventoryRules::IsValidState(NewState))
	{
		UE_LOG(LogDigetherInventory, Warning, TEXT("소지품 복원을 거절했다: 권한이 없거나 상태가 올바르지 않다"));
		return false;
	}
	State = NewState;
	if (bNotify)
	{
		OnInventoryChanged.Broadcast();
	}
	return true;
}

void UDigetherInventoryComponent::OnRep_State()
{
	OnInventoryChanged.Broadcast();
}
