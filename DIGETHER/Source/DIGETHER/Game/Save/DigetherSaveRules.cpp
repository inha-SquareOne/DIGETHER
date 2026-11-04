#include "Game/Save/DigetherSaveRules.h"

#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Life/DigetherLifeRules.h"
#include "Game/Run/DigetherRunRules.h"
#include "Game/Save/DigetherSaveTuning.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace
{
	bool IsRecoverableReadFailure(const EDigetherSaveResult Result)
	{
		return Result == EDigetherSaveResult::NotFound || Result == EDigetherSaveResult::CorruptData
			|| Result == EDigetherSaveResult::InvalidState || Result == EDigetherSaveResult::UnsafePhase;
	}

	bool IsBoundedString(const FString& Value)
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

	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}

	bool IsValidTransformParts(const FVector& Location, const FQuat& Rotation, const FVector& Scale)
	{
		return IsFiniteVector(Location) && IsFiniteVector(Scale) && FMath::IsFinite(Rotation.X)
			&& FMath::IsFinite(Rotation.Y) && FMath::IsFinite(Rotation.Z) && FMath::IsFinite(Rotation.W)
			&& Rotation.IsNormalized();
	}

	bool IsValidPlayerRecord(const FDigetherSavePlayerRecord& Player)
	{
		return DigetherSaveRules::IsValidAuthenticatedPlayerKey(Player.AuthenticatedPlayerKey)
			&& Player.GameplayPlayerId.IsValid() && FDigetherEconomyRules::IsValidWalletState(Player.Wallet)
			&& FDigetherLifeRules::IsValidState(Player.Life)
			&& IsBoundedString(Player.Life.PrivateState.SpectateTargetPlayerKey)
			&& (!Player.bHasHubTransform || IsValidTransformParts(Player.HubTransform.GetTranslation(),
				Player.HubTransform.GetRotation(), Player.HubTransform.GetScale3D()));
	}

	bool IsValidPlayers(const TArray<FDigetherSavePlayerRecord>& Players)
	{
		if (Players.IsEmpty() || Players.Num() > DigetherSaveTuning::MaxPlayerRecords)
		{
			return false;
		}
		TSet<FString> AuthenticatedKeys;
		TSet<FGuid> GameplayIds;
		for (const FDigetherSavePlayerRecord& Player : Players)
		{
			if (!IsValidPlayerRecord(Player) || AuthenticatedKeys.Contains(Player.AuthenticatedPlayerKey)
				|| GameplayIds.Contains(Player.GameplayPlayerId))
			{
				return false;
			}
			AuthenticatedKeys.Add(Player.AuthenticatedPlayerKey);
			GameplayIds.Add(Player.GameplayPlayerId);
		}
		return true;
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
		const FDigetherEconomyTuning& Economy = State.EconomyTuning;
		const int32 Percentages[] = { Economy.DayOneEfficiencyPercent, Economy.DayTwoEfficiencyPercent,
			Economy.DayThreeEfficiencyPercent, Economy.DayFourEfficiencyPercent };
		for (const int32 Percent : Percentages)
		{
			if (Percent < 1 || Percent > 100)
			{
				return false;
			}
		}
		const FDigetherRunRuleTuning& Rules = State.RunRuleTuning;
		return Economy.TargetIncreaseCredits >= 0 && FMath::IsFinite(Economy.DepositSessionLifetimeSeconds)
			&& Economy.DepositSessionLifetimeSeconds > 0.0 && Rules.AftershockStageCount > 0
			&& FMath::IsFinite(Rules.AftershockMinIntervalSeconds) && Rules.AftershockMinIntervalSeconds > 0.0
			&& FMath::IsFinite(Rules.AftershockMaxIntervalSeconds)
			&& Rules.AftershockMaxIntervalSeconds >= Rules.AftershockMinIntervalSeconds;
	}

	// 문자열·배열 길이를 할당 전에 제한한다. 손상된 파일에 FArchive의 기본 컨테이너 로드를 쓰지 않는다.
	void SerializeString(FArchive& Archive, FString& Value)
	{
		int32 Length = Archive.IsSaving() ? Value.Len() : 0;
		Archive << Length;
		if (Archive.IsError() || Length < 0 || Length > DigetherSaveTuning::MaxStringLength
			|| (Archive.IsLoading() && static_cast<int64>(Length) * static_cast<int64>(sizeof(uint16)) > Archive.TotalSize() - Archive.Tell()))
		{
			Archive.SetError();
			return;
		}
		if (Archive.IsLoading())
		{
			Value.Reset(Length);
		}
		for (int32 Index = 0; Index < Length; ++Index)
		{
			uint16 Character = Archive.IsSaving() ? static_cast<uint16>(Value[Index]) : 0;
			Archive << Character;
			if (Archive.IsLoading())
			{
				Value.AppendChar(static_cast<TCHAR>(Character));
			}
		}
	}

	void SerializeBoolean(FArchive& Archive, bool& bValue)
	{
		uint8 Byte = bValue ? 1 : 0;
		Archive << Byte;
		if (Byte > 1)
		{
			Archive.SetError();
		}
		if (Archive.IsLoading())
		{
			bValue = Byte == 1;
		}
	}

	template <typename TEnum>
	void SerializeEnum(FArchive& Archive, TEnum& Value)
	{
		uint8 Byte = static_cast<uint8>(Value);
		Archive << Byte;
		if (Archive.IsLoading())
		{
			Value = static_cast<TEnum>(Byte);
		}
	}

	template <typename TValue, typename TSerializer>
	void SerializeArray(FArchive& Archive, TArray<TValue>& Values, const int32 Maximum, TSerializer SerializeValue)
	{
		int32 Count = Archive.IsSaving() ? Values.Num() : 0;
		Archive << Count;
		if (Archive.IsError() || Count < 0 || Count > Maximum
			|| (Archive.IsLoading() && Count > Archive.TotalSize() - Archive.Tell()))
		{
			Archive.SetError();
			return;
		}
		if (Archive.IsLoading())
		{
			Values.SetNum(Count);
		}
		for (TValue& Value : Values)
		{
			SerializeValue(Archive, Value);
			if (Archive.IsError())
			{
				return;
			}
		}
	}

	void SerializeWallet(FArchive& Archive, FDigetherWalletState& Wallet)
	{
		Archive << Wallet.BalanceCredits << Wallet.LastRunPayoutCredits << Wallet.HiddenLifetimeCredits
			<< Wallet.TotalDeathLossCredits << Wallet.LastPreDeathBalanceCredits;
	}

	void SerializeLife(FArchive& Archive, FDigetherLifeComponentState& Life)
	{
		SerializeEnum(Archive, Life.PublicState.Life);
		SerializeBoolean(Archive, Life.PublicState.bSpectating);
		SerializeEnum(Archive, Life.PrivateState.DeathCause);
		SerializeEnum(Archive, Life.PrivateState.EscapeKind);
		SerializeString(Archive, Life.PrivateState.SpectateTargetPlayerKey);
	}

	void SerializePlayer(FArchive& Archive, FDigetherSavePlayerRecord& Player)
	{
		SerializeString(Archive, Player.AuthenticatedPlayerKey);
		Archive << Player.GameplayPlayerId;
		SerializeWallet(Archive, Player.Wallet);
		SerializeLife(Archive, Player.Life);
		SerializeBoolean(Archive, Player.bHasHubTransform);
		if (Player.bHasHubTransform)
		{
			FVector Location = Player.HubTransform.GetTranslation();
			FQuat Rotation = Player.HubTransform.GetRotation();
			FVector Scale = Player.HubTransform.GetScale3D();
			// FTransform의 엔진 내부 표현을 저장하지 않고 v3 좌표·회전·크기 성분을 고정한다.
			Archive << Location.X << Location.Y << Location.Z
				<< Rotation.X << Rotation.Y << Rotation.Z << Rotation.W
				<< Scale.X << Scale.Y << Scale.Z;
			if (Archive.IsLoading())
			{
				if (Archive.IsError() || !IsValidTransformParts(Location, Rotation, Scale))
				{
					Archive.SetError();
					return;
				}
				Player.HubTransform = FTransform(Rotation, Location, Scale);
			}
		}
		else if (Archive.IsLoading())
		{
			Player.HubTransform = FTransform::Identity;
		}
	}

	void SerializeTuning(FArchive& Archive, FDigetherRunTuning& Tuning)
	{
		FString SourceProfile = Archive.IsSaving() ? Tuning.SourceProfile.ToString() : FString();
		SerializeString(Archive, SourceProfile);
		if (Archive.IsLoading())
		{
			Tuning.SourceProfile = FPrimaryAssetId(SourceProfile);
		}
		Archive << Tuning.ProfileVersion << Tuning.InitialQuotaCredits << Tuning.RunSeconds
			<< Tuning.InteractionDistanceCm << Tuning.ElevatorCapacityKg << Tuning.ElevatorTripLimit
			<< Tuning.AscentPenaltySeconds << Tuning.ElevatorTravelSeconds << Tuning.DepositIntervalSeconds
			<< Tuning.EmergencyWeightLimitKg << Tuning.DeathLossPercent << Tuning.CorpseRefundPercent;
	}

	void SerializeSnapshot(FArchive& Archive, FDigetherSaveSnapshot& Snapshot)
	{
		SerializeString(Archive, Snapshot.HostAuthenticatedPlayerKey);
		Archive << Snapshot.GameId << Snapshot.RunId;
		FDigetherQuotaState& Quota = Snapshot.Quota.Quota;
		Archive << Quota.Day << Quota.TargetCredits << Quota.DepositedCredits << Quota.PeriodGeneration;
		SerializeArray(Archive, Snapshot.Quota.HiddenReveals, DigetherSaveTuning::MaxPlayerRecords,
			[](FArchive& EntryArchive, FDigetherHiddenRevealEntry& Entry)
			{
				SerializeString(EntryArchive, Entry.PlayerKey);
				EntryArchive << Entry.HiddenLifetimeCredits;
			});
		SerializeEnum(Archive, Snapshot.RunPublicState.Phase);
		Archive << Snapshot.RunPublicState.AftershockStage;
		SerializeBoolean(Archive, Snapshot.RunPublicState.bGameOver);
		SerializeBoolean(Archive, Snapshot.RunPublicState.bPaymentDeadline);
		SerializeTuning(Archive, Snapshot.RunTuning);
		FDigetherEconomyTuning& Economy = Snapshot.EconomyTuning;
		Archive << Economy.DayOneEfficiencyPercent << Economy.DayTwoEfficiencyPercent
			<< Economy.DayThreeEfficiencyPercent << Economy.DayFourEfficiencyPercent
			<< Economy.TargetIncreaseCredits << Economy.DepositSessionLifetimeSeconds;
		FDigetherRunRuleTuning& Rules = Snapshot.RunRuleTuning;
		Archive << Rules.AftershockStageCount << Rules.AftershockMaxIntervalSeconds << Rules.AftershockMinIntervalSeconds;
		SerializeArray(Archive, Snapshot.Players, DigetherSaveTuning::MaxPlayerRecords, SerializePlayer);
		SerializeArray(Archive, Snapshot.CompletedSettlementRunIds, DigetherSaveTuning::MaxCompletedRunRecords,
			[](FArchive& EntryArchive, FGuid& RunId) { EntryArchive << RunId; });
	}

	EDigetherSaveResult ValidateSlot(const FDigetherSaveSlotData& Slot)
	{
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
}

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
		// 프로세스 수명의 GetHandle은 저장하지 않는다. 제공자의 실제 계정 문자열만 사용한다.
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

