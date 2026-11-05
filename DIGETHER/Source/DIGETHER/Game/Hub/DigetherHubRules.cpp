#include "Game/Hub/DigetherHubRules.h"

#include "Game/Economy/DigetherEconomyRules.h"

namespace
{
	bool AreStacksValid(const TArray<FDigetherHubItemStack>& Stacks)
	{
		if (Stacks.Num() > DigetherHubRules::MaxItemStacks)
		{
			return false;
		}
		TSet<FName> Seen;
		for (const FDigetherHubItemStack& Stack : Stacks)
		{
			if (!DigetherHubRules::IsIdentifierValid(Stack.ItemId) || Seen.Contains(Stack.ItemId)
				|| Stack.Count <= 0 || Stack.Count > DigetherHubRules::MaxStackCount)
			{
				return false;
			}
			Seen.Add(Stack.ItemId);
		}
		return true;
	}

	bool AddStack(TArray<FDigetherHubItemStack>& Stacks, FName ItemId, int32 Count)
	{
		for (FDigetherHubItemStack& Stack : Stacks)
		{
			if (Stack.ItemId == ItemId)
			{
				if (Count > DigetherHubRules::MaxStackCount - Stack.Count)
				{
					return false;
				}
				Stack.Count += Count;
				return true;
			}
		}
		if (Stacks.Num() >= DigetherHubRules::MaxItemStacks || Count > DigetherHubRules::MaxStackCount)
		{
			return false;
		}
		FDigetherHubItemStack& Added = Stacks.AddDefaulted_GetRef();
		Added.ItemId = ItemId;
		Added.Count = Count;
		return true;
	}

	FDigetherHubPlayerProgress* FindMutablePlayer(FDigetherHubProgress& Progress, FGuid PlayerId)
	{
		return Progress.Players.FindByPredicate([PlayerId](const FDigetherHubPlayerProgress& Player)
		{
			return Player.PlayerId == PlayerId;
		});
	}
}

bool DigetherHubRules::IsIdentifierValid(FName Id)
{
	if (Id.IsNone())
	{
		return false;
	}
	const FString Text = Id.ToString();
	if (Text.Len() > MaxIdentifierLength)
	{
		return false;
	}
	for (const TCHAR Character : Text)
	{
		if (FChar::IsWhitespace(Character) || FChar::IsControl(Character))
		{
			return false;
		}
	}
	return true;
}

bool DigetherHubRules::IsProgressStructurallyValid(const FDigetherHubProgress& Progress)
{
	if (!Progress.bInitialized)
	{
		return Progress.Players.IsEmpty() && Progress.UnlockedStageIds.IsEmpty()
			&& Progress.SelectedStageId.IsNone() && Progress.TotalOfferingCredits == 0;
	}
	if (Progress.Players.Num() > MaxProgressPlayers || Progress.UnlockedStageIds.IsEmpty()
		|| Progress.UnlockedStageIds.Num() > MaxUnlockedStages || Progress.TotalOfferingCredits < 0
		|| !Progress.UnlockedStageIds.Contains(Progress.SelectedStageId))
	{
		return false;
	}
	TSet<FName> Stages;
	for (FName Stage : Progress.UnlockedStageIds)
	{
		if (!IsIdentifierValid(Stage) || Stages.Contains(Stage))
		{
			return false;
		}
		Stages.Add(Stage);
	}
	TSet<FGuid> Players;
	for (const FDigetherHubPlayerProgress& Player : Progress.Players)
	{
		if (!Player.PlayerId.IsValid() || Players.Contains(Player.PlayerId)
			|| !AreStacksValid(Player.Inventory) || !AreStacksValid(Player.PendingDelivery))
		{
			return false;
		}
		Players.Add(Player.PlayerId);
	}
	return true;
}

int32 DigetherHubRules::GetItemCount(const TArray<FDigetherHubItemStack>& Stacks, FName ItemId)
{
	const FDigetherHubItemStack* Stack = Stacks.FindByPredicate([ItemId](const FDigetherHubItemStack& Item)
	{
		return Item.ItemId == ItemId;
	});
	return Stack ? Stack->Count : 0;
}

