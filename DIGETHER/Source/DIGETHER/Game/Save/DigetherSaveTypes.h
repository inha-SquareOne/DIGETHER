#pragma once

#include "CoreMinimal.h"
#include "Game/Data/DigetherTuningProfile.h"
#include "Game/Economy/DigetherEconomyTuning.h"
#include "Game/Economy/DigetherEconomyTypes.h"
#include "Game/Life/DigetherLifeTypes.h"
#include "Game/Run/DigetherRunRuleTuning.h"
#include "Game/Run/DigetherRunTypes.h"

enum class EDigetherSaveResult : uint8
{
	Success,
	NotAuthority,
	HostIdentityMissing,
	HostIdentityMismatch,
	NotFound,
	UnsupportedVersion,
	CorruptData,
	InvalidState,
	UnsafePhase,
	CheckpointMissing,
	CheckpointMismatch,
	AlreadySettled,
	PlayerNotFound,
	PlayerIdentityMismatch,
	UnsupportedIdentity,
	IoFailure
};

enum class EDigetherSaveWriteStep : uint8
{
	WriteBackup,
	VerifyBackup,
	WritePrimary,
	VerifyPrimary,
	Complete,
	Failed
};

// 인증 ID는 서버 전용이며 이름·IP·공개 PlayerId와 바꾸어 쓰지 않는다.
struct DIGETHER_API FDigetherSavePlayerRecord
{
	FString AuthenticatedPlayerKey;
	FGuid GameplayPlayerId;
	FDigetherWalletState Wallet;
	FDigetherLifeComponentState Life;
	// 출발 복원은 저장 당시의 거점 배치를 되돌린다. 값이 없으면 연결층이 시작 지점을 고른다.
	bool bHasHubTransform = false;
	FTransform HubTransform = FTransform::Identity;
};

// 월드·Pawn·Controller 참조를 포함하지 않아 미접속 참가자에게도 같은 규칙을 적용한다.
struct DIGETHER_API FDigetherSaveRuntimeState
{
	FString HostAuthenticatedPlayerKey;
	FGuid GameId;
	FGuid RunId;
	FDigetherQuotaPublicState Quota;
	FDigetherRunPublicState RunPublicState;
	FDigetherRunTuning RunTuning;
	FDigetherEconomyTuning EconomyTuning;
	FDigetherRunRuleTuning RunRuleTuning;
	TArray<FDigetherSavePlayerRecord> Players;
	TArray<FGuid> CompletedSettlementRunIds;
};

// v2 타입이나 UObject 직렬화에 연결하지 않는 v3 값 스키마다.
struct DIGETHER_API FDigetherSaveSnapshot : public FDigetherSaveRuntimeState
{
	int32 SchemaVersion = 3;
};

struct DIGETHER_API FDigetherSaveSlotData
{
	FDigetherSaveSnapshot Progress;
	FDigetherSaveSnapshot DepartureCheckpoint;
	bool bHasDepartureCheckpoint = false;
};
