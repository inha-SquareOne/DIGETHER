#pragma once

#include "CoreMinimal.h"
#include "Game/Economy/DigetherEconomyTypes.h"
#include "Game/Hub/DigetherHubCatalog.h"
#include "Game/Hub/DigetherHubTypes.h"

enum class EDigetherHubResult : uint8
{
	Success,
	InvalidState,
	InsufficientFunds,
	CapacityExceeded,
	NothingToClaim,
	AlreadyUnlocked,
	StageLocked,
	DestinationLocked
};

struct FDigetherHubRequestKey
{
	FGuid PlayerId;
	FGuid RequestId;
};

// 월드·복제 상태와 분리한 거점 거래·저장 검증
namespace DigetherHubRules
{
	// 저장 입력·수령 수량·중복 요청 기록의 상한
	inline constexpr int32 MaxProgressPlayers = 64;
	inline constexpr int32 MaxItemStacks = 32;
	inline constexpr int32 MaxUnlockedStages = 32;
	inline constexpr int32 MaxStackCount = 1000;
	inline constexpr int32 MaxIdentifierLength = 64;
	inline constexpr int32 MaxRememberedRequests = 64;

	DIGETHER_API bool IsIdentifierValid(FName Id);
	DIGETHER_API bool IsProgressStructurallyValid(const FDigetherHubProgress& Progress);
	DIGETHER_API int32 GetItemCount(const TArray<FDigetherHubItemStack>& Stacks, FName ItemId);
	DIGETHER_API const FDigetherHubPlayerProgress* FindPlayer(const FDigetherHubProgress& Progress, FGuid PlayerId);
	DIGETHER_API EDigetherHubResult ConsiderTrialStarterGrant(FDigetherHubProgress& Progress, FGuid PlayerId,
		FDigetherWalletState& Wallet, int32 TrialCredits, int32& OutGrantedCredits);
	DIGETHER_API EDigetherHubResult Purchase(FDigetherHubProgress& Progress, FGuid PlayerId,
		FDigetherWalletState& Wallet, const FDigetherHubOffer& Offer);
	DIGETHER_API EDigetherHubResult ClaimDelivery(FDigetherHubProgress& Progress, FGuid PlayerId);
	DIGETHER_API EDigetherHubResult UnlockStage(FDigetherHubProgress& Progress,
		FDigetherWalletState& Wallet, const FDigetherHubStage& Stage, bool bDestinationLocked);
	DIGETHER_API EDigetherHubResult SelectStage(FDigetherHubProgress& Progress, FName StageId, bool bDestinationLocked);
	DIGETHER_API EDigetherHubResult Offer(FDigetherHubProgress& Progress, FDigetherWalletState& Wallet, int32 Cost);
	DIGETHER_API bool HasAppliedRequest(const TArray<FDigetherHubRequestKey>& Requests, FGuid PlayerId, FGuid RequestId);
	DIGETHER_API void RememberAppliedRequest(TArray<FDigetherHubRequestKey>& Requests, FGuid PlayerId, FGuid RequestId);
}
