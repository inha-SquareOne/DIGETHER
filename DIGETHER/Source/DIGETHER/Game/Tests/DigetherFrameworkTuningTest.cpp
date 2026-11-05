#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Data/DigetherTuningProfile.h"
#include "Game/Framework/DigetherGameplayGameMode.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	// 유효성 검사와 복사 경계를 확인하는 시험값이며 최종 기획 수치가 아니다.
	FDigetherRunTuning MakeFrameworkTuningFixture()
	{
		FDigetherRunTuning Tuning;
		Tuning.ProfileVersion = 17;
		Tuning.InitialQuotaCredits = 175;
		Tuning.RunSeconds = 540.f;
		Tuning.InteractionDistanceCm = 245.f;
		Tuning.ElevatorCapacityKg = 35.f;
		Tuning.ElevatorTripLimit = 2;
		Tuning.AscentPenaltySeconds = 5.f;
		Tuning.ElevatorTravelSeconds = 3.f;
		Tuning.DepositIntervalSeconds = 0.2f;
		Tuning.EmergencyWeightLimitKg = 4.f;
		Tuning.DeathLossPercent = 80;
		Tuning.CorpseRefundPercent = 30;
		return Tuning;
	}

	bool AreFrameworkTuningSnapshotsEqual(const FDigetherRunTuning& Left, const FDigetherRunTuning& Right)
	{
		return FDigetherRunTuning::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
	}

	bool HasFrameworkTuningProblem(const TArray<FText>& Problems, const TCHAR* FieldName)
	{
		return Problems.ContainsByPredicate([FieldName](const FText& Problem)
		{
			return Problem.ToString().Contains(FieldName);
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkMissingTuningTest,
	"DIGETHER.Game.Framework.Tuning.RejectMissingAndEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkMissingTuningTest::RunTest(const FString& Parameters)
{
	FDigetherRunTuning Snapshot = MakeFrameworkTuningFixture();
	Snapshot.SourceProfile = FPrimaryAssetId(UDigetherTuningProfile::PrimaryAssetType, TEXT("PreservedFrameworkProfile"));
	const FDigetherRunTuning PreviousSnapshot = Snapshot;
	TArray<FText> Problems;
	TestFalse(TEXT("프로필 미지정 거절"), ADigetherGameplayGameMode::TryMakeRunSnapshot(nullptr, Snapshot, Problems));
	TestTrue(TEXT("미지정 거절 이유 제공"), Problems.ContainsByPredicate([](const FText& Problem)
	{
		return !Problem.IsEmpty();
	}));
	TestTrue(TEXT("프로필 미지정 실패 시 기존 스냅샷 보존"),
		AreFrameworkTuningSnapshotsEqual(Snapshot, PreviousSnapshot));

	const UDigetherTuningProfile* EmptyProfile = NewObject<UDigetherTuningProfile>();
	Problems.Reset();
	TestFalse(TEXT("빈 프로필 거절"), ADigetherGameplayGameMode::TryMakeRunSnapshot(EmptyProfile, Snapshot, Problems));
	TestTrue(TEXT("빈 프로필의 버전 누락 이유 제공"), HasFrameworkTuningProblem(Problems, TEXT("ProfileVersion")));
	TestTrue(TEXT("빈 프로필의 목표액 누락 이유 제공"), HasFrameworkTuningProblem(Problems, TEXT("InitialQuotaCredits")));
	TestTrue(TEXT("빈 프로필 실패 시 기존 스냅샷 보존"),
		AreFrameworkTuningSnapshotsEqual(Snapshot, PreviousSnapshot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkInvalidTuningTest,
	"DIGETHER.Game.Framework.Tuning.RejectInvalidValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkInvalidTuningTest::RunTest(const FString& Parameters)
{
	UDigetherTuningProfile* InvalidProfile = NewObject<UDigetherTuningProfile>();
	InvalidProfile->Values = MakeFrameworkTuningFixture();
	InvalidProfile->Values.RunSeconds = -1.f;
	InvalidProfile->Values.CorpseRefundPercent = 101;
	FDigetherRunTuning Snapshot = MakeFrameworkTuningFixture();
	Snapshot.ProfileVersion = 29;
	Snapshot.SourceProfile = FPrimaryAssetId(UDigetherTuningProfile::PrimaryAssetType, TEXT("ExistingFrameworkProfile"));
	const FDigetherRunTuning PreviousSnapshot = Snapshot;
	TArray<FText> Problems;
	TestFalse(TEXT("범위를 벗어난 프로필 거절"),
		ADigetherGameplayGameMode::TryMakeRunSnapshot(InvalidProfile, Snapshot, Problems));
	TestTrue(TEXT("음수 시간의 거절 이유 제공"), HasFrameworkTuningProblem(Problems, TEXT("RunSeconds")));
	TestTrue(TEXT("100% 초과 복구율의 거절 이유 제공"), HasFrameworkTuningProblem(Problems, TEXT("CorpseRefundPercent")));
	TestTrue(TEXT("필드 검증 실패 시 기존 스냅샷과 출처 보존"),
		AreFrameworkTuningSnapshotsEqual(Snapshot, PreviousSnapshot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkTuningSnapshotTest,
	"DIGETHER.Game.Framework.Tuning.CaptureSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkTuningSnapshotTest::RunTest(const FString& Parameters)
{
	UDigetherTuningProfile* Profile = NewObject<UDigetherTuningProfile>();
	Profile->Values = MakeFrameworkTuningFixture();
	FDigetherRunTuning Snapshot;
	TArray<FText> Problems;
	if (!TestTrue(TEXT("유효한 프로필 수락"), ADigetherGameplayGameMode::TryMakeRunSnapshot(Profile, Snapshot, Problems)))
	{
		return false;
	}
	TestEqual(TEXT("유효한 프로필에는 오류 없음"), Problems.Num(), 0);
	TestEqual(TEXT("스냅샷에 출처 ID 기록"), Snapshot.SourceProfile, Profile->GetPrimaryAssetId());
	TestEqual(TEXT("스냅샷에 프로필 버전 기록"), Snapshot.ProfileVersion, Profile->Values.ProfileVersion);
	TestEqual(TEXT("목표액 복사"), Snapshot.InitialQuotaCredits, Profile->Values.InitialQuotaCredits);
	TestEqual(TEXT("제한 시간 복사"), Snapshot.RunSeconds, Profile->Values.RunSeconds);
	TestFalse(TEXT("원본 프로필에 스냅샷 출처를 쓰지 않음"), Profile->Values.SourceProfile.IsValid());

	const FDigetherRunTuning CapturedSnapshot = Snapshot;
	Profile->Values.ProfileVersion += 1;
	Profile->Values.InitialQuotaCredits += 50;
	Profile->Values.RunSeconds += 60.f;
	TestTrue(TEXT("원본 값 변경 후 기존 런 스냅샷 불변"),
		AreFrameworkTuningSnapshotsEqual(Snapshot, CapturedSnapshot));
	return true;
}

#endif