const FDigetherHubPlayerProgress* DigetherHubRules::FindPlayer(const FDigetherHubProgress& Progress, FGuid PlayerId)
{
	return Progress.Players.FindByPredicate([PlayerId](const FDigetherHubPlayerProgress& Player)
	{
		return Player.PlayerId == PlayerId;
	});
}

EDigetherHubResult DigetherHubRules::Purchase(FDigetherHubProgress& Progress, FGuid PlayerId,
	FDigetherWalletState& Wallet, const FDigetherHubOffer& Offer)
{
	if (!Progress.bInitialized || !IsProgressStructurallyValid(Progress) || !PlayerId.IsValid()
		|| !FDigetherEconomyRules::IsValidWalletState(Wallet) || !IsIdentifierValid(Offer.ItemId)
		|| Offer.PriceCredits <= 0 || Offer.Quantity <= 0 || Offer.Quantity > MaxStackCount)
	{
		return EDigetherHubResult::InvalidState;
	}
	if (Wallet.BalanceCredits < Offer.PriceCredits)
	{
		return EDigetherHubResult::InsufficientFunds;
	}
	// 배송 공간 확인 전 잔고·원본 진행 변경 방지
	FDigetherHubProgress Candidate = Progress;
	FDigetherHubPlayerProgress* Player = FindMutablePlayer(Candidate, PlayerId);
	if (!Player)
	{
		if (Candidate.Players.Num() >= MaxProgressPlayers)
		{
			return EDigetherHubResult::CapacityExceeded;
		}
		Player = &Candidate.Players.AddDefaulted_GetRef();
		Player->PlayerId = PlayerId;
	}
	if (!AddStack(Player->PendingDelivery, Offer.ItemId, Offer.Quantity))
	{
		return EDigetherHubResult::CapacityExceeded;
	}
	Wallet.BalanceCredits -= Offer.PriceCredits;
	Progress = MoveTemp(Candidate);
	return EDigetherHubResult::Success;
}

EDigetherHubResult DigetherHubRules::ConsiderTrialStarterGrant(FDigetherHubProgress& Progress, FGuid PlayerId,
	FDigetherWalletState& Wallet, int32 TrialCredits, int32& OutGrantedCredits)
{
	OutGrantedCredits = 0;
	if (!Progress.bInitialized || !IsProgressStructurallyValid(Progress) || !PlayerId.IsValid()
		|| !FDigetherEconomyRules::IsValidWalletState(Wallet) || TrialCredits < 0)
	{
		return EDigetherHubResult::InvalidState;
	}
	FDigetherHubProgress Candidate = Progress;
	FDigetherHubPlayerProgress* Player = FindMutablePlayer(Candidate, PlayerId);
	if (Player && Player->bStarterGrantConsidered)
	{
		return EDigetherHubResult::Success;
	}
	if (!Player)
	{
		if (Candidate.Players.Num() >= MaxProgressPlayers)
		{
			return EDigetherHubResult::CapacityExceeded;
		}
		Player = &Candidate.Players.AddDefaulted_GetRef();
		Player->PlayerId = PlayerId;
	}
	// 지급 여부와 무관한 최초 확인 기록, 재접속·재요청 중복 지급 방지
	Player->bStarterGrantConsidered = true;
	if (Wallet.BalanceCredits == 0)
	{
		OutGrantedCredits = TrialCredits;
		Wallet.BalanceCredits = TrialCredits;
	}
	Progress = MoveTemp(Candidate);
	return EDigetherHubResult::Success;
}

EDigetherHubResult DigetherHubRules::ClaimDelivery(FDigetherHubProgress& Progress, FGuid PlayerId)
{
	if (!Progress.bInitialized || !IsProgressStructurallyValid(Progress) || !PlayerId.IsValid())
	{
		return EDigetherHubResult::InvalidState;
	}
	FDigetherHubProgress Candidate = Progress;
	FDigetherHubPlayerProgress* Player = FindMutablePlayer(Candidate, PlayerId);
	if (!Player || Player->PendingDelivery.IsEmpty())
	{
		return EDigetherHubResult::NothingToClaim;
	}
	// 전체 품목 수령 성공 후 원본 일괄 반영
	for (const FDigetherHubItemStack& Stack : Player->PendingDelivery)
	{
		if (!AddStack(Player->Inventory, Stack.ItemId, Stack.Count))
		{
			return EDigetherHubResult::CapacityExceeded;
		}
	}
	Player->PendingDelivery.Reset();
	Progress = MoveTemp(Candidate);
	return EDigetherHubResult::Success;
}

