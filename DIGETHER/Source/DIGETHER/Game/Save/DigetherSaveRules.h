#pragma once

#include "Game/Save/DigetherSaveTypes.h"

struct FUniqueNetIdRepl;

// 실패 시 모든 출력 인자를 유지한다. 파일·월드·전역 시각을 읽지 않는다.
namespace DigetherSaveRules
{
	DIGETHER_API EDigetherSaveResult MakeAuthenticatedPlayerKey(const FUniqueNetIdRepl& UniqueId,
		FString& OutPlayerKey);
	DIGETHER_API bool IsValidAuthenticatedPlayerKey(const FString& PlayerKey);
	DIGETHER_API EDigetherSaveResult ValidateSnapshot(const FDigetherSaveSnapshot& Snapshot);
	DIGETHER_API EDigetherSaveResult TryCaptureSnapshot(const FDigetherSaveRuntimeState& Runtime,
		FDigetherSaveSnapshot& OutSnapshot);
	DIGETHER_API EDigetherSaveResult TryRestoreSnapshot(const FDigetherSaveSnapshot& Snapshot,
		FDigetherSaveRuntimeState& OutRuntime);
	DIGETHER_API EDigetherSaveResult TryBuildSlot(const FDigetherSaveSnapshot& Snapshot,
		const FDigetherSaveSlotData* Existing, bool bExplicitNewGame, bool bCheckpoint,
		FDigetherSaveSlotData& OutSlot);
	DIGETHER_API EDigetherSaveResult SerializeSlot(const FDigetherSaveSlotData& Slot,
		TArray<uint8>& OutBytes);
	DIGETHER_API EDigetherSaveResult DeserializeSlot(const TArray<uint8>& Bytes,
		FDigetherSaveSlotData& OutSlot);
	DIGETHER_API EDigetherSaveResult SelectReadableSlot(EDigetherSaveResult PrimaryResult,
		EDigetherSaveResult BackupResult, bool& bOutUseBackup);
	DIGETHER_API EDigetherSaveResult CanWriteSlots(EDigetherSaveResult PrimaryResult,
		EDigetherSaveResult BackupResult, bool bExplicitNewGame);
	DIGETHER_API EDigetherSaveWriteStep AdvanceWriteStep(EDigetherSaveWriteStep Current, bool bSucceeded);
	// 유일한 유효본을 먼저 덮지 않는다. 주 슬롯이 무효이고 백업만 유효하면 주 슬롯부터 쓴다.
	DIGETHER_API bool ShouldWritePrimaryFirst(EDigetherSaveResult PrimaryResult, EDigetherSaveResult BackupResult);
	DIGETHER_API EDigetherSaveResult CanDepartWithCheckpoint(const FDigetherSaveSnapshot& Current,
		const FDigetherSaveSlotData& Slot);
	DIGETHER_API EDigetherSaveResult TryResolveReturningPlayer(const TArray<FDigetherSavePlayerRecord>& Players,
		const FUniqueNetIdRepl& UniqueId, FDigetherSavePlayerRecord& OutPlayer, bool& bOutSkipInitialGrant);
	DIGETHER_API EDigetherSaveResult TryApplyOfflineDeath(const FDigetherSavePlayerRecord& Player,
		EDigetherDeathCause Cause, int32 DeathLossPercent, FDigetherSavePlayerRecord& OutPlayer);
	DIGETHER_API EDigetherSaveResult TryApplySettlement(const FDigetherSaveSnapshot& Snapshot,
		const FGuid& RunId, const TArray<FDigetherPlayerSettlement>& Settlements,
		FDigetherSaveSnapshot& OutSnapshot);
}
