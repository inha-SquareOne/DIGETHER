#include "Game/Save/DigetherSaveGame.h"

#include "Game/Save/DigetherSaveRules.h"
#include "Game/Save/DigetherSaveTuning.h"
#include "Misc/Crc.h"

void UDigetherSaveGame::Serialize(FArchive& Archive)
{
	if (!Archive.IsPersistent() || (!Archive.IsLoading() && !Archive.IsSaving()))
	{
		Super::Serialize(Archive);
		return;
	}

	// 영구 저장은 고정 값 codec만 사용, 파일의 UObject 태그·참조를 로드하지 않음
	TArray<uint8> Payload;
	if (Archive.IsSaving() && EncodeToMemory(Payload) != EDigetherSaveResult::Success)
	{
		Archive.SetError();
		return;
	}
	int32 PayloadSize = Payload.Num();
	Archive << PayloadSize;
	if (Archive.IsError() || PayloadSize < static_cast<int32>(sizeof(int32))
		|| PayloadSize > DigetherSaveTuning::MaxSaveBytes
		|| (Archive.IsLoading() && static_cast<int64>(PayloadSize) + static_cast<int64>(sizeof(uint32))
			> Archive.TotalSize() - Archive.Tell()))
	{
		DataResult = EDigetherSaveResult::CorruptData;
		Archive.SetError();
		return;
	}
	if (Archive.IsLoading())
	{
		Payload.SetNumUninitialized(PayloadSize);
	}
	Archive.Serialize(Payload.GetData(), PayloadSize);
	uint32 Checksum = Archive.IsSaving() ? FCrc::MemCrc32(Payload.GetData(), Payload.Num()) : 0;
	Archive << Checksum;
	if (Archive.IsLoading())
	{
		DataResult = EDigetherSaveResult::CorruptData;
		if (Archive.IsError())
		{
			return;
		}
		FDigetherSaveSlotData Candidate;
		const EDigetherSaveResult Result = DigetherSaveRules::DeserializeSlot(Payload, Candidate);
		// 미래 스키마는 CRC 손상이 있어도 보호, 지원하지 않는 데이터를 초기화하지 않음
		if (Result == EDigetherSaveResult::UnsupportedVersion)
		{
			DataResult = Result;
			return;
		}
		if (Checksum != FCrc::MemCrc32(Payload.GetData(), Payload.Num()))
		{
			return;
		}
		DataResult = Result;
		if (Result == EDigetherSaveResult::Success)
		{
			Data = MoveTemp(Candidate);
			EncodedPayload.Reset();
		}
	}
}

EDigetherSaveResult UDigetherSaveGame::SetSlotData(const FDigetherSaveSlotData& Slot)
{
	TArray<uint8> ValidatedBytes;
	const EDigetherSaveResult Result = DigetherSaveRules::SerializeSlot(Slot, ValidatedBytes);
	if (Result == EDigetherSaveResult::Success)
	{
		Data = Slot;
		DataResult = Result;
		EncodedPayload = MoveTemp(ValidatedBytes);
	}
	return Result;
}

const FDigetherSaveSlotData& UDigetherSaveGame::GetSlotData() const
{
	return Data;
}

EDigetherSaveResult UDigetherSaveGame::EncodeToMemory(TArray<uint8>& OutBytes) const
{
	// SerializeSlot은 같은 Data에 같은 바이트, 캐시는 성공 결과만 보관
	if (EncodedPayload.IsSet())
	{
		OutBytes = EncodedPayload.GetValue();
		return EDigetherSaveResult::Success;
	}
	return DigetherSaveRules::SerializeSlot(Data, OutBytes);
}

EDigetherSaveResult UDigetherSaveGame::DecodeFromMemory(const TArray<uint8>& Bytes)
{
	EncodedPayload.Reset();
	DataResult = DigetherSaveRules::DeserializeSlot(Bytes, Data);
	return DataResult;
}
