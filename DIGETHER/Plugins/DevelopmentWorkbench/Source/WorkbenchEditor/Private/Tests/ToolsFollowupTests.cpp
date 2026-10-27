#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Tools/WorkbenchToolsAlgo.h"

#include <limits>

// 후속 보완에서 실제로 발견한 결함의 회귀 테스트. 모든 값은 합성 예시이며 게임 규칙 결정이 아니다.
using namespace WorkbenchToolsAlgo;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FToolsFollowupMultiRunFiniteTest, "DevelopmentWorkbench.Tools.T06.FiniteInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FToolsFollowupMultiRunFiniteTest::RunTest(const FString& Parameters)
{
	FMultiRunRequest Valid;
	Valid.MapPackagePath = TEXT("/Game/Maps/SyntheticMap");

	const float NaN = std::numeric_limits<float>::quiet_NaN();
	const float Inf = std::numeric_limits<float>::infinity();

	FMultiRunRequest Request = Valid;
	Request.MinLatencyMs = NaN;
	TestTrue(TEXT("NaN 최소 지연 거부"), ValidateMultiRunRequest(Request).Num() > 0);

	Request = Valid;
	Request.MaxLatencyMs = Inf;
	TestTrue(TEXT("Inf 최대 지연 거부"), ValidateMultiRunRequest(Request).Num() > 0);

	Request = Valid;
	Request.PacketLossPercent = NaN;
	TestTrue(TEXT("NaN 손실 거부"), ValidateMultiRunRequest(Request).Num() > 0);

	Request = Valid;
	Request.PacketLossPercent = Inf;
	TestTrue(TEXT("Inf 손실 거부"), ValidateMultiRunRequest(Request).Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FToolsFollowupRunPolicyTest, "DevelopmentWorkbench.Tools.T06.RunPolicyAndResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FToolsFollowupRunPolicyTest::RunTest(const FString& Parameters)
{
	TestNotEqual(TEXT("종료 상태는 시작 대기가 아니다"), RoleRunStateLabel(ERoleRunState::ExitedOk), RoleRunStateLabel(ERoleRunState::NotLaunched));
	TestNotEqual(TEXT("패널 종료 상태는 시작 대기가 아니다"), RoleRunStateLabel(ERoleRunState::KilledByPanel), RoleRunStateLabel(ERoleRunState::NotLaunched));

	FRoleRunRecord Running;
	Running.RoleLabel = TEXT("Host");
	Running.State = ERoleRunState::Running;
	FRoleRunRecord ExitedOk = Running;
	ExitedOk.RoleLabel = TEXT("Client1");
	ExitedOk.State = ERoleRunState::ExitedOk;
	FRoleRunRecord Failed = Running;
	Failed.RoleLabel = TEXT("Client2");
	Failed.State = ERoleRunState::ExitedFailed;

	TestFalse(TEXT("정상 실행 중에는 회수하지 않는다"), ShouldAbortRun({Running, Running}, true));
	TestTrue(TEXT("시작 단계의 조기 종료(코드 0 포함)는 전체 회수"), ShouldAbortRun({Running, ExitedOk}, true));
	TestFalse(TEXT("시작 후 코드 0 종료(창 닫음)는 다른 역할 유지"), ShouldAbortRun({Running, ExitedOk}, false));
	TestTrue(TEXT("시작 후 비정상 종료는 전체 회수"), ShouldAbortRun({Running, Failed}, false));

	FRoleRunRecord LaunchFailed = Running;
	LaunchFailed.State = ERoleRunState::LaunchFailed;
	TestTrue(TEXT("실행 실패는 전체 회수"), ShouldAbortRun({Running, LaunchFailed}, false));

	FRunSummary Summary;
	Summary.RunId = TEXT("00000000000000000000000000000000");
	Summary.MapPackagePath = TEXT("/Game/Maps/SyntheticMap");
	Summary.Port = 7777;
	Summary.FinalStatus = TEXT("조기 실패로 회수");
	Running.ProcessId = 1234;
	Running.StartedAtUtc = TEXT("2000-01-01T00:00:00Z");
	Summary.Roles = {Running, Failed};

	const FString Json = BuildRunResultJson(Summary);
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (TestTrue(TEXT("result.json 파싱"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()))
	{
		bool bVerified = true;
		TestTrue(TEXT("connectionVerified 필드 존재"), Root->TryGetBoolField(TEXT("connectionVerified"), bVerified));
		TestFalse(TEXT("접속 성공을 기록하지 않는다"), bVerified);
		FString Verdict;
		Root->TryGetStringField(TEXT("m2Verdict"), Verdict);
		TestEqual(TEXT("M2는 자동 통과하지 않는다"), Verdict, FString(TEXT("미판정")));
		const TArray<TSharedPtr<FJsonValue>>* Roles = nullptr;
		if (TestTrue(TEXT("roles 배열"), Root->TryGetArrayField(TEXT("roles"), Roles)))
		{
			TestEqual(TEXT("역할 수"), Roles->Num(), 2);
			double Pid = 0;
			const TSharedPtr<FJsonObject>* First = nullptr;
			if ((*Roles)[0]->TryGetObject(First) && First)
			{
				(*First)->TryGetNumberField(TEXT("pid"), Pid);
			}
			TestEqual(TEXT("PID 보존"), static_cast<int32>(Pid), 1234);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FToolsFollowupPlacementTest, "DevelopmentWorkbench.Tools.T11.RangeAndFinite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FToolsFollowupPlacementTest::RunTest(const FString& Parameters)
{
	FPlacementParams Params;
	Params.Count = 3;
	TestEqual(TEXT("정상 입력은 사유 없음"), ValidatePlacementParams(Params).Num(), 0);

	Params.Count = MaxPlacementCount + 1;
	TestTrue(TEXT("Count 상한 초과 거부"), ValidatePlacementParams(Params).Num() > 0);
	TestEqual(TEXT("상한 초과는 미리보기를 만들지 않는다"), BuildPlacementPreview(Params).Num(), 0);

	Params.Count = 3;
	Params.Spacing = FVector(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0);
	TestTrue(TEXT("NaN 간격 거부"), ValidatePlacementParams(Params).Num() > 0);
	TestEqual(TEXT("NaN 입력은 미리보기를 만들지 않는다"), BuildPlacementPreview(Params).Num(), 0);

	Params.Spacing = FVector(100.0, 0.0, 0.0);
	Params.PositionJitter = std::numeric_limits<float>::infinity();
	TestTrue(TEXT("Inf 지터 거부"), ValidatePlacementParams(Params).Num() > 0);

	Params.PositionJitter = 0.f;
	Params.Seed = FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.0, 0.0, 1.0));
	TestTrue(TEXT("0 스케일 시드 거부"), ValidatePlacementParams(Params).Num() > 0);

	Params.Seed = FTransform::Identity;
	Params.UniformScaleStep = -0.5f;
	TestTrue(TEXT("스케일이 0 이하가 되는 단위 거부"), ValidatePlacementParams(Params).Num() > 0);

	const FTransform ZeroScale(FRotator::ZeroRotator, FVector::ZeroVector, FVector(0.0, 1.0, 1.0));
	TestTrue(TEXT("0 스케일은 유효하지 않음"), HasInvalidScale({ZeroScale}));
	TestTrue(TEXT("0 스케일 미리보기는 차단"), HasBlockingIssue(CheckPlacement({FPlacementActorState(), FPlacementActorState()}, {FTransform::Identity, ZeroScale}, false)));

	const FTransform Moved(FRotator::ZeroRotator, FVector(5.0, 0.0, 0.0));
	TestTrue(TEXT("같은 변환은 변경 없음"), IsTransformUnchanged(FTransform::Identity, FTransform::Identity));
	TestFalse(TEXT("이동한 변환은 변경됨"), IsTransformUnchanged(FTransform::Identity, Moved));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FToolsFollowupInspectorPathTest, "DevelopmentWorkbench.Tools.T07.FullClassPathRequired",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FToolsFollowupInspectorPathTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("전체 경로"), IsFullClassPath(TEXT("/Game/Synthetic/BP_Synthetic.BP_Synthetic_C")));
	TestTrue(TEXT("네이티브 경로"), IsFullClassPath(TEXT("/Script/Engine.Actor")));
	TestFalse(TEXT("짧은 이름"), IsFullClassPath(TEXT("BP_Synthetic_C")));
	TestFalse(TEXT("빈 값"), IsFullClassPath(FString()));
	TestFalse(TEXT("와일드카드"), IsFullClassPath(TEXT("*")));
	TestFalse(TEXT("점 없음"), IsFullClassPath(TEXT("/Game/Synthetic/BP_Synthetic")));
	TestFalse(TEXT("점 두 개"), IsFullClassPath(TEXT("/Game/A.B.C")));

	FInspectorProfile Profile;
	TArray<FString> Errors;
	TestFalse(TEXT("targetClass 누락 거부"), ParseInspectorProfile(TEXT("{\"rules\":[{\"property\":\"A\"}]}"), Profile, Errors));
	TestFalse(TEXT("짧은 targetClass 거부"), ParseInspectorProfile(TEXT("{\"targetClass\":\"BP_X_C\",\"rules\":[{\"property\":\"A\"}]}"), Profile, Errors));

	FInspectorProfile Empty;
	TestFalse(TEXT("빈 대상은 어떤 클래스에도 일치하지 않는다"), ProfileMatchesClassChain(Empty, {TEXT("/Script/Engine.Actor")}));
	FInspectorProfile ShortName;
	ShortName.TargetClass = TEXT("Actor");
	TestFalse(TEXT("짧은 이름은 일치하지 않는다"), ProfileMatchesClassChain(ShortName, {TEXT("Actor")}));
	return true;
}
