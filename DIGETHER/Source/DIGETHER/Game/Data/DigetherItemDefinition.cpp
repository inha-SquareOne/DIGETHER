#include "Game/Data/DigetherItemDefinition.h"

#include "Game/Core/DigetherGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "DigetherItemDefinition"

const FPrimaryAssetType UDigetherItemDefinition::PrimaryAssetType(TEXT("DigetherItem"));

FPrimaryAssetId UDigetherItemDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, GetFName());
}

bool UDigetherItemDefinition::IsCargo() const
{
	return ItemType.MatchesTagExact(TAG_Item_Type_Cargo);
}

bool UDigetherItemDefinition::IsEquipment() const
{
	return ItemType.MatchesTagExact(TAG_Item_Type_Equipment);
}

bool UDigetherItemDefinition::HasTrait(const FGameplayTag& Trait) const
{
	return Traits.HasTagExact(Trait);
}

bool UDigetherItemDefinition::CollectProblems(TArray<FText>& OutProblems) const
{
	const int32 Before = OutProblems.Num();
	if (!IsCargo() && !IsEquipment())
	{
		OutProblems.Add(LOCTEXT("InvalidType", "ItemType은 Item.Type.Cargo 또는 Item.Type.Equipment여야 합니다."));
	}
	if (ValueCredits < 0)
	{
		OutProblems.Add(LOCTEXT("NegativeValue", "ValueCredits는 0 이상이어야 합니다."));
	}
	if (IsEquipment() && ValueCredits != 0)
	{
		OutProblems.Add(LOCTEXT("EquipmentValue", "장비는 정산하지 않으므로 ValueCredits가 0이어야 합니다."));
	}
	if (!(WeightKg >= 0.f))
	{
		OutProblems.Add(LOCTEXT("NegativeWeight", "WeightKg는 0 이상이어야 합니다."));
	}
	return OutProblems.Num() == Before;
}

#if WITH_EDITOR
EDataValidationResult UDigetherItemDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Result = Super::IsDataValid(Context);
	TArray<FText> Problems;
	if (!CollectProblems(Problems))
	{
		for (const FText& Problem : Problems)
		{
			Context.AddError(Problem);
		}
		return EDataValidationResult::Invalid;
	}
	return Result == EDataValidationResult::Invalid ? Result : EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
