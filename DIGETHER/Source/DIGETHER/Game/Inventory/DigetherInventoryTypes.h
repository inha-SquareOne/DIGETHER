#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "DigetherInventoryTypes.generated.h"

// 아이템 인스턴스: 같은 정의의 아이템이라도 InstanceId로 구분함
// 한 인스턴스는 월드(ADigetherWorldItemActor)와 인벤토리 슬롯 중 한 곳에만 존재함(줍기·버리기는 이동이며 복사가 아님)
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherItemInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Inventory")
	FGuid InstanceId;

	// UDigetherItemDefinition 에셋 이름(Asset Manager의 DigetherItem 타입 PrimaryAssetId 이름)
	UPROPERTY(BlueprintReadOnly, Category="Digether|Inventory")
	FName DefinitionId;

	// 인스턴스가 채워졌는지(비어 있는 슬롯과 구분함)
	bool IsSet() const { return InstanceId.IsValid() && !DefinitionId.IsNone(); }

	// 새 인스턴스를 만듦(시험 지급·시험 생성 경로용, 정식 생성 경로는 서버가 호출함)
	static FDigetherItemInstance MakeNew(const FName InDefinitionId)
	{
		FDigetherItemInstance Instance;
		Instance.InstanceId = FGuid::NewGuid();
		Instance.DefinitionId = InDefinitionId;
		return Instance;
	}
};

// 인벤토리 슬롯 상태(값 타입), 일반 슬롯만 있음
// 헤드랜턴·히든·시체 전용 슬롯은 해당 기능(P2)에서 추가함
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherInventoryState
{
	GENERATED_BODY()

	// 길이는 항상 FDigetherInventoryRules::GeneralSlotCount, 빈 슬롯은 IsSet()이 거짓
	UPROPERTY(BlueprintReadOnly, Category="Digether|Inventory")
	TArray<FDigetherItemInstance> GeneralSlots;

	// 손에 든 슬롯(0부터, 1~4 키), 빈 슬롯을 골라도 됨(빈손)
	UPROPERTY(BlueprintReadOnly, Category="Digether|Inventory")
	int32 SelectedSlot = 0;

	// 개인 시작 장비를 이미 받았는지, 재접속·월드 이동 인계 때 다시 주지 않으려고 상태와 함께 옮김
	UPROPERTY()
	bool bStartingItemsGranted = false;
};
