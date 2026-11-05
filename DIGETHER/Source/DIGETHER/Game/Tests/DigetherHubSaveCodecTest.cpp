#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Hub/DigetherHubRules.h"
#include "Game/Save/DigetherSaveGame.h"
#include "Game/Save/DigetherSaveRules.h"
#include "Game/Save/DigetherSaveTuning.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "String/HexToBytes.h"

namespace
{
	FDigetherSaveSnapshot MakeHubSaveFixture()
	{
		// 이 파일의 ID·수치는 호환성 검사용이며 실제 시험 카탈로그 값과 독립이다.
		FDigetherSaveSnapshot Snapshot;
		Snapshot.HostAuthenticatedPlayerKey = TEXT("TEST:host");
		Snapshot.GameId = FGuid(1, 2, 3, 4);
		Snapshot.RunId = FGuid(5, 6, 7, 8);
		Snapshot.Quota.Quota.Day = 1;
		Snapshot.Quota.Quota.TargetCredits = 100;
		Snapshot.RunTuning.SourceProfile = FPrimaryAssetId(TEXT("DigetherTuningProfile:DA_HubSaveFixture"));
		Snapshot.RunTuning.ProfileVersion = 1;
		Snapshot.RunTuning.InitialQuotaCredits = 100;
		Snapshot.RunTuning.RunSeconds = 120.f;
		Snapshot.RunTuning.InteractionDistanceCm = 200.f;
		Snapshot.RunTuning.ElevatorCapacityKg = 500.f;
		Snapshot.RunTuning.ElevatorTripLimit = 2;
		Snapshot.RunTuning.AscentPenaltySeconds = 10.f;
		Snapshot.RunTuning.ElevatorTravelSeconds = 2.f;
		Snapshot.RunTuning.DepositIntervalSeconds = .5f;
		Snapshot.RunTuning.EmergencyWeightLimitKg = 3.f;
		Snapshot.RunTuning.DeathLossPercent = 100;
		Snapshot.RunTuning.CorpseRefundPercent = 50;
		Snapshot.EconomyTuning.DayOneEfficiencyPercent = 30;
		Snapshot.EconomyTuning.DayTwoEfficiencyPercent = 50;
		Snapshot.EconomyTuning.DayThreeEfficiencyPercent = 70;
		Snapshot.EconomyTuning.DayFourEfficiencyPercent = 100;
		Snapshot.EconomyTuning.TargetIncreaseCredits = 100;
		Snapshot.EconomyTuning.DepositSessionLifetimeSeconds = 10.;
		Snapshot.RunRuleTuning.AftershockStageCount = 3;
		Snapshot.RunRuleTuning.AftershockMaxIntervalSeconds = 30.;
		Snapshot.RunRuleTuning.AftershockMinIntervalSeconds = 5.;
		FDigetherSavePlayerRecord& Player = Snapshot.Players.AddDefaulted_GetRef();
		Player.AuthenticatedPlayerKey = Snapshot.HostAuthenticatedPlayerKey;
		Player.GameplayPlayerId = FGuid(11, 12, 13, 14);
		Player.Wallet.BalanceCredits = 70;
		Player.Wallet.HiddenLifetimeCredits = 15;
		return Snapshot;
	}

	TArray<uint8> LegacyV3GoldenBytes()
	{
		// Hub 확장 이전 v3 wire 형식의 고정 픽스처다. 새 codec으로 생성하지 않는다.
		// 기준점 없음, MakeHubSaveFixture의 값, little-endian int/float 및 uint16 문자열이다.
		const FString Hex =
			TEXT("03000000000900000054004500530054003a0068006f007300740001000000020000000300000004000000")
			TEXT("05000000060000000700000008000000010000006400000000000000000000000000000000000000000000")
			TEXT("2700000044006900670065007400680065007200540075006e0069006e006700500072006f00660069006c00")
			TEXT("65003a00440041005f0048007500620053006100760065004600690078007400750072006500010000006400")
			TEXT("00000000f042000048430000fa430200000000002041000000400000003f0000404064000000320000001e00")
			TEXT("0000320000004600000064000000640000000000000000002440030000000000000000003e40000000000000")
			TEXT("1440010000000900000054004500530054003a0068006f00730074000b0000000c0000000d0000000e000000")
			TEXT("46000000000000000f000000000000000000000000000000000000000000000000");
		TArray<uint8> Bytes;
		Bytes.SetNumUninitialized(Hex.Len() / 2);
		const int32 Written = UE::String::HexToBytes(FWideStringView(*Hex, Hex.Len()), Bytes.GetData());
		check(Written == Bytes.Num());
		return Bytes;
	}

	void AddHubProgress(FDigetherSaveSnapshot& Snapshot)
	{
		FDigetherHubProgress& Hub = Snapshot.HubProgress;
		Hub.bInitialized = true;
		Hub.UnlockedStageIds = { FName(TEXT("StageA")), FName(TEXT("StageB")) };
		Hub.SelectedStageId = TEXT("StageB");
		Hub.TotalOfferingCredits = 25;
		FDigetherHubPlayerProgress& Player = Hub.Players.AddDefaulted_GetRef();
		Player.PlayerId = Snapshot.Players[0].GameplayPlayerId;
		Player.bStarterGrantConsidered = true;
		FDigetherHubItemStack& Item = Player.Inventory.AddDefaulted_GetRef();
		Item.ItemId = TEXT("Wood");
		Item.Count = 2;
		FDigetherHubItemStack& Delivery = Player.PendingDelivery.AddDefaulted_GetRef();
		Delivery.ItemId = TEXT("Lamp");
		Delivery.Count = 1;
	}

