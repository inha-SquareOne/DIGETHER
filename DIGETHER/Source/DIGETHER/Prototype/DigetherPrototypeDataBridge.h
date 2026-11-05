#pragma once

#include "CoreMinimal.h"
#include "DigetherPrototypeTypes.h"
#include "Game/Core/DigetherGameplayTags.h"
#include "Game/Data/DigetherItemDefinition.h"

// 코드 구조 계획 P1의 연결층. P2에서 FPrototypeItem이 정의 ID를 가진 인스턴스로 바뀌면 이 파일을 지운다.
namespace DigetherPrototypeData
{
	inline FPrototypeItem MakeItem(const UDigetherItemDefinition& Definition, const FGuid& Id)
	{
		FPrototypeItem Item;
		Item.Id = Id;
		Item.Value = Definition.ValueCredits;
		Item.WeightKg = Definition.WeightKg;
		Item.bEquipment = Definition.IsEquipment();
		Item.bTwoHanded = Definition.HasTrait(TAG_Item_Trait_TwoHanded);
		Item.bEmergencyDevice = Definition.HasTrait(TAG_Item_Trait_EmergencyDevice);
		return Item;
	}
}
