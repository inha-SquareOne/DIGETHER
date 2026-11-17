#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Inventory/DigetherInventoryRules.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherInventoryRulesTest,
	"DIGETHER.Game.Inventory.Rules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherInventoryRulesTest::RunTest(const FString& Parameters)
{
	FDigetherInventoryState State = FDigetherInventoryRules::MakeEmpty();
	TestEqual(TEXT("일반 슬롯 4칸"), State.GeneralSlots.Num(), FDigetherInventoryRules::GeneralSlotCount);
	TestTrue(TEXT("빈 상태는 올바름"), FDigetherInventoryRules::IsValidState(State));
	TestEqual(TEXT("처음 손에 든 슬롯은 1번"), State.SelectedSlot, 0);

	// 빈 인스턴스는 거절
	int32 Slot = INDEX_NONE;
	TestTrue(TEXT("빈 인스턴스는 거절"), FDigetherInventoryRules::TryAdd(State, FDigetherItemInstance(), Slot) == EDigetherInventoryResult::InvalidItem);

	// 앞 슬롯부터 채움
	const FDigetherItemInstance Pickaxe = FDigetherItemInstance::MakeNew(TEXT("DA_Item_Pickaxe"));
	TestTrue(TEXT("인스턴스가 채워짐"), Pickaxe.IsSet());
	TestTrue(TEXT("첫 줍기 성공"), FDigetherInventoryRules::TryAdd(State, Pickaxe, Slot) == EDigetherInventoryResult::Success);
	TestEqual(TEXT("첫 슬롯"), Slot, 0);

	// 같은 인스턴스는 두 번 들어갈 수 없음(한 곳에만 존재)
	TestTrue(TEXT("같은 인스턴스 중복 거절"), FDigetherInventoryRules::TryAdd(State, Pickaxe, Slot) == EDigetherInventoryResult::DuplicateInstance);
	TestEqual(TEXT("거절 시 슬롯 값 없음"), Slot, static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("인스턴스 포함 확인"), FDigetherInventoryRules::ContainsInstance(State, Pickaxe.InstanceId));

	// 같은 정의라도 다른 인스턴스는 별도 슬롯
	TestTrue(TEXT("같은 정의 다른 인스턴스 성공"), FDigetherInventoryRules::TryAdd(State, FDigetherItemInstance::MakeNew(TEXT("DA_Item_Pickaxe")), Slot) == EDigetherInventoryResult::Success);
	TestEqual(TEXT("두 번째 슬롯"), Slot, 1);

	// 가득 차면 거절하고 상태를 바꾸지 않음
	FDigetherInventoryRules::TryAdd(State, FDigetherItemInstance::MakeNew(TEXT("A")), Slot);
	FDigetherInventoryRules::TryAdd(State, FDigetherItemInstance::MakeNew(TEXT("B")), Slot);
	TestEqual(TEXT("네 번째 슬롯"), Slot, 3);
	const FDigetherInventoryState FullState = State;
	TestTrue(TEXT("가득 차면 거절"), FDigetherInventoryRules::TryAdd(State, FDigetherItemInstance::MakeNew(TEXT("C")), Slot) == EDigetherInventoryResult::Full);
	TestEqual(TEXT("거절해도 상태 그대로"), State.GeneralSlots[3].InstanceId, FullState.GeneralSlots[3].InstanceId);

	// 버리기: 슬롯을 비우고 꺼낸 인스턴스가 그대로 돌아옴
	FDigetherItemInstance Removed;
	TestTrue(TEXT("슬롯 1 꺼내기 성공"), FDigetherInventoryRules::TryRemove(State, 0, Removed) == EDigetherInventoryResult::Success);
	TestEqual(TEXT("꺼낸 인스턴스 보존"), Removed.InstanceId, Pickaxe.InstanceId);
	TestFalse(TEXT("꺼낸 슬롯은 빔"), State.GeneralSlots[0].IsSet());
	TestFalse(TEXT("꺼낸 뒤 포함하지 않음"), FDigetherInventoryRules::ContainsInstance(State, Pickaxe.InstanceId));
	TestTrue(TEXT("빈 슬롯 꺼내기 거절"), FDigetherInventoryRules::TryRemove(State, 0, Removed) == EDigetherInventoryResult::EmptySlot);
	TestTrue(TEXT("범위 밖 슬롯 거절"), FDigetherInventoryRules::TryRemove(State, 4, Removed) == EDigetherInventoryResult::InvalidSlot);
	TestTrue(TEXT("음수 슬롯 거절"), FDigetherInventoryRules::TryRemove(State, -1, Removed) == EDigetherInventoryResult::InvalidSlot);

	// 비운 자리가 다시 가장 앞 빈 슬롯이 됨
	TestTrue(TEXT("비운 자리에 다시 줍기"), FDigetherInventoryRules::TryAdd(State, Pickaxe, Slot) == EDigetherInventoryResult::Success);
	TestEqual(TEXT("비운 슬롯 재사용"), Slot, 0);

	// 손에 든 빈 슬롯이 있으면 거기에 먼저 넣음
	FDigetherItemInstance Second;
	FDigetherInventoryRules::TryRemove(State, 2, Second);
	TestTrue(TEXT("선호 슬롯이 비면 거기에 넣음"), FDigetherInventoryRules::TryAdd(State, Second, Slot, 2) == EDigetherInventoryResult::Success);
	TestEqual(TEXT("선호 슬롯 사용"), Slot, 2);
	FDigetherInventoryRules::TryRemove(State, 3, Removed);
	TestTrue(TEXT("선호 슬롯이 차 있으면 앞 빈 슬롯"), FDigetherInventoryRules::TryAdd(State, Removed, Slot, 0) == EDigetherInventoryResult::Success);
	TestEqual(TEXT("앞 빈 슬롯 사용"), Slot, 3);

	// 슬롯 선택: 빈 슬롯도 고를 수 있고 범위 밖은 거절
	TestTrue(TEXT("3번 슬롯 선택"), FDigetherInventoryRules::TrySelect(State, 2) == EDigetherInventoryResult::Success);
	TestEqual(TEXT("선택 반영"), State.SelectedSlot, 2);
	TestTrue(TEXT("범위 밖 선택 거절"), FDigetherInventoryRules::TrySelect(State, 4) == EDigetherInventoryResult::InvalidSlot);
	TestEqual(TEXT("거절해도 선택 그대로"), State.SelectedSlot, 2);
	FDigetherInventoryState BadSelection = FDigetherInventoryRules::MakeEmpty();
	BadSelection.SelectedSlot = -1;
	TestFalse(TEXT("선택 슬롯이 범위 밖이면 잘못된 상태"), FDigetherInventoryRules::IsValidState(BadSelection));

	// 깨진 상태는 모든 변경을 거절
	FDigetherInventoryState Broken;
	TestFalse(TEXT("슬롯 수가 다르면 잘못된 상태"), FDigetherInventoryRules::IsValidState(Broken));
	TestTrue(TEXT("잘못된 상태에는 줍기 거절"), FDigetherInventoryRules::TryAdd(Broken, Pickaxe, Slot) == EDigetherInventoryResult::InvalidState);
	FDigetherInventoryState Twin = FDigetherInventoryRules::MakeEmpty();
	Twin.GeneralSlots[0] = Pickaxe;
	Twin.GeneralSlots[1] = Pickaxe;
	TestFalse(TEXT("같은 인스턴스가 두 슬롯이면 잘못된 상태"), FDigetherInventoryRules::IsValidState(Twin));

	return true;
}

#endif
