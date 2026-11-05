// v3 진행 스키마의 wire 바이트 변환(SerializeSlot·DeserializeSlot)을 구현한다. 쓰기는 v5(진행 확장 v2),
// 읽기는 v3·v4·v5(진행 확장 v1·v2)다.
#include "Game/Save/DigetherSaveRules.h"

#include "Game/Hub/DigetherHubRules.h"
#include "Game/Save/DigetherSaveRulesInternal.h"
#include "Game/Save/DigetherSaveTuning.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace
{
	// wire v4는 v3 기본 본문 뒤에 이 블록을 붙인다. v3 슬롯 이름·진행 값 스키마는 유지한다.
	constexpr uint32 HubExtensionMagic = 0x31425548; // HUB1
	// v1: Hub 진행. v2: v1 뒤에 바닥 금화 목록(진행, 기준점이 있으면 기준점 순)을 붙인다. v1을 읽으면 금화는 빈 목록이다.
	constexpr int32 HubExtensionVersion = 2;
	constexpr int32 FloorCoinExtensionVersion = 2;
	// wire v5는 Hub 확장 뒤에 이 블록을 붙인다. v1: 저장 시각, 진행(·기준점) 전체 일차.
	// v2: v1 뒤에 진행(·기준점)마다 결과 통계(플레이어 누적 획득·복구·표시 이름·참가 순서)와 게임 결과 기록을 붙인다.
	constexpr uint32 ProgressExtensionMagic = 0x31475250; // PRG1
	constexpr int32 ProgressExtensionVersion = 2;
	constexpr int32 GameResultExtensionVersion = 2;

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
				if (Archive.IsError() || !DigetherSaveRulesInternal::IsValidTransformParts(Location, Rotation, Scale))
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

	void SerializeHubId(FArchive& Archive, FName& Id)
	{
		FString Value = Archive.IsSaving() && !Id.IsNone() ? Id.ToString() : FString();
		SerializeString(Archive, Value);
		if (Archive.IsError())
		{
			return;
		}
		// FName 생성 전에 엔진 이름 길이와 제어 문자를 거절한다.
		if (Value.Len() > DigetherHubRules::MaxIdentifierLength
			|| !DigetherSaveRulesInternal::IsBoundedString(Value))
		{
			Archive.SetError();
			return;
		}
		if (Archive.IsLoading())
		{
			Id = Value.IsEmpty() ? NAME_None : FName(*Value);
		}
	}

	void SerializeHubStack(FArchive& Archive, FDigetherHubItemStack& Stack)
	{
		SerializeHubId(Archive, Stack.ItemId);
		Archive << Stack.Count;
	}

	void SerializeHubPlayer(FArchive& Archive, FDigetherHubPlayerProgress& Player)
	{
		Archive << Player.PlayerId;
		SerializeBoolean(Archive, Player.bStarterGrantConsidered);
		SerializeArray(Archive, Player.Inventory, DigetherHubRules::MaxItemStacks, SerializeHubStack);
		SerializeArray(Archive, Player.PendingDelivery, DigetherHubRules::MaxItemStacks, SerializeHubStack);
	}

	void SerializeHubProgress(FArchive& Archive, FDigetherHubProgress& Progress)
	{
		SerializeBoolean(Archive, Progress.bInitialized);
		SerializeArray(Archive, Progress.Players, DigetherHubRules::MaxProgressPlayers, SerializeHubPlayer);
		SerializeArray(Archive, Progress.UnlockedStageIds, DigetherHubRules::MaxUnlockedStages, SerializeHubId);
		SerializeHubId(Archive, Progress.SelectedStageId);
		Archive << Progress.TotalOfferingCredits;
	}

	void SerializeFloorCoin(FArchive& Archive, FDigetherSaveFloorCoin& Coin)
	{
		Archive << Coin.CoinId;
		SerializeHubId(Archive, Coin.StationId);
		Archive << Coin.AmountCredits;
		FVector Location = Coin.Transform.GetTranslation();
		FQuat Rotation = Coin.Transform.GetRotation();
		FVector Scale = Coin.Transform.GetScale3D();
		// 플레이어 거점 변환과 같이 FTransform 내부 표현 대신 좌표·회전·크기 성분을 고정한다.
		Archive << Location.X << Location.Y << Location.Z
			<< Rotation.X << Rotation.Y << Rotation.Z << Rotation.W
			<< Scale.X << Scale.Y << Scale.Z;
		if (Archive.IsLoading())
		{
			if (Archive.IsError() || !DigetherSaveRulesInternal::IsValidTransformParts(Location, Rotation, Scale))
			{
				Archive.SetError();
				return;
			}
			Coin.Transform = FTransform(Rotation, Location, Scale);
		}
	}

	void SerializeFloorCoins(FArchive& Archive, TArray<FDigetherSaveFloorCoin>& Coins)
	{
		SerializeArray(Archive, Coins, DigetherSaveTuning::MaxFloorCoins, SerializeFloorCoin);
	}

	bool HasInitializedHubProgress(const FDigetherSaveSlotData& Slot)
	{
		return Slot.Progress.HubProgress.bInitialized
			|| (Slot.bHasDepartureCheckpoint && Slot.DepartureCheckpoint.HubProgress.bInitialized);
	}

	void WriteHubExtension(FMemoryWriter& Writer, FDigetherSaveSlotData& Slot)
	{
		TArray<uint8> Payload;
		FMemoryWriter PayloadWriter(Payload, true);
		SerializeHubProgress(PayloadWriter, Slot.Progress.HubProgress);
		if (Slot.bHasDepartureCheckpoint)
		{
			SerializeHubProgress(PayloadWriter, Slot.DepartureCheckpoint.HubProgress);
		}
		SerializeFloorCoins(PayloadWriter, Slot.Progress.FloorCoins);
		if (Slot.bHasDepartureCheckpoint)
		{
			SerializeFloorCoins(PayloadWriter, Slot.DepartureCheckpoint.FloorCoins);
		}
		if (PayloadWriter.IsError() || Payload.Num() > DigetherSaveTuning::MaxSaveBytes)
		{
			Writer.SetError();
			return;
		}
		uint32 Magic = HubExtensionMagic;
		int32 Version = HubExtensionVersion;
		int32 PayloadSize = Payload.Num();
		Writer << Magic << Version << PayloadSize;
		Writer.Serialize(Payload.GetData(), PayloadSize);
	}

	EDigetherSaveResult ReadHubExtension(FMemoryReader& Reader, FDigetherSaveSlotData& Slot)
	{
		if (Reader.Tell() == Reader.TotalSize())
		{
			return EDigetherSaveResult::CorruptData;
		}
		if (Reader.TotalSize() - Reader.Tell() < static_cast<int64>(sizeof(uint32) + sizeof(int32)))
		{
			return EDigetherSaveResult::CorruptData;
		}
		uint32 Magic = 0;
		int32 Version = 0;
		Reader << Magic << Version;
		if (Reader.IsError() || Magic != HubExtensionMagic || Version < 1)
		{
			return EDigetherSaveResult::CorruptData;
		}
		// 모르는 미래 확장은 본문을 해석하거나 기존 슬롯으로 덮어쓰지 않는다.
		if (Version > HubExtensionVersion)
		{
			return EDigetherSaveResult::UnsupportedVersion;
		}
		if (Reader.TotalSize() - Reader.Tell() < static_cast<int64>(sizeof(int32)))
		{
			return EDigetherSaveResult::CorruptData;
		}
		int32 PayloadSize = 0;
		Reader << PayloadSize;
		// v2는 진행마다 바닥 금화 개수(int32)가 하나 더 있다.
		const int32 MinimumProgressBytes = static_cast<int32>(sizeof(uint8) + 4 * sizeof(int32)
			+ (Version >= FloorCoinExtensionVersion ? sizeof(int32) : 0));
		const int32 MinimumPayloadSize = MinimumProgressBytes * (Slot.bHasDepartureCheckpoint ? 2 : 1);
		// wire v5는 이 블록 뒤에 진행 확장이 이어지므로 남은 길이 이하만 확인한다. 블록 뒤 끝 검사는 호출자가 한다.
		if (Reader.IsError() || PayloadSize < MinimumPayloadSize || PayloadSize > DigetherSaveTuning::MaxSaveBytes
			|| PayloadSize > Reader.TotalSize() - Reader.Tell())
		{
			return EDigetherSaveResult::CorruptData;
		}
		TArray<uint8> Payload;
		Payload.SetNumUninitialized(PayloadSize);
		Reader.Serialize(Payload.GetData(), PayloadSize);
		FMemoryReader PayloadReader(Payload, true);
		SerializeHubProgress(PayloadReader, Slot.Progress.HubProgress);
		if (!PayloadReader.IsError() && Slot.bHasDepartureCheckpoint)
		{
			SerializeHubProgress(PayloadReader, Slot.DepartureCheckpoint.HubProgress);
		}
		if (Version >= FloorCoinExtensionVersion)
		{
			if (!PayloadReader.IsError())
			{
				SerializeFloorCoins(PayloadReader, Slot.Progress.FloorCoins);
			}
			if (!PayloadReader.IsError() && Slot.bHasDepartureCheckpoint)
			{
				SerializeFloorCoins(PayloadReader, Slot.DepartureCheckpoint.FloorCoins);
			}
		}
		return !Reader.IsError() && !PayloadReader.IsError() && PayloadReader.Tell() == PayloadReader.TotalSize()
			? EDigetherSaveResult::Success : EDigetherSaveResult::CorruptData;
	}

	int32 GetProgressExtensionV1PayloadSize(const FDigetherSaveSlotData& Slot)
	{
		// 저장 시각(int64) + 진행 전체 일차(int32) [+ 기준점 전체 일차(int32)].
		return static_cast<int32>(sizeof(int64) + sizeof(int32) * (Slot.bHasDepartureCheckpoint ? 2 : 1));
	}

	void SerializeGameResultPlayer(FArchive& Archive, FDigetherGameResultPlayer& Player)
	{
		SerializeString(Archive, Player.DisplayName);
		Archive << Player.JoinOrder << Player.EarnedCredits << Player.HiddenCredits << Player.DeathLossCredits;
	}

	void SerializeGameResult(FArchive& Archive, FDigetherGameResult& Result)
	{
		Archive << Result.ResultId << Result.GameId << Result.FinalTotalDay << Result.CompletedPeriods
			<< Result.TotalEarnedCredits;
		SerializeArray(Archive, Result.Players, DigetherSaveTuning::MaxPlayerRecords, SerializeGameResultPlayer);
	}

	// 진행 확장 v2의 스냅샷별 값이다. 플레이어 통계는 기본 본문의 플레이어 배열과 같은 순서·개수다.
	void SerializeGameResultExtension(FArchive& Archive, FDigetherSaveSnapshot& Snapshot)
	{
		int32 PlayerCount = Archive.IsSaving() ? Snapshot.Players.Num() : 0;
		Archive << PlayerCount;
		if (Archive.IsError() || PlayerCount != Snapshot.Players.Num())
		{
			Archive.SetError();
			return;
		}
		for (FDigetherSavePlayerRecord& Player : Snapshot.Players)
		{
			Archive << Player.Wallet.LifetimeEarnedCredits << Player.Wallet.LifetimeCorpseRefundCredits;
			SerializeString(Archive, Player.DisplayName);
			Archive << Player.JoinOrder;
			if (Archive.IsError())
			{
				return;
			}
		}
		SerializeBoolean(Archive, Snapshot.bHasGameResult);
		if (Snapshot.bHasGameResult && !Archive.IsError())
		{
			SerializeGameResult(Archive, Snapshot.GameResult);
		}
		else if (Archive.IsLoading())
		{
			Snapshot.GameResult = FDigetherGameResult();
		}
		if (!Archive.IsError())
		{
			SerializeArray(Archive, Snapshot.ArchivedGameResults, DigetherSaveTuning::MaxArchivedGameResults,
				SerializeGameResult);
		}
	}

	// wire v5는 Hub 확장 뒤에 이 블록을 붙인다. v3 기본 본문의 고정 배치를 바꾸지 않으려고 새 값을 여기에 둔다.
	void WriteProgressExtension(FMemoryWriter& Writer, FDigetherSaveSlotData& Slot)
	{
		TArray<uint8> Payload;
		FMemoryWriter PayloadWriter(Payload, true);
		PayloadWriter << Slot.SavedAtUtcTicks;
		PayloadWriter << Slot.Progress.Quota.Quota.TotalDay;
		if (Slot.bHasDepartureCheckpoint)
		{
			PayloadWriter << Slot.DepartureCheckpoint.Quota.Quota.TotalDay;
		}
		SerializeGameResultExtension(PayloadWriter, Slot.Progress);
		if (Slot.bHasDepartureCheckpoint)
		{
			SerializeGameResultExtension(PayloadWriter, Slot.DepartureCheckpoint);
		}
		if (PayloadWriter.IsError() || Payload.Num() > DigetherSaveTuning::MaxSaveBytes)
		{
			Writer.SetError();
			return;
		}
		uint32 Magic = ProgressExtensionMagic;
		int32 Version = ProgressExtensionVersion;
		int32 PayloadSize = Payload.Num();
		Writer << Magic << Version << PayloadSize;
		Writer.Serialize(Payload.GetData(), PayloadSize);
	}

	EDigetherSaveResult ReadProgressExtension(FMemoryReader& Reader, FDigetherSaveSlotData& Slot)
	{
		if (Reader.TotalSize() - Reader.Tell() < static_cast<int64>(sizeof(uint32) + sizeof(int32)))
		{
			return EDigetherSaveResult::CorruptData;
		}
		uint32 Magic = 0;
		int32 Version = 0;
		Reader << Magic << Version;
		if (Reader.IsError() || Magic != ProgressExtensionMagic || Version < 1)
		{
			return EDigetherSaveResult::CorruptData;
		}
		// Hub 확장과 같이 모르는 미래 확장은 본문을 해석하지 않고 보존한다.
		if (Version > ProgressExtensionVersion)
		{
			return EDigetherSaveResult::UnsupportedVersion;
		}
		if (Reader.TotalSize() - Reader.Tell() < static_cast<int64>(sizeof(int32)))
		{
			return EDigetherSaveResult::CorruptData;
		}
		int32 PayloadSize = 0;
		Reader << PayloadSize;
		// 진행 확장은 마지막 블록이다. 길이가 정확히 맞아야 하며 뒤에 남는 바이트를 허용하지 않는다.
		// v1은 고정 길이다. v2는 결과 통계·기록 때문에 길이가 바뀌므로 하한만 고정한다.
		const int32 V1PayloadSize = GetProgressExtensionV1PayloadSize(Slot);
		if (Reader.IsError() || PayloadSize != Reader.TotalSize() - Reader.Tell()
			|| (Version < GameResultExtensionVersion && PayloadSize != V1PayloadSize)
			|| (Version >= GameResultExtensionVersion && PayloadSize < V1PayloadSize)
			|| PayloadSize > DigetherSaveTuning::MaxSaveBytes)
		{
			return EDigetherSaveResult::CorruptData;
		}
		TArray<uint8> Payload;
		Payload.SetNumUninitialized(PayloadSize);
		Reader.Serialize(Payload.GetData(), PayloadSize);
		FMemoryReader PayloadReader(Payload, true);
		PayloadReader << Slot.SavedAtUtcTicks;
		PayloadReader << Slot.Progress.Quota.Quota.TotalDay;
		if (Slot.bHasDepartureCheckpoint)
		{
			PayloadReader << Slot.DepartureCheckpoint.Quota.Quota.TotalDay;
		}
		// v1에는 결과 통계·기록이 없다. 누적 획득·복구는 0, 표시 이름은 빈 값, 참가 순서는 0(알 수 없음)으로 둔다.
		if (Version >= GameResultExtensionVersion && !PayloadReader.IsError())
		{
			SerializeGameResultExtension(PayloadReader, Slot.Progress);
			if (Slot.bHasDepartureCheckpoint && !PayloadReader.IsError())
			{
				SerializeGameResultExtension(PayloadReader, Slot.DepartureCheckpoint);
			}
		}
		return !Reader.IsError() && Reader.Tell() == Reader.TotalSize() && !PayloadReader.IsError()
			&& PayloadReader.Tell() == PayloadReader.TotalSize()
			? EDigetherSaveResult::Success : EDigetherSaveResult::CorruptData;
	}

	// wire v3·v4에는 전체 일차가 없다. 이전 기간에 지난 날 수는 복원할 수 없으므로 확실한 하한인 기간 일차를 쓴다.
	// 전체 일차는 기간 일차보다 작을 수 없고(같은 기간 안에서 함께 오른다), 첫 기간이면 정확히 같다. 저장 시각은 알 수 없음(0)이다.
	void ApplyLegacyProgressEstimate(FDigetherSaveSlotData& Slot)
	{
		Slot.SavedAtUtcTicks = 0;
		Slot.Progress.Quota.Quota.TotalDay = Slot.Progress.Quota.Quota.Day;
		if (Slot.bHasDepartureCheckpoint)
		{
			Slot.DepartureCheckpoint.Quota.Quota.TotalDay = Slot.DepartureCheckpoint.Quota.Quota.Day;
		}
	}
}

