// v3 진행 스키마의 wire 바이트 변환(SerializeSlot·DeserializeSlot), 쓰기 v6(진행 확장 v2),
// 읽기 v3·v4·v5·v6(진행 확장 v1·v2), v5 이하 런 튜닝은 회수 손실률로 이관
#include "Game/Save/DigetherSaveRules.h"

#include "Game/Hub/DigetherHubRules.h"
#include "Game/Save/DigetherSaveRulesInternal.h"
#include "Game/Save/DigetherSaveTuning.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace
{
	// wire v4부터 v3 기본 본문 뒤에 붙는 블록, v3 슬롯 이름·진행 값 스키마 유지
	constexpr uint32 HubExtensionMagic = 0x31425548; // HUB1
	// v1: Hub 진행, v2: v1 뒤에 바닥 금화 목록(진행, 기준점이 있으면 기준점 순), v1을 읽으면 금화는 빈 목록
	constexpr int32 HubExtensionVersion = 2;
	constexpr int32 FloorCoinExtensionVersion = 2;
	// wire v5부터 Hub 확장 뒤에 붙는 블록, v1: 저장 시각, 진행(·기준점) 전체 일차
	// v2: v1 뒤에 진행(·기준점)마다 결과 통계(플레이어 누적 획득·복구·표시 이름·참가 순서)와 게임 결과 기록
	constexpr uint32 ProgressExtensionMagic = 0x31475250; // PRG1
	constexpr int32 ProgressExtensionVersion = 2;
	constexpr int32 GameResultExtensionVersion = 2;

	// 할당 전에 문자열·배열 길이 제한, 손상 파일에 FArchive 기본 컨테이너 로드 사용 금지
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
			// FTransform 엔진 내부 표현 대신 v3 좌표·회전·크기 성분으로 고정
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
		// 마지막 칸: wire v6부터 회수 손실률, v5 이하는 시체 복구율(읽은 뒤 MigrateLegacyTuning이 이관)
		Archive << Tuning.ProfileVersion << Tuning.InitialQuotaCredits << Tuning.RunSeconds
			<< Tuning.InteractionDistanceCm << Tuning.ElevatorCapacityKg << Tuning.ElevatorTripLimit
			<< Tuning.AscentPenaltySeconds << Tuning.ElevatorTravelSeconds << Tuning.DepositIntervalSeconds
			<< Tuning.EmergencyWeightLimitKg << Tuning.DeathLossPercent << Tuning.RecoveredDeathLossPercent;
	}

	// wire v5 이하 튜닝 마지막 칸을 복구율로 재해석해 순 손실률로 이관, Game 코드는 복구율 미사용이라 0으로 비움
	void MigrateLegacyTuning(FDigetherRunTuning& Tuning)
	{
		Tuning.CorpseRefundPercent = Tuning.RecoveredDeathLossPercent;
		Tuning.RecoveredDeathLossPercent = INDEX_NONE;
		Tuning.MigrateLegacyCorpseRefund();
		Tuning.CorpseRefundPercent = 0;
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
		// FName 생성 전에 엔진 이름 길이·제어 문자 거절
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
		// 플레이어 거점 변환과 같이 FTransform 내부 표현 대신 좌표·회전·크기 성분으로 고정
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
		// 모르는 미래 확장은 본문 해석·기존 슬롯 덮어쓰기 금지
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
		// v2는 진행마다 바닥 금화 개수(int32) 추가
		const int32 MinimumProgressBytes = static_cast<int32>(sizeof(uint8) + 4 * sizeof(int32)
			+ (Version >= FloorCoinExtensionVersion ? sizeof(int32) : 0));
		const int32 MinimumPayloadSize = MinimumProgressBytes * (Slot.bHasDepartureCheckpoint ? 2 : 1);
		// wire v5부터는 이 블록 뒤에 진행 확장이 이어져 남은 길이 이하만 확인, 블록 뒤 끝 검사는 호출자 담당
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
		// 저장 시각(int64) + 진행 전체 일차(int32) [+ 기준점 전체 일차(int32)]
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

	// 진행 확장 v2의 스냅샷별 값, 플레이어 통계는 기본 본문 플레이어 배열과 같은 순서·개수
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

	// wire v5부터 Hub 확장 뒤에 붙는 블록, v3 기본 본문 고정 배치를 유지하려고 새 값은 여기에 추가
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
		// Hub 확장과 같이 모르는 미래 확장은 본문을 해석하지 않고 보존
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
		// 진행 확장은 마지막 블록, 길이가 정확히 맞아야 하며 뒤에 남는 바이트 불허
		// v1은 고정 길이, v2는 결과 통계·기록으로 길이가 달라져 하한만 고정
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
		// v1에는 결과 통계·기록 없음, 누적 획득·복구 0, 표시 이름 빈 값, 참가 순서 0(알 수 없음)
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

	// wire v3·v4에는 전체 일차 없음, 이전 기간의 지난 날 수는 복원 불가라 확실한 하한인 기간 일차 사용
	// 전체 일차 ≥ 기간 일차(같은 기간 안에서 함께 증가), 첫 기간이면 동일, 저장 시각은 알 수 없음(0)
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

EDigetherSaveResult DigetherSaveRules::SerializeSlot(const FDigetherSaveSlotData& Slot, TArray<uint8>& OutBytes)
{
	const EDigetherSaveResult Validation = DigetherSaveRulesInternal::ValidateSlot(Slot);
	if (Validation != EDigetherSaveResult::Success)
	{
		return Validation;
	}
	TArray<uint8> Candidate;
	FMemoryWriter Writer(Candidate, true);
	// 항상 최신 wire로 쓰기, 구 v3·v4 실행 파일은 첫 버전만 보고 UnsupportedVersion 반환
	// 그래서 전체 일차·Hub 재산을 모르는 실행 파일이 이 슬롯을 덮어쓰지 못함
	int32 Version = DigetherSaveWire::LatestVersion;
	Writer << Version;
	FDigetherSaveSlotData Copy = Slot;
	SerializeBoolean(Writer, Copy.bHasDepartureCheckpoint);
	SerializeSnapshot(Writer, Copy.Progress);
	if (Copy.bHasDepartureCheckpoint)
	{
		SerializeSnapshot(Writer, Copy.DepartureCheckpoint);
	}
	// v5는 Hub 진행이 비초기화여도 확장을 씀, 블록 위치가 v4와 같아 Hub 확장 해석 공유
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
		&& Version != DigetherSaveWire::DayVersion && Version != DigetherSaveWire::RecoveredDeathLossVersion)
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
			// v4는 Hub 확장이 마지막 블록, Hub 재산이 있을 때만 기록됨
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
	if (Version < DigetherSaveWire::DayVersion)
	{
		ApplyLegacyProgressEstimate(Candidate);
	}
	if (Version < DigetherSaveWire::RecoveredDeathLossVersion)
	{
		MigrateLegacyTuning(Candidate.Progress.RunTuning);
		if (Candidate.bHasDepartureCheckpoint)
		{
			MigrateLegacyTuning(Candidate.DepartureCheckpoint.RunTuning);
		}
	}
	const EDigetherSaveResult Validation = DigetherSaveRulesInternal::ValidateSlot(Candidate);
	if (Validation != EDigetherSaveResult::Success)
	{
		return Validation;
	}
	OutSlot = MoveTemp(Candidate);
	return EDigetherSaveResult::Success;
}