EDigetherHubResult DigetherHubRules::UnlockStage(FDigetherHubProgress& Progress,
	FDigetherWalletState& Wallet, const FDigetherHubStage& Stage, bool bDestinationLocked)
{
	if (!Progress.bInitialized || !IsProgressStructurallyValid(Progress)
		|| !FDigetherEconomyRules::IsValidWalletState(Wallet) || !IsIdentifierValid(Stage.StageId)
		|| Stage.UnlockCostCredits < 0)
	{
		return EDigetherHubResult::InvalidState;
	}
	if (bDestinationLocked)
	{
		return EDigetherHubResult::DestinationLocked;
	}
	// 팀 공용 해금 재구매 차감 방지
	if (Progress.UnlockedStageIds.Contains(Stage.StageId))
	{
		return EDigetherHubResult::AlreadyUnlocked;
	}
	if (Progress.UnlockedStageIds.Num() >= MaxUnlockedStages)
	{
		return EDigetherHubResult::CapacityExceeded;
	}
	if (Wallet.BalanceCredits < Stage.UnlockCostCredits)
	{
		return EDigetherHubResult::InsufficientFunds;
	}
	Wallet.BalanceCredits -= Stage.UnlockCostCredits;
	Progress.UnlockedStageIds.Add(Stage.StageId);
	return EDigetherHubResult::Success;
}

EDigetherHubResult DigetherHubRules::SelectStage(FDigetherHubProgress& Progress, FName StageId, bool bDestinationLocked)
{
	if (!Progress.bInitialized || !IsProgressStructurallyValid(Progress) || !IsIdentifierValid(StageId))
	{
		return EDigetherHubResult::InvalidState;
	}
	if (bDestinationLocked)
	{
		return EDigetherHubResult::DestinationLocked;
	}
	if (!Progress.UnlockedStageIds.Contains(StageId))
	{
		return EDigetherHubResult::StageLocked;
	}
	Progress.SelectedStageId = StageId;
	return EDigetherHubResult::Success;
}

EDigetherHubResult DigetherHubRules::Offer(FDigetherHubProgress& Progress, FDigetherWalletState& Wallet, int32 Cost)
{
	if (!Progress.bInitialized || !IsProgressStructurallyValid(Progress)
		|| !FDigetherEconomyRules::IsValidWalletState(Wallet) || Cost <= 0)
	{
		return EDigetherHubResult::InvalidState;
	}
	if (Wallet.BalanceCredits < Cost)
	{
		return EDigetherHubResult::InsufficientFunds;
	}
	if (Cost > MAX_int32 - Progress.TotalOfferingCredits)
	{
		return EDigetherHubResult::CapacityExceeded;
	}
	Wallet.BalanceCredits -= Cost;
	Progress.TotalOfferingCredits += Cost;
	return EDigetherHubResult::Success;
}

bool DigetherHubRules::HasAppliedRequest(const TArray<FDigetherHubRequestKey>& Requests, FGuid PlayerId, FGuid RequestId)
{
	return Requests.ContainsByPredicate([PlayerId, RequestId](const FDigetherHubRequestKey& Request)
	{
		return Request.PlayerId == PlayerId && Request.RequestId == RequestId;
	});
}

void DigetherHubRules::RememberAppliedRequest(TArray<FDigetherHubRequestKey>& Requests, FGuid PlayerId, FGuid RequestId)
{
	if (!PlayerId.IsValid() || !RequestId.IsValid() || HasAppliedRequest(Requests, PlayerId, RequestId))
	{
		return;
	}
	if (Requests.Num() >= MaxRememberedRequests)
	{
		Requests.RemoveAt(0, Requests.Num() - MaxRememberedRequests + 1, EAllowShrinking::No);
	}
	Requests.Add({PlayerId, RequestId});
}