bool DigetherSaveRulesInternal::TryEncodeSnapshot(const FDigetherSaveSnapshot& Snapshot, TArray<uint8>& OutBytes)
{
	FMemoryWriter Writer(OutBytes, true);
	FDigetherSaveSnapshot Copy = Snapshot;
	SerializeSnapshot(Writer, Copy);
	SerializeHubProgress(Writer, Copy.HubProgress);
	SerializeFloorCoins(Writer, Copy.FloorCoins);
	Writer << Copy.Quota.Quota.TotalDay;
	return !Writer.IsError();
}

EDigetherSaveResult DigetherSaveRules::SerializeSlot(const FDigetherSaveSlotData& Slot, TArray<uint8>& OutBytes)
{
	const EDigetherSaveResult Validation = DigetherSaveRulesInternal::ValidateSlot(Slot);
	if (Validation != EDigetherSaveResult::Success)
	{
		return Validation;
	}
	TArray<uint8> Candidate;
	FMemoryWriter Writer(Candidate, true);
	// 항상 최신 wire로 쓴다. 구 v3·v4 실행 파일은 첫 버전만 보고 UnsupportedVersion을 반환하므로
	// 전체 일차·Hub 재산을 모르는 실행 파일이 이 슬롯을 덮어쓰지 못한다.
	int32 Version = DigetherSaveWire::LatestVersion;
	Writer << Version;
	FDigetherSaveSlotData Copy = Slot;
	SerializeBoolean(Writer, Copy.bHasDepartureCheckpoint);
	SerializeSnapshot(Writer, Copy.Progress);
	if (Copy.bHasDepartureCheckpoint)
	{
		SerializeSnapshot(Writer, Copy.DepartureCheckpoint);
	}
	// v5는 Hub 진행이 비초기화여도 확장을 쓴다. 블록 위치가 v4와 같아 Hub 확장 해석을 그대로 공유한다.
	WriteHubExtension(Writer, Copy);
	WriteProgressExtension(Writer, Copy);
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
	if (Version != DigetherSaveWire::LegacyVersion && Version != DigetherSaveWire::HubVersion
		&& Version != DigetherSaveWire::DayVersion)
	{
		return Version > DigetherSaveWire::LatestVersion
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
	if (Reader.IsError())
	{
		return EDigetherSaveResult::CorruptData;
	}
	if (Version == DigetherSaveWire::LegacyVersion)
	{
		if (Reader.Tell() != Reader.TotalSize())
		{
			return EDigetherSaveResult::CorruptData;
		}
	}
	else
	{
		const EDigetherSaveResult ExtensionResult = ReadHubExtension(Reader, Candidate);
		if (ExtensionResult != EDigetherSaveResult::Success)
		{
			return ExtensionResult;
		}
		if (Version == DigetherSaveWire::HubVersion)
		{
			// v4는 Hub 확장이 마지막 블록이고, Hub 재산이 있을 때만 썼다.
			if (Reader.Tell() != Reader.TotalSize())
			{
				return EDigetherSaveResult::CorruptData;
			}
			if (!HasInitializedHubProgress(Candidate))
			{
				return EDigetherSaveResult::InvalidState;
			}
		}
		else
		{
			const EDigetherSaveResult ProgressResult = ReadProgressExtension(Reader, Candidate);
			if (ProgressResult != EDigetherSaveResult::Success)
			{
				return ProgressResult;
			}
		}
	}
	if (Version != DigetherSaveWire::DayVersion)
	{
		ApplyLegacyProgressEstimate(Candidate);
	}
	const EDigetherSaveResult Validation = DigetherSaveRulesInternal::ValidateSlot(Candidate);
	if (Validation != EDigetherSaveResult::Success)
	{
		return Validation;
	}
	OutSlot = MoveTemp(Candidate);
	return EDigetherSaveResult::Success;
}
