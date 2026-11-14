#if WITH_DEV_AUTOMATION_TESTS

// 아래 숫자·ID는 테스트 픽스처, 기획 값 아님

#include "Game/Save/DigetherSaveGame.h"
#include "Game/Save/DigetherSaveRules.h"
#include "Game/Save/DigetherSaveTuning.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Misc/AutomationTest.h"
#include "OnlineSubsystemTypes.h"

namespace
{
	FDigetherSaveSnapshot MakeSaveFixture()
	{
		// 수치·ID는 저장 계약 검사용 픽스처, 기획 값 아님
		FDigetherSaveSnapshot Snapshot;
		Snapshot.HostAuthenticatedPlayerKey = TEXT("SAVE_TEST:host");
		Snapshot.GameId = FGuid(1, 2, 3, 4);
		Snapshot.RunId = FGuid(5, 6, 7, 8);
		Snapshot.Quota.Quota.Day = 1;
		Snapshot.Quota.Quota.TotalDay = 1;
		Snapshot.Quota.Quota.TargetCredits = 100;
		Snapshot.RunTuning.SourceProfile = FPrimaryAssetId(TEXT("DigetherTuningProfile:DA_SaveFixture"));
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
		Snapshot.RunTuning.RecoveredDeathLossPercent = 50;
		FDigetherSavePlayerRecord Host;
		Host.AuthenticatedPlayerKey = Snapshot.HostAuthenticatedPlayerKey;
		Host.GameplayPlayerId = FGuid(11, 12, 13, 14);
		Host.Wallet.BalanceCredits = 70;
		Host.Wallet.HiddenLifetimeCredits = 15;
		Host.bHasHubTransform = true;
		Host.HubTransform = FTransform(FRotator(5.0, 30.0, -10.0), FVector(10.0, 20.0, 30.0), FVector(1.0, 2.0, 1.5));
		Snapshot.Players.Add(Host);
		FDigetherSavePlayerRecord Offline;
		Offline.AuthenticatedPlayerKey = TEXT("SAVE_TEST:offline");
		Offline.GameplayPlayerId = FGuid(21, 22, 23, 24);
		Offline.Wallet.BalanceCredits = 90;
		Snapshot.Players.Add(Offline);
		Snapshot.CompletedSettlementRunIds.Add(FGuid(31, 32, 33, 34));
		return Snapshot;
	}