bool DigetherSaveRules::IsValidAuthenticatedPlayerKey(const FString& PlayerKey)
{
	int32 Separator = INDEX_NONE;
	return IsBoundedString(PlayerKey) && PlayerKey.FindChar(TEXT(':'), Separator)
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
		|| !Snapshot.GameId.IsValid() || !Snapshot.RunId.IsValid() || !IsValidPlayers(Snapshot.Players)
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
		if (!IsBoundedString(Entry.PlayerKey))
		{
			return EDigetherSaveResult::InvalidState;
		}
	}
	TSet<FGuid> CompletedIds;
	for (const FGuid& RunId : Snapshot.CompletedSettlementRunIds)
	{
		if (!RunId.IsValid() || CompletedIds.Contains(RunId))
		{
			return EDigetherSaveResult::InvalidState;
		}
		CompletedIds.Add(RunId);
	}
	return EDigetherSaveResult::Success;
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
	const FDigetherSaveSlotData* Existing, const bool bExplicitNewGame, const bool bCheckpoint,
	FDigetherSaveSlotData& OutSlot)
{
	const EDigetherSaveResult SnapshotResult = ValidateSnapshot(Snapshot);
	if (SnapshotResult != EDigetherSaveResult::Success)
	{
		return SnapshotResult;
	}
	FDigetherSaveSlotData Candidate;
	if (Existing && !bExplicitNewGame)
	{
		const EDigetherSaveResult ExistingResult = ValidateSlot(*Existing);
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
	if (bCheckpoint)
	{
		Candidate.DepartureCheckpoint = Snapshot;
		Candidate.bHasDepartureCheckpoint = true;
	}
	OutSlot = MoveTemp(Candidate);
	return EDigetherSaveResult::Success;
}

EDigetherSaveResult DigetherSaveRules::SerializeSlot(const FDigetherSaveSlotData& Slot, TArray<uint8>& OutBytes)
{
	const EDigetherSaveResult Validation = ValidateSlot(Slot);
	if (Validation != EDigetherSaveResult::Success)
	{
		return Validation;
	}
	TArray<uint8> Candidate;
	FMemoryWriter Writer(Candidate, true);
	int32 Version = DigetherSaveTuning::SchemaVersion;
	Writer << Version;
	FDigetherSaveSlotData Copy = Slot;
	SerializeBoolean(Writer, Copy.bHasDepartureCheckpoint);
	SerializeSnapshot(Writer, Copy.Progress);
	if (Copy.bHasDepartureCheckpoint)
	{
		SerializeSnapshot(Writer, Copy.DepartureCheckpoint);
	}
	if (Writer.IsError() || Candidate.Num() > DigetherSaveTuning::MaxSaveBytes)
	{
		return EDigetherSaveResult::InvalidState;
	}
	OutBytes = MoveTemp(Candidate);
	return EDigetherSaveResult::Success;
}

EDigetherSaveResult DigetherSaveRules::DeserializeSlot(const TArray<uint8>& Bytes, FDigetherSaveSlotData& OutSlot)
{
	if (Bytes.Num() < static_cast<int32>(sizeof(int32)))
	{
		return EDigetherSaveResult::CorruptData;
	}
	FMemoryReader Reader(Bytes, true);
	int32 Version = 0;
	Reader << Version;
	if (Reader.IsError())
	{
		return EDigetherSaveResult::CorruptData;
	}
	if (Version != DigetherSaveTuning::SchemaVersion)
	{
		return Version > DigetherSaveTuning::SchemaVersion
			? EDigetherSaveResult::UnsupportedVersion : EDigetherSaveResult::CorruptData;
	}
	if (Bytes.Num() > DigetherSaveTuning::MaxSaveBytes)
	{
		return EDigetherSaveResult::CorruptData;
	}
	FDigetherSaveSlotData Candidate;
	SerializeBoolean(Reader, Candidate.bHasDepartureCheckpoint);
	SerializeSnapshot(Reader, Candidate.Progress);
	if (Candidate.bHasDepartureCheckpoint && !Reader.IsError())
	{
		SerializeSnapshot(Reader, Candidate.DepartureCheckpoint);
	}
	if (Reader.IsError() || Reader.Tell() != Reader.TotalSize())
	{
		return EDigetherSaveResult::CorruptData;
	}
	const EDigetherSaveResult Validation = ValidateSlot(Candidate);
	if (Validation != EDigetherSaveResult::Success)
	{
		return Validation;
	}
	OutSlot = MoveTemp(Candidate);
	return EDigetherSaveResult::Success;
}

EDigetherSaveResult DigetherSaveRules::SelectReadableSlot(const EDigetherSaveResult PrimaryResult,
	const EDigetherSaveResult BackupResult, bool& bOutUseBackup)
{
	if (PrimaryResult == EDigetherSaveResult::UnsupportedVersion
		|| BackupResult == EDigetherSaveResult::UnsupportedVersion)
	{
		return EDigetherSaveResult::UnsupportedVersion;
	}
	if (PrimaryResult == EDigetherSaveResult::Success)
	{
		bOutUseBackup = false;
		return EDigetherSaveResult::Success;
	}
	if (!IsRecoverableReadFailure(PrimaryResult))
	{
		return PrimaryResult;
	}
	if (BackupResult == EDigetherSaveResult::Success)
	{
		bOutUseBackup = true;
		return EDigetherSaveResult::Success;
	}
	if (!IsRecoverableReadFailure(BackupResult) || PrimaryResult == EDigetherSaveResult::NotFound)
	{
		return BackupResult;
	}
	return PrimaryResult;
}

EDigetherSaveResult DigetherSaveRules::CanWriteSlots(const EDigetherSaveResult PrimaryResult,
	const EDigetherSaveResult BackupResult, const bool bExplicitNewGame)
{
	if (PrimaryResult == EDigetherSaveResult::UnsupportedVersion
		|| BackupResult == EDigetherSaveResult::UnsupportedVersion)
	{
		return EDigetherSaveResult::UnsupportedVersion;
	}
	if (bExplicitNewGame)
	{
		return EDigetherSaveResult::Success;
	}
	// 읽을 수 있는 primary가 있어도 검사하지 못한 backup을 일반 저장으로 덮지 않는다.
	if (PrimaryResult != EDigetherSaveResult::Success && !IsRecoverableReadFailure(PrimaryResult))
	{
		return PrimaryResult;
	}
	if (BackupResult != EDigetherSaveResult::Success && !IsRecoverableReadFailure(BackupResult))
	{
		return BackupResult;
	}
	bool bUseBackup = false;
	const EDigetherSaveResult ReadResult = SelectReadableSlot(PrimaryResult, BackupResult, bUseBackup);
	return PrimaryResult == EDigetherSaveResult::NotFound && BackupResult == EDigetherSaveResult::NotFound
		? EDigetherSaveResult::Success : ReadResult;
}

EDigetherSaveWriteStep DigetherSaveRules::AdvanceWriteStep(const EDigetherSaveWriteStep Current,
	const bool bSucceeded)
{
	if (Current == EDigetherSaveWriteStep::Complete || Current == EDigetherSaveWriteStep::Failed)
	{
		return Current;
	}
	if (!bSucceeded)
	{
		return EDigetherSaveWriteStep::Failed;
	}
	switch (Current)
	{
	case EDigetherSaveWriteStep::WriteBackup:
		return EDigetherSaveWriteStep::VerifyBackup;
	case EDigetherSaveWriteStep::VerifyBackup:
		return EDigetherSaveWriteStep::WritePrimary;
	case EDigetherSaveWriteStep::WritePrimary:
		return EDigetherSaveWriteStep::VerifyPrimary;
	case EDigetherSaveWriteStep::VerifyPrimary:
		return EDigetherSaveWriteStep::Complete;
	default:
		return EDigetherSaveWriteStep::Failed;
	}
}

bool DigetherSaveRules::ShouldWritePrimaryFirst(const EDigetherSaveResult PrimaryResult,
	const EDigetherSaveResult BackupResult)
{
	return PrimaryResult != EDigetherSaveResult::Success && BackupResult == EDigetherSaveResult::Success;
}

EDigetherSaveResult DigetherSaveRules::CanDepartWithCheckpoint(const FDigetherSaveSnapshot& Current,
	const FDigetherSaveSlotData& Slot)
{
	const EDigetherSaveResult CurrentResult = ValidateSnapshot(Current);
	if (CurrentResult != EDigetherSaveResult::Success)
	{
		return CurrentResult;
	}
	const EDigetherSaveResult SlotResult = ValidateSlot(Slot);
	if (SlotResult != EDigetherSaveResult::Success)
	{
		return SlotResult;
	}
	if (!Slot.bHasDepartureCheckpoint)
	{
		return EDigetherSaveResult::CheckpointMissing;
	}
	const FDigetherSaveSnapshot& Checkpoint = Slot.DepartureCheckpoint;
	if (Current.HostAuthenticatedPlayerKey != Checkpoint.HostAuthenticatedPlayerKey
		|| Current.GameId != Checkpoint.GameId || Current.RunId != Checkpoint.RunId
		|| Current.Quota.Quota.Day != Checkpoint.Quota.Quota.Day
		|| Current.Quota.Quota.PeriodGeneration != Checkpoint.Quota.Quota.PeriodGeneration)
	{
		return EDigetherSaveResult::CheckpointMismatch;
	}
	TArray<uint8> CurrentBytes;
	TArray<uint8> CheckpointBytes;
	FMemoryWriter CurrentWriter(CurrentBytes, true);
	FMemoryWriter CheckpointWriter(CheckpointBytes, true);
	FDigetherSaveSnapshot CurrentCopy = Current;
	FDigetherSaveSnapshot CheckpointCopy = Checkpoint;
	SerializeSnapshot(CurrentWriter, CurrentCopy);
	SerializeSnapshot(CheckpointWriter, CheckpointCopy);
	if (CurrentWriter.IsError() || CheckpointWriter.IsError() || CurrentBytes != CheckpointBytes)
	{
		return EDigetherSaveResult::CheckpointMismatch;
	}
	return Current.RunPublicState.bGameOver ? EDigetherSaveResult::UnsafePhase : EDigetherSaveResult::Success;
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
	if (!IsValidPlayers(Players))
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
	if (!IsValidPlayerRecord(Player) || DeathLossPercent < 0 || DeathLossPercent > 100)
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