	int32 FindPattern(const TArray<uint8>& Bytes, const TArray<uint8>& Pattern, const int32 Start)
	{
		for (int32 Index = Start; Index <= Bytes.Num() - Pattern.Num(); ++Index)
		{
			if (FMemory::Memcmp(Bytes.GetData() + Index, Pattern.GetData(), Pattern.Num()) == 0)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherHubLegacySaveTest,
	"DIGETHER.Game.Save.Hub.LegacyV3GoldenCompatible",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherHubLegacySaveTest::RunTest(const FString& Parameters)
{
	const TArray<uint8> Golden = LegacyV3GoldenBytes();
	TestEqual(TEXT("구 v3 고정 픽스처 길이"), Golden.Num(), 339);
	FDigetherSaveSlotData Restored;
	if (!TestTrue(TEXT("확장이 없는 구 v3를 읽음"), DigetherSaveRules::DeserializeSlot(Golden, Restored)
		== EDigetherSaveResult::Success))
	{
		return false;
	}
	TestFalse(TEXT("구 저장에는 Hub 진행이 초기화되지 않음"), Restored.Progress.HubProgress.bInitialized);
	TestEqual(TEXT("구 개인 잔고 유지"), Restored.Progress.Players[0].Wallet.BalanceCredits, 70);
	TestEqual(TEXT("구 히든 수입 유지"), Restored.Progress.Players[0].Wallet.HiddenLifetimeCredits, 15);
	TestEqual(TEXT("구 할당량 유지"), Restored.Progress.Quota.Quota.TargetCredits, 100);
	TestTrue(TEXT("구 게임 ID 유지"), Restored.Progress.GameId == FGuid(1, 2, 3, 4));
	// 구 v3에는 전체 일차가 없다. 확실한 하한인 기간 일차(1)로 추정하고 저장 시각은 모름(0)이다.
	TestEqual(TEXT("구 v3 기간 일차 유지"), Restored.Progress.Quota.Quota.Day, 1);
	TestEqual(TEXT("구 v3 전체 일차는 기간 일차로 추정"), Restored.Progress.Quota.Quota.TotalDay, 1);
	TestEqual(TEXT("구 v3 저장 시각은 모름"), Restored.SavedAtUtcTicks, static_cast<int64>(0));
	FDigetherSaveSlotData Slot;
	Slot.Progress = MakeHubSaveFixture();
	TArray<uint8> Bytes;
	TestTrue(TEXT("Hub를 쓰지 않는 맵의 저장 성공"), DigetherSaveRules::SerializeSlot(Slot, Bytes)
		== EDigetherSaveResult::Success);
	// 새 쓰기는 항상 최신 wire다. 버전 뒤 v3 기본 본문 배치는 바뀌지 않아야 한다(확장은 본문 뒤에만 붙는다).
	int32 WireVersion = 0;
	FMemory::Memcpy(&WireVersion, Bytes.GetData(), sizeof(WireVersion));
	TestEqual(TEXT("빈 Hub 슬롯도 최신 wire로 씀"), WireVersion, DigetherSaveWire::LatestVersion);
	TestTrue(TEXT("최신 wire는 구 v3보다 길다(확장 포함)"), Bytes.Num() > Golden.Num());
	TestTrue(TEXT("버전 뒤 기본 본문은 구 v3와 바이트 단위로 동일"), Bytes.Num() > Golden.Num()
		&& FMemory::Memcmp(Bytes.GetData() + sizeof(int32), Golden.GetData() + sizeof(int32),
			Golden.Num() - sizeof(int32)) == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherHubSaveRoundTripTest,
	"DIGETHER.Game.Save.Hub.ProgressAndCheckpointRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherHubSaveRoundTripTest::RunTest(const FString& Parameters)
{
	FDigetherSaveSlotData Slot;
	Slot.Progress = MakeHubSaveFixture();
	AddHubProgress(Slot.Progress);
	Slot.bHasDepartureCheckpoint = true;
	Slot.DepartureCheckpoint = Slot.Progress;
	Slot.DepartureCheckpoint.HubProgress.Players[0].Inventory[0].Count = 1;
	TArray<uint8> Bytes;
	if (!TestTrue(TEXT("Hub 진행과 별도 기준점을 저장"), DigetherSaveRules::SerializeSlot(Slot, Bytes)
		== EDigetherSaveResult::Success))
	{
		return false;
	}
	int32 WireVersion = 0;
	FMemory::Memcpy(&WireVersion, Bytes.GetData(), sizeof(WireVersion));
	TestEqual(TEXT("Hub 재산이 있는 슬롯은 최신 wire(v5)"), WireVersion, DigetherSaveWire::LatestVersion);
	TestTrue(TEXT("구 v3 실행 파일은 첫 버전을 미래로 분류"), WireVersion > DigetherSaveTuning::SchemaVersion);
	FDigetherSaveSlotData Restored;
	if (!TestTrue(TEXT("Hub 확장을 읽음"), DigetherSaveRules::DeserializeSlot(Bytes, Restored)
		== EDigetherSaveResult::Success))
	{
		return false;
	}
	TestEqual(TEXT("읽은 진행 값 스키마는 v3 유지"), Restored.Progress.SchemaVersion, 3);
	TestTrue(TEXT("시험 지원금 중복 방지 값 유지"), Restored.Progress.HubProgress.Players[0].bStarterGrantConsidered);
	TestEqual(TEXT("진행의 보유 수량 유지"), Restored.Progress.HubProgress.Players[0].Inventory[0].Count, 2);
	TestEqual(TEXT("기준점의 별도 보유 수량 유지"), Restored.DepartureCheckpoint.HubProgress.Players[0].Inventory[0].Count, 1);
	TestEqual(TEXT("배송 대기 수량 유지"), Restored.Progress.HubProgress.Players[0].PendingDelivery[0].Count, 1);
	TestEqual(TEXT("팀 해금 목록 유지"), Restored.Progress.HubProgress.UnlockedStageIds.Num(), 2);
	TestTrue(TEXT("선택 목적지 유지"), Restored.Progress.HubProgress.SelectedStageId == FName(TEXT("StageB")));
	TestEqual(TEXT("제단 누적 헌납 유지"), Restored.Progress.HubProgress.TotalOfferingCredits, 25);
	TArray<uint8> Reencoded;
	TestTrue(TEXT("재직렬화 성공"), DigetherSaveRules::SerializeSlot(Restored, Reencoded) == EDigetherSaveResult::Success);
	TestTrue(TEXT("확장 재직렬화가 바이트 단위로 동일"), Bytes == Reencoded);
	FDigetherSaveRuntimeState Runtime;
	TestTrue(TEXT("런타임 복원에 Hub 포함"), DigetherSaveRules::TryRestoreSnapshot(Restored.Progress, Runtime)
		== EDigetherSaveResult::Success);
	TestEqual(TEXT("런타임 보유 수량 유지"), Runtime.HubProgress.Players[0].Inventory[0].Count, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherHubSaveMalformedTest,
	"DIGETHER.Game.Save.Hub.RejectMalformedExtension",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherHubSaveMalformedTest::RunTest(const FString& Parameters)
{
	FDigetherSaveSlotData Slot;
	Slot.Progress = MakeHubSaveFixture();
	AddHubProgress(Slot.Progress);
	TArray<uint8> Bytes;
	if (!TestTrue(TEXT("확장 거절 검사 준비"), DigetherSaveRules::SerializeSlot(Slot, Bytes) == EDigetherSaveResult::Success))
	{
		return false;
	}
	const int32 ExtensionOffset = LegacyV3GoldenBytes().Num();
	const FGuid Sentinel(91, 92, 93, 94);
	FDigetherSaveSlotData Output;
	Output.Progress.GameId = Sentinel;
	auto Reject = [this, &Output, Sentinel](const TArray<uint8>& Corrupt, const TCHAR* Message,
		const EDigetherSaveResult Expected = EDigetherSaveResult::CorruptData)
	{
		TestTrue(Message, DigetherSaveRules::DeserializeSlot(Corrupt, Output) == Expected);
		TestTrue(TEXT("거절 시 출력 진행을 바꾸지 않음"), Output.Progress.GameId == Sentinel);
	};
	TArray<uint8> Corrupt = Bytes;
	Corrupt.SetNum(ExtensionOffset);
	Reject(Corrupt, TEXT("wire v5에 필수 Hub 확장이 없으면 거절"));
	Corrupt = Bytes;
	const int32 LegacyVersion = DigetherSaveWire::LegacyVersion;
	FMemory::Memcpy(Corrupt.GetData(), &LegacyVersion, sizeof(LegacyVersion));
	Reject(Corrupt, TEXT("구 wire v3에 확장을 덧붙여도 거절"));
	Corrupt = Bytes;
	const int32 HubWireVersion = DigetherSaveWire::HubVersion;
	FMemory::Memcpy(Corrupt.GetData(), &HubWireVersion, sizeof(HubWireVersion));
	Reject(Corrupt, TEXT("구 wire v4는 Hub 확장이 마지막이라 진행 확장이 붙으면 거절"));
	Corrupt = Bytes;
	Corrupt.SetNum(Corrupt.Num() - 1);
	Reject(Corrupt, TEXT("잘린 진행 확장 거절"));
	Corrupt = Bytes;
	Corrupt.Add(0);
	Reject(Corrupt, TEXT("확장 뒤 남는 바이트 거절"));
	Corrupt = Bytes;
	Corrupt[ExtensionOffset] = 0;
	Reject(Corrupt, TEXT("확장 magic 손상 거절"));
	Corrupt = Bytes;
	// v2는 바닥 금화를 담는 현재 확장이다. 그다음 버전이 미래 확장이다.
	const int32 FutureVersion = 3;
	FMemory::Memcpy(Corrupt.GetData() + ExtensionOffset + sizeof(uint32), &FutureVersion, sizeof(FutureVersion));
	Reject(Corrupt, TEXT("미래 확장은 손상이 아닌 미지원 버전"), EDigetherSaveResult::UnsupportedVersion);
	Corrupt.SetNum(ExtensionOffset + sizeof(uint32) + sizeof(int32));
	Reject(Corrupt, TEXT("미래 확장은 본문 해석 전 거절"), EDigetherSaveResult::UnsupportedVersion);
	Corrupt = Bytes;
	const int32 OversizedPayload = DigetherSaveTuning::MaxSaveBytes + 1;
	FMemory::Memcpy(Corrupt.GetData() + ExtensionOffset + 2 * sizeof(int32), &OversizedPayload, sizeof(OversizedPayload));
	Reject(Corrupt, TEXT("확장 길이 상한 초과를 할당 전 거절"));
	Corrupt = Bytes;
	const int32 ExcessivePlayers = DigetherHubRules::MaxProgressPlayers + 1;
	FMemory::Memcpy(Corrupt.GetData() + ExtensionOffset + 3 * sizeof(int32) + sizeof(uint8),
		&ExcessivePlayers, sizeof(ExcessivePlayers));
	Reject(Corrupt, TEXT("플레이어 배열 상한 초과를 할당 전 거절"));
	Corrupt = Bytes;
	Corrupt[ExtensionOffset + 3 * sizeof(int32) + sizeof(uint8) + sizeof(int32) + sizeof(FGuid)] = 2;
	Reject(Corrupt, TEXT("잘못된 시험 지원금 bool 표현 거절"));
	const TArray<uint8> StageB = { 'S', 0, 't', 0, 'a', 0, 'g', 0, 'e', 0, 'B', 0 };
	const TArray<uint8> StageA = { 'S', 0, 't', 0, 'a', 0, 'g', 0, 'e', 0, 'A', 0 };
	const int32 StageOffset = FindPattern(Bytes, StageB, ExtensionOffset);
	if (TestTrue(TEXT("확장의 StageB wire 값을 찾음"), StageOffset != INDEX_NONE))
	{
		Corrupt = Bytes;
		FMemory::Memcpy(Corrupt.GetData() + StageOffset, StageA.GetData(), StageA.Num());
		Reject(Corrupt, TEXT("중복 해금 ID 의미 오류 거절"), EDigetherSaveResult::InvalidState);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherHubSaveInvalidStateTest,
	"DIGETHER.Game.Save.Hub.RejectInvalidProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherHubSaveInvalidStateTest::RunTest(const FString& Parameters)
{
	FDigetherSaveSlotData Slot;
	Slot.Progress = MakeHubSaveFixture();
	AddHubProgress(Slot.Progress);
	TArray<uint8> Bytes = { 0x42 };
	auto Reject = [this, &Bytes](const FDigetherSaveSlotData& Invalid, const TCHAR* Message)
	{
		TestTrue(Message, DigetherSaveRules::SerializeSlot(Invalid, Bytes) == EDigetherSaveResult::InvalidState);
		TestTrue(TEXT("저장 거절 시 출력 바이트 유지"), Bytes.Num() == 1 && Bytes[0] == 0x42);
	};
	FDigetherSaveSlotData Invalid = Slot;
	Invalid.Progress.HubProgress.Players[0].Inventory[0].Count = 0;
	Reject(Invalid, TEXT("0개 아이템 스택 거절"));
	Invalid = Slot;
	const FDigetherHubItemStack DuplicateItem = Invalid.Progress.HubProgress.Players[0].Inventory[0];
	Invalid.Progress.HubProgress.Players[0].Inventory.Add(DuplicateItem);
	Reject(Invalid, TEXT("중복 아이템 ID 거절"));
	Invalid = Slot;
	Invalid.Progress.HubProgress.Players[0].PlayerId = FGuid(31, 32, 33, 34);
	Reject(Invalid, TEXT("기존 저장 플레이어에 없는 Hub 재산 레코드 거절"));
	Invalid = Slot;
	Invalid.Progress.HubProgress.SelectedStageId = TEXT("LockedStage");
	Reject(Invalid, TEXT("미해금 목적지 선택 거절"));
	Invalid = Slot;
	Invalid.Progress.HubProgress.bInitialized = false;
	Reject(Invalid, TEXT("비초기화 값에 숨긴 재산 거절"));
	Invalid = Slot;
	Invalid.Progress.HubProgress.TotalOfferingCredits = -1;
	Reject(Invalid, TEXT("음수 제단 헌납 거절"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherHubSaveProgressWriteTest,
	"DIGETHER.Game.Save.Hub.ProgressWriteIncludesLatestFacilityProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherHubSaveProgressWriteTest::RunTest(const FString& Parameters)
{
	// 출발 기준점 비교는 폐기됐다(기획 1.1.0). 다음 저장 시점(호스트 거점 종료 등)이 마지막 저장 이후의
	// 시설 재산 변화까지 진행에 담는지 확인한다.
	FDigetherSaveSnapshot Current = MakeHubSaveFixture();
	AddHubProgress(Current);
	FDigetherSaveSlotData Slot;
	if (!TestTrue(TEXT("Hub 재산을 포함한 첫 진행 저장"), DigetherSaveRules::TryBuildSlot(Current, nullptr, false, Slot)
		== EDigetherSaveResult::Success))
	{
		return false;
	}
	const int32 SavedCount = Slot.Progress.HubProgress.Players[0].PendingDelivery[0].Count;
	Current.HubProgress.Players[0].PendingDelivery[0].Count += 1;
	if (!TestTrue(TEXT("저장 이후 달라진 배송 재산으로 다음 진행 저장"), DigetherSaveRules::TryBuildSlot(Current, &Slot, false, Slot)
		== EDigetherSaveResult::Success))
	{
		return false;
	}
	TestEqual(TEXT("다음 저장은 최신 배송 재산"), Slot.Progress.HubProgress.Players[0].PendingDelivery[0].Count, SavedCount + 1);
	TestFalse(TEXT("진행 저장은 출발 기준점을 만들지 않음"), Slot.bHasDepartureCheckpoint);
	TArray<uint8> Bytes;
	FDigetherSaveSlotData Restored;
	TestTrue(TEXT("최신 Hub 진행 직렬화"), DigetherSaveRules::SerializeSlot(Slot, Bytes) == EDigetherSaveResult::Success);
	TestTrue(TEXT("최신 Hub 진행 역직렬화"), DigetherSaveRules::DeserializeSlot(Bytes, Restored) == EDigetherSaveResult::Success);
	TestEqual(TEXT("다시 읽은 배송 재산"), Restored.Progress.HubProgress.Players[0].PendingDelivery[0].Count, SavedCount + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherHubSaveWireProtectionTest,
	"DIGETHER.Game.Save.Hub.WireVersionProtectsDownlevelAndFuture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherHubSaveWireProtectionTest::RunTest(const FString& Parameters)
{
	FDigetherSaveSlotData Slot;
	Slot.Progress = MakeHubSaveFixture();
	AddHubProgress(Slot.Progress);
	UDigetherSaveGame* Save = NewObject<UDigetherSaveGame>();
	if (!TestTrue(TEXT("Hub 진행을 엔진 저장 래퍼에 설정"), Save->SetSlotData(Slot) == EDigetherSaveResult::Success))
	{
		return false;
	}
	TArray<uint8> Envelope;
	FMemoryWriter Writer(Envelope, true);
	Save->Serialize(Writer);
	if (!TestFalse(TEXT("크기·payload·CRC 래퍼 쓰기 오류 없음"), Writer.IsError()))
	{
		return false;
	}
	UDigetherSaveGame* ReadBack = NewObject<UDigetherSaveGame>();
	FMemoryReader Reader(Envelope, true);
	ReadBack->Serialize(Reader);
	TestTrue(TEXT("엔진 저장 래퍼가 최신 wire를 정상 분류"), ReadBack->GetDataResult() == EDigetherSaveResult::Success);
	TestEqual(TEXT("최신 wire도 런타임 스키마 v3"), ReadBack->GetSlotData().Progress.SchemaVersion, 3);
	TestTrue(TEXT("래퍼가 Hub 구매품을 보존"), ReadBack->GetSlotData().Progress.HubProgress.bInitialized);

	// 구 reader(v3 전용)는 payload 첫 int32가 3보다 크면, v4 reader는 4보다 크면 본문/CRC와 무관하게 UnsupportedVersion이었다.
	int32 StoredWireVersion = 0;
	FMemory::Memcpy(&StoredWireVersion, Envelope.GetData() + sizeof(int32), sizeof(StoredWireVersion));
	TestEqual(TEXT("구 reader가 볼 첫 버전은 5"), StoredWireVersion, 5);
	TestTrue(TEXT("v4 reader도 미래 버전으로 분류"), StoredWireVersion > DigetherSaveWire::HubVersion);
	TestEqual(TEXT("진행 스키마 버전은 3 유지"), DigetherSaveTuning::SchemaVersion, 3);
	TestEqual(TEXT("슬롯 이름 버전은 3 유지"), DigetherSaveTuning::SlotNameVersion, 3);
	TestTrue(TEXT("구 reader의 미래 판정이 명시 새 게임 덮어쓰기도 차단"),
		DigetherSaveRules::CanWriteSlots(EDigetherSaveResult::UnsupportedVersion,
			EDigetherSaveResult::Success, true) == EDigetherSaveResult::UnsupportedVersion);

	const int32 FutureWireVersion = DigetherSaveWire::LatestVersion + 1;
	FMemory::Memcpy(Envelope.GetData() + sizeof(int32), &FutureWireVersion, sizeof(FutureWireVersion));
	// CRC를 일부러 갱신하지 않는다. 미래 wire 보호가 CRC 손상 분류보다 앞서야 한다.
	UDigetherSaveGame* FutureRead = NewObject<UDigetherSaveGame>();
	FMemoryReader FutureReader(Envelope, true);
	FutureRead->Serialize(FutureReader);
	TestTrue(TEXT("미래 wire는 CRC가 달라도 래퍼에서 미지원으로 보존"),
		FutureRead->GetDataResult() == EDigetherSaveResult::UnsupportedVersion);
	TestTrue(TEXT("현재 reader도 미래 저장의 명시 새 게임을 거절"),
		DigetherSaveRules::CanWriteSlots(EDigetherSaveResult::Success,
			FutureRead->GetDataResult(), true) == EDigetherSaveResult::UnsupportedVersion);
	return true;
}

namespace
{
	// 시험용 바닥 금화다. ID·금액·좌표는 픽스처이며 기획 값이 아니다.
	FDigetherSaveFloorCoin MakeFloorCoinFixture(const int32 Seed, const int32 Amount)
	{
		FDigetherSaveFloorCoin Coin;
		Coin.CoinId = FGuid(Seed, Seed + 1, Seed + 2, Seed + 3);
		Coin.StationId = TEXT("StationA");
		Coin.AmountCredits = Amount;
		Coin.Transform = FTransform(FRotator(0.0, 90.0, 0.0).Quaternion(), FVector(100.0, -20.0, 5.0));
		return Coin;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherHubSaveFloorCoinTest,
	"DIGETHER.Game.Save.Hub.FloorCoinsExtensionV2",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherHubSaveFloorCoinTest::RunTest(const FString& Parameters)
{
	FDigetherSaveSlotData Slot;
	Slot.Progress = MakeHubSaveFixture();
	AddHubProgress(Slot.Progress);
	Slot.Progress.FloorCoins = { MakeFloorCoinFixture(41, 7), MakeFloorCoinFixture(51, 3) };
	Slot.Progress.FloorCoins[1].StationId = TEXT("StationB");
	TArray<uint8> Bytes;
	if (!TestTrue(TEXT("바닥 금화를 담은 Hub 확장 저장"), DigetherSaveRules::SerializeSlot(Slot, Bytes)
		== EDigetherSaveResult::Success))
	{
		return false;
	}
	const int32 ExtensionOffset = LegacyV3GoldenBytes().Num();
	int32 ExtensionVersion = 0;
	FMemory::Memcpy(&ExtensionVersion, Bytes.GetData() + ExtensionOffset + sizeof(uint32), sizeof(ExtensionVersion));
	TestEqual(TEXT("바닥 금화 확장은 v2"), ExtensionVersion, 2);
	FDigetherSaveSlotData Restored;
	if (!TestTrue(TEXT("v2 확장 읽기"), DigetherSaveRules::DeserializeSlot(Bytes, Restored) == EDigetherSaveResult::Success)
		|| !TestEqual(TEXT("바닥 금화 더미 수 유지"), Restored.Progress.FloorCoins.Num(), 2))
	{
		return false;
	}
	const FDigetherSaveFloorCoin& First = Restored.Progress.FloorCoins[0];
	TestTrue(TEXT("금화 ID 유지"), First.CoinId == FGuid(41, 42, 43, 44));
	TestTrue(TEXT("정산기 ID 유지"), First.StationId == FName(TEXT("StationA")));
	TestEqual(TEXT("금액 유지"), First.AmountCredits, 7);
	TestTrue(TEXT("위치 유지"), First.Transform.GetLocation().Equals(FVector(100.0, -20.0, 5.0)));
	TestTrue(TEXT("회전 유지"), First.Transform.GetRotation().Equals(FRotator(0.0, 90.0, 0.0).Quaternion()));
	TestEqual(TEXT("두 번째 금액 유지"), Restored.Progress.FloorCoins[1].AmountCredits, 3);
	TArray<uint8> Reencoded;
	TestTrue(TEXT("금화 재직렬화 성공"), DigetherSaveRules::SerializeSlot(Restored, Reencoded) == EDigetherSaveResult::Success);
	TestTrue(TEXT("금화 재직렬화가 바이트 단위로 동일"), Bytes == Reencoded);

	// v1 확장(금화 이전)은 같은 Hub 진행 뒤에 금화 목록이 없다. 읽으면 금화는 빈 목록이다.
	FDigetherSaveSlotData NoCoins;
	NoCoins.Progress = MakeHubSaveFixture();
	AddHubProgress(NoCoins.Progress);
	TArray<uint8> V2Empty;
	if (TestTrue(TEXT("금화 없는 v2 저장"), DigetherSaveRules::SerializeSlot(NoCoins, V2Empty) == EDigetherSaveResult::Success))
	{
		TArray<uint8> V1 = V2Empty;
		const int32 V1Version = 1;
		FMemory::Memcpy(V1.GetData() + ExtensionOffset + sizeof(uint32), &V1Version, sizeof(V1Version));
		int32 PayloadSize = 0;
		FMemory::Memcpy(&PayloadSize, V1.GetData() + ExtensionOffset + 2 * sizeof(int32), sizeof(PayloadSize));
		// wire v5는 Hub 확장 뒤에 진행 확장이 붙으므로 금화 개수는 파일 끝이 아니라 Hub 확장 payload의 마지막 int32다.
		const int32 HubPayloadEnd = ExtensionOffset + 3 * static_cast<int32>(sizeof(int32)) + PayloadSize;
		PayloadSize -= static_cast<int32>(sizeof(int32));
		FMemory::Memcpy(V1.GetData() + ExtensionOffset + 2 * sizeof(int32), &PayloadSize, sizeof(PayloadSize));
		V1.RemoveAt(HubPayloadEnd - static_cast<int32>(sizeof(int32)), static_cast<int32>(sizeof(int32)));
		FDigetherSaveSlotData FromV1;
		FromV1.Progress.FloorCoins = { MakeFloorCoinFixture(61, 1) };
		TestTrue(TEXT("v1 확장 읽기"), DigetherSaveRules::DeserializeSlot(V1, FromV1) == EDigetherSaveResult::Success);
		TestTrue(TEXT("v1은 바닥 금화 빈 목록"), FromV1.Progress.FloorCoins.IsEmpty());
		TestTrue(TEXT("v1의 Hub 진행 유지"), FromV1.Progress.HubProgress.bInitialized);

		// 금화 개수 상한을 넘는 손상 파일은 할당 전에 거절한다. 금화 없는 v2 Hub 확장 payload의 마지막 int32가 개수다.
		TArray<uint8> Excessive = V2Empty;
		const int32 TooMany = DigetherSaveTuning::MaxFloorCoins + 1;
		FMemory::Memcpy(Excessive.GetData() + HubPayloadEnd - sizeof(int32), &TooMany, sizeof(TooMany));
		FDigetherSaveSlotData Rejected;
		TestTrue(TEXT("금화 개수 상한 초과 거절"), DigetherSaveRules::DeserializeSlot(Excessive, Rejected)
			== EDigetherSaveResult::CorruptData);
	}

	// 의미가 잘못된 금화는 저장하지 않는다.
	TArray<uint8> Output = { 0x42 };
	auto Reject = [this, &Output](const FDigetherSaveSlotData& Invalid, const TCHAR* Message)
	{
		TestTrue(Message, DigetherSaveRules::SerializeSlot(Invalid, Output) == EDigetherSaveResult::InvalidState);
		TestTrue(TEXT("저장 거절 시 출력 바이트 유지"), Output.Num() == 1 && Output[0] == 0x42);
	};
	FDigetherSaveSlotData Invalid = Slot;
	Invalid.Progress.FloorCoins[0].AmountCredits = 0;
	Reject(Invalid, TEXT("금액 0 금화 거절"));
	Invalid = Slot;
	Invalid.Progress.FloorCoins[1].CoinId = Invalid.Progress.FloorCoins[0].CoinId;
	Reject(Invalid, TEXT("중복 금화 ID 거절"));
	Invalid = Slot;
	Invalid.Progress.FloorCoins[0].StationId = NAME_None;
	Reject(Invalid, TEXT("정산기 없는 금화 거절"));
	Invalid = Slot;
	Invalid.Progress.FloorCoins[0].Transform.SetLocation(FVector(TNumericLimits<double>::Max() * 2.0, 0.0, 0.0));
	Reject(Invalid, TEXT("유한하지 않은 위치 거절"));
	Invalid = Slot;
	Invalid.Progress.FloorCoins.Reset();
	for (int32 Index = 0; Index <= DigetherSaveTuning::MaxFloorCoins; ++Index)
	{
		Invalid.Progress.FloorCoins.Add(MakeFloorCoinFixture(1000 + Index * 4, 1));
	}
	Reject(Invalid, TEXT("금화 개수 상한 초과 저장 거절"));
	// 금화는 Hub 확장에만 저장한다. Hub 진행이 없는 구 v3 본문에 숨기지 않는다.
	Invalid = Slot;
	Invalid.Progress.HubProgress = FDigetherHubProgress();
	Reject(Invalid, TEXT("Hub 진행 없는 금화 거절"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSaveTotalDayWireTest,
	"DIGETHER.Game.Save.Hub.TotalDayWireV5AndLegacyV4",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSaveTotalDayWireTest::RunTest(const FString& Parameters)
{
	// 일차·시각 값은 시험 픽스처이며 기획 값이 아니다. 전체 일차는 기간 일차와 다른 값으로 둔다.
	FDigetherSaveSlotData Slot;
	Slot.Progress = MakeHubSaveFixture();
	AddHubProgress(Slot.Progress);
	Slot.Progress.Quota.Quota.Day = 3;
	Slot.Progress.Quota.Quota.TotalDay = 9;
	Slot.SavedAtUtcTicks = FDateTime(2026, 11, 5, 12, 30, 0).GetTicks();
	TArray<uint8> V5;
	if (!TestTrue(TEXT("전체 일차·저장 시각을 담은 저장"), DigetherSaveRules::SerializeSlot(Slot, V5)
		== EDigetherSaveResult::Success))
	{
		return false;
	}
	FDigetherSaveSlotData Restored;
	if (!TestTrue(TEXT("v5 읽기"), DigetherSaveRules::DeserializeSlot(V5, Restored) == EDigetherSaveResult::Success))
	{
		return false;
	}
	TestEqual(TEXT("v5 기간 일차 왕복"), Restored.Progress.Quota.Quota.Day, 3);
	TestEqual(TEXT("v5 전체 일차 왕복"), Restored.Progress.Quota.Quota.TotalDay, 9);
	TestEqual(TEXT("v5 저장 시각 왕복"), Restored.SavedAtUtcTicks, Slot.SavedAtUtcTicks);
	TArray<uint8> Reencoded;
	TestTrue(TEXT("v5 재직렬화가 바이트 단위로 동일"), DigetherSaveRules::SerializeSlot(Restored, Reencoded)
		== EDigetherSaveResult::Success && Reencoded == V5);

	// 진행 확장은 마지막 블록(magic PRG1·버전·길이 + 저장 시각 int64 + 전체 일차 int32 + v2 결과 통계)이다.
	// v2부터 길이가 바뀌므로 magic 위치를 뒤에서 찾아 블록 경계를 정한다. 블록을 떼고 버전을 4로 바꾸면
	// 전체 일차 도입 전 실행 파일이 Hub 재산 슬롯에 쓰던 wire v4와 같은 배치다.
	const uint8 ProgressMagicBytes[] = { 0x50, 0x52, 0x47, 0x31 };
	int32 ProgressExtensionOffset = INDEX_NONE;
	for (int32 Offset = V5.Num() - static_cast<int32>(sizeof(ProgressMagicBytes)); Offset >= 0; --Offset)
	{
		if (FMemory::Memcmp(V5.GetData() + Offset, ProgressMagicBytes, sizeof(ProgressMagicBytes)) == 0)
		{
			ProgressExtensionOffset = Offset;
			break;
		}
	}
	if (!TestTrue(TEXT("진행 확장 블록 위치"), ProgressExtensionOffset != INDEX_NONE))
	{
		return false;
	}
	const int32 ProgressExtensionBytes = V5.Num() - ProgressExtensionOffset;
	const int32 ProgressHeaderBytes = static_cast<int32>(3 * sizeof(int32));
	const int32 TotalDayOffset = ProgressExtensionOffset + ProgressHeaderBytes + static_cast<int32>(sizeof(int64));
	TArray<uint8> V4 = V5;
	V4.SetNum(V4.Num() - ProgressExtensionBytes);
	const int32 HubWireVersion = DigetherSaveWire::HubVersion;
	FMemory::Memcpy(V4.GetData(), &HubWireVersion, sizeof(HubWireVersion));
	FDigetherSaveSlotData FromV4;
	if (!TestTrue(TEXT("구 v4 읽기"), DigetherSaveRules::DeserializeSlot(V4, FromV4) == EDigetherSaveResult::Success))
	{
		return false;
	}
	TestEqual(TEXT("구 v4 기간 일차 유지"), FromV4.Progress.Quota.Quota.Day, 3);
	TestEqual(TEXT("구 v4 전체 일차는 기간 일차로 추정"), FromV4.Progress.Quota.Quota.TotalDay, 3);
	TestEqual(TEXT("구 v4 저장 시각은 모름"), FromV4.SavedAtUtcTicks, static_cast<int64>(0));
	TestTrue(TEXT("구 v4 Hub 재산 유지"), FromV4.Progress.HubProgress.bInitialized);
	TArray<uint8> Upgraded;
	if (TestTrue(TEXT("구 v4를 다시 쓰면 최신 wire"), DigetherSaveRules::SerializeSlot(FromV4, Upgraded)
		== EDigetherSaveResult::Success))
	{
		int32 UpgradedVersion = 0;
		FMemory::Memcpy(&UpgradedVersion, Upgraded.GetData(), sizeof(UpgradedVersion));
		TestEqual(TEXT("다시 쓴 wire 버전"), UpgradedVersion, DigetherSaveWire::LatestVersion);
		FDigetherSaveSlotData UpgradedRead;
		TestTrue(TEXT("다시 쓴 슬롯 읽기"), DigetherSaveRules::DeserializeSlot(Upgraded, UpgradedRead)
			== EDigetherSaveResult::Success && UpgradedRead.Progress.Quota.Quota.TotalDay == 3);
	}

	// Hub 진행이 없는 슬롯도 v5에서는 비초기화 Hub 확장과 함께 왕복한다(v4와 달리 Hub 재산을 요구하지 않는다).
	FDigetherSaveSlotData NoHub;
	NoHub.Progress = MakeHubSaveFixture();
	NoHub.Progress.Quota.Quota.TotalDay = 4;
	TArray<uint8> NoHubBytes;
	FDigetherSaveSlotData NoHubRead;
	TestTrue(TEXT("Hub 없는 v5 왕복"), DigetherSaveRules::SerializeSlot(NoHub, NoHubBytes) == EDigetherSaveResult::Success
		&& DigetherSaveRules::DeserializeSlot(NoHubBytes, NoHubRead) == EDigetherSaveResult::Success);
	TestFalse(TEXT("Hub 없는 v5는 Hub 비초기화 유지"), NoHubRead.Progress.HubProgress.bInitialized);
	TestEqual(TEXT("Hub 없는 v5 전체 일차 왕복"), NoHubRead.Progress.Quota.Quota.TotalDay, 4);

	// 진행 확장 v1(결과 통계 도입 전, 2026-11-05 이전 실행 파일)은 고정 길이다. 통계는 0·빈 값으로 읽는다.
	{
		TArray<uint8> V1 = V5;
		V1.SetNum(ProgressExtensionOffset + ProgressHeaderBytes + static_cast<int32>(sizeof(int64) + sizeof(int32)));
		const int32 V1Version = 1;
		const int32 V1PayloadSize = static_cast<int32>(sizeof(int64) + sizeof(int32));
		FMemory::Memcpy(V1.GetData() + ProgressExtensionOffset + sizeof(uint32), &V1Version, sizeof(V1Version));
		FMemory::Memcpy(V1.GetData() + ProgressExtensionOffset + 2 * sizeof(int32), &V1PayloadSize, sizeof(V1PayloadSize));
		FDigetherSaveSlotData FromV1;
		if (TestTrue(TEXT("진행 확장 v1 읽기"), DigetherSaveRules::DeserializeSlot(V1, FromV1) == EDigetherSaveResult::Success))
		{
			TestEqual(TEXT("v1 전체 일차 유지"), FromV1.Progress.Quota.Quota.TotalDay, 9);
			TestEqual(TEXT("v1 저장 시각 유지"), FromV1.SavedAtUtcTicks, Slot.SavedAtUtcTicks);
			TestFalse(TEXT("v1에는 게임 결과 없음"), FromV1.Progress.bHasGameResult);
			TestTrue(TEXT("v1에는 보관 결과 없음"), FromV1.Progress.ArchivedGameResults.IsEmpty());
			TestEqual(TEXT("v1 누적 획득은 0으로 읽음"), FromV1.Progress.Players[0].Wallet.LifetimeEarnedCredits, 0);
			TestEqual(TEXT("v1 참가 순서는 알 수 없음(0)"), FromV1.Progress.Players[0].JoinOrder, 0);
		}
		TArray<uint8> V1Truncated = V1;
		V1Truncated.Pop();
		FDigetherSaveSlotData Ignored;
		TestTrue(TEXT("v1 길이 부족 거절"), DigetherSaveRules::DeserializeSlot(V1Truncated, Ignored)
			== EDigetherSaveResult::CorruptData);
	}

	// 진행 확장의 손상·미래 버전은 Hub 확장과 같은 규칙으로 거절한다.
	FDigetherSaveSlotData Output;
	TArray<uint8> Corrupt = V5;
	Corrupt[ProgressExtensionOffset] = 0;
	TestTrue(TEXT("진행 확장 magic 손상 거절"), DigetherSaveRules::DeserializeSlot(Corrupt, Output)
		== EDigetherSaveResult::CorruptData);
	Corrupt = V5;
	// 현재 진행 확장은 v2다. 그다음 번호를 미래 버전으로 쓴다.
	const int32 FutureProgressExtension = 3;
	FMemory::Memcpy(Corrupt.GetData() + ProgressExtensionOffset + sizeof(uint32), &FutureProgressExtension,
		sizeof(FutureProgressExtension));
	TestTrue(TEXT("미래 진행 확장은 미지원 버전"), DigetherSaveRules::DeserializeSlot(Corrupt, Output)
		== EDigetherSaveResult::UnsupportedVersion);
	Corrupt = V5;
	const int32 WrongPayloadSize = 4;
	FMemory::Memcpy(Corrupt.GetData() + ProgressExtensionOffset + 2 * sizeof(int32), &WrongPayloadSize,
		sizeof(WrongPayloadSize));
	TestTrue(TEXT("진행 확장 길이 불일치 거절"), DigetherSaveRules::DeserializeSlot(Corrupt, Output)
		== EDigetherSaveResult::CorruptData);
	Corrupt = V5;
	const int32 NegativeTotalDay = -1;
	FMemory::Memcpy(Corrupt.GetData() + TotalDayOffset, &NegativeTotalDay, sizeof(NegativeTotalDay));
	TestTrue(TEXT("음수 전체 일차 거절"), DigetherSaveRules::DeserializeSlot(Corrupt, Output)
		== EDigetherSaveResult::InvalidState);
	Corrupt = V5;
	const int64 NegativeTicks = -1;
	FMemory::Memcpy(Corrupt.GetData() + ProgressExtensionOffset + 3 * sizeof(int32), &NegativeTicks, sizeof(NegativeTicks));
	TestTrue(TEXT("음수 저장 시각 거절"), DigetherSaveRules::DeserializeSlot(Corrupt, Output)
		== EDigetherSaveResult::InvalidState);
	return true;
}

#endif