	FUniqueNetIdRepl MakeSaveIdentity(const FString& Account, const FName Provider = FName(TEXT("SAVE_TEST")))
	{
		return FUniqueNetIdRepl(FUniqueNetIdRef(FUniqueNetIdString::Create(Account, Provider)));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSaveRoundTripTest,
	"DIGETHER.Game.Save.Schema.V3MemoryRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSaveRoundTripTest::RunTest(const FString& Parameters)
{
	const FDigetherSaveSnapshot Snapshot = MakeSaveFixture();
	FDigetherSaveSlotData Slot;
	if (!TestTrue(TEXT("v3 값으로 진행을 구성"), DigetherSaveRules::TryBuildSlot(Snapshot, nullptr,
		false, Slot) == EDigetherSaveResult::Success))
	{
		return false;
	}
	// 폐기된 출발 기준점이 든 이전 파일도 같은 형식으로 읽고 다시 쓰기(저장 형식 유지)
	Slot.DepartureCheckpoint = Snapshot;
	Slot.bHasDepartureCheckpoint = true;
	TArray<uint8> Bytes;
	if (!TestTrue(TEXT("월드 없는 메모리 직렬화"),
		DigetherSaveRules::SerializeSlot(Slot, Bytes) == EDigetherSaveResult::Success))
	{
		return false;
	}
	int32 StoredVersion = 0;
	FMemory::Memcpy(&StoredVersion, Bytes.GetData(), sizeof(StoredVersion));
	// 첫 int32는 진행 스키마(v3)가 아닌 wire 버전, 새 쓰기는 항상 최신 wire
	TestEqual(TEXT("첫 4바이트는 int32 wire 버전"), StoredVersion, DigetherSaveWire::LatestVersion);
	TestEqual(TEXT("버전 바로 뒤에 기준점 존재 여부 기록"), Bytes[sizeof(int32)], static_cast<uint8>(1));
	FDigetherSaveSlotData Restored;
	TestTrue(TEXT("v3 메모리 역직렬화"), DigetherSaveRules::DeserializeSlot(Bytes, Restored) == EDigetherSaveResult::Success);
	TArray<uint8> EncodedAgain;
	TestTrue(TEXT("재직렬화 성공"), DigetherSaveRules::SerializeSlot(Restored, EncodedAgain) == EDigetherSaveResult::Success);
	TestTrue(TEXT("진행·기준점의 모든 저장 필드가 동일"), Bytes == EncodedAgain);
	TestEqual(TEXT("미접속 참가자도 보존"), Restored.Progress.Players.Num(), 2);
	TestEqual(TEXT("고정 튜닝 버전 보존"), Restored.Progress.RunTuning.ProfileVersion, 7);
	TestEqual(TEXT("전체 일차 보존"), Restored.Progress.Quota.Quota.TotalDay, 1);
	TestEqual(TEXT("기준점 전체 일차 보존"), Restored.DepartureCheckpoint.Quota.Quota.TotalDay, 1);
	TestEqual(TEXT("고정 튜닝 실제 값 보존"), Restored.Progress.RunTuning.RunSeconds, 120.f);
	TestTrue(TEXT("고정 튜닝 출처 보존"), Restored.Progress.RunTuning.SourceProfile == Snapshot.RunTuning.SourceProfile);
	TestTrue(TEXT("완료 정산 기록 보존"), Restored.Progress.CompletedSettlementRunIds == Snapshot.CompletedSettlementRunIds);
	TestTrue(TEXT("GameplayPlayerId 보존"), Restored.Progress.Players[1].GameplayPlayerId == Snapshot.Players[1].GameplayPlayerId);
	TestTrue(TEXT("출발 기준점 보존"), Restored.bHasDepartureCheckpoint);
	TestTrue(TEXT("거점 Transform 존재 여부 보존"), Restored.Progress.Players[0].bHasHubTransform);
	TestTrue(TEXT("거점 위치 보존"), Restored.Progress.Players[0].HubTransform.GetTranslation()
		.Equals(Snapshot.Players[0].HubTransform.GetTranslation()));
	TestTrue(TEXT("거점 회전 보존"), Restored.Progress.Players[0].HubTransform.GetRotation()
		.Equals(Snapshot.Players[0].HubTransform.GetRotation()));
	TestTrue(TEXT("거점 크기 보존"), Restored.Progress.Players[0].HubTransform.GetScale3D()
		.Equals(Snapshot.Players[0].HubTransform.GetScale3D()));
	TestTrue(TEXT("기준점의 거점 Transform 보존"), Restored.DepartureCheckpoint.Players[0].HubTransform
		.Equals(Snapshot.Players[0].HubTransform));
	TestTrue(TEXT("거점 Transform 없는 미접속 레코드 유지"), !Restored.Progress.Players[1].bHasHubTransform);

	FDigetherSaveRuntimeState Runtime;
	TestTrue(TEXT("저장 값에서 런타임 값 복원"), DigetherSaveRules::TryRestoreSnapshot(Restored.Progress, Runtime)
		== EDigetherSaveResult::Success);
	FDigetherSaveSnapshot Captured;
	TestTrue(TEXT("런타임 값에서 저장 값 캡처"), DigetherSaveRules::TryCaptureSnapshot(Runtime, Captured)
		== EDigetherSaveResult::Success);
	TestTrue(TEXT("순수 변환의 게임 식별자 보존"), Captured.GameId == Snapshot.GameId);
	TestEqual(TEXT("순수 변환의 지갑 보존"), Captured.Players[0].Wallet.BalanceCredits, 70);

	UDigetherSaveGame* SaveGame = NewObject<UDigetherSaveGame>();
	TestTrue(TEXT("실제 SaveGame 래퍼도 같은 v3 codec으로 읽음"), SaveGame->DecodeFromMemory(Bytes)
		== EDigetherSaveResult::Success);
	TArray<uint8> WrappedBytes;
	TestTrue(TEXT("실제 SaveGame 래퍼 쓰기 성공"), SaveGame->EncodeToMemory(WrappedBytes)
		== EDigetherSaveResult::Success);
	TestTrue(TEXT("실제 래퍼와 순수 codec의 바이트 일치"), WrappedBytes == Bytes);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSaveRejectTest,
	"DIGETHER.Game.Save.Schema.RejectWithoutMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSaveRejectTest::RunTest(const FString& Parameters)
{
	const FDigetherSaveSnapshot Snapshot = MakeSaveFixture();
	FDigetherSaveSlotData Slot;
	DigetherSaveRules::TryBuildSlot(Snapshot, nullptr, false, Slot);
	// 이전 파일처럼 출발 기준점이 든 바이트로 거절 경로 검사(저장 형식 유지)
	Slot.DepartureCheckpoint = Snapshot;
	Slot.bHasDepartureCheckpoint = true;
	TArray<uint8> Bytes;
	if (!TestTrue(TEXT("거절 검사용 v3 바이트 구성"),
		DigetherSaveRules::SerializeSlot(Slot, Bytes) == EDigetherSaveResult::Success))
	{
		return false;
	}
	FDigetherSaveSlotData Output = Slot;
	const FGuid Sentinel(91, 92, 93, 94);
	Output.Progress.GameId = Sentinel;
	for (const int32 Version : { -1, 0, 2, DigetherSaveWire::LatestVersion + 1 })
	{
		const EDigetherSaveResult Expected = Version > DigetherSaveWire::LatestVersion
			? EDigetherSaveResult::UnsupportedVersion : EDigetherSaveResult::CorruptData;
		TArray<uint8> WrongVersion = Bytes;
		FMemory::Memcpy(WrongVersion.GetData(), &Version, sizeof(Version));
		TestTrue(TEXT("이전 버전은 손상·미래 버전은 미지원으로 거절"),
			DigetherSaveRules::DeserializeSlot(WrongVersion, Output) == Expected);
		TestTrue(TEXT("버전 거절 시 출력 유지"), Output.Progress.GameId == Sentinel);
		WrongVersion.SetNum(sizeof(int32));
		TestTrue(TEXT("버전별 분류는 본문 파싱 전에 확정"),
			DigetherSaveRules::DeserializeSlot(WrongVersion, Output) == Expected);
		TestTrue(TEXT("본문 없는 미지원 버전도 출력 유지"), Output.Progress.GameId == Sentinel);
	}
	TArray<uint8> Corrupt = Bytes;
	Corrupt[sizeof(int32)] = 2;
	TestTrue(TEXT("잘못된 bool 표현 거절"), DigetherSaveRules::DeserializeSlot(Corrupt, Output) == EDigetherSaveResult::CorruptData);
	TestTrue(TEXT("손상 거절 시 출력 유지"), Output.Progress.GameId == Sentinel);
	Corrupt = Bytes;
	Corrupt.SetNum(Corrupt.Num() - 1);
	TestTrue(TEXT("잘린 파일 거절"), DigetherSaveRules::DeserializeSlot(Corrupt, Output) == EDigetherSaveResult::CorruptData);
	TestTrue(TEXT("잘린 파일 거절 시 출력 유지"), Output.Progress.GameId == Sentinel);
	Corrupt = Bytes;
	Corrupt.Add(0);
	TestTrue(TEXT("본문 뒤 불필요한 바이트 거절"), DigetherSaveRules::DeserializeSlot(Corrupt, Output)
		== EDigetherSaveResult::CorruptData);
	TestTrue(TEXT("남는 바이트 거절 시 출력 유지"), Output.Progress.GameId == Sentinel);
	Corrupt = Bytes;
	Corrupt.SetNum(sizeof(int32) - 1);
	TestTrue(TEXT("버전도 완전하지 않은 바이트 거절"), DigetherSaveRules::DeserializeSlot(Corrupt, Output)
		== EDigetherSaveResult::CorruptData);
	TestTrue(TEXT("불완전 버전 거절 시 출력 유지"), Output.Progress.GameId == Sentinel);
	Corrupt = Bytes;
	Corrupt.SetNumZeroed(DigetherSaveTuning::MaxSaveBytes + 1);
	TestTrue(TEXT("전체 바이트 상한 초과 거절"), DigetherSaveRules::DeserializeSlot(Corrupt, Output)
		== EDigetherSaveResult::CorruptData);
	TestTrue(TEXT("전체 바이트 상한 거절 시 출력 유지"), Output.Progress.GameId == Sentinel);
	Corrupt = Bytes;
	// CRC와 별도로 codec 자체가 필수 값 의미 검증
	const int32 GameIdOffset = sizeof(int32) + sizeof(uint8) + sizeof(int32)
		+ Snapshot.HostAuthenticatedPlayerKey.Len() * sizeof(uint16);
	FMemory::Memzero(Corrupt.GetData() + GameIdOffset, sizeof(FGuid));
	TestTrue(TEXT("필수 필드 이상 거절"), DigetherSaveRules::DeserializeSlot(Corrupt, Output) == EDigetherSaveResult::InvalidState);
	TestTrue(TEXT("필수 필드 거절 시 출력 유지"), Output.Progress.GameId == Sentinel);
	Corrupt = Bytes;
	const int32 ExcessiveLength = DigetherSaveTuning::MaxStringLength + 1;
	FMemory::Memcpy(Corrupt.GetData() + sizeof(int32) + sizeof(uint8),
		&ExcessiveLength, sizeof(ExcessiveLength));
	TestTrue(TEXT("과다 길이를 할당 전에 거절"), DigetherSaveRules::DeserializeSlot(Corrupt, Output)
		== EDigetherSaveResult::CorruptData);
	TestTrue(TEXT("길이 거절 시 출력 유지"), Output.Progress.GameId == Sentinel);
	const double PositionPattern[] = { 10.0, 20.0, 30.0 };
	int32 PositionOffset = INDEX_NONE;
	for (int32 Index = sizeof(int32);
		Index <= Bytes.Num() - static_cast<int32>(sizeof(PositionPattern)); ++Index)
	{
		if (FMemory::Memcmp(Bytes.GetData() + Index, PositionPattern, sizeof(PositionPattern)) == 0)
		{
			PositionOffset = Index;
			break;
		}
	}
	if (TestTrue(TEXT("명시 성분으로 저장한 거점 위치를 찾음"), PositionOffset != INDEX_NONE))
	{
		// 위치·회전·크기는 double 3·4·3개로 기록, 비유한 성분은 Transform 생성 전 거절
		const int32 ComponentOffsets[] = { 0, 3, 7 };
		const uint64 NotFiniteBits = 0x7ff8000000000000ULL;
		for (const int32 ComponentOffset : ComponentOffsets)
		{
			Corrupt = Bytes;
			FMemory::Memcpy(Corrupt.GetData() + PositionOffset + ComponentOffset * sizeof(double),
				&NotFiniteBits, sizeof(NotFiniteBits));
			TestTrue(TEXT("거점 위치·회전·크기의 비유한 성분 거절"), DigetherSaveRules::DeserializeSlot(Corrupt, Output)
				== EDigetherSaveResult::CorruptData);
			TestTrue(TEXT("거점 Transform 거절 시 출력 유지"), Output.Progress.GameId == Sentinel);
		}
	}

	FDigetherSaveSnapshot Invalid = Snapshot;
	Invalid.Players[1].GameplayPlayerId = Invalid.Players[0].GameplayPlayerId;
	TArray<uint8> UnchangedBytes = { 7, 8, 9 };
	TestTrue(TEXT("중복 참가자 GUID 저장 거절"), DigetherSaveRules::TryBuildSlot(Invalid, nullptr, false, Output)
		== EDigetherSaveResult::InvalidState);
	TestTrue(TEXT("구성 실패 시 출력 유지"), Output.Progress.GameId == Sentinel);
	for (const int32 Version : { -1, 0, 2, 4 })
	{
		Invalid = Snapshot;
		Invalid.SchemaVersion = Version;
		const EDigetherSaveResult Expected = Version > DigetherSaveTuning::SchemaVersion
			? EDigetherSaveResult::UnsupportedVersion : EDigetherSaveResult::CorruptData;
		FDigetherSaveRuntimeState Runtime;
		Runtime.GameId = Sentinel;
		TestTrue(TEXT("이전·미래 스냅샷 복원 분류"), DigetherSaveRules::TryRestoreSnapshot(Invalid, Runtime) == Expected);
		TestTrue(TEXT("복원 실패 시 런타임 출력 유지"), Runtime.GameId == Sentinel);
		Slot.Progress = Invalid;
		TestTrue(TEXT("이전·미래 스냅샷 저장 거절"), DigetherSaveRules::SerializeSlot(Slot, UnchangedBytes) == Expected);
		TestTrue(TEXT("직렬화 실패 시 바이트 출력 유지"), UnchangedBytes == TArray<uint8>({ 7, 8, 9 }));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSaveCompletedInProgressTest,
	"DIGETHER.Game.Save.Schema.InProgressCompletedRunRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSaveCompletedInProgressTest::RunTest(const FString& Parameters)
{
	FDigetherSaveSnapshot Invalid = MakeSaveFixture();
	Invalid.RunPublicState.Phase = EDigetherRunPhase::InProgress;
	Invalid.CompletedSettlementRunIds.Add(Invalid.RunId);
	TestTrue(TEXT("진행 중 현재 RunId가 정산 완료 목록에 있으면 거절"),
		DigetherSaveRules::ValidateSnapshot(Invalid) == EDigetherSaveResult::UnsafePhase);
	const FGuid Sentinel(91, 92, 93, 94);
	FDigetherSaveSnapshot Captured;
	Captured.GameId = Sentinel;
	TestTrue(TEXT("모순된 진행 상태 캡처 거절"),
		DigetherSaveRules::TryCaptureSnapshot(Invalid, Captured) == EDigetherSaveResult::UnsafePhase);
	TestTrue(TEXT("캡처 실패 시 출력 유지"), Captured.GameId == Sentinel);
	FDigetherSaveRuntimeState Restored;
	Restored.GameId = Sentinel;
	TestTrue(TEXT("모순된 진행 상태 복원 거절"),
		DigetherSaveRules::TryRestoreSnapshot(Invalid, Restored) == EDigetherSaveResult::UnsafePhase);
	TestTrue(TEXT("복원 실패 시 출력 유지"), Restored.GameId == Sentinel);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSaveSlotSelectionTest,
	"DIGETHER.Game.Save.Slots.SelectionAndWritePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSaveSlotSelectionTest::RunTest(const FString& Parameters)
{
	using EResult = EDigetherSaveResult;
	struct FCase
	{
		const TCHAR* Label;
		EResult Primary;
		EResult Backup;
		EResult ReadResult;
		bool bUseBackup;
		EResult WriteResult;
	};
	const FCase Cases[] =
	{
		{ TEXT("양쪽 정상"), EResult::Success, EResult::Success,
			EResult::Success, false, EResult::Success },
		{ TEXT("primary 정상·backup 없음"), EResult::Success, EResult::NotFound,
			EResult::Success, false, EResult::Success },
		{ TEXT("primary 정상·backup 손상"), EResult::Success, EResult::CorruptData,
			EResult::Success, false, EResult::Success },
		{ TEXT("primary 정상·backup 의미 오류"), EResult::Success, EResult::InvalidState,
			EResult::Success, false, EResult::Success },
		{ TEXT("primary 정상·backup 단계 오류"), EResult::Success, EResult::UnsafePhase,
			EResult::Success, false, EResult::Success },
		{ TEXT("primary 없음·backup 정상"), EResult::NotFound, EResult::Success,
			EResult::Success, true, EResult::Success },
		{ TEXT("primary 손상·backup 정상"), EResult::CorruptData, EResult::Success,
			EResult::Success, true, EResult::Success },
		{ TEXT("primary 의미 오류·backup 정상"), EResult::InvalidState, EResult::Success,
			EResult::Success, true, EResult::Success },
		{ TEXT("primary 단계 오류·backup 정상"), EResult::UnsafePhase, EResult::Success,
			EResult::Success, true, EResult::Success },
		{ TEXT("양쪽 없음"), EResult::NotFound, EResult::NotFound,
			EResult::NotFound, false, EResult::Success },
		{ TEXT("primary 없음·backup 손상"), EResult::NotFound, EResult::CorruptData,
			EResult::CorruptData, false, EResult::CorruptData },
		{ TEXT("primary 손상·backup 없음"), EResult::CorruptData, EResult::NotFound,
			EResult::CorruptData, false, EResult::CorruptData },
		{ TEXT("양쪽 손상"), EResult::CorruptData, EResult::CorruptData,
			EResult::CorruptData, false, EResult::CorruptData },
		{ TEXT("양쪽 의미·단계 오류"), EResult::InvalidState, EResult::UnsafePhase,
			EResult::InvalidState, false, EResult::InvalidState },
		{ TEXT("primary 읽기 실패·backup 정상"), EResult::IoFailure, EResult::Success,
			EResult::IoFailure, false, EResult::IoFailure },
		{ TEXT("primary 호스트 불일치·backup 정상"), EResult::HostIdentityMismatch, EResult::Success,
			EResult::HostIdentityMismatch, false, EResult::HostIdentityMismatch },
		{ TEXT("primary 권위 없음·backup 정상"), EResult::NotAuthority, EResult::Success,
			EResult::NotAuthority, false, EResult::NotAuthority },
		{ TEXT("primary 없음·backup 읽기 실패"), EResult::NotFound, EResult::IoFailure,
			EResult::IoFailure, false, EResult::IoFailure },
		{ TEXT("primary 손상·backup 호스트 불일치"), EResult::CorruptData, EResult::HostIdentityMismatch,
			EResult::HostIdentityMismatch, false, EResult::HostIdentityMismatch },
		{ TEXT("primary 정상·backup 읽기 실패"), EResult::Success, EResult::IoFailure,
			EResult::Success, false, EResult::IoFailure },
		{ TEXT("primary 정상·backup 호스트 불일치"), EResult::Success, EResult::HostIdentityMismatch,
			EResult::Success, false, EResult::HostIdentityMismatch },
		{ TEXT("primary 정상·backup 호스트 누락"), EResult::Success, EResult::HostIdentityMissing,
			EResult::Success, false, EResult::HostIdentityMissing },
		{ TEXT("primary 미지원 버전·backup 정상"), EResult::UnsupportedVersion, EResult::Success,
			EResult::UnsupportedVersion, false, EResult::UnsupportedVersion },
		{ TEXT("primary 정상·backup 미지원 버전"), EResult::Success, EResult::UnsupportedVersion,
			EResult::UnsupportedVersion, false, EResult::UnsupportedVersion },
		{ TEXT("primary 미지원 버전·backup 읽기 실패"), EResult::UnsupportedVersion, EResult::IoFailure,
			EResult::UnsupportedVersion, false, EResult::UnsupportedVersion },
		{ TEXT("primary 읽기 실패·backup 미지원 버전"), EResult::IoFailure, EResult::UnsupportedVersion,
			EResult::UnsupportedVersion, false, EResult::UnsupportedVersion }
	};
	for (const FCase& Case : Cases)
	{
		for (const bool bInitialUseBackup : { false, true })
		{
			bool bUseBackup = bInitialUseBackup;
			TestTrue(*FString::Printf(TEXT("%s 읽기 결과"), Case.Label),
				DigetherSaveRules::SelectReadableSlot(Case.Primary, Case.Backup, bUseBackup) == Case.ReadResult);
			TestEqual(*FString::Printf(TEXT("%s 읽기 성공 시 선택·실패 시 출력 유지"), Case.Label), bUseBackup,
				Case.ReadResult == EResult::Success ? Case.bUseBackup : bInitialUseBackup);
		}
		TestTrue(*FString::Printf(TEXT("%s 일반 저장 허용 여부"), Case.Label),
			DigetherSaveRules::CanWriteSlots(Case.Primary, Case.Backup, false) == Case.WriteResult);
		const EResult ExplicitWriteResult = Case.Primary == EResult::UnsupportedVersion
			|| Case.Backup == EResult::UnsupportedVersion ? EResult::UnsupportedVersion : EResult::Success;
		TestTrue(*FString::Printf(TEXT("%s 명시 새 게임도 미래 버전 보호"), Case.Label),
			DigetherSaveRules::CanWriteSlots(Case.Primary, Case.Backup, true) == ExplicitWriteResult);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSaveWriteStepTest,
	"DIGETHER.Game.Save.Slots.VerifiedWriteOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSaveWriteStepTest::RunTest(const FString& Parameters)
{
	using EStep = EDigetherSaveWriteStep;
	struct FCase
	{
		EStep Current;
		EStep OnSuccess;
		EStep OnFailure;
	};
	const FCase Cases[] =
	{
		{ EStep::WriteBackup, EStep::VerifyBackup, EStep::Failed },
		{ EStep::VerifyBackup, EStep::WritePrimary, EStep::Failed },
		{ EStep::WritePrimary, EStep::VerifyPrimary, EStep::Failed },
		{ EStep::VerifyPrimary, EStep::Complete, EStep::Failed },
		{ EStep::Complete, EStep::Complete, EStep::Complete },
		{ EStep::Failed, EStep::Failed, EStep::Failed }
	};
	for (const FCase& Case : Cases)
	{
		TestTrue(TEXT("성공할 때만 backup 쓰기·검증·primary 쓰기·검증 순서로 진행"),
			DigetherSaveRules::AdvanceWriteStep(Case.Current, true) == Case.OnSuccess);
		TestTrue(TEXT("중간 실패는 즉시 중단하고 종료 상태는 유지"),
			DigetherSaveRules::AdvanceWriteStep(Case.Current, false) == Case.OnFailure);
	}
	using ESave = EDigetherSaveResult;
	TestTrue(TEXT("주 손상·백업 유효면 유일한 유효본(백업)을 나중에 쓴다"),
		DigetherSaveRules::ShouldWritePrimaryFirst(ESave::CorruptData, ESave::Success));
	TestTrue(TEXT("주 없음·백업 유효면 주부터 쓴다"),
		DigetherSaveRules::ShouldWritePrimaryFirst(ESave::NotFound, ESave::Success));
	TestFalse(TEXT("둘 다 유효면 백업부터 쓴다"),
		DigetherSaveRules::ShouldWritePrimaryFirst(ESave::Success, ESave::Success));
	TestFalse(TEXT("주만 유효면 백업부터 쓴다"),
		DigetherSaveRules::ShouldWritePrimaryFirst(ESave::Success, ESave::CorruptData));
	TestFalse(TEXT("둘 다 없으면 백업부터 쓴다"),
		DigetherSaveRules::ShouldWritePrimaryFirst(ESave::NotFound, ESave::NotFound));
	for (const EStep FailureStep : { EStep::WriteBackup, EStep::VerifyBackup })
	{
		TArray<EStep> VisitedSteps;
		EStep Current = EStep::WriteBackup;
		for (int32 VisitCount = 0; VisitCount < 4
			&& Current != EStep::Complete && Current != EStep::Failed; ++VisitCount)
		{
			VisitedSteps.Add(Current);
			Current = DigetherSaveRules::AdvanceWriteStep(Current, Current != FailureStep);
		}
		TestTrue(TEXT("backup 쓰기·검증 실패가 실행 루프를 Failed로 종료"), Current == EStep::Failed);
		TestEqual(TEXT("backup 실패 단계까지만 실행"), VisitedSteps.Num(), FailureStep == EStep::WriteBackup ? 1 : 2);
		TestTrue(TEXT("backup 실패 후 primary 쓰기를 실행하지 않음"), !VisitedSteps.Contains(EStep::WritePrimary));
		TestTrue(TEXT("backup 실패 후 primary 검증을 실행하지 않음"), !VisitedSteps.Contains(EStep::VerifyPrimary));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSaveSettlementTest,
	"DIGETHER.Game.Save.Settlement.CompletedRunPreventsDuplicate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSaveSettlementTest::RunTest(const FString& Parameters)
{
	const FDigetherSaveSnapshot Snapshot = MakeSaveFixture();
	FDigetherPlayerSettlement HostSettlement;
	HostSettlement.GeneralCredits = 20;
	HostSettlement.PayoutCredits = 20;
	FDigetherPlayerSettlement OfflineSettlement;
	OfflineSettlement.HiddenCredits = 5;
	OfflineSettlement.PayoutCredits = 5;
	const TArray<FDigetherPlayerSettlement> Settlements = { HostSettlement, OfflineSettlement };
	FDigetherSaveSnapshot Settled;
	TestTrue(TEXT("접속·미접속 참가자 정산을 한 후보에 반영"), DigetherSaveRules::TryApplySettlement(
		Snapshot, Snapshot.RunId, Settlements, Settled) == EDigetherSaveResult::Success);
	TestEqual(TEXT("호스트 지급"), Settled.Players[0].Wallet.BalanceCredits, 90);
	TestEqual(TEXT("미접속 지급"), Settled.Players[1].Wallet.BalanceCredits, 95);
	TestTrue(TEXT("지급과 함께 완료 RunId 기록"), Settled.CompletedSettlementRunIds.Contains(Snapshot.RunId));
	FDigetherSaveSlotData Slot;
	DigetherSaveRules::TryBuildSlot(Settled, nullptr, false, Slot);
	TArray<uint8> Bytes;
	DigetherSaveRules::SerializeSlot(Slot, Bytes);
	FDigetherSaveSlotData Reloaded;
	DigetherSaveRules::DeserializeSlot(Bytes, Reloaded);
	FDigetherSaveSnapshot Output = Snapshot;
	const FGuid Sentinel(71, 72, 73, 74);
	Output.GameId = Sentinel;
	TestTrue(TEXT("재로드 후 같은 런 중복 정산 거절"), DigetherSaveRules::TryApplySettlement(
		Reloaded.Progress, Snapshot.RunId, Settlements, Output) == EDigetherSaveResult::AlreadySettled);
	TestTrue(TEXT("중복 정산 거절 시 출력 유지"), Output.GameId == Sentinel);
	TestEqual(TEXT("재로드 잔고는 한 번 지급된 값"), Reloaded.Progress.Players[0].Wallet.BalanceCredits, 90);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSaveReconnectRecordTest,
	"DIGETHER.Game.Save.Records.IdentityAndOfflineDeath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSaveReconnectRecordTest::RunTest(const FString& Parameters)
{
	const FDigetherSaveSnapshot Snapshot = MakeSaveFixture();
	FDigetherSavePlayerRecord Output;
	bool bSkipInitialGrant = false;
	TestTrue(TEXT("인증 제공자·ID로 기존 참가자를 찾음"), DigetherSaveRules::TryResolveReturningPlayer(
		Snapshot.Players, MakeSaveIdentity(TEXT("offline")), Output, bSkipInitialGrant) == EDigetherSaveResult::Success);
	TestTrue(TEXT("불러온 참가자는 초기 지급 생략"), bSkipInitialGrant);
	TestTrue(TEXT("저장된 GameplayPlayerId 유지"), Output.GameplayPlayerId == Snapshot.Players[1].GameplayPlayerId);
	TestEqual(TEXT("저장된 잔고 복원"), Output.Wallet.BalanceCredits, 90);
	const FGuid Sentinel = Output.GameplayPlayerId;
	TestTrue(TEXT("같은 문자열이라도 다른 제공자는 다른 참가자"), DigetherSaveRules::TryResolveReturningPlayer(
		Snapshot.Players, MakeSaveIdentity(TEXT("offline"), FName(TEXT("OTHER_TEST"))), Output, bSkipInitialGrant)
		== EDigetherSaveResult::PlayerNotFound);
	TestTrue(TEXT("매칭 실패 시 출력 레코드 유지"), Output.GameplayPlayerId == Sentinel);
	FString Key = TEXT("보존");
	TestTrue(TEXT("유효하지 않은 인증 ID 거절"), DigetherSaveRules::MakeAuthenticatedPlayerKey(FUniqueNetIdRepl(), Key)
		== EDigetherSaveResult::PlayerIdentityMismatch);
	TestEqual(TEXT("무효 ID 실패 시 키 출력 유지"), Key, FString(TEXT("보존")));

	FDigetherSavePlayerRecord Dead;
	TestTrue(TEXT("미접속 레코드에도 같은 사망·손실 규칙 적용"), DigetherSaveRules::TryApplyOfflineDeath(
		Snapshot.Players[1], EDigetherDeathCause::Killed, 100, Dead) == EDigetherSaveResult::Success);
	TestTrue(TEXT("미접속 사망 상태 유지"), Dead.Life.PublicState.Life == EDigetherLifeState::Dead);
	TestEqual(TEXT("미접속 잔고 손실"), Dead.Wallet.BalanceCredits, 0);
	TestEqual(TEXT("사망 직전 잔고 보존"), Dead.Wallet.LastPreDeathBalanceCredits, 90);
	TestEqual(TEXT("손실 통계 보존"), Dead.Wallet.TotalDeathLossCredits, 90);
	Output = Snapshot.Players[0];
	TestTrue(TEXT("같은 사망을 두 번 적용하지 않음"), DigetherSaveRules::TryApplyOfflineDeath(
		Dead, EDigetherDeathCause::Killed, 100, Output) == EDigetherSaveResult::InvalidState);
	TestEqual(TEXT("중복 사망 실패 시 출력 지갑 유지"), Output.Wallet.BalanceCredits, 70);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSaveSlotWriteTest,
	"DIGETHER.Game.Save.Slot.ProgressWriteDropsLegacyCheckpointAndExplicitReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSaveSlotWriteTest::RunTest(const FString& Parameters)
{
	const FDigetherSaveSnapshot Snapshot = MakeSaveFixture();
	FDigetherSaveSlotData Slot;
	TestTrue(TEXT("첫 진행 슬롯 구성"), DigetherSaveRules::TryBuildSlot(Snapshot, nullptr, false, Slot)
		== EDigetherSaveResult::Success);
	TestFalse(TEXT("새 슬롯에는 출발 기준점을 만들지 않음"), Slot.bHasDepartureCheckpoint);
	// 폐기 규칙으로 쓰인 이전 파일: 기준점 지갑이 진행과 다름
	Slot.DepartureCheckpoint = Snapshot;
	Slot.DepartureCheckpoint.Players[0].Wallet.BalanceCredits = 10;
	Slot.bHasDepartureCheckpoint = true;
	FDigetherSaveSnapshot Changed = Snapshot;
	Changed.Players[0].Wallet.BalanceCredits = 60;
	TestTrue(TEXT("이전 기준점이 있는 슬롯에 같은 게임 진행 쓰기"), DigetherSaveRules::TryBuildSlot(Changed, &Slot, false, Slot)
		== EDigetherSaveResult::Success);
	TestEqual(TEXT("진행은 최신 저장 시점 값"), Slot.Progress.Players[0].Wallet.BalanceCredits, 60);
	TestFalse(TEXT("새로 쓴 슬롯은 이전 기준점을 옮기지 않음"), Slot.bHasDepartureCheckpoint);
	TArray<uint8> Bytes;
	FDigetherSaveSlotData Reloaded;
	TestTrue(TEXT("기준점 없는 슬롯 직렬화"), DigetherSaveRules::SerializeSlot(Slot, Bytes) == EDigetherSaveResult::Success);
	TestTrue(TEXT("기준점 없는 슬롯 역직렬화"), DigetherSaveRules::DeserializeSlot(Bytes, Reloaded) == EDigetherSaveResult::Success);
	TestEqual(TEXT("다시 읽은 진행 지갑"), Reloaded.Progress.Players[0].Wallet.BalanceCredits, 60);
	TestFalse(TEXT("다시 읽어도 기준점 없음"), Reloaded.bHasDepartureCheckpoint);

	// 탐사 중 상태는 미저장(S30), 거절해도 이전 슬롯 보존
	Changed = Snapshot;
	Changed.RunPublicState.Phase = EDigetherRunPhase::InProgress;
	const FDigetherSaveSlotData Before = Slot;
	TestTrue(TEXT("탐사 중 진행 저장 거절"), DigetherSaveRules::TryBuildSlot(Changed, &Slot, false, Slot)
		== EDigetherSaveResult::UnsafePhase);
	TestEqual(TEXT("거절 시 이전 진행 지갑 보존"), Slot.Progress.Players[0].Wallet.BalanceCredits,
		Before.Progress.Players[0].Wallet.BalanceCredits);
	TestTrue(TEXT("거절 시 이전 진행 게임 보존"), Slot.Progress.GameId == Before.Progress.GameId);

	Changed = Snapshot;
	Changed.Quota.Quota.Day = 4;
	TestTrue(TEXT("4일차 거점 진행은 마감 표시가 false여도 유효"), DigetherSaveRules::ValidateSnapshot(Changed)
		== EDigetherSaveResult::Success);
	Changed.RunPublicState.bPaymentDeadline = true;
	TestTrue(TEXT("4일차 게이지 완료에서 기록한 마감 표시도 유효"), DigetherSaveRules::ValidateSnapshot(Changed)
		== EDigetherSaveResult::Success);
	TestTrue(TEXT("기록한 마감은 진행 저장에 그대로 남음"), DigetherSaveRules::TryBuildSlot(Changed, &Slot, false, Slot)
		== EDigetherSaveResult::Success && Slot.Progress.RunPublicState.bPaymentDeadline);
	Changed.Quota.Quota.Day = 3;
	TestTrue(TEXT("다른 일차에 마감 표시를 쓰면 거절"), DigetherSaveRules::ValidateSnapshot(Changed)
		== EDigetherSaveResult::InvalidState);

	Changed = Snapshot;
	Changed.GameId = FGuid(41, 42, 43, 44);
	TestTrue(TEXT("일반 쓰기로 다른 게임을 덮지 않음"), DigetherSaveRules::TryBuildSlot(Changed, &Slot, false, Slot)
		== EDigetherSaveResult::InvalidState);
	Slot.DepartureCheckpoint = Slot.Progress;
	Slot.bHasDepartureCheckpoint = true;
	TestTrue(TEXT("명시 새 게임은 슬롯 전체 초기화"), DigetherSaveRules::TryBuildSlot(Changed, &Slot, true, Slot)
		== EDigetherSaveResult::Success);
	TestTrue(TEXT("명시 초기화 뒤 이전 기준점 제거"), !Slot.bHasDepartureCheckpoint);
	Slot.Progress.SchemaVersion = 4;
	TestTrue(TEXT("미래 버전은 일반 쓰기 거절"), DigetherSaveRules::TryBuildSlot(Changed, &Slot, false, Slot)
		== EDigetherSaveResult::UnsupportedVersion);
	TestTrue(TEXT("명시 초기화 값 후보 구성(디스크 쓰기 권한은 별도 판정)"),
		DigetherSaveRules::TryBuildSlot(Changed, &Slot, true, Slot)
		== EDigetherSaveResult::Success);
	Changed = Snapshot;
	Changed.Quota.Quota.Day = 5;
	Changed.RunPublicState.bGameOver = true;
	Changed.RunPublicState.bPaymentDeadline = true;
	TestTrue(TEXT("게임 오버 확정 진행은 저장 가능"), DigetherSaveRules::ValidateSnapshot(Changed) == EDigetherSaveResult::Success);
	TestTrue(TEXT("게임 오버 확정 진행 슬롯 구성"), DigetherSaveRules::TryBuildSlot(Changed, nullptr, false, Slot)
		== EDigetherSaveResult::Success);
	// 슬롯 값 계층은 확정 표시 보존, 다음 실행의 같은 슬롯 새 게임 전환은 GameMode 시작 처리(S33 240행)
	TestTrue(TEXT("게임 오버 확정 표시는 슬롯에 그대로 남음"), Slot.Progress.RunPublicState.bGameOver);

	// 기획 S33: 이번 게임 결과는 게임 오버 확정 진행에만, 같은 게임 ID 필수
	FDigetherGameResult Result;
	Result.ResultId = FGuid(51, 52, 53, 54);
	Result.GameId = Changed.GameId;
	Result.FinalTotalDay = 5;
	FDigetherGameResultPlayer& ResultPlayer = Result.Players.AddDefaulted_GetRef();
	ResultPlayer.DisplayName = TEXT("Host");
	ResultPlayer.JoinOrder = 1;
	ResultPlayer.EarnedCredits = 30;
	ResultPlayer.HiddenCredits = 15;
	Result.TotalEarnedCredits = 30;
	Changed.bHasGameResult = true;
	Changed.GameResult = Result;
	TestTrue(TEXT("게임 오버 확정 진행에 결과 기록 저장 가능"), DigetherSaveRules::ValidateSnapshot(Changed)
		== EDigetherSaveResult::Success);
	FDigetherSaveSnapshot WrongGame = Changed;
	WrongGame.GameResult.GameId = FGuid(61, 62, 63, 64);
	TestTrue(TEXT("다른 게임 ID의 결과 거절"), DigetherSaveRules::ValidateSnapshot(WrongGame) == EDigetherSaveResult::InvalidState);
	FDigetherSaveSnapshot WrongTotal = Changed;
	WrongTotal.GameResult.TotalEarnedCredits = 45;
	TestTrue(TEXT("히든을 다시 더한 전체 획득 거절"), DigetherSaveRules::ValidateSnapshot(WrongTotal) == EDigetherSaveResult::InvalidState);
	FDigetherSaveSnapshot NotGameOver = Snapshot;
	NotGameOver.bHasGameResult = true;
	NotGameOver.GameResult = Result;
	TestTrue(TEXT("게임 오버가 아닌 진행의 결과 거절"), DigetherSaveRules::ValidateSnapshot(NotGameOver) == EDigetherSaveResult::InvalidState);
	// 보관한 이전 결과는 다른 게임 ID, 결과 ID 중복 없음
	FDigetherSaveSnapshot Archived = Snapshot;
	FDigetherGameResult Previous = Result;
	Previous.GameId = FGuid(71, 72, 73, 74);
	Archived.ArchivedGameResults.Add(Previous);
	TestTrue(TEXT("이전 게임 결과 보관 가능"), DigetherSaveRules::ValidateSnapshot(Archived) == EDigetherSaveResult::Success);
	Archived.ArchivedGameResults.Add(Previous);
	TestTrue(TEXT("같은 결과 ID 중복 보관 거절"), DigetherSaveRules::ValidateSnapshot(Archived) == EDigetherSaveResult::InvalidState);
	Archived.ArchivedGameResults.Reset();
	FDigetherGameResult SameGame = Previous;
	SameGame.GameId = Snapshot.GameId;
	Archived.ArchivedGameResults.Add(SameGame);
	TestTrue(TEXT("현재 게임 ID의 보관 결과 거절"), DigetherSaveRules::ValidateSnapshot(Archived) == EDigetherSaveResult::InvalidState);
	return true;
}

#endif
