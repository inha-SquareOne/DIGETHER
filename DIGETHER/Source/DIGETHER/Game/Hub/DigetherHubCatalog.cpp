#include "Game/Hub/DigetherHubCatalog.h"

#include "Game/Hub/DigetherHubRules.h"
#include "Misc/DataValidation.h"

FPrimaryAssetId UDigetherHubCatalog::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("DigetherHubCatalog")), GetFName());
}

const FDigetherHubOffer* UDigetherHubCatalog::FindOffer(FName ItemId) const
{
	return Offers.FindByPredicate([ItemId](const FDigetherHubOffer& Offer) { return Offer.ItemId == ItemId; });
}

const FDigetherHubStage* UDigetherHubCatalog::FindStage(FName StageId) const
{
	return Stages.FindByPredicate([StageId](const FDigetherHubStage& Stage) { return Stage.StageId == StageId; });
}

bool UDigetherHubCatalog::IsCatalogValid() const
{
	// 선택지 3개인 시험 패널에서 구매 항목 누락 방지
	if (Offers.IsEmpty() || Offers.Num() > 3 || Stages.IsEmpty() || Stages.Num() > 3
		|| OfferingCostCredits <= 0 || TrialStarterCredits < 0
		|| !FMath::IsFinite(OfferingPulseSeconds) || OfferingPulseSeconds <= 0.f)
	{
		return false;
	}
	TSet<FName> OfferIds;
	for (const FDigetherHubOffer& Offer : Offers)
	{
		if (!DigetherHubRules::IsIdentifierValid(Offer.ItemId) || OfferIds.Contains(Offer.ItemId)
			|| Offer.DisplayName.IsEmpty() || Offer.PriceCredits <= 0 || Offer.Quantity <= 0
			|| Offer.Quantity > DigetherHubRules::MaxStackCount)
		{
			return false;
		}
		OfferIds.Add(Offer.ItemId);
	}
	TSet<FName> StageIds;
	bool bHasInitialStage = false;
	for (const FDigetherHubStage& Stage : Stages)
	{
		if (!DigetherHubRules::IsIdentifierValid(Stage.StageId) || StageIds.Contains(Stage.StageId)
			|| Stage.DisplayName.IsEmpty() || Stage.UnlockCostCredits < 0)
		{
			return false;
		}
		StageIds.Add(Stage.StageId);
		bHasInitialStage |= Stage.bInitiallyUnlocked;
	}
	return bHasInitialStage;
}

#if WITH_EDITOR
EDataValidationResult UDigetherHubCatalog::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	if (!IsCatalogValid())
	{
		Context.AddError(NSLOCTEXT("DigetherHub", "InvalidCatalog", "시설 카탈로그의 고유 ID·시험값·초기 목적지·표시명을 확인하세요."));
		return EDataValidationResult::Invalid;
	}
	return SuperResult == EDataValidationResult::Invalid ? SuperResult : EDataValidationResult::Valid;
}
#endif
