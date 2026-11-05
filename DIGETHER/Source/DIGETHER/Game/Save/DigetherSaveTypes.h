#pragma once

#include "CoreMinimal.h"
#include "Game/Data/DigetherTuningProfile.h"
#include "Game/Economy/DigetherEconomyTuning.h"
#include "Game/Economy/DigetherEconomyTypes.h"
#include "Game/Hub/DigetherHubTypes.h"
#include "Game/Life/DigetherLifeTypes.h"
#include "Game/Run/DigetherGameOverTypes.h"
#include "Game/Run/DigetherRunRuleTuning.h"
#include "Game/Run/DigetherRunTypes.h"
#include "Game/Save/DigetherSaveTuning.h"

// 슬롯 이름(DigetherSaveTuning::SlotNameVersion)과 런타임 스냅샷 스키마는 v3를 유지한다.
// 새 wire는 구 실행 파일의 미래 버전 보호를 받는다(구 reader는 모르는 첫 버전을 UnsupportedVersion으로 보존).
namespace DigetherSaveWire
{
	// v3: 기본 본문만. Hub 진행이 없는 슬롯을 2026-11-05 이전 실행 파일이 썼다.
	inline constexpr int32 LegacyVersion = 3;
	// v4: v3 본문 + Hub 확장(초기화된 Hub 진행 필수).
	inline constexpr int32 HubVersion = 4;
	// v5: v3 본문 + Hub 확장(항상, 비초기화 허용) + 진행 확장(전체 일차·저장 시각). 현재 쓰는 형식이다.
	// 진행 확장 v2(2026-11-05)는 결과 통계(플레이어 누적 획득·복구·표시 이름·참가 순서)와 게임 결과 기록을 덧붙인다.
	// 진행 확장 v1 파일은 그 값을 0·빈 값으로 읽는다. v2를 모르는 실행 파일은 확장 미래 버전으로 보고 덮어쓰지 않는다.
	inline constexpr int32 DayVersion = 5;
	inline constexpr int32 LatestVersion = DayVersion;
}

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
	AlreadySettled,
	PlayerNotFound,
	PlayerIdentityMismatch,
	UnsupportedIdentity,
	IoFailure
};

// 로그·테스트 메시지용 열거자 이름이다. 화면 문구나 저장 형식에 쓰지 않는다.
DIGETHER_API const TCHAR* LexToString(EDigetherSaveResult Result);

// 단계 이름의 Backup은 "먼저 쓰는 슬롯", Primary는 "나중에 쓰는 슬롯"이다.
// 실제 슬롯 순서는 DigetherSaveRules::ShouldWritePrimaryFirst가 정한다.
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
	// 불러오기·공통 로딩 실패 복귀는 기록 당시의 거점 배치를 되돌린다. 값이 없으면 연결층이 시작 지점을 고른다.
	bool bHasHubTransform = false;
	FTransform HubTransform = FTransform::Identity;
	// 결과 크레딧 표시 이름이다(기록 당시 PlayerState 이름). 진행 확장 v2부터 저장하며 이전 파일은 빈 값이다.
	FString DisplayName;
	// 이 게임의 방 참가 순서(1부터)다. 결과 크레딧 블록 순서에 쓴다. 0은 알 수 없음(이전 파일)이다.
	int32 JoinOrder = 0;
};

// 목표를 넘겨 정산기 앞 바닥에 남은 금화 더미 하나다(기획 S24·T79 이어하기 복원).
// Hub 확장 v2에만 저장한다. Hub 진행이 없는 맵(구 v3 wire)에서는 저장되지 않는다.
struct DIGETHER_API FDigetherSaveFloorCoin
{
	FGuid CoinId;
	FName StationId;
	int32 AmountCredits = 0;
	FTransform Transform = FTransform::Identity;
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
	// v3 기본 본문은 유지하고 슬롯의 선택 확장에 보관한다. 구 v3에는 비초기화 값이다.
	FDigetherHubProgress HubProgress;
	// 바닥 금화 목록이다. Hub 진행과 같은 선택 확장(v2)에 보관하며 Hub 진행이 초기화된 경우에만 둘 수 있다.
	TArray<FDigetherSaveFloorCoin> FloorCoins;
	// 기획 S33: 게임 오버 확정 저장에 함께 쓰는 이번 게임의 결과 기록이다. bGameOver일 때만 둘 수 있다.
	// 진행 확장 v2에 저장한다. 이전 파일의 게임 오버 저장에는 없으며, 불러올 때 플레이어 기록으로 다시 만든다.
	bool bHasGameResult = false;
	FDigetherGameResult GameResult;
	// 이 슬롯에서 끝난 이전 게임들의 결과다(오래된 순). 새 게임으로 바뀌어도 따로 보관한다(S33 240행).
	TArray<FDigetherGameResult> ArchivedGameResults;
};

// v2 타입이나 UObject 직렬화에 연결하지 않는 v3 값 스키마다.
struct DIGETHER_API FDigetherSaveSnapshot : public FDigetherSaveRuntimeState
{
	int32 SchemaVersion = DigetherSaveTuning::SchemaVersion;
};

struct DIGETHER_API FDigetherSaveSlotData
{
	FDigetherSaveSnapshot Progress;
	// 폐기된 출발 기준점(기획 1.1.0 이전 규칙)이다. 이전 파일을 읽을 수 있게 저장 형식에만 남기며
	// 런타임은 쓰거나 복원하지 않는다. 새로 쓰는 슬롯은 항상 비운다(DigetherSaveRules::TryBuildSlot).
	FDigetherSaveSnapshot DepartureCheckpoint;
	bool bHasDepartureCheckpoint = false;
	// 마지막으로 이 슬롯을 쓴 UTC 시각(FDateTime 틱)이다. 슬롯 목록 표시용이며 진행 판정에 쓰지 않는다.
	// wire v5부터 저장한다. 그 전 파일은 0(알 수 없음)이다. 값은 저장 서브시스템이 쓰기 직전에 채운다.
	int64 SavedAtUtcTicks = 0;
};
