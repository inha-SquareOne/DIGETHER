#if WITH_DEV_AUTOMATION_TESTS

// 아래 숫자·ID는 테스트 픽스처, 기획 값 아님

#include "Game/Save/DigetherSaveGame.h"
#include "Game/Save/DigetherSaveRules.h"
#include "Game/Save/DigetherSaveStorage.h"
#include "Game/Save/DigetherSaveTuning.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/EngineVersion.h"
#include "Misc/ScopeExit.h"
#include "Misc/SecureHash.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/ObjectVersion.h"

namespace
{
	constexpr int32 StorageTestUserIndex = 0;
	const FString StorageTestHostKey = TEXT("SAVE_STORAGE_TEST:host");

	FDigetherSaveSlotNames MakeTemporarySlotNames()
	{
		// 호스트 실제 슬롯 이름 미사용, 반복 실행·다른 테스트와 격리
		const FString Prefix = TEXT("Digether_Automation_Save_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
		return { Prefix + TEXT("_Primary"), Prefix + TEXT("_Backup") };
	}

	void DeleteTemporarySlots(FAutomationTestBase& Test, const FDigetherSaveSlotNames& Names)
	{
		for (const FString& SlotName : { Names.Primary, Names.Backup })
		{
			if (UGameplayStatics::DoesSaveGameExist(SlotName, StorageTestUserIndex))
			{
				Test.TestTrue(FString::Printf(TEXT("임시 슬롯 삭제 성공: %s"), *SlotName),
					UGameplayStatics::DeleteGameInSlot(SlotName, StorageTestUserIndex));
				Test.TestFalse(FString::Printf(TEXT("삭제 후 임시 슬롯 없음: %s"), *SlotName),
					UGameplayStatics::DoesSaveGameExist(SlotName, StorageTestUserIndex));
			}
		}
	}

	FDigetherSaveSlotData MakeStorageSlot(const int32 HostCredits = 70)
	{
		// 수치·ID는 저장 계약 검사용 픽스처, 기획 값 아님
		FDigetherSaveSnapshot Snapshot;
		Snapshot.HostAuthenticatedPlayerKey = StorageTestHostKey;
		Snapshot.GameId = FGuid(1, 2, 3, 4);
		Snapshot.RunId = FGuid(5, 6, 7, 8);
		Snapshot.Quota.Quota.Day = 1;
		Snapshot.Quota.Quota.TargetCredits = 100;
		Snapshot.RunTuning.SourceProfile = FPrimaryAssetId(TEXT("DigetherTuningProfile:DA_SaveStorageFixture"));
		Snapshot.RunTuning.ProfileVersion = 7;
		Snapshot.RunTuning.InitialQuotaCredits = 100;
		Snapshot.RunTuning.RunSeconds = 120.f;
		Snapshot.RunTuning.InteractionDistanceCm = 200.f;
		Snapshot.RunTuning.ElevatorCapacityKg = 500.f;
		Snapshot.RunTuning.ElevatorTripLimit = 2;
		Snapshot.RunTuning.AscentPenaltySeconds = 10.f;
		Snapshot.RunTuning.ElevatorTravelSeconds = 2.f;
		Snapshot.RunTuning.DepositIntervalSeconds = 0.5f;
		Snapshot.RunTuning.EmergencyWeightLimitKg = 3.f;
		Snapshot.RunTuning.DeathLossPercent = 100;
		Snapshot.RunTuning.CorpseRefundPercent = 50;
		FDigetherSavePlayerRecord Host;
		Host.AuthenticatedPlayerKey = StorageTestHostKey;
		Host.GameplayPlayerId = FGuid(11, 12, 13, 14);
		Host.Wallet.BalanceCredits = HostCredits;
		Host.Wallet.HiddenLifetimeCredits = 15;
		Host.bHasHubTransform = true;
		Host.HubTransform = FTransform(FRotator(5.0, 30.0, -10.0), FVector(10.0, 20.0, 30.0), FVector(1.0, 2.0, 1.5));
		Snapshot.Players.Add(Host);
		FDigetherSavePlayerRecord Offline;
		Offline.AuthenticatedPlayerKey = TEXT("SAVE_STORAGE_TEST:offline");
		Offline.GameplayPlayerId = FGuid(21, 22, 23, 24);
		Offline.Wallet.BalanceCredits = 90;
		Snapshot.Players.Add(Offline);
		Snapshot.CompletedSettlementRunIds.Add(FGuid(31, 32, 33, 34));
		FDigetherSaveSlotData Slot;
		Slot.Progress = Snapshot;
		Slot.DepartureCheckpoint = Snapshot;
		Slot.bHasDepartureCheckpoint = true;
		return Slot;
	}

	bool TestStorageSlotEquals(FAutomationTestBase& Test, const FDigetherSaveSlotData& Expected,
		const FDigetherSaveSlotData& Actual)
	{
		TArray<uint8> ExpectedPayload;
		TArray<uint8> ActualPayload;
		const bool bExpectedEncoded = Test.TestTrue(TEXT("비교할 기대 슬롯 직렬화"),
			DigetherSaveRules::SerializeSlot(Expected, ExpectedPayload) == EDigetherSaveResult::Success);
		const bool bActualEncoded = Test.TestTrue(TEXT("비교할 실제 슬롯 직렬화"),
			DigetherSaveRules::SerializeSlot(Actual, ActualPayload) == EDigetherSaveResult::Success);
		return bExpectedEncoded && bActualEncoded
			&& Test.TestTrue(TEXT("진행·기준점의 모든 저장 필드 일치"), ExpectedPayload == ActualPayload);
	}

	bool MakeEngineSaveBytes(FAutomationTestBase& Test, const FDigetherSaveSlotData& Slot,
		TArray<uint8>& OutBytes)
	{
		UDigetherSaveGame* SaveGame = NewObject<UDigetherSaveGame>();
		if (!Test.TestTrue(TEXT("엔진 SaveGame에 유효한 v3 값 설정"),
			SaveGame->SetSlotData(Slot) == EDigetherSaveResult::Success))
		{
			return false;
		}
		return Test.TestTrue(TEXT("엔진 헤더를 포함한 메모리 저장"), UGameplayStatics::SaveGameToMemory(SaveGame, OutBytes));
	}

	bool SaveRawSlot(FAutomationTestBase& Test, const FString& SlotName, const TArray<uint8>& Bytes)
	{
		return Test.TestTrue(TEXT("엔진 실제 슬롯에 시험 바이트 저장"),
			UGameplayStatics::SaveDataToSlot(Bytes, SlotName, StorageTestUserIndex));
	}

	bool TestRawSlotEquals(FAutomationTestBase& Test, const FString& SlotName, const TArray<uint8>& Expected)
	{
		TArray<uint8> Actual;
		if (!Test.TestTrue(TEXT("엔진 실제 슬롯의 바이트 읽기"),
			UGameplayStatics::LoadDataFromSlot(Actual, SlotName, StorageTestUserIndex)))
		{
			return false;
		}
		return Test.TestTrue(FString::Printf(TEXT("슬롯 바이트 보존: %s"), *SlotName), Actual == Expected);
	}

	bool FindUniqueBytePattern(FAutomationTestBase& Test, const TArray<uint8>& Bytes,
		const TArray<uint8>& Pattern, int32& OutOffset)
	{
		if (!Test.TestFalse(TEXT("찾을 바이트 패턴은 비어 있지 않음"), Pattern.IsEmpty()))
		{
			return false;
		}
		int32 MatchCount = 0;
		for (int32 Index = 0; Index <= Bytes.Num() - Pattern.Num(); ++Index)
		{
			if (FMemory::Memcmp(Bytes.GetData() + Index, Pattern.GetData(), Pattern.Num()) == 0)
			{
				OutOffset = Index;
				++MatchCount;
			}
		}
		return Test.TestEqual(TEXT("엔진 blob에서 변조 대상 패턴을 하나만 찾음"), MatchCount, 1);
	}

	bool MakeSchemaVersionEngineSaveBytes(FAutomationTestBase& Test, const FDigetherSaveSlotData& Slot,
		const int32 Version, TArray<uint8>& OutBytes)
	{
		TArray<uint8> Payload;
		if (!MakeEngineSaveBytes(Test, Slot, OutBytes)
			|| !Test.TestTrue(TEXT("버전 주입용 현재 payload 생성"),
				DigetherSaveRules::SerializeSlot(Slot, Payload) == EDigetherSaveResult::Success))
		{
			return false;
		}
		int32 PayloadOffset = INDEX_NONE;
		if (!FindUniqueBytePattern(Test, OutBytes, Payload, PayloadOffset))
		{
			return false;
		}
		// 엔진 헤더·클래스는 유지, 스키마만 변경, 미래 버전 보호가 CRC 검사보다 우선
		FMemory::Memcpy(OutBytes.GetData() + PayloadOffset, &Version, sizeof(Version));
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSaveStorageRoundTripTest,
	"DIGETHER.Game.Save.Storage.EngineMemoryAndSlotRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSaveStorageRoundTripTest::RunTest(const FString& Parameters)
{
	const FDigetherSaveSlotNames Names = MakeTemporarySlotNames();
	ON_SCOPE_EXIT { DeleteTemporarySlots(*this, Names); };
	const FDigetherSaveSlotData Slot = MakeStorageSlot();
	TArray<uint8> Bytes;
	if (!MakeEngineSaveBytes(*this, Slot, Bytes))
	{
		return false;
	}
	UDigetherSaveGame* MemoryLoaded = Cast<UDigetherSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("엔진 메모리 SaveGame 왕복"), MemoryLoaded))
	{
		return false;
	}
	TestTrue(TEXT("메모리 복원 값과 CRC 유효"), MemoryLoaded->GetDataResult() == EDigetherSaveResult::Success);
	TestStorageSlotEquals(*this, Slot, MemoryLoaded->GetSlotData());
	if (!TestTrue(TEXT("빈 주·백업 실제 슬롯에 저장"),
		DigetherSaveStorage::Write(Names, StorageTestHostKey, Slot, false) == EDigetherSaveResult::Success))
	{
		return false;
	}
	for (const FString& SlotName : { Names.Primary, Names.Backup })
	{
		FDigetherSaveSlotData Loaded;
		if (TestTrue(TEXT("주·백업 슬롯 개별 복원"),
			DigetherSaveStorage::ReadSlot(SlotName, StorageTestHostKey, Loaded) == EDigetherSaveResult::Success))
		{
			TestStorageSlotEquals(*this, Slot, Loaded);
		}
	}
	FDigetherSaveSlotData Loaded;
	bool bRecovered = true;
	if (TestTrue(TEXT("저장 계층으로 실제 슬롯 복원"),
		DigetherSaveStorage::Read(Names, StorageTestHostKey, Loaded, bRecovered) == EDigetherSaveResult::Success))
	{
		TestFalse(TEXT("정상 주 슬롯 읽기는 백업 복구 아님"), bRecovered);
		TestStorageSlotEquals(*this, Slot, Loaded);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSaveStoragePrimaryPreferenceTest,
	"DIGETHER.Game.Save.Storage.PrimaryWinsWhenBothValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSaveStoragePrimaryPreferenceTest::RunTest(const FString& Parameters)
{
	const FDigetherSaveSlotNames Names = MakeTemporarySlotNames();
	ON_SCOPE_EXIT { DeleteTemporarySlots(*this, Names); };
	const FDigetherSaveSlotData Primary = MakeStorageSlot(70);
	const FDigetherSaveSlotData Backup = MakeStorageSlot(90);
	TArray<uint8> PrimaryBytes;
	TArray<uint8> BackupBytes;
	if (!MakeEngineSaveBytes(*this, Primary, PrimaryBytes) || !MakeEngineSaveBytes(*this, Backup, BackupBytes)
		|| !SaveRawSlot(*this, Names.Primary, PrimaryBytes) || !SaveRawSlot(*this, Names.Backup, BackupBytes))
	{
		return false;
	}
	FDigetherSaveSlotData Loaded;
	bool bRecovered = true;
	if (TestTrue(TEXT("양쪽 유효 시 읽기 성공"),
		DigetherSaveStorage::Read(Names, StorageTestHostKey, Loaded, bRecovered) == EDigetherSaveResult::Success))
	{
		TestFalse(TEXT("양쪽 유효 시 백업 복구 표시 제거"), bRecovered);
		TestStorageSlotEquals(*this, Primary, Loaded);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSaveStorageMissingPrimaryTest,
	"DIGETHER.Game.Save.Storage.MissingPrimaryRecoversBackup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSaveStorageMissingPrimaryTest::RunTest(const FString& Parameters)
{
	const FDigetherSaveSlotNames Names = MakeTemporarySlotNames();
	ON_SCOPE_EXIT { DeleteTemporarySlots(*this, Names); };
	const FDigetherSaveSlotData Backup = MakeStorageSlot(90);
	TArray<uint8> BackupBytes;
	if (!MakeEngineSaveBytes(*this, Backup, BackupBytes) || !SaveRawSlot(*this, Names.Backup, BackupBytes))
	{
		return false;
	}
	FDigetherSaveSlotData Loaded = MakeStorageSlot(777);
	const FDigetherSaveSlotData Sentinel = Loaded;
	TestTrue(TEXT("없는 주 슬롯은 NotFound"),
		DigetherSaveStorage::ReadSlot(Names.Primary, StorageTestHostKey, Loaded) == EDigetherSaveResult::NotFound);
	TestStorageSlotEquals(*this, Sentinel, Loaded);
	bool bRecovered = false;
	AddExpectedMessagePlain(TEXT("내부 백업에서 진행을 복구했습니다."), ELogVerbosity::Warning);
	if (TestTrue(TEXT("주 슬롯 누락 시 백업에서 읽기"),
		DigetherSaveStorage::Read(Names, StorageTestHostKey, Loaded, bRecovered) == EDigetherSaveResult::Success))
	{
		TestTrue(TEXT("주 슬롯 누락의 백업 복구 표시"), bRecovered);
		TestStorageSlotEquals(*this, Backup, Loaded);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSaveStorageCorruptPrimaryTest,
	"DIGETHER.Game.Save.Storage.CorruptPrimaryRecoversBackup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSaveStorageCorruptPrimaryTest::RunTest(const FString& Parameters)
{
	const FDigetherSaveSlotNames Names = MakeTemporarySlotNames();
	ON_SCOPE_EXIT { DeleteTemporarySlots(*this, Names); };
	const FDigetherSaveSlotData Slot = MakeStorageSlot(90);
	TArray<uint8> BackupBytes;
	if (!MakeEngineSaveBytes(*this, Slot, BackupBytes) || !SaveRawSlot(*this, Names.Backup, BackupBytes))
	{
		return false;
	}
	TArray<uint8> CorruptBytes = BackupBytes;
	// 사용자 값은 유효, 엔진 blob 끝 CRC만 손상
	CorruptBytes.Last() ^= 0x80;
	if (!SaveRawSlot(*this, Names.Primary, CorruptBytes))
	{
		return false;
	}
	FDigetherSaveSlotData Loaded = MakeStorageSlot(777);
	const FDigetherSaveSlotData Sentinel = Loaded;
	TestTrue(TEXT("주 슬롯 CRC 손상 거절"),
		DigetherSaveStorage::ReadSlot(Names.Primary, StorageTestHostKey, Loaded) == EDigetherSaveResult::CorruptData);
	TestStorageSlotEquals(*this, Sentinel, Loaded);
	bool bRecovered = false;
	AddExpectedMessagePlain(TEXT("내부 백업에서 진행을 복구했습니다."), ELogVerbosity::Warning);
	if (TestTrue(TEXT("손상된 주 슬롯 대신 백업 읽기"),
		DigetherSaveStorage::Read(Names, StorageTestHostKey, Loaded, bRecovered) == EDigetherSaveResult::Success))
	{
		TestTrue(TEXT("손상된 주 슬롯의 백업 복구 표시"), bRecovered);
		TestStorageSlotEquals(*this, Slot, Loaded);
	}
	const FDigetherSaveSlotData Updated = MakeStorageSlot(110);
	if (TestTrue(TEXT("백업 복구 후 일반 저장으로 손상된 주 슬롯 교체"),
		DigetherSaveStorage::Write(Names, StorageTestHostKey, Updated, false) == EDigetherSaveResult::Success))
	{
		for (const FString& SlotName : { Names.Primary, Names.Backup })
		{
			if (TestTrue(TEXT("백업 복구 뒤 일반 저장은 양쪽 슬롯을 갱신"),
				DigetherSaveStorage::ReadSlot(SlotName, StorageTestHostKey, Loaded) == EDigetherSaveResult::Success))
			{
				TestStorageSlotEquals(*this, Updated, Loaded);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSaveStorageOlderVersionTest,
	"DIGETHER.Game.Save.Storage.OlderVersionRecoversBackup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSaveStorageOlderVersionTest::RunTest(const FString& Parameters)
{
	const FDigetherSaveSlotNames Names = MakeTemporarySlotNames();
	ON_SCOPE_EXIT { DeleteTemporarySlots(*this, Names); };
	const FDigetherSaveSlotData Backup = MakeStorageSlot(90);
	TArray<uint8> BackupBytes;
	if (!MakeEngineSaveBytes(*this, Backup, BackupBytes))
	{
		return false;
	}
	AddExpectedMessagePlain(TEXT("내부 백업에서 진행을 복구했습니다."), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 4);
	for (const int32 Version : { 0, 2 })
	{
		TArray<uint8> OlderSchemaBytes;
		if (!MakeSchemaVersionEngineSaveBytes(*this, Backup, Version, OlderSchemaBytes))
		{
			return false;
		}
		TArray<uint8> OlderHeaderBytes = BackupBytes;
		// 엔진 헤더 두 번째 int32가 파일 버전, 이전 본문은 파싱 안 함
		FMemory::Memcpy(OlderHeaderBytes.GetData() + sizeof(int32), &Version, sizeof(Version));
		for (const TArray<uint8>& PrimaryBytes : { OlderSchemaBytes, OlderHeaderBytes })
		{
			if (!SaveRawSlot(*this, Names.Primary, PrimaryBytes) || !SaveRawSlot(*this, Names.Backup, BackupBytes))
			{
				return false;
			}
			FDigetherSaveSlotData Loaded = MakeStorageSlot(777);
			const FDigetherSaveSlotData Sentinel = Loaded;
			TestTrue(TEXT("버전 0·2는 미래 버전 보호 대상이 아닌 손상으로 분류"),
				DigetherSaveStorage::ReadSlot(Names.Primary, StorageTestHostKey, Loaded) == EDigetherSaveResult::CorruptData);
			TestStorageSlotEquals(*this, Sentinel, Loaded);
			bool bRecovered = false;
			if (TestTrue(TEXT("이전 스키마·엔진 헤더 대신 정상 백업 복구"),
				DigetherSaveStorage::Read(Names, StorageTestHostKey, Loaded, bRecovered) == EDigetherSaveResult::Success))
			{
				TestTrue(TEXT("이전 버전 주 슬롯의 백업 복구 표시"), bRecovered);
				TestStorageSlotEquals(*this, Backup, Loaded);
			}
			TestRawSlotEquals(*this, Names.Primary, PrimaryBytes);
			TestRawSlotEquals(*this, Names.Backup, BackupBytes);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSaveStorageInvalidHeaderTest,
	"DIGETHER.Game.Save.Storage.InvalidEngineHeaderPreservesOutput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSaveStorageInvalidHeaderTest::RunTest(const FString& Parameters)
{
	const FDigetherSaveSlotNames Names = MakeTemporarySlotNames();
	ON_SCOPE_EXIT { DeleteTemporarySlots(*this, Names); };
	TArray<uint8> ValidBytes;
	if (!MakeEngineSaveBytes(*this, MakeStorageSlot(), ValidBytes))
	{
		return false;
	}
	// 엔진이 만든 신뢰 가능한 헤더에서만 위치 탐색, 손상 입력 로드는 저장 계층 담당
	FMemoryReader HeaderReader(ValidBytes, true);
	int32 FileType = 0;
	int32 FileVersion = 0;
	FPackageFileVersion PackageVersion;
	FEngineVersion EngineVersion;
	HeaderReader << FileType << FileVersion;
	HeaderReader << PackageVersion;
	HeaderReader << EngineVersion;
	const int64 FormatOffset = HeaderReader.Tell();
	if (!TestFalse(TEXT("신뢰 가능한 엔진 헤더 위치 읽기 성공"), HeaderReader.IsError())
		|| !TestTrue(TEXT("custom-version 형식과 개수가 헤더 안에 있음"),
			FormatOffset >= 0 && FormatOffset + 2 * static_cast<int64>(sizeof(int32)) <= static_cast<int64>(ValidBytes.Num())))
	{
		return false;
	}
	const int32 InvalidHeaderValue = MAX_int32;
	const int32 InvalidHeaderFormat = 0;
	TArray<uint8> InvalidFormat = ValidBytes;
	FMemory::Memcpy(InvalidFormat.GetData() + FormatOffset, &InvalidHeaderFormat, sizeof(InvalidHeaderFormat));
	const int32 OlderHeaderFormat = 2;
	TArray<uint8> OlderFormat = ValidBytes;
	FMemory::Memcpy(OlderFormat.GetData() + FormatOffset, &OlderHeaderFormat, sizeof(OlderHeaderFormat));
	TArray<uint8> FutureFormat = ValidBytes;
	FMemory::Memcpy(FutureFormat.GetData() + FormatOffset, &InvalidHeaderValue, sizeof(InvalidHeaderValue));
	TArray<uint8> ExcessiveCount = ValidBytes;
	FMemory::Memcpy(ExcessiveCount.GetData() + FormatOffset + sizeof(int32),
		&InvalidHeaderValue, sizeof(InvalidHeaderValue));
	FString ClassPath = UDigetherSaveGame::StaticClass()->GetPathName();
	TArray<uint8> ClassPathBytes;
	FMemoryWriter ClassPathWriter(ClassPathBytes, true);
	ClassPathWriter << ClassPath;
	int32 ClassPathOffset = INDEX_NONE;
	if (!FindUniqueBytePattern(*this, ValidBytes, ClassPathBytes, ClassPathOffset))
	{
		return false;
	}
	// 다른 UObject 클래스 지정 없이 존재하지 않는 클래스 경로 한 글자 생성
	ClassPath[ClassPath.Len() - 1] = TEXT('X');
	TArray<uint8> ChangedClassPathBytes;
	FMemoryWriter ChangedClassPathWriter(ChangedClassPathBytes, true);
	ChangedClassPathWriter << ClassPath;
	if (!TestEqual(TEXT("클래스 경로 변조의 직렬화 길이 유지"), ChangedClassPathBytes.Num(), ClassPathBytes.Num()))
	{
		return false;
	}
	TArray<uint8> WrongClass = ValidBytes;
	FMemory::Memcpy(WrongClass.GetData() + ClassPathOffset, ChangedClassPathBytes.GetData(), ChangedClassPathBytes.Num());
	const TArray<uint8> InvalidHeader = { 0x44, 0x49, 0x47, 0x21, 0xff, 0xff, 0xff, 0xff };
	struct FHeaderCase
	{
		const TCHAR* Label;
		TArray<uint8> Bytes;
		EDigetherSaveResult Expected;
	};
	const FHeaderCase Cases[] = {
		{ TEXT("엔진 헤더가 아닌 바이트"), InvalidHeader, EDigetherSaveResult::CorruptData },
		{ TEXT("유효하지 않은 custom-version 형식"), InvalidFormat, EDigetherSaveResult::CorruptData },
		{ TEXT("이전 custom-version 형식"), OlderFormat, EDigetherSaveResult::CorruptData },
		{ TEXT("과다 custom-version 개수"), ExcessiveCount, EDigetherSaveResult::CorruptData },
		{ TEXT("다른 클래스 경로"), WrongClass, EDigetherSaveResult::CorruptData },
		{ TEXT("지원하지 않는 custom-version 형식"), FutureFormat, EDigetherSaveResult::UnsupportedVersion }
	};
	for (const FHeaderCase& HeaderCase : Cases)
	{
		if (!SaveRawSlot(*this, Names.Primary, HeaderCase.Bytes))
		{
			return false;
		}
		FDigetherSaveSlotData Loaded = MakeStorageSlot(777);
		const FDigetherSaveSlotData Sentinel = Loaded;
		TestTrue(FString::Printf(TEXT("%s 입력을 UObject 로드 전에 거절"), HeaderCase.Label),
			DigetherSaveStorage::ReadSlot(Names.Primary, StorageTestHostKey, Loaded) == HeaderCase.Expected);
		TestStorageSlotEquals(*this, Sentinel, Loaded);
		TestRawSlotEquals(*this, Names.Primary, HeaderCase.Bytes);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSaveStorageCorruptSlotsTest,
	"DIGETHER.Game.Save.Storage.CorruptSlotsRequireExplicitNewGame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSaveStorageCorruptSlotsTest::RunTest(const FString& Parameters)
{
	const FDigetherSaveSlotNames Names = MakeTemporarySlotNames();
	ON_SCOPE_EXIT { DeleteTemporarySlots(*this, Names); };
	const FDigetherSaveSlotData Slot = MakeStorageSlot();
	TArray<uint8> CorruptBytes;
	if (!MakeEngineSaveBytes(*this, Slot, CorruptBytes))
	{
		return false;
	}
	CorruptBytes.Last() ^= 0x80;
	if (!SaveRawSlot(*this, Names.Primary, CorruptBytes) || !SaveRawSlot(*this, Names.Backup, CorruptBytes))
	{
		return false;
	}
	FDigetherSaveSlotData Loaded = MakeStorageSlot(777);
	const FDigetherSaveSlotData Sentinel = Loaded;
	bool bRecovered = false;
	TestTrue(TEXT("양쪽 손상은 복원 거절"),
		DigetherSaveStorage::Read(Names, StorageTestHostKey, Loaded, bRecovered) == EDigetherSaveResult::CorruptData);
	TestStorageSlotEquals(*this, Sentinel, Loaded);
	TestTrue(TEXT("양쪽 손상 상태의 일반 저장 거절"),
		DigetherSaveStorage::Write(Names, StorageTestHostKey, Slot, false) == EDigetherSaveResult::CorruptData);
	TestRawSlotEquals(*this, Names.Primary, CorruptBytes);
	TestRawSlotEquals(*this, Names.Backup, CorruptBytes);
	if (TestTrue(TEXT("명시 새 게임은 손상된 양쪽 슬롯 교체 허용"),
		DigetherSaveStorage::Write(Names, StorageTestHostKey, Slot, true) == EDigetherSaveResult::Success))
	{
		for (const FString& SlotName : { Names.Primary, Names.Backup })
		{
			if (TestTrue(TEXT("명시 초기화 뒤 주·백업 모두 유효"),
				DigetherSaveStorage::ReadSlot(SlotName, StorageTestHostKey, Loaded) == EDigetherSaveResult::Success))
			{
				TestStorageSlotEquals(*this, Slot, Loaded);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSaveStorageFutureVersionTest,
	"DIGETHER.Game.Save.Storage.FutureVersionInEitherSlotPreservesBoth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSaveStorageFutureVersionTest::RunTest(const FString& Parameters)
{
	const FDigetherSaveSlotNames Names = MakeTemporarySlotNames();
	ON_SCOPE_EXIT { DeleteTemporarySlots(*this, Names); };
	const FDigetherSaveSlotData Slot = MakeStorageSlot();
	TArray<uint8> CurrentBytes;
	TArray<uint8> FutureBytes;
	if (!MakeEngineSaveBytes(*this, Slot, CurrentBytes)
		|| !MakeSchemaVersionEngineSaveBytes(*this, Slot, DigetherSaveWire::LatestVersion + 1, FutureBytes))
	{
		return false;
	}
	for (const bool bFuturePrimary : { true, false })
	{
		const TArray<uint8>& PrimaryBytes = bFuturePrimary ? FutureBytes : CurrentBytes;
		const TArray<uint8>& BackupBytes = bFuturePrimary ? CurrentBytes : FutureBytes;
		if (!SaveRawSlot(*this, Names.Primary, PrimaryBytes) || !SaveRawSlot(*this, Names.Backup, BackupBytes))
		{
			return false;
		}
		FDigetherSaveSlotData Loaded = MakeStorageSlot(777);
		const FDigetherSaveSlotData Sentinel = Loaded;
		bool bRecovered = false;
		TestTrue(bFuturePrimary ? TEXT("미래 주 슬롯이 있으면 현재 백업으로 복구하지 않음")
			: TEXT("현재 주 슬롯이 있어도 미래 백업을 보호"),
			DigetherSaveStorage::Read(Names, StorageTestHostKey, Loaded, bRecovered) == EDigetherSaveResult::UnsupportedVersion);
		TestStorageSlotEquals(*this, Sentinel, Loaded);
		TestRawSlotEquals(*this, Names.Primary, PrimaryBytes);
		TestRawSlotEquals(*this, Names.Backup, BackupBytes);
		TestTrue(TEXT("어느 한쪽의 미래 버전도 일반 저장 거절"),
			DigetherSaveStorage::Write(Names, StorageTestHostKey, Slot, false) == EDigetherSaveResult::UnsupportedVersion);
		TestRawSlotEquals(*this, Names.Primary, PrimaryBytes);
		TestRawSlotEquals(*this, Names.Backup, BackupBytes);
		TestTrue(TEXT("명시 새 게임도 어느 한쪽의 미래 버전이 있으면 거절"),
			DigetherSaveStorage::Write(Names, StorageTestHostKey, Slot, true) == EDigetherSaveResult::UnsupportedVersion);
		TestRawSlotEquals(*this, Names.Primary, PrimaryBytes);
		TestRawSlotEquals(*this, Names.Backup, BackupBytes);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSaveStorageSlotNamesTest,
	"DIGETHER.Game.Save.Storage.SlotNamesStayStable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSaveStorageSlotNamesTest::RunTest(const FString& Parameters)
{
	// 슬롯 이름이 바뀌면 기존 저장 탐색 실패, v3 이름은 상수로 만들어도 문자열 불변
	const FTCHARToUTF8 HostBytes(*StorageTestHostKey);
	const FString ExpectedPrimary = FString(TEXT("Digether_HostProgress_v3_"))
		+ FSHA1::HashBuffer(HostBytes.Get(), HostBytes.Length()).ToString();
	const FDigetherSaveSlotNames Names = DigetherSaveStorage::MakeSlotNames(StorageTestHostKey);
	TestEqual(TEXT("주 슬롯 이름 유지"), Names.Primary, ExpectedPrimary);
	TestEqual(TEXT("백업 슬롯 이름 유지"), Names.Backup, ExpectedPrimary + TEXT("_Backup"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSaveStorageWriteWithReadsTest,
	"DIGETHER.Game.Save.Storage.WriteUsesProvidedReads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSaveStorageWriteWithReadsTest::RunTest(const FString& Parameters)
{
	const FDigetherSaveSlotNames Names = MakeTemporarySlotNames();
	ON_SCOPE_EXIT { DeleteTemporarySlots(*this, Names); };
	const FDigetherSaveSlotData Slot = MakeStorageSlot();
	// 디스크가 비어도 전달한 읽기 결과가 미래 버전이면 재읽기 없이 거절
	FDigetherSaveSlotReads FutureReads;
	FutureReads.PrimaryResult = EDigetherSaveResult::UnsupportedVersion;
	TestTrue(TEXT("전달한 미래 버전 읽기 결과로 쓰기 거절"),
		DigetherSaveStorage::Write(Names, StorageTestHostKey, Slot, true, FutureReads)
			== EDigetherSaveResult::UnsupportedVersion);
	TestFalse(TEXT("거절 시 주 슬롯을 만들지 않음"),
		UGameplayStatics::DoesSaveGameExist(Names.Primary, StorageTestUserIndex));
	TestFalse(TEXT("거절 시 백업 슬롯을 만들지 않음"),
		UGameplayStatics::DoesSaveGameExist(Names.Backup, StorageTestUserIndex));
	const FDigetherSaveSlotReads Reads = DigetherSaveStorage::ReadSlots(Names, StorageTestHostKey);
	TestTrue(TEXT("빈 주 슬롯 읽기 결과"), Reads.PrimaryResult == EDigetherSaveResult::NotFound);
	TestTrue(TEXT("빈 백업 슬롯 읽기 결과"), Reads.BackupResult == EDigetherSaveResult::NotFound);
	if (!TestTrue(TEXT("읽기 결과를 재사용한 저장 성공"),
		DigetherSaveStorage::Write(Names, StorageTestHostKey, Slot, false, Reads) == EDigetherSaveResult::Success))
	{
		return false;
	}
	FDigetherSaveSlotData Loaded;
	bool bRecovered = true;
	if (TestTrue(TEXT("재사용 저장 후 슬롯 복원"),
		DigetherSaveStorage::Read(Names, StorageTestHostKey, Loaded, bRecovered) == EDigetherSaveResult::Success))
	{
		TestFalse(TEXT("주 슬롯에서 복원"), bRecovered);
		TestStorageSlotEquals(*this, Slot, Loaded);
	}
	return true;
}

#endif
