#pragma once

#include "Game/Save/DigetherSaveSlotTypes.h"
#include "Game/Save/DigetherSaveTypes.h"

struct FUniqueNetIdRepl;

// 실패 시 모든 출력 인자 유지, 파일·월드·전역 시각 비의존
namespace DigetherSaveRules
{
	DIGETHER_API EDigetherSaveResult MakeAuthenticatedPlayerKey(const FUniqueNetIdRepl& UniqueId,
		FString& OutPlayerKey);
	DIGETHER_API bool IsValidAuthenticatedPlayerKey(const FString& PlayerKey);
	// 결과 크레딧용 표시 이름을 저장 가능한 형태로 변환(제어 문자 제거·길이 상한), 화면 표시 이름은 그대로
	DIGETHER_API FString MakeStoredDisplayName(const FString& PlayerName);
	DIGETHER_API EDigetherSaveResult ValidateSnapshot(const FDigetherSaveSnapshot& Snapshot);
	DIGETHER_API EDigetherSaveResult TryCaptureSnapshot(const FDigetherSaveRuntimeState& Runtime,
		FDigetherSaveSnapshot& OutSnapshot);
	DIGETHER_API EDigetherSaveResult TryRestoreSnapshot(const FDigetherSaveSnapshot& Snapshot,
		FDigetherSaveRuntimeState& OutRuntime);
	DIGETHER_API EDigetherSaveResult TryBuildSlot(const FDigetherSaveSnapshot& Snapshot,
		const FDigetherSaveSlotData* Existing, bool bExplicitNewGame, FDigetherSaveSlotData& OutSlot);
	DIGETHER_API EDigetherSaveResult SerializeSlot(const FDigetherSaveSlotData& Slot,
		TArray<uint8>& OutBytes);
	DIGETHER_API EDigetherSaveResult DeserializeSlot(const TArray<uint8>& Bytes,
		FDigetherSaveSlotData& OutSlot);
	DIGETHER_API EDigetherSaveResult SelectReadableSlot(EDigetherSaveResult PrimaryResult,
		EDigetherSaveResult BackupResult, bool& bOutUseBackup);
	DIGETHER_API EDigetherSaveResult CanWriteSlots(EDigetherSaveResult PrimaryResult,
		EDigetherSaveResult BackupResult, bool bExplicitNewGame);
	DIGETHER_API EDigetherSaveWriteStep AdvanceWriteStep(EDigetherSaveWriteStep Current, bool bSucceeded);
	// 유일한 유효본을 먼저 덮지 않음, 주 슬롯 무효·백업만 유효하면 주 슬롯부터
	DIGETHER_API bool ShouldWritePrimaryFirst(EDigetherSaveResult PrimaryResult, EDigetherSaveResult BackupResult);
	// 진행 슬롯 번호가 0..MaxProgressSlots-1인지
	DIGETHER_API bool IsValidSlotIndex(int32 SlotIndex);
	// 한 슬롯 읽기 결과(DigetherSaveStorage::Read)로 목록 요약 생성, Slot은 ReadResult가 Success일 때만 읽음
	DIGETHER_API FDigetherSaveSlotSummary MakeSlotSummary(int32 SlotIndex, EDigetherSaveResult ReadResult,
		const FDigetherSaveSlotData& Slot);
	// 게임 맵 시작 시 새 게임 여부, 개발용 URL 옵션(DigetherNewGame)은 선택과 무관하게 새 게임
	DIGETHER_API bool ShouldStartNewGame(const FDigetherSaveSlotSelection& Selection, bool bNewGameUrlOption);
	// 콘솔 RoomHost 저장 옵션 해석, 빈 값은 슬롯 0 이어하기(기본), "new"·"newN"은 슬롯 N 새 게임,
	// "load"·"loadN"은 슬롯 N 이어하기(대소문자 무시, N 생략 시 0), 실패 시 출력 인자 유지
	DIGETHER_API bool TryParseSlotOption(const FString& Option, FDigetherSaveSlotSelection& OutSelection);
	DIGETHER_API EDigetherSaveResult TryResolveReturningPlayer(const TArray<FDigetherSavePlayerRecord>& Players,
		const FUniqueNetIdRepl& UniqueId, FDigetherSavePlayerRecord& OutPlayer, bool& bOutSkipInitialGrant);
	DIGETHER_API EDigetherSaveResult TryApplyOfflineDeath(const FDigetherSavePlayerRecord& Player,
		EDigetherDeathCause Cause, int32 DeathLossPercent, FDigetherSavePlayerRecord& OutPlayer);
	DIGETHER_API EDigetherSaveResult TryApplySettlement(const FDigetherSaveSnapshot& Snapshot,
		const FGuid& RunId, const TArray<FDigetherPlayerSettlement>& Settlements,
		FDigetherSaveSnapshot& OutSnapshot);
}
