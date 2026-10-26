#include "Misc/AutomationTest.h"
#include "Analytics/WorkbenchRoomConnectivity.h"

using namespace WorkbenchAnalytics;

namespace
{
	// 파싱에 실패하면 빈 보고서를 반환한다. 파싱 오류 자체는 ParserErrors 테스트에서 검증한다.
	FRoomManifestReport AnalyzeText(const FString& Text, const FRoomManifestSpec& Spec = FRoomManifestSpec())
	{
		FRoomManifest Manifest;
		TArray<FString> ParseErrors;
		if (!ParseRoomManifestText(Text, Manifest, ParseErrors))
		{
			return FRoomManifestReport();
		}
		return AnalyzeRoomManifest(Manifest, Spec);
	}

	FRoomManifestReport AnalyzeSample(ERoomManifestSample Sample)
	{
		return AnalyzeText(GetRoomManifestSampleText(Sample));
	}

	FString NormalWith(const TCHAR* From, const TCHAR* To)
	{
		return GetRoomManifestSampleText(ERoomManifestSample::Normal).Replace(From, To, ESearchCase::CaseSensitive);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchRoomConnectivity_NormalSample, "DevelopmentWorkbench.Analytics.T04.NormalSample", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchRoomConnectivity_NormalSample::RunTest(const FString& Parameters)
{
	const FRoomManifestReport Report = AnalyzeSample(ERoomManifestSample::Normal);
	TestEqual(TEXT("오류 0건"), Report.CountErrors(), 0);
	TestTrue(TEXT("그래프 검사 통과"), Report.PassesGraphChecks());
	TestEqual(TEXT("룸 수"), Report.RoomCount, 3);
	TestEqual(TEXT("문 수"), Report.DoorCount, 4);
	TestEqual(TEXT("양방향 연결 수"), Report.MutualLinkCount, 2);
	TestEqual(TEXT("도달 가능 룸 수"), Report.ReachableRooms.Num(), 3);
	TestEqual(TEXT("귀환 불가 룸 없음"), Report.RoomsWithoutReturn.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchRoomConnectivity_FailureSamples, "DevelopmentWorkbench.Analytics.T04.FailureSamples", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchRoomConnectivity_FailureSamples::RunTest(const FString& Parameters)
{
	const FRoomManifestReport OneSided = AnalyzeSample(ERoomManifestSample::OneSidedLink);
	TestTrue(TEXT("한쪽 연결"), OneSided.HasIssue(ERoomIssueType::OneSidedLink));
	TestTrue(TEXT("한쪽 연결의 귀환 불가"), OneSided.HasIssue(ERoomIssueType::NoReturnRoom));

	TestTrue(TEXT("방향 뒤집힘"), AnalyzeSample(ERoomManifestSample::FlippedDirection).HasIssue(ERoomIssueType::FlippedDirection));
	TestTrue(TEXT("스케일 불일치"), AnalyzeSample(ERoomManifestSample::ScaleMismatch).HasIssue(ERoomIssueType::LinkedScaleMismatch));
	TestTrue(TEXT("문 크기 불일치"), AnalyzeSample(ERoomManifestSample::SizeMismatch).HasIssue(ERoomIssueType::DoorSizeMismatch));
	TestTrue(TEXT("룸 겹침"), AnalyzeSample(ERoomManifestSample::Overlap).HasIssue(ERoomIssueType::RoomOverlap));
	TestTrue(TEXT("고립 룸"), AnalyzeSample(ERoomManifestSample::IsolatedRoom).HasIssue(ERoomIssueType::UnreachableRoom));

	const ERoomManifestSample FailureSamples[] = { ERoomManifestSample::OneSidedLink, ERoomManifestSample::FlippedDirection, ERoomManifestSample::ScaleMismatch,
		ERoomManifestSample::SizeMismatch, ERoomManifestSample::Overlap, ERoomManifestSample::IsolatedRoom };
	for (ERoomManifestSample Sample : FailureSamples)
	{
		TestFalse(TEXT("모든 실패 예제는 그래프 검사 실패"), AnalyzeSample(Sample).PassesGraphChecks());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchRoomConnectivity_ParserErrors, "DevelopmentWorkbench.Analytics.T04.ParserErrors", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchRoomConnectivity_ParserErrors::RunTest(const FString& Parameters)
{
	FRoomManifest Manifest;
	TArray<FString> Errors;
	TestTrue(TEXT("정상 예제 파싱"), ParseRoomManifestText(GetRoomManifestSampleText(ERoomManifestSample::Normal), Manifest, Errors));
	TestEqual(TEXT("파싱된 룸 수"), Manifest.Rooms.Num(), 3);
	TestEqual(TEXT("파싱된 문 수"), Manifest.Doors.Num(), 4);
	TestEqual(TEXT("입구"), Manifest.EntryRoomIds.Num(), 1);
	TestEqual(TEXT("출구"), Manifest.ExitRoomIds.Num(), 1);

	FRoomManifest Bad;
	TestFalse(TEXT("열 수 오류"), ParseRoomManifestText(TEXT("ROOM,A,800,800\n"), Bad, Errors));
	TestEqual(TEXT("열 수 오류 1건"), Errors.Num(), 1);
	TestFalse(TEXT("잘못된 방향"), ParseRoomManifestText(TEXT("DOOR,D1,A,+Z,0,200,250\n"), Bad, Errors));
	TestFalse(TEXT("숫자가 아닌 값"), ParseRoomManifestText(TEXT("ROOM,A,800,abc,300,1,0,0,0\n"), Bad, Errors));
	TestFalse(TEXT("무한대 값"), ParseRoomManifestText(TEXT("ROOM,A,800,inf,300,1,0,0,0\n"), Bad, Errors));
	TestFalse(TEXT("알 수 없는 행 종류"), ParseRoomManifestText(TEXT("WALL,A\n"), Bad, Errors));
	TestFalse(TEXT("ENTRY 열 수 오류"), ParseRoomManifestText(TEXT("ENTRY\n"), Bad, Errors));
	TestTrue(TEXT("주석과 빈 줄만 있으면 파싱 성공"), ParseRoomManifestText(TEXT("# 주석\n\n"), Bad, Errors));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchRoomConnectivity_SchemaChecks, "DevelopmentWorkbench.Analytics.T04.SchemaChecks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchRoomConnectivity_SchemaChecks::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("룸 ID 중복"), AnalyzeText(NormalWith(TEXT("ENTRY,Start\n"), TEXT("ROOM,Start,800,800,300,1,0,0,0\nENTRY,Start\n"))).HasIssue(ERoomIssueType::DuplicateRoomId));
	TestTrue(TEXT("문 ID 중복"), AnalyzeText(NormalWith(TEXT("ENTRY,Start\n"), TEXT("DOOR,D_S_E,Start,+Y,0,200,250\nENTRY,Start\n"))).HasIssue(ERoomIssueType::DuplicateDoorId));
	TestTrue(TEXT("존재하지 않는 룸의 문"), AnalyzeText(NormalWith(TEXT("ENTRY,Start\n"), TEXT("DOOR,D_X,Nowhere,+Y,0,200,250\nENTRY,Start\n"))).HasIssue(ERoomIssueType::DoorUnknownRoom));
	TestTrue(TEXT("룸 크기 0"), AnalyzeText(NormalWith(TEXT("ROOM,Hall,800,800,300"), TEXT("ROOM,Hall,0,800,300"))).HasIssue(ERoomIssueType::InvalidRoomSize));
	TestTrue(TEXT("스케일 0"), AnalyzeText(NormalWith(TEXT("ROOM,Hall,800,800,300,1,"), TEXT("ROOM,Hall,800,800,300,0,"))).HasIssue(ERoomIssueType::InvalidScale));
	TestTrue(TEXT("90도 배수가 아닌 Yaw"), AnalyzeText(NormalWith(TEXT("ROOM,Hall,800,800,300,1,800,0,0"), TEXT("ROOM,Hall,800,800,300,1,800,0,45"))).HasIssue(ERoomIssueType::InvalidYaw));
	TestTrue(TEXT("문 너비 0"), AnalyzeText(NormalWith(TEXT("DOOR,D_S_E,Start,+X,0,200,250"), TEXT("DOOR,D_S_E,Start,+X,0,0,250"))).HasIssue(ERoomIssueType::InvalidDoorSize));
	TestTrue(TEXT("벽을 벗어난 문"), AnalyzeText(NormalWith(TEXT("DOOR,D_S_E,Start,+X,0,200,250"), TEXT("DOOR,D_S_E,Start,+X,500,200,250"))).HasIssue(ERoomIssueType::DoorOutsideWall));
	TestTrue(TEXT("룸보다 높은 문"), AnalyzeText(NormalWith(TEXT("DOOR,D_S_E,Start,+X,0,200,250"), TEXT("DOOR,D_S_E,Start,+X,0,200,400"))).HasIssue(ERoomIssueType::DoorTooTall));
	TestTrue(TEXT("존재하지 않는 연결 대상"), AnalyzeText(NormalWith(TEXT("DOOR,D_S_E,Start,+X,0,200,250,D_H_W"), TEXT("DOOR,D_S_E,Start,+X,0,200,250,D_NONE"))).HasIssue(ERoomIssueType::LinkTargetMissing));
	TestTrue(TEXT("자기 연결"), AnalyzeText(NormalWith(TEXT("DOOR,D_S_E,Start,+X,0,200,250,D_H_W"), TEXT("DOOR,D_S_E,Start,+X,0,200,250,D_S_E"))).HasIssue(ERoomIssueType::LinkToSelf));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchRoomConnectivity_ToleranceAndSpec, "DevelopmentWorkbench.Analytics.T04.ToleranceAndSpec", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchRoomConnectivity_ToleranceAndSpec::RunTest(const FString& Parameters)
{
	const TCHAR* From = TEXT("DOOR,D_H_W,Hall,-X,0,200,250,D_S_E");
	const FRoomManifestSpec DefaultSpec;

	TestFalse(TEXT("허용 오차 이내(0.5cm)는 위치 오류 아님"), AnalyzeText(NormalWith(From, TEXT("DOOR,D_H_W,Hall,-X,0.5,200,250,D_S_E")), DefaultSpec).HasIssue(ERoomIssueType::PositionMismatch));
	TestTrue(TEXT("허용 오차 초과(5cm)는 위치 오류"), AnalyzeText(NormalWith(From, TEXT("DOOR,D_H_W,Hall,-X,5,200,250,D_S_E")), DefaultSpec).HasIssue(ERoomIssueType::PositionMismatch));

	FRoomManifestSpec Loose;
	Loose.PositionToleranceCm = 10.0;
	TestFalse(TEXT("허용 오차를 키우면 5cm는 통과"), AnalyzeText(NormalWith(From, TEXT("DOOR,D_H_W,Hall,-X,5,200,250,D_S_E")), Loose).HasIssue(ERoomIssueType::PositionMismatch));

	FRoomManifestSpec ScaleSpec;
	ScaleSpec.ExpectedScale = 2.0;
	TestTrue(TEXT("기대 스케일 불일치"), AnalyzeText(GetRoomManifestSampleText(ERoomManifestSample::Normal), ScaleSpec).HasIssue(ERoomIssueType::ScaleSpecMismatch));

	FRoomManifestSpec DoorSpec;
	DoorSpec.ExpectedDoorWidth = 300.0;
	TestTrue(TEXT("기대 문 너비 불일치"), AnalyzeText(GetRoomManifestSampleText(ERoomManifestSample::Normal), DoorSpec).HasIssue(ERoomIssueType::DoorSizeSpecMismatch));

	FRoomManifestSpec MatchingSpec;
	MatchingSpec.ExpectedScale = 1.0;
	MatchingSpec.ExpectedDoorWidth = 200.0;
	MatchingSpec.ExpectedDoorHeight = 250.0;
	TestEqual(TEXT("규격과 일치하면 오류 0건"), AnalyzeText(GetRoomManifestSampleText(ERoomManifestSample::Normal), MatchingSpec).CountErrors(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchRoomConnectivity_GraphReachability, "DevelopmentWorkbench.Analytics.T04.GraphReachability", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorkbenchRoomConnectivity_GraphReachability::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("입구 없음"), AnalyzeText(NormalWith(TEXT("ENTRY,Start\n"), TEXT(""))).HasIssue(ERoomIssueType::NoEntry));
	TestTrue(TEXT("존재하지 않는 입구"), AnalyzeText(NormalWith(TEXT("ENTRY,Start\n"), TEXT("ENTRY,Ghost\n"))).HasIssue(ERoomIssueType::EntryUnknownRoom));
	TestTrue(TEXT("존재하지 않는 출구"), AnalyzeText(NormalWith(TEXT("EXIT,Vault\n"), TEXT("EXIT,Ghost\n"))).HasIssue(ERoomIssueType::ExitUnknownRoom));

	const FRoomManifestReport IsolatedExit = AnalyzeText(GetRoomManifestSampleText(ERoomManifestSample::IsolatedRoom) + TEXT("EXIT,Annex\n"));
	TestTrue(TEXT("도달할 수 없는 출구"), IsolatedExit.HasIssue(ERoomIssueType::UnreachableExit));
	TestFalse(TEXT("고립 룸은 도달 목록에 없음"), IsolatedExit.ReachableRooms.Contains(TEXT("Annex")));

	const FRoomManifestReport OneSided = AnalyzeSample(ERoomManifestSample::OneSidedLink);
	TestTrue(TEXT("귀환 불가 룸 목록에 Vault"), OneSided.RoomsWithoutReturn.Contains(TEXT("Vault")));
	TestTrue(TEXT("귀환 불가여도 도달은 가능"), OneSided.ReachableRooms.Contains(TEXT("Vault")));
	return true;
}
