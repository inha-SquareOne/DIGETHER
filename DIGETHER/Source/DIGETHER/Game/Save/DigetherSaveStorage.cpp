#include "Game/Save/DigetherSaveStorage.h"

#include "Game/Save/DigetherSaveGame.h"
#include "Game/Save/DigetherSaveLog.h"
#include "Game/Save/DigetherSaveRules.h"
#include "Game/Save/DigetherSaveTuning.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/EngineVersion.h"
#include "Misc/SecureHash.h"
#include "Serialization/CustomVersion.h"
#include "Serialization/MemoryReader.h"
#include "UObject/ObjectVersion.h"

namespace
{
	// 일반 파일 백엔드는 이 값을 무시한다. 계정 분리는 인증 키를 해시한 슬롯 이름이 담당한다.
	constexpr int32 SaveUserIndex = 0;

	EDigetherSaveResult ValidateEngineHeader(const TArray<uint8>& Bytes, int64& OutHeaderEnd)
	{
		FMemoryReader Reader(Bytes, true);
		Reader.ArMaxSerializeSize = DigetherSaveTuning::MaxStringLength * sizeof(TCHAR);
		int32 FileType = 0;
		int32 FileVersion = 0;
		Reader << FileType << FileVersion;
		// UE 5.8.4 GameplayStatics.cpp의 FSaveGameHeader와 같은 형식만 읽는다.
		// StripSaveGameHeader는 미지의 custom-version format을 assert하므로 먼저 길이·형식을 검사한다.
		if (Reader.IsError() || FileType != 0x53415647)
		{
			return EDigetherSaveResult::CorruptData;
		}
		if (FileVersion != 3)
		{
			return FileVersion > 3 ? EDigetherSaveResult::UnsupportedVersion : EDigetherSaveResult::CorruptData;
		}
		FPackageFileVersion PackageVersion;
		FEngineVersion EngineVersion;
		int32 CustomVersionFormat = 0;
		int32 CustomVersionCount = 0;
		Reader << PackageVersion;
		Reader << EngineVersion;
		Reader << CustomVersionFormat;
		if (Reader.IsError())
		{
			return EDigetherSaveResult::CorruptData;
		}
		if (CustomVersionFormat != static_cast<int32>(ECustomVersionSerializationFormat::Optimized))
		{
			// 더 큰 엔진 형식만 미래 스키마처럼 보존한다. 이전 형식은 읽지 않고 손상으로 분류한다.
			return CustomVersionFormat > static_cast<int32>(ECustomVersionSerializationFormat::Optimized)
				? EDigetherSaveResult::UnsupportedVersion : EDigetherSaveResult::CorruptData;
		}
		Reader << CustomVersionCount;
		constexpr int64 CustomVersionEntryBytes = sizeof(FGuid) + sizeof(int32);
		if (Reader.IsError() || CustomVersionCount < 0
			|| static_cast<int64>(CustomVersionCount) * CustomVersionEntryBytes > Reader.TotalSize() - Reader.Tell())
		{
			return EDigetherSaveResult::CorruptData;
		}
		Reader.Seek(Reader.Tell() + static_cast<int64>(CustomVersionCount) * CustomVersionEntryBytes);
		FString ClassName;
		Reader << ClassName;
		if (Reader.IsError() || ClassName != UDigetherSaveGame::StaticClass()->GetPathName())
		{
			return EDigetherSaveResult::CorruptData;
		}
		OutHeaderEnd = Reader.Tell();
		return EDigetherSaveResult::Success;
	}

	EDigetherSaveResult DecodeEngineSave(const TArray<uint8>& Bytes, const FString& HostKey,
		FDigetherSaveSlotData& OutSlot)
	{
		if (Bytes.IsEmpty() || Bytes.Num() > DigetherSaveTuning::MaxSaveBytes)
		{
			return EDigetherSaveResult::CorruptData;
		}
		int64 HeaderEnd = 0;
		const EDigetherSaveResult HeaderResult = ValidateEngineHeader(Bytes, HeaderEnd);
		if (HeaderResult != EDigetherSaveResult::Success)
		{
			return HeaderResult;
		}
		FMemoryReader Reader = UGameplayStatics::StripSaveGameHeader(Bytes);
		if (Reader.IsError() || Reader.Tell() != HeaderEnd)
		{
			return EDigetherSaveResult::CorruptData;
		}
		// LoadGameFromMemory는 파일의 클래스에 LoadObject·NewObject를 실행한다. 고정 클래스만 생성한다.
		UDigetherSaveGame* SaveGame = NewObject<UDigetherSaveGame>();
		SaveGame->Serialize(Reader);
		const EDigetherSaveResult Result = SaveGame->GetDataResult();
		if (Result == EDigetherSaveResult::UnsupportedVersion)
		{
			return Result;
		}
		if (Reader.IsError() || Reader.Tell() != Reader.TotalSize())
		{
			return EDigetherSaveResult::CorruptData;
		}
		if (Result != EDigetherSaveResult::Success)
		{
			return Result;
		}
		if (SaveGame->GetSlotData().Progress.HostAuthenticatedPlayerKey != HostKey)
		{
			return EDigetherSaveResult::HostIdentityMismatch;
		}
		OutSlot = SaveGame->GetSlotData();
		return EDigetherSaveResult::Success;
	}

