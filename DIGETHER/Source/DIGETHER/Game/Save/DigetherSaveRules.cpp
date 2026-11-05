#include "Game/Save/DigetherSaveRules.h"

#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Life/DigetherLifeRules.h"
#include "Game/Save/DigetherSaveRulesInternal.h"
#include "Game/Save/DigetherSaveTuning.h"
#include "GameFramework/OnlineReplStructs.h"

// 바이트 변환은 DigetherSaveCodec.cpp, 스냅샷·슬롯 검증은 DigetherSaveValidation.cpp,
// 주·백업 슬롯 선택 정책은 DigetherSaveSlotPolicy.cpp

EDigetherSaveResult DigetherSaveRules::MakeAuthenticatedPlayerKey(const FUniqueNetIdRepl& UniqueId,
	FString& OutPlayerKey)
{
	if (!UniqueId.IsValid())
	{
		return EDigetherSaveResult::PlayerIdentityMismatch;
	}
	FString Candidate;
	if (UniqueId.IsV1())
	{
		if (UniqueId.GetType().IsNone())
		{
			return EDigetherSaveResult::PlayerIdentityMismatch;
		}
		Candidate = UniqueId.GetType().ToString() + TEXT(":") + UniqueId.ToString();
	}
	else if (UniqueId.IsV2())
	{
		const UE::Online::FAccountId AccountId = UniqueId.GetV2Unsafe();
		const UE::Online::IOnlineAccountIdRegistry* Registry = UE::Online::FOnlineIdRegistryRegistry::Get()
			.GetAccountIdRegistry(AccountId.GetOnlineServicesType());
		if (!Registry)
		{
			return EDigetherSaveResult::UnsupportedIdentity;
		}
		// 프로세스 수명 GetHandle은 저장 금지, 제공자의 실제 계정 문자열만 사용
		Candidate = FString(TEXT("OSSv2.")) + UE::Online::LexToString(AccountId.GetOnlineServicesType())
			+ TEXT(":") + Registry->ToString(AccountId);
	}
	else
	{
		return EDigetherSaveResult::UnsupportedIdentity;
	}
	if (!IsValidAuthenticatedPlayerKey(Candidate))
	{
		return EDigetherSaveResult::PlayerIdentityMismatch;
	}
	OutPlayerKey = Candidate;
	return EDigetherSaveResult::Success;
}

FString DigetherSaveRules::MakeStoredDisplayName(const FString& PlayerName)
{
	FString Result;
	Result.Reserve(FMath::Min(PlayerName.Len(), DigetherSaveTuning::MaxStringLength));
	for (const TCHAR Character : PlayerName)
	{
		if (Result.Len() >= DigetherSaveTuning::MaxStringLength)
		{
			break;
		}
		if (Character >= TEXT(' '))
		{
			Result.AppendChar(Character);
		}
	}
	return Result;
}

EDigetherSaveResult DigetherSaveRules::TryCaptureSnapshot(const FDigetherSaveRuntimeState& Runtime,
	FDigetherSaveSnapshot& OutSnapshot)
{
	FDigetherSaveSnapshot Candidate;
	static_cast<FDigetherSaveRuntimeState&>(Candidate) = Runtime;
	const EDigetherSaveResult Result = ValidateSnapshot(Candidate);
	if (Result == EDigetherSaveResult::Success)
	{
		OutSnapshot = MoveTemp(Candidate);
	}
	return Result;
}

EDigetherSaveResult DigetherSaveRules::TryRestoreSnapshot(const FDigetherSaveSnapshot& Snapshot,
	FDigetherSaveRuntimeState& OutRuntime)
{
	const EDigetherSaveResult Result = ValidateSnapshot(Snapshot);
	if (Result == EDigetherSaveResult::Success)
	{
		OutRuntime = Snapshot;
	}
	return Result;
}

EDigetherSaveResult DigetherSaveRules::TryBuildSlot(const FDigetherSaveSnapshot& Snapshot,
	const FDigetherSaveSlotData* Existing, const bool bExplicitNewGame, FDigetherSaveSlotData& OutSlot)
{
	const EDigetherSaveResult SnapshotResult = ValidateSnapshot(Snapshot);
	if (SnapshotResult != EDigetherSaveResult::Success)
	{
		return SnapshotResult;
	}
	FDigetherSaveSlotData Candidate;
	if (Existing && !bExplicitNewGame)
	{
		const EDigetherSaveResult ExistingResult = DigetherSaveRulesInternal::ValidateSlot(*Existing);
		if (ExistingResult != EDigetherSaveResult::Success)
		{
			return ExistingResult;
		}
		if (Existing->Progress.HostAuthenticatedPlayerKey != Snapshot.HostAuthenticatedPlayerKey)
		{
			return EDigetherSaveResult::HostIdentityMismatch;
		}
		if (Existing->Progress.GameId != Snapshot.GameId)
		{
			return EDigetherSaveResult::InvalidState;
		}
		Candidate = *Existing;
	}
	Candidate.Progress = Snapshot;
	// 기획 1.1.0: 출발 기준점 저장·디스크 복원 규칙 폐기, 필드·codec은 이전 파일 읽기용으로 유지
	// 새로 쓰는 슬롯에는 지난 기준점을 옮기지 않음
	Candidate.DepartureCheckpoint = FDigetherSaveSnapshot();
	Candidate.bHasDepartureCheckpoint = false;
	OutSlot = MoveTemp(Candidate);
	return EDigetherSaveResult::Success;
}

