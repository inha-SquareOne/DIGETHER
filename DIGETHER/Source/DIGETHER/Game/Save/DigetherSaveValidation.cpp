// DigetherSaveRules의 스냅샷·슬롯 검증, 폐기된 출발 기준점 필드는 이전 파일 읽기용으로만 검증
#include "Game/Save/DigetherSaveRules.h"

#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Hub/DigetherHubRules.h"
#include "Game/Life/DigetherLifeRules.h"
#include "Game/Run/DigetherGameOverRules.h"
#include "Game/Run/DigetherRunRules.h"
#include "Game/Save/DigetherSaveRulesInternal.h"
#include "Game/Save/DigetherSaveTuning.h"

namespace
{
	bool IsValidStoredGameResult(const FDigetherGameResult& Result)
	{
		if (!DigetherGameOverRules::IsValidGameResult(Result, DigetherSaveTuning::MaxPlayerRecords))
		{
			return false;
		}
		for (const FDigetherGameResultPlayer& Player : Result.Players)
		{
			if (!DigetherSaveRulesInternal::IsBoundedString(Player.DisplayName))
			{
				return false;
			}
		}
		return true;
	}

	// 기획 S33: 이번 게임 결과는 게임 오버 확정 저장에만 있고 같은 게임 ID, 보관한 이전 결과는 다른 게임 ID
	// 결과 ID 중복 불허(새 게임 ID가 하나만 만들어지는지·이전 결과가 섞이지 않는지 T38)
	bool IsValidGameResults(const FDigetherSaveRuntimeState& State)
	{
		if (State.bHasGameResult && (!State.RunPublicState.bGameOver || State.GameResult.GameId != State.GameId
			|| !IsValidStoredGameResult(State.GameResult)))
		{
			return false;
		}
		if (State.ArchivedGameResults.Num() > DigetherSaveTuning::MaxArchivedGameResults)
		{
			return false;
		}
		TSet<FGuid> ResultIds;
		if (State.bHasGameResult)
		{
			ResultIds.Add(State.GameResult.ResultId);
		}
		for (const FDigetherGameResult& Archived : State.ArchivedGameResults)
		{
			if (!IsValidStoredGameResult(Archived) || Archived.GameId == State.GameId)
			{
				return false;
			}
			bool bDuplicateId = false;
			ResultIds.Add(Archived.ResultId, &bDuplicateId);
			if (bDuplicateId)
			{
				return false;
			}
		}
		return true;
	}

	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}

	bool IsValidFixedTuning(const FDigetherSaveRuntimeState& State)
	{
		TArray<FText> Problems;
		if (!State.RunTuning.SourceProfile.IsValid() || !State.RunTuning.CollectProblems(Problems))
		{
			return false;
		}
		const float Values[] = { State.RunTuning.RunSeconds, State.RunTuning.InteractionDistanceCm,
			State.RunTuning.ElevatorCapacityKg, State.RunTuning.AscentPenaltySeconds,
			State.RunTuning.ElevatorTravelSeconds, State.RunTuning.DepositIntervalSeconds,
			State.RunTuning.EmergencyWeightLimitKg };
		for (const float Value : Values)
		{
			if (!FMath::IsFinite(Value))
			{
				return false;
			}
		}
		return FDigetherEconomyRules::IsValidTuning(State.EconomyTuning)
			&& FDigetherRunRules::IsValidRuleTuning(State.RunRuleTuning);
	}

	// 바닥 금화는 금액 1 이상·고유 ID·정산기 ID·유한 변환만 허용, Hub 확장 밖(구 v3)에는 둘 수 없음
	bool IsValidFloorCoins(const FDigetherSaveRuntimeState& State)
	{
		if (State.FloorCoins.Num() > DigetherSaveTuning::MaxFloorCoins
			|| (!State.FloorCoins.IsEmpty() && !State.HubProgress.bInitialized))
		{
			return false;
		}
		TSet<FGuid> CoinIds;
		CoinIds.Reserve(State.FloorCoins.Num());
		for (const FDigetherSaveFloorCoin& Coin : State.FloorCoins)
		{
			if (!Coin.CoinId.IsValid() || Coin.StationId.IsNone()
				|| Coin.AmountCredits < 1
				|| !DigetherSaveRulesInternal::IsBoundedString(Coin.StationId.ToString())
				|| !DigetherSaveRulesInternal::IsValidTransformParts(Coin.Transform.GetTranslation(),
					Coin.Transform.GetRotation(), Coin.Transform.GetScale3D()))
			{
				return false;
			}
			bool bDuplicateId = false;
			CoinIds.Add(Coin.CoinId, &bDuplicateId);
			if (bDuplicateId)
			{
				return false;
			}
		}
		return true;
	}
}

