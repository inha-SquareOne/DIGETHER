#include "Misc/AutomationTest.h"
#include "Tools/WorkbenchToolsAlgo.h"

// 모든 입력은 동작 확인용 합성 예시이며 게임 규칙·수치·스키마 결정이 아니다.
using namespace WorkbenchToolsAlgo;

namespace
{
	FRenameCandidate MakeCandidate(const TCHAR* Path, const TCHAR* Name, bool bReadOnly = false, bool bOccupyOnly = false)
	{
		FRenameCandidate Candidate;
		Candidate.PackagePath = Path;
		Candidate.AssetName = Name;
		Candidate.bReadOnly = bReadOnly;
		Candidate.bOccupyOnly = bOccupyOnly;
		return Candidate;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FToolsPrefixValidationTest, "DevelopmentWorkbench.Tools.T09.PrefixValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FToolsPrefixValidationTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("영문+밑줄 허용"), IsValidAssetNamePrefix(TEXT("T_")));
	TestFalse(TEXT("빈 접두사 거부"), IsValidAssetNamePrefix(TEXT("")));
	TestFalse(TEXT("공백 거부"), IsValidAssetNamePrefix(TEXT("T ")));
	TestFalse(TEXT("경로 문자 거부"), IsValidAssetNamePrefix(TEXT("A/B")));
	TestFalse(TEXT("33자 거부"), IsValidAssetNamePrefix(FString::ChrN(33, TEXT('A'))));
	TestTrue(TEXT("32자 허용"), IsValidAssetNamePrefix(FString::ChrN(32, TEXT('A'))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FToolsRenamePlanTest, "DevelopmentWorkbench.Tools.T09.RenamePlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FToolsRenamePlanTest::RunTest(const FString& Parameters)
{
	const TArray<FRenameCandidate> Candidates = {
		MakeCandidate(TEXT("/Game/Items"), TEXT("Sword")),
		MakeCandidate(TEXT("/Game/Items"), TEXT("T_Shield")),
		MakeCandidate(TEXT("/Game/Items"), TEXT("Axe"), true),
		MakeCandidate(TEXT("/Game/Items"), TEXT("T_Bow"), false, true),
		MakeCandidate(TEXT("/Game/Items"), TEXT("Bow")),
		MakeCandidate(TEXT("/Game/Other"), TEXT("Sword")),
	};
	const TArray<FRenamePlanEntry> Plan = BuildRenamePlan(Candidates, TEXT("T_"));

	auto Find = [&Plan](const TCHAR* Path, const TCHAR* Name) -> const FRenamePlanEntry*
	{
		return Plan.FindByPredicate([&](const FRenamePlanEntry& E) { return E.PackagePath == Path && E.OldName == Name; });
	};

	const FRenamePlanEntry* Sword = Find(TEXT("/Game/Items"), TEXT("Sword"));
	const FRenamePlanEntry* Shield = Find(TEXT("/Game/Items"), TEXT("T_Shield"));
	const FRenamePlanEntry* Axe = Find(TEXT("/Game/Items"), TEXT("Axe"));
	const FRenamePlanEntry* Bow = Find(TEXT("/Game/Items"), TEXT("Bow"));
	const FRenamePlanEntry* OtherSword = Find(TEXT("/Game/Other"), TEXT("Sword"));

	if (!TestNotNull(TEXT("Sword 항목"), Sword) || !TestNotNull(TEXT("Shield 항목"), Shield) || !TestNotNull(TEXT("Axe 항목"), Axe)
		|| !TestNotNull(TEXT("Bow 항목"), Bow) || !TestNotNull(TEXT("다른 폴더 Sword 항목"), OtherSword))
	{
		return false;
	}

	TestEqual(TEXT("정상 변경"), Sword->Status, ERenamePlanStatus::Ok);
	TestEqual(TEXT("새 이름"), Sword->NewName, FString(TEXT("T_Sword")));
	TestEqual(TEXT("이미 접두사"), Shield->Status, ERenamePlanStatus::NoChange);
	TestEqual(TEXT("읽기 전용"), Axe->Status, ERenamePlanStatus::ReadOnly);
	TestEqual(TEXT("점유 이름과 충돌"), Bow->Status, ERenamePlanStatus::Collision);
	TestEqual(TEXT("다른 폴더는 충돌 아님"), OtherSword->Status, ERenamePlanStatus::Ok);
	TestNull(TEXT("점유 전용 에셋은 계획 항목이 없다"), Find(TEXT("/Game/Items"), TEXT("T_Bow")));

	TestFalse(TEXT("충돌·읽기 전용이 있으면 적용 불가"), IsRenamePlanApplyable(Plan));

	TArray<FRenamePlanEntry> OkOnly;
	OkOnly.Add(*Sword);
	TestTrue(TEXT("Ok만 있으면 적용 가능"), IsRenamePlanApplyable(OkOnly));
	TestFalse(TEXT("빈 계획은 적용 불가"), IsRenamePlanApplyable(TArray<FRenamePlanEntry>()));

	const FString Recovery = FormatRenameRecoveryList(OkOnly);
	TestTrue(TEXT("복구 목록에 현재 이름 포함"), Recovery.Contains(TEXT("Sword")));
	TestTrue(TEXT("복구 목록은 탭 구분"), Recovery.Contains(TEXT("\t")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FToolsRenameCaseCollisionTest, "DevelopmentWorkbench.Tools.T09.CaseInsensitiveCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FToolsRenameCaseCollisionTest::RunTest(const FString& Parameters)
{
	const TArray<FRenameCandidate> Candidates = {
		MakeCandidate(TEXT("/Game/Items"), TEXT("Rock")),
		MakeCandidate(TEXT("/Game/Items"), TEXT("t_rock"), false, true),
	};
	const TArray<FRenamePlanEntry> Plan = BuildRenamePlan(Candidates, TEXT("T_"));
	if (!TestEqual(TEXT("계획 항목 수"), Plan.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("대소문자 무시 충돌"), Plan[0].Status, ERenamePlanStatus::Collision);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FToolsPlacementPreviewTest, "DevelopmentWorkbench.Tools.T11.PreviewDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FToolsPlacementPreviewTest::RunTest(const FString& Parameters)
{
	FPlacementParams Params;
	Params.Seed = FTransform(FRotator::ZeroRotator, FVector(10.f, 20.f, 30.f));
	Params.Count = 4;
	Params.Spacing = FVector(100.f, 0.f, 0.f);
	Params.RotationStep = FRotator(0.f, 15.f, 0.f);
	Params.RandomSeed = 7;
	Params.PositionJitter = 5.f;

	const TArray<FTransform> A = BuildPlacementPreview(Params);
	const TArray<FTransform> B = BuildPlacementPreview(Params);
	if (!TestEqual(TEXT("개수"), A.Num(), 4) || !TestEqual(TEXT("같은 시드 같은 개수"), B.Num(), 4))
	{
		return false;
	}
	TestTrue(TEXT("0번은 시드 그대로"), A[0].Equals(Params.Seed, 0.001f));
	for (int32 Index = 0; Index < A.Num(); ++Index)
	{
		TestTrue(*FString::Printf(TEXT("같은 시드는 같은 결과 #%d"), Index), A[Index].Equals(B[Index], 0.001f));
	}

	Params.PositionJitter = 0.f;
	const TArray<FTransform> NoJitter = BuildPlacementPreview(Params);
	if (TestEqual(TEXT("지터 없음 개수"), NoJitter.Num(), 4))
	{
		TestEqual(TEXT("간격 적용"), NoJitter[1].GetLocation().X - NoJitter[0].GetLocation().X, 100.0, 0.01);
	}

	Params.Count = 0;
	TestEqual(TEXT("Count 0은 빈 배열"), BuildPlacementPreview(Params).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FToolsPlacementCheckTest, "DevelopmentWorkbench.Tools.T11.Checks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FToolsPlacementCheckTest::RunTest(const FString& Parameters)
{
	const FTransform NegativeX(FRotator::ZeroRotator, FVector::ZeroVector, FVector(-1.f, 1.f, 1.f));
	TestFalse(TEXT("음수 스케일 없음"), HasNegativeScale({FTransform::Identity}));
	TestTrue(TEXT("음수 스케일 감지"), HasNegativeScale({NegativeX}));

	const TArray<FTransform> Preview = {FTransform::Identity, FTransform::Identity};

	TestTrue(TEXT("선택 없음은 차단"), HasBlockingIssue(CheckPlacement({}, {}, false)));

	FPlacementActorState Seed;
	FPlacementActorState Target;
	TestFalse(TEXT("정상 입력은 차단 없음"), HasBlockingIssue(CheckPlacement({Seed, Target}, Preview, false)));
	TestTrue(TEXT("PIE는 차단"), HasBlockingIssue(CheckPlacement({Seed, Target}, Preview, true)));

	FPlacementActorState LockedSeed;
	LockedSeed.bLocked = true;
	TestFalse(TEXT("시드의 잠금은 검사하지 않는다"), HasBlockingIssue(CheckPlacement({LockedSeed, Target}, Preview, false)));

	FPlacementActorState Locked;
	Locked.bLocked = true;
	TestTrue(TEXT("대상 잠금은 차단"), HasBlockingIssue(CheckPlacement({Seed, Locked}, Preview, false)));

	FPlacementActorState Attached;
	Attached.bAttachedToParent = true;
	TestTrue(TEXT("부착 대상은 차단"), HasBlockingIssue(CheckPlacement({Seed, Attached}, Preview, false)));

	FPlacementActorState External;
	External.bExternalPackage = true;
	const TArray<FPlacementIssue> ExternalIssues = CheckPlacement({Seed, External}, Preview, false);
	TestFalse(TEXT("외부 액터는 경고만"), HasBlockingIssue(ExternalIssues));
	TestTrue(TEXT("외부 액터 경고 존재"), ExternalIssues.ContainsByPredicate([](const FPlacementIssue& I) { return I.Kind == EPlacementIssueKind::ExternalActor; }));

	const TArray<FTransform> NegativePreview = {FTransform::Identity, NegativeX};
	TestTrue(TEXT("음수 스케일 미리보기는 차단"), HasBlockingIssue(CheckPlacement({Seed, Target}, NegativePreview, false)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FToolsLaunchPlanTest, "DevelopmentWorkbench.Tools.T06.LaunchPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FToolsLaunchPlanTest::RunTest(const FString& Parameters)
{
	FMultiRunRequest Request;
	Request.MapPackagePath = TEXT("/Game/Maps/SyntheticMap");
	Request.PlayerCount = 3;
	Request.Port = 7800;
	const FGuid RunId = FGuid::NewGuid();
	const FString LogDir = TEXT("C:/Synthetic/Logs");

	Request.bDedicated = false;
	const TArray<FLaunchRoleSpec> Listen = BuildLaunchPlan(Request, RunId, LogDir);
	if (TestEqual(TEXT("리슨: 호스트 1 + 클라 2"), Listen.Num(), 3))
	{
		TestTrue(TEXT("호스트가 listen 인자를 가진다"), Listen[0].CommandLine.Contains(TEXT("listen")));
		TestFalse(TEXT("호스트는 숨기지 않는다"), Listen[0].bHidden);
		TestTrue(TEXT("클라는 서버 주소로 접속"), Listen[1].CommandLine.Contains(TEXT("127.0.0.1")) && Listen[1].CommandLine.Contains(TEXT("7800")));
		TestTrue(TEXT("역할별 로그 경로"), Listen[1].LogPath.StartsWith(LogDir) && Listen[1].LogPath != Listen[2].LogPath);
	}

	Request.bDedicated = true;
	const TArray<FLaunchRoleSpec> Dedicated = BuildLaunchPlan(Request, RunId, LogDir);
	if (TestEqual(TEXT("데디케이티드: 서버 1 + 클라 3"), Dedicated.Num(), 4))
	{
		TestTrue(TEXT("서버가 첫 역할이며 숨김"), Dedicated[0].bHidden);
	}

	Request.bDedicated = false;
	Request.PlayerCount = 9;
	TestEqual(TEXT("인원은 4로 클램프"), BuildLaunchPlan(Request, RunId, LogDir).Num(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FToolsMultiRunValidationTest, "DevelopmentWorkbench.Tools.T06.RequestValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FToolsMultiRunValidationTest::RunTest(const FString& Parameters)
{
	FMultiRunRequest Valid;
	Valid.MapPackagePath = TEXT("/Game/Maps/SyntheticMap");
	TestEqual(TEXT("정상 요청은 사유 없음"), ValidateMultiRunRequest(Valid).Num(), 0);

	FMultiRunRequest NoMap = Valid;
	NoMap.MapPackagePath.Reset();
	TestTrue(TEXT("맵 없음 거부"), ValidateMultiRunRequest(NoMap).Num() > 0);

	FMultiRunRequest LowPort = Valid;
	LowPort.Port = 80;
	TestTrue(TEXT("낮은 포트 거부"), ValidateMultiRunRequest(LowPort).Num() > 0);

	FMultiRunRequest BadLoss = Valid;
	BadLoss.PacketLossPercent = 150.f;
	TestTrue(TEXT("패킷 손실 범위 초과 거부"), ValidateMultiRunRequest(BadLoss).Num() > 0);

	FMultiRunRequest BadLatency = Valid;
	BadLatency.MinLatencyMs = 200.f;
	BadLatency.MaxLatencyMs = 100.f;
	TestTrue(TEXT("지연 최소>최대 거부"), ValidateMultiRunRequest(BadLatency).Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FToolsInspectorProfileTest, "DevelopmentWorkbench.Tools.T07.ProfileParsing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FToolsInspectorProfileTest::RunTest(const FString& Parameters)
{
	FInspectorProfile Profile;
	TArray<FString> Errors;

	const FString Good = TEXT("{\"targetClass\":\"/Game/Synthetic/BP_Synthetic.BP_Synthetic_C\",\"rules\":[{\"property\":\"Amount\",\"group\":\"Extra\",\"visibleWhen\":\"bEnabled\",\"negate\":false}]}");
	TestTrue(TEXT("정상 JSON"), ParseInspectorProfile(Good, Profile, Errors));
	TestEqual(TEXT("오류 없음"), Errors.Num(), 0);
	if (TestEqual(TEXT("규칙 1개"), Profile.Rules.Num(), 1))
	{
		TestEqual(TEXT("프로퍼티"), Profile.Rules[0].Property, FString(TEXT("Amount")));
		TestEqual(TEXT("조건"), Profile.Rules[0].ConditionProperty, FString(TEXT("bEnabled")));
	}

	Errors.Reset();
	TestFalse(TEXT("깨진 JSON 거부"), ParseInspectorProfile(TEXT("{not json"), Profile, Errors));
	TestTrue(TEXT("깨진 JSON 사유"), Errors.Num() > 0);
	TestEqual(TEXT("실패 시 프로필 비움"), Profile.Rules.Num(), 0);

	Errors.Reset();
	const FString SelfRef = TEXT("{\"rules\":[{\"property\":\"A\",\"visibleWhen\":\"A\"}]}");
	TestFalse(TEXT("자기 참조 거부"), ParseInspectorProfile(SelfRef, Profile, Errors));

	Errors.Reset();
	const FString Cycle = TEXT("{\"rules\":[{\"property\":\"A\",\"visibleWhen\":\"B\"},{\"property\":\"B\",\"visibleWhen\":\"A\"}]}");
	TestFalse(TEXT("순환 조건 거부"), ParseInspectorProfile(Cycle, Profile, Errors));

	Errors.Reset();
	const FString Duplicate = TEXT("{\"rules\":[{\"property\":\"A\"},{\"property\":\"A\"}]}");
	TestFalse(TEXT("중복 프로퍼티 거부"), ParseInspectorProfile(Duplicate, Profile, Errors));

	Errors.Reset();
	const FString BadIdentifier = TEXT("{\"rules\":[{\"property\":\"A B\"}]}");
	TestFalse(TEXT("식별자 형식 위반 거부"), ParseInspectorProfile(BadIdentifier, Profile, Errors));

	FInspectorProfile Targeted;
	Targeted.TargetClass = TEXT("/Game/Synthetic/BP_Synthetic.BP_Synthetic_C");
	TestTrue(TEXT("클래스 계층 일치"), ProfileMatchesClassChain(Targeted, {TEXT("/Game/Synthetic/BP_Synthetic.BP_Synthetic_C"), TEXT("/Script/Engine.Actor")}));
	TestFalse(TEXT("클래스 계층 불일치"), ProfileMatchesClassChain(Targeted, {TEXT("/Script/Engine.Actor")}));
	return true;
}
