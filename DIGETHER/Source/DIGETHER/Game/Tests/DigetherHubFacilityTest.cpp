#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Hub/DigetherHubRules.h"
#include "Misc/AutomationTest.h"

namespace
{
	FDigetherHubProgress MakeHubProgress()
	{
		FDigetherHubProgress Progress;
		Progress.bInitialized = true;
		Progress.UnlockedStageIds.Add(TEXT("MineFree"));
		Progress.SelectedStageId = TEXT("MineFree");
		return Progress;
	}

	FDigetherHubOffer MakeOffer()
	{
		FDigetherHubOffer Offer;
		Offer.ItemId = TEXT("WoodBundle");
		Offer.DisplayName = FText::FromString(TEXT("Wood bundle"));
		Offer.PriceCredits = 10;
		Offer.Quantity = 1;
		return Offer;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherHubPurchaseDeliveryTest,
	"DIGETHER.Game.Hub.PurchaseDelivery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherHubPurchaseDeliveryTest::RunTest(const FString& Parameters)
{
	FDigetherHubProgress Progress = MakeHubProgress();
	const FGuid Buyer = FGuid::NewGuid();
	const FGuid Other = FGuid::NewGuid();
	FDigetherWalletState Wallet;
	Wallet.BalanceCredits = 9;
	const FDigetherHubOffer Offer = MakeOffer();
	TestTrue(TEXT("Insufficient funds rejects purchase"), DigetherHubRules::Purchase(Progress, Buyer, Wallet, Offer)
		== EDigetherHubResult::InsufficientFunds);
	TestEqual(TEXT("Failed purchase preserves balance"), Wallet.BalanceCredits, 9);
	TestTrue(TEXT("Failed purchase creates no delivery/player record"), Progress.Players.IsEmpty());
	Wallet.BalanceCredits = 25;
	TestTrue(TEXT("Purchase succeeds"), DigetherHubRules::Purchase(Progress, Buyer, Wallet, Offer) == EDigetherHubResult::Success);
	TestEqual(TEXT("Server price debited once"), Wallet.BalanceCredits, 15);
	const FDigetherHubPlayerProgress* Player = DigetherHubRules::FindPlayer(Progress, Buyer);
	TestNotNull(TEXT("Buyer delivery record exists"), Player);
	if (!Player) return false;
	TestEqual(TEXT("Purchase is pending, not already received"), DigetherHubRules::GetItemCount(Player->PendingDelivery, Offer.ItemId), 1);
	TestTrue(TEXT("Unreceived inventory empty"), Player->Inventory.IsEmpty());
	TestTrue(TEXT("Other player cannot claim buyer delivery"), DigetherHubRules::ClaimDelivery(Progress, Other)
		== EDigetherHubResult::NothingToClaim);
	TestTrue(TEXT("Owner claims delivery"), DigetherHubRules::ClaimDelivery(Progress, Buyer) == EDigetherHubResult::Success);
	Player = DigetherHubRules::FindPlayer(Progress, Buyer);
	TestEqual(TEXT("Item entered own inventory"), DigetherHubRules::GetItemCount(Player->Inventory, Offer.ItemId), 1);
	TestTrue(TEXT("Pending queue cleared"), Player->PendingDelivery.IsEmpty());
	TestTrue(TEXT("Repeated claim does not duplicate items"), DigetherHubRules::ClaimDelivery(Progress, Buyer)
		== EDigetherHubResult::NothingToClaim);
	Progress.Players[0].PendingDelivery.Add({Offer.ItemId, DigetherHubRules::MaxStackCount});
	TestTrue(TEXT("Pending stack overflow rejects whole purchase"), DigetherHubRules::Purchase(Progress, Buyer, Wallet, Offer)
		== EDigetherHubResult::CapacityExceeded);
	TestEqual(TEXT("Overflow does not charge wallet"), Wallet.BalanceCredits, 15);
	TestTrue(TEXT("Claim overflow leaves queue unchanged"), DigetherHubRules::ClaimDelivery(Progress, Buyer)
		== EDigetherHubResult::CapacityExceeded);
	TestEqual(TEXT("Overflow leaves owned count unchanged"), Progress.Players[0].Inventory[0].Count, 1);
	TestEqual(TEXT("Overflow preserves pending count"), Progress.Players[0].PendingDelivery[0].Count, DigetherHubRules::MaxStackCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherHubSharedStageTest,
	"DIGETHER.Game.Hub.SharedStage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherHubSharedStageTest::RunTest(const FString& Parameters)
{
	FDigetherHubProgress Progress = MakeHubProgress();
	FDigetherHubStage Stage;
	Stage.StageId = TEXT("Ruins");
	Stage.UnlockCostCredits = 100;
	FDigetherWalletState FirstWallet;
	FirstWallet.BalanceCredits = 150;
	FDigetherWalletState SecondWallet;
	SecondWallet.BalanceCredits = 500;
	TestTrue(TEXT("Locked stage cannot be selected"), DigetherHubRules::SelectStage(Progress, Stage.StageId, false)
		== EDigetherHubResult::StageLocked);
	TestTrue(TEXT("First requester unlocks shared destination"), DigetherHubRules::UnlockStage(Progress, FirstWallet, Stage, false)
		== EDigetherHubResult::Success);
	TestEqual(TEXT("First requester pays"), FirstWallet.BalanceCredits, 50);
	TestTrue(TEXT("Second concurrent requester sees already-unlocked result"), DigetherHubRules::UnlockStage(Progress, SecondWallet, Stage, false)
		== EDigetherHubResult::AlreadyUnlocked);
	TestEqual(TEXT("Second requester never pays twice"), SecondWallet.BalanceCredits, 500);
	TestTrue(TEXT("Any player can select unlocked destination"), DigetherHubRules::SelectStage(Progress, Stage.StageId, false)
		== EDigetherHubResult::Success);
	TestEqual(TEXT("Selection updated"), Progress.SelectedStageId, Stage.StageId);
	TestTrue(TEXT("Holding departure lever locks selection"), DigetherHubRules::SelectStage(Progress, TEXT("MineFree"), true)
		== EDigetherHubResult::DestinationLocked);
	TestEqual(TEXT("Rejected selection preserves destination"), Progress.SelectedStageId, Stage.StageId);
	Stage.StageId = TEXT("Crystal");
	TestTrue(TEXT("Departure lock also blocks unlock transaction"), DigetherHubRules::UnlockStage(Progress, SecondWallet, Stage, true)
		== EDigetherHubResult::DestinationLocked);
	TestEqual(TEXT("Locked unlock preserves wallet"), SecondWallet.BalanceCredits, 500);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherHubStarterGrantTest,
	"DIGETHER.Game.Hub.StarterGrantOnce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherHubStarterGrantTest::RunTest(const FString& Parameters)
{
	FDigetherHubProgress Progress = MakeHubProgress();
	const FGuid PlayerId = FGuid::NewGuid();
	FDigetherWalletState Wallet;
	int32 Granted = -1;
	TestTrue(TEXT("Empty wallet receives one trial grant"), DigetherHubRules::ConsiderTrialStarterGrant(Progress, PlayerId, Wallet, 500, Granted)
		== EDigetherHubResult::Success);
	TestEqual(TEXT("Grant amount"), Granted, 500);
	TestEqual(TEXT("Wallet after grant"), Wallet.BalanceCredits, 500);
	Wallet.BalanceCredits = 0;
	TestTrue(TEXT("Reopening after spending does not grant again"), DigetherHubRules::ConsiderTrialStarterGrant(Progress, PlayerId, Wallet, 500, Granted)
		== EDigetherHubResult::Success);
	TestEqual(TEXT("Repeated grant amount is zero"), Granted, 0);
	TestEqual(TEXT("Spent wallet stays zero"), Wallet.BalanceCredits, 0);
	FDigetherHubProgress Restored = Progress;
	TestTrue(TEXT("Saved considered flag blocks reconnect/restore grant"), DigetherHubRules::ConsiderTrialStarterGrant(Restored, PlayerId, Wallet, 500, Granted)
		== EDigetherHubResult::Success);
	TestEqual(TEXT("Restored grant stays zero"), Granted, 0);
	const FGuid ExistingPlayerId = FGuid::NewGuid();
	Wallet.BalanceCredits = 99;
	TestTrue(TEXT("Existing nonzero wallet preserved"), DigetherHubRules::ConsiderTrialStarterGrant(Progress, ExistingPlayerId, Wallet, 500, Granted)
		== EDigetherHubResult::Success);
	TestEqual(TEXT("Existing balance not replaced"), Wallet.BalanceCredits, 99);
	TestEqual(TEXT("Existing player gets no grant"), Granted, 0);
	Wallet.BalanceCredits = 0;
	DigetherHubRules::ConsiderTrialStarterGrant(Progress, ExistingPlayerId, Wallet, 500, Granted);
	TestEqual(TEXT("Later empty existing wallet cannot claim grant"), Granted, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherHubProgressAndReplayTest,
	"DIGETHER.Game.Hub.ProgressAndReplay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherHubProgressAndReplayTest::RunTest(const FString& Parameters)
{
	FDigetherHubProgress Legacy;
	TestTrue(TEXT("Absent legacy extension is valid"), DigetherHubRules::IsProgressStructurallyValid(Legacy));
	Legacy.TotalOfferingCredits = 1;
	TestFalse(TEXT("Uninitialized extension cannot hide progress"), DigetherHubRules::IsProgressStructurallyValid(Legacy));
	FDigetherHubProgress Progress = MakeHubProgress();
	TestTrue(TEXT("Initialized default progress valid"), DigetherHubRules::IsProgressStructurallyValid(Progress));
	Progress.UnlockedStageIds.Add(TEXT("MineFree"));
	TestFalse(TEXT("Duplicate stage IDs invalid"), DigetherHubRules::IsProgressStructurallyValid(Progress));
	Progress = MakeHubProgress();
	Progress.SelectedStageId = TEXT("UnknownLocked");
	TestFalse(TEXT("Selected stage must be unlocked"), DigetherHubRules::IsProgressStructurallyValid(Progress));
	TestFalse(TEXT("Whitespace identifiers invalid"), DigetherHubRules::IsIdentifierValid(TEXT("bad id")));
	Progress = MakeHubProgress();
	Progress.TotalOfferingCredits = MAX_int32;
	FDigetherWalletState Wallet;
	Wallet.BalanceCredits = 100;
	TestTrue(TEXT("Offering overflow rejected"), DigetherHubRules::Offer(Progress, Wallet, 5) == EDigetherHubResult::CapacityExceeded);
	TestEqual(TEXT("Overflow preserves balance"), Wallet.BalanceCredits, 100);
	const FGuid Owner = FGuid::NewGuid();
	const FGuid Other = FGuid::NewGuid();
	const FGuid Request = FGuid::NewGuid();
	TArray<FDigetherHubRequestKey> Requests;
	TestFalse(TEXT("New transaction not cached"), DigetherHubRules::HasAppliedRequest(Requests, Owner, Request));
	DigetherHubRules::RememberAppliedRequest(Requests, Owner, Request);
	TestTrue(TEXT("Same owner replay is cached"), DigetherHubRules::HasAppliedRequest(Requests, Owner, Request));
	TestFalse(TEXT("Other owner's same token is separate"), DigetherHubRules::HasAppliedRequest(Requests, Other, Request));
	DigetherHubRules::RememberAppliedRequest(Requests, Owner, Request);
	TestEqual(TEXT("Replay does not add another cache entry"), Requests.Num(), 1);
	for (int32 Index = 0; Index < 100; ++Index)
	{
		DigetherHubRules::RememberAppliedRequest(Requests, Owner, FGuid::NewGuid());
	}
	TestEqual(TEXT("Request cache memory bounded"), Requests.Num(), DigetherHubRules::MaxRememberedRequests);
	return true;
}

#endif