bool DigetherSaveRulesInternal::IsBoundedString(const FString& Value)
{
	if (Value.Len() > DigetherSaveTuning::MaxStringLength)
	{
		return false;
	}
	for (const TCHAR Character : Value)
	{
		if (Character < TEXT(' '))
		{
			return false;
		}
	}
	return true;
}

bool DigetherSaveRulesInternal::IsValidTransformParts(const FVector& Location, const FQuat& Rotation,
	const FVector& Scale)
{
	return IsFiniteVector(Location) && IsFiniteVector(Scale) && FMath::IsFinite(Rotation.X)
		&& FMath::IsFinite(Rotation.Y) && FMath::IsFinite(Rotation.Z) && FMath::IsFinite(Rotation.W)
		&& Rotation.IsNormalized();
}

bool DigetherSaveRulesInternal::IsValidPlayerRecord(const FDigetherSavePlayerRecord& Player)
{
	return DigetherSaveRules::IsValidAuthenticatedPlayerKey(Player.AuthenticatedPlayerKey)
		&& Player.GameplayPlayerId.IsValid() && FDigetherEconomyRules::IsValidWalletState(Player.Wallet)
		&& FDigetherLifeRules::IsValidState(Player.Life)
		&& IsBoundedString(Player.Life.PrivateState.SpectateTargetPlayerKey)
		&& IsBoundedString(Player.DisplayName) && Player.JoinOrder >= 0
		&& (!Player.bHasHubTransform || IsValidTransformParts(Player.HubTransform.GetTranslation(),
			Player.HubTransform.GetRotation(), Player.HubTransform.GetScale3D()));
}

bool DigetherSaveRulesInternal::IsValidPlayers(const TArray<FDigetherSavePlayerRecord>& Players)
{
	if (Players.IsEmpty() || Players.Num() > DigetherSaveTuning::MaxPlayerRecords)
	{
		return false;
	}
	TSet<FString> AuthenticatedKeys;
	TSet<FGuid> GameplayIds;
	AuthenticatedKeys.Reserve(Players.Num());
	GameplayIds.Reserve(Players.Num());
	for (const FDigetherSavePlayerRecord& Player : Players)
	{
		if (!IsValidPlayerRecord(Player))
		{
			return false;
		}
		bool bDuplicateKey = false;
		bool bDuplicateId = false;
		AuthenticatedKeys.Add(Player.AuthenticatedPlayerKey, &bDuplicateKey);
		GameplayIds.Add(Player.GameplayPlayerId, &bDuplicateId);
		if (bDuplicateKey || bDuplicateId)
		{
			return false;
		}
	}
	return true;
}

EDigetherSaveResult DigetherSaveRulesInternal::ValidateSlot(const FDigetherSaveSlotData& Slot)
{
	// 0은 "알 수 없음"(v5 이전 파일), FDateTime이 표현하지 못하는 틱은 목록 표시 전에 거절
	if (Slot.SavedAtUtcTicks < 0 || Slot.SavedAtUtcTicks > FDateTime::MaxValue().GetTicks())
	{
		return EDigetherSaveResult::InvalidState;
	}
	const EDigetherSaveResult ProgressResult = DigetherSaveRules::ValidateSnapshot(Slot.Progress);
	if (ProgressResult != EDigetherSaveResult::Success)
	{
		return ProgressResult;
	}
	if (Slot.bHasDepartureCheckpoint)
	{
		const EDigetherSaveResult CheckpointResult = DigetherSaveRules::ValidateSnapshot(Slot.DepartureCheckpoint);
		if (CheckpointResult != EDigetherSaveResult::Success)
		{
			return CheckpointResult;
		}
		if (Slot.Progress.GameId != Slot.DepartureCheckpoint.GameId
			|| Slot.Progress.HostAuthenticatedPlayerKey != Slot.DepartureCheckpoint.HostAuthenticatedPlayerKey)
		{
			return EDigetherSaveResult::InvalidState;
		}
	}
	return EDigetherSaveResult::Success;
}