	EDigetherSaveResult VerifyWrittenSlot(const FString& SlotName, const FString& HostKey,
		const TArray<uint8>& ExpectedPayload)
	{
		FDigetherSaveSlotData ReadBack;
		const EDigetherSaveResult Result = DigetherSaveStorage::ReadSlot(SlotName, HostKey, ReadBack);
		if (Result != EDigetherSaveResult::Success)
		{
			return Result;
		}
		TArray<uint8> ReadBackPayload;
		return DigetherSaveRules::SerializeSlot(ReadBack, ReadBackPayload) == EDigetherSaveResult::Success
			&& ReadBackPayload == ExpectedPayload ? EDigetherSaveResult::Success : EDigetherSaveResult::IoFailure;
	}

	// 한 슬롯의 쓰기와 읽기 검증이다. 쓰기 실패는 IoFailure, 검증 실패는 검증 결과를 돌려준다.
	EDigetherSaveResult WriteAndVerifySlot(const FString& SlotName, const FString& HostKey,
		const TArray<uint8>& Bytes, const TArray<uint8>& ExpectedPayload)
	{
		if (!UGameplayStatics::SaveDataToSlot(Bytes, SlotName, SaveUserIndex))
		{
			return EDigetherSaveResult::IoFailure;
		}
		return VerifyWrittenSlot(SlotName, HostKey, ExpectedPayload);
	}

	EDigetherSaveResult ValidateWriteRequest(const FDigetherSaveSlotNames& Names, const FString& HostKey,
		const FDigetherSaveSlotData& Slot)
	{
		if (!IsInGameThread() || Names.Primary.IsEmpty() || Names.Backup.IsEmpty() || Names.Primary == Names.Backup)
		{
			return EDigetherSaveResult::InvalidState;
		}
		if (Slot.Progress.HostAuthenticatedPlayerKey != HostKey)
		{
			return EDigetherSaveResult::HostIdentityMismatch;
		}
		return EDigetherSaveResult::Success;
	}
}

FDigetherSaveSlotNames DigetherSaveStorage::MakeSlotNames(const FString& HostKey, const int32 SlotIndex)
{
	FDigetherSaveSlotNames Names;
	if (!DigetherSaveRules::IsValidSlotIndex(SlotIndex))
	{
		return Names;
	}
	const FTCHARToUTF8 HostBytes(*HostKey);
	const FSHAHash HostHash = FSHA1::HashBuffer(HostBytes.Get(), HostBytes.Length());
	// 확장자·저장 경로는 엔진이 정한다. v2 슬롯과 인증 ID 원문을 노출하는 이름은 사용하지 않는다.
	// 슬롯 0은 "Digether_HostProgress_v3_<해시>"로 슬롯 도입 전 파일을 그대로 찾는다. 슬롯 1 이상은 "_S<번호>"를 붙인다.
	// 이름의 버전은 SlotNameVersion이다. 스키마·wire 버전을 올려도 이름이 바뀌지 않는다.
	Names.Primary = FString::Printf(TEXT("Digether_HostProgress_v%d_"), DigetherSaveTuning::SlotNameVersion)
		+ HostHash.ToString();
	if (SlotIndex > 0)
	{
		Names.Primary += FString::Printf(TEXT("_S%d"), SlotIndex);
	}
	// 백업은 주 이름 뒤에 붙인다. 슬롯 0 백업("…_Backup")과 슬롯 N 주("…_SN")는 접미사가 달라 겹치지 않는다.
	Names.Backup = Names.Primary + TEXT("_Backup");
	return Names;
}

EDigetherSaveResult DigetherSaveStorage::ReadSlot(const FString& SlotName, const FString& HostKey,
	FDigetherSaveSlotData& OutSlot)
{
	if (!IsInGameThread() || SlotName.IsEmpty() || !DigetherSaveRules::IsValidAuthenticatedPlayerKey(HostKey))
	{
		return EDigetherSaveResult::InvalidState;
	}
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, SaveUserIndex))
	{
		return EDigetherSaveResult::NotFound;
	}
	TArray<uint8> Bytes;
	if (!UGameplayStatics::LoadDataFromSlot(Bytes, SlotName, SaveUserIndex))
	{
		return EDigetherSaveResult::IoFailure;
	}
	return DecodeEngineSave(Bytes, HostKey, OutSlot);
}

FDigetherSaveSlotReads DigetherSaveStorage::ReadSlots(const FDigetherSaveSlotNames& Names, const FString& HostKey)
{
	FDigetherSaveSlotReads Reads;
	Reads.PrimaryResult = ReadSlot(Names.Primary, HostKey, Reads.Primary);
	// 주 슬롯이 유효해도 백업의 미래 버전 보호 여부를 반드시 확인한다.
	Reads.BackupResult = ReadSlot(Names.Backup, HostKey, Reads.Backup);
	return Reads;
}