EDigetherSaveResult DigetherSaveRules::TryResolveReturningPlayer(const TArray<FDigetherSavePlayerRecord>& Players,
	const FUniqueNetIdRepl& UniqueId, FDigetherSavePlayerRecord& OutPlayer, bool& bOutSkipInitialGrant)
{
	FString PlayerKey;
	const EDigetherSaveResult KeyResult = MakeAuthenticatedPlayerKey(UniqueId, PlayerKey);
	if (KeyResult != EDigetherSaveResult::Success)
	{
		return KeyResult;
	}
	if (!DigetherSaveRulesInternal::IsValidPlayers(Players))
	{
		return EDigetherSaveResult::InvalidState;
	}
	for (const FDigetherSavePlayerRecord& Player : Players)
	{
		if (Player.AuthenticatedPlayerKey == PlayerKey)
		{
			OutPlayer = Player;
			bOutSkipInitialGrant = true;
			return EDigetherSaveResult::Success;
		}
	}
	return EDigetherSaveResult::PlayerNotFound;
}

EDigetherSaveResult DigetherSaveRules::TryApplyOfflineDeath(const FDigetherSavePlayerRecord& Player,
	const EDigetherDeathCause Cause, const int32 DeathLossPercent, FDigetherSavePlayerRecord& OutPlayer)
{
	if (!DigetherSaveRulesInternal::IsValidPlayerRecord(Player) || DeathLossPercent < 0 || DeathLossPercent > 100)
	{
		return EDigetherSaveResult::InvalidState;
	}
	const FDigetherLifeUpdateResult Life = FDigetherLifeRules::ApplyDeath(Player.Life, Cause);
	const FDigetherWalletUpdateResult Wallet = FDigetherEconomyRules::ApplyDeathLoss(Player.Wallet, DeathLossPercent);
	if (Life.Result != EDigetherLifeResult::Success || Wallet.Result != EDigetherEconomyResult::Success)
	{
		return EDigetherSaveResult::InvalidState;
	}
	FDigetherSavePlayerRecord Candidate = Player;
	Candidate.Life = Life.State;
	Candidate.Wallet = Wallet.State;
	OutPlayer = MoveTemp(Candidate);
	return EDigetherSaveResult::Success;
}

EDigetherSaveResult DigetherSaveRules::TryApplySettlement(const FDigetherSaveSnapshot& Snapshot,
	const FGuid& RunId, const TArray<FDigetherPlayerSettlement>& Settlements, FDigetherSaveSnapshot& OutSnapshot)
{
	const EDigetherSaveResult Validation = ValidateSnapshot(Snapshot);
	if (Validation != EDigetherSaveResult::Success)
	{
		return Validation;
	}
	if (!RunId.IsValid() || RunId != Snapshot.RunId || Settlements.Num() != Snapshot.Players.Num())
	{
		return EDigetherSaveResult::InvalidState;
	}
	if (Snapshot.CompletedSettlementRunIds.Contains(RunId))
	{
		return EDigetherSaveResult::AlreadySettled;
	}
	if (Snapshot.CompletedSettlementRunIds.Num() >= DigetherSaveTuning::MaxCompletedRunRecords)
	{
		return EDigetherSaveResult::InvalidState;
	}
	FDigetherSaveSnapshot Candidate = Snapshot;
	for (int32 Index = 0; Index < Candidate.Players.Num(); ++Index)
	{
		const FDigetherWalletUpdateResult Wallet = FDigetherEconomyRules::ApplyRunSettlement(
			Candidate.Players[Index].Wallet, Settlements[Index]);
		if (Wallet.Result != EDigetherEconomyResult::Success)
		{
			return EDigetherSaveResult::InvalidState;
		}
		Candidate.Players[Index].Wallet = Wallet.State;
	}
	Candidate.CompletedSettlementRunIds.Add(RunId);
	OutSnapshot = MoveTemp(Candidate);
	return EDigetherSaveResult::Success;
}
