#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Inventory/DigetherInventoryRules.h"
#include "DigetherInventoryComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnDigetherInventoryChanged);

// 플레이어 소지품(일반 슬롯), PlayerState에 붙어 Pawn이 바뀌어도 유지됨
// 서버가 원본이고 슬롯 내용은 소유자에게만 복제함(다른 플레이어는 소지품을 알 수 없음)
// 변경은 서버에서만 함: 줍기·버리기는 이 컴포넌트와 월드 아이템 사이의 이동이며 한 인스턴스가 두 곳에 남지 않게 호출자가 순서를 지킴
UCLASS(ClassGroup=(Digether))
class DIGETHER_API UDigetherInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버 전용: 손에 든 슬롯이 비어 있으면 거기에, 아니면 앞의 빈 슬롯에 넣음, 실패하면 아무것도 바꾸지 않음
	EDigetherInventoryResult TryAddItem(const FDigetherItemInstance& Item, int32& OutSlotIndex);

	// 서버 전용: 손에 들 슬롯을 고름(빈 슬롯이면 빈손)
	EDigetherInventoryResult TrySelectSlot(int32 SlotIndex);

	// 서버 전용: 개인 시작 장비를 아직 받지 않았으면 넣고 받은 것으로 기록함, 이미 받았으면 아무것도 하지 않음
	// 칸이 모자라 못 넣은 정의는 건너뜀
	void GrantStartingItemsOnce(const TArray<FName>& DefinitionIds);

	// 서버 전용: 슬롯을 비우고 꺼냄
	EDigetherInventoryResult TryRemoveSlot(int32 SlotIndex, FDigetherItemInstance& OutItem);

	// 소유자 화면과 서버에서만 의미 있음
	const FDigetherInventoryState& GetState() const { return State; }
	int32 GetSelectedSlot() const { return State.SelectedSlot; }
	// 손에 든 슬롯의 아이템, 비어 있으면 IsSet()이 거짓
	const FDigetherItemInstance& GetSelectedItem() const { return State.GeneralSlots[State.SelectedSlot]; }
	bool IsFull() const;

	// 저장·월드 이동 인계용(서버 전용), 규칙에 맞지 않는 값은 거절함
	FDigetherInventoryState CaptureState() const { return State; }
	bool RestoreState(const FDigetherInventoryState& NewState, bool bNotify = true);

	// 서버는 변경 직후, 소유 클라이언트는 복제 수신 후 호출됨(HUD·표시용)
	FOnDigetherInventoryChanged OnInventoryChanged;

private:
	UFUNCTION()
	void OnRep_State();

	UPROPERTY(ReplicatedUsing=OnRep_State)
	FDigetherInventoryState State;
};