bool DigetherSaveRules::IsValidAuthenticatedPlayerKey(const FString& PlayerKey)
{
	int32 Separator = INDEX_NONE;
	return DigetherSaveRulesInternal::IsBoundedString(PlayerKey) && PlayerKey.FindChar(TEXT(':'), Separator)
		&& Separator > 0 && Separator < PlayerKey.Len() - 1;
}

EDigetherSaveResult DigetherSaveRules::ValidateSnapshot(const FDigetherSaveSnapshot& Snapshot)
{
	if (Snapshot.SchemaVersion != DigetherSaveTuning::SchemaVersion)
	{
		return Snapshot.SchemaVersion > DigetherSaveTuning::SchemaVersion
			? EDigetherSaveResult::UnsupportedVersion : EDigetherSaveResult::CorruptData;
	}
	if (Snapshot.RunPublicState.Phase != EDigetherRunPhase::NotStarted)
	{
		return EDigetherSaveResult::UnsafePhase;
	}
	if (!IsValidAuthenticatedPlayerKey(Snapshot.HostAuthenticatedPlayerKey)
		|| !Snapshot.GameId.IsValid() || !Snapshot.RunId.IsValid()
		|| !DigetherSaveRulesInternal::IsValidPlayers(Snapshot.Players)
		|| !DigetherHubRules::IsProgressStructurallyValid(Snapshot.HubProgress)
		|| !IsValidFloorCoins(Snapshot)
		|| !IsValidGameResults(Snapshot)
		|| !IsValidFixedTuning(Snapshot) || !FDigetherEconomyRules::IsValidPublicState(Snapshot.Quota)
		|| Snapshot.Quota.Quota.Day < 1 || Snapshot.Quota.Quota.Day > DigetherRunRules::PaymentDeadlineDay + 1
		|| Snapshot.Quota.Quota.TargetCredits <= 0 || Snapshot.RunPublicState.AftershockStage != 0
		|| Snapshot.Quota.HiddenReveals.Num() > DigetherSaveTuning::MaxPlayerRecords
		|| Snapshot.CompletedSettlementRunIds.Num() > DigetherSaveTuning::MaxCompletedRunRecords)
	{
		return EDigetherSaveResult::InvalidState;
	}
	bool bHasHostRecord = false;
	for (const FDigetherSavePlayerRecord& Player : Snapshot.Players)
	{
		bHasHostRecord |= Player.AuthenticatedPlayerKey == Snapshot.HostAuthenticatedPlayerKey;
	}
	for (const FDigetherHubPlayerProgress& HubPlayer : Snapshot.HubProgress.Players)
	{
		if (!Snapshot.Players.ContainsByPredicate([&HubPlayer](const FDigetherSavePlayerRecord& Player)
			{ return Player.GameplayPlayerId == HubPlayer.PlayerId; }))
		{
			return EDigetherSaveResult::InvalidState;
		}
	}
	const FDigetherRunQuotaResult QuotaResult = FDigetherRunRules::EvaluateQuota(Snapshot.Quota.Quota);
	if (!bHasHostRecord || QuotaResult.Result != EDigetherRunResult::Success
		|| QuotaResult.bGameOver != Snapshot.RunPublicState.bGameOver
		|| (Snapshot.RunPublicState.bPaymentDeadline && !QuotaResult.bPaymentDeadline && !QuotaResult.bGameOver)
		|| (!Snapshot.RunPublicState.bGameOver && !Snapshot.Quota.HiddenReveals.IsEmpty()))
	{
		return EDigetherSaveResult::InvalidState;
	}
	for (const FDigetherHiddenRevealEntry& Entry : Snapshot.Quota.HiddenReveals)
	{
		if (!DigetherSaveRulesInternal::IsBoundedString(Entry.PlayerKey))
		{
			return EDigetherSaveResult::InvalidState;
		}
	}
	TSet<FGuid> CompletedIds;
	CompletedIds.Reserve(Snapshot.CompletedSettlementRunIds.Num());
	for (const FGuid& RunId : Snapshot.CompletedSettlementRunIds)
	{
		if (!RunId.IsValid())
		{
			return EDigetherSaveResult::InvalidState;
		}
		bool bDuplicateId = false;
		CompletedIds.Add(RunId, &bDuplicateId);
		if (bDuplicateId)
		{
			return EDigetherSaveResult::InvalidState;
		}
	}
	return EDigetherSaveResult::Success;
}