EDigetherSaveResult DigetherSaveStorage::Read(const FDigetherSaveSlotNames& Names, const FString& HostKey,
	FDigetherSaveSlotData& OutSlot, bool& bOutRecoveredFromBackup)
{
	return Read(ReadSlots(Names, HostKey), OutSlot, bOutRecoveredFromBackup);
}

EDigetherSaveResult DigetherSaveStorage::Read(const FDigetherSaveSlotReads& Reads,
	FDigetherSaveSlotData& OutSlot, bool& bOutRecoveredFromBackup)
{
	bool bUseBackup = false;
	const EDigetherSaveResult Result = DigetherSaveRules::SelectReadableSlot(Reads.PrimaryResult, Reads.BackupResult,
		bUseBackup);
	if (Result == EDigetherSaveResult::Success)
	{
		OutSlot = bUseBackup ? Reads.Backup : Reads.Primary;
		bOutRecoveredFromBackup = bUseBackup;
		if (bUseBackup)
		{
			UE_LOG(LogDigetherSave, Warning, TEXT("주 저장 슬롯이 없거나 손상되어 내부 백업에서 진행을 복구했습니다."));
		}
	}
	return Result;
}

EDigetherSaveResult DigetherSaveStorage::Write(const FDigetherSaveSlotNames& Names, const FString& HostKey,
	const FDigetherSaveSlotData& Slot, const bool bExplicitNewGame)
{
	const EDigetherSaveResult RequestResult = ValidateWriteRequest(Names, HostKey, Slot);
	if (RequestResult != EDigetherSaveResult::Success)
	{
		return RequestResult;
	}
	return Write(Names, HostKey, Slot, bExplicitNewGame, ReadSlots(Names, HostKey));
}

EDigetherSaveResult DigetherSaveStorage::Write(const FDigetherSaveSlotNames& Names, const FString& HostKey,
	const FDigetherSaveSlotData& Slot, const bool bExplicitNewGame, const FDigetherSaveSlotReads& Reads)
{
	const EDigetherSaveResult RequestResult = ValidateWriteRequest(Names, HostKey, Slot);
	if (RequestResult != EDigetherSaveResult::Success)
	{
		return RequestResult;
	}
	const EDigetherSaveResult Permission = DigetherSaveRules::CanWriteSlots(Reads.PrimaryResult, Reads.BackupResult,
		bExplicitNewGame);
	if (Permission != EDigetherSaveResult::Success)
	{
		return Permission;
	}
	UDigetherSaveGame* SaveGame = NewObject<UDigetherSaveGame>();
	const EDigetherSaveResult SetResult = SaveGame->SetSlotData(Slot);
	if (SetResult != EDigetherSaveResult::Success)
	{
		return SetResult;
	}
	// SetSlotData가 만든 바이트를 재사용한다. SaveGameToMemory 안의 Serialize도 같은 바이트를 쓴다.
	TArray<uint8> ExpectedPayload;
	const EDigetherSaveResult EncodeResult = SaveGame->EncodeToMemory(ExpectedPayload);
	if (EncodeResult != EDigetherSaveResult::Success)
	{
		return EncodeResult;
	}
	TArray<uint8> Bytes;
	if (!UGameplayStatics::SaveGameToMemory(SaveGame, Bytes) || Bytes.Num() > DigetherSaveTuning::MaxSaveBytes)
	{
		return EDigetherSaveResult::InvalidState;
	}
	// SaveGameToMemory의 true는 Archive 오류까지 보증하지 않는다. 디스크에 쓰기 전에 자체 결과도 확인한다.
	FDigetherSaveSlotData EncodedSlot;
	const EDigetherSaveResult MemoryResult = DecodeEngineSave(Bytes, HostKey, EncodedSlot);
	if (MemoryResult != EDigetherSaveResult::Success)
	{
		return MemoryResult;
	}
	// 평소에는 백업 → 주 순서다. 주 슬롯이 무효이고 백업만 유효하면 그 유일한 유효본을 지키려고 주 → 백업 순서로 쓴다.
	// 먼저 쓴 슬롯의 쓰기·검증이 성공하기 전에는 나중 슬롯을 건드리지 않는다(EDigetherSaveWriteStep 순서와 같다).
	const bool bPrimaryFirst = DigetherSaveRules::ShouldWritePrimaryFirst(Reads.PrimaryResult, Reads.BackupResult);
	const FString& FirstSlot = bPrimaryFirst ? Names.Primary : Names.Backup;
	const FString& SecondSlot = bPrimaryFirst ? Names.Backup : Names.Primary;
	const EDigetherSaveResult FirstResult = WriteAndVerifySlot(FirstSlot, HostKey, Bytes, ExpectedPayload);
	if (FirstResult != EDigetherSaveResult::Success)
	{
		return FirstResult;
	}
	return WriteAndVerifySlot(SecondSlot, HostKey, Bytes, ExpectedPayload);
}
