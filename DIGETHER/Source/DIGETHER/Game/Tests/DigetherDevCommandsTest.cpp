#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Dev/DigetherCheatManager.h"
#include "Game/Dev/DigetherDevCommands.h"
#include "Game/Dev/DigetherDevLog.h"
#include "Misc/AutomationTest.h"

// 치트 명령 틀: 예약 목록, 미연결 응답, 처리기 등록·해제, 예약 외 이름 거절.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherDevCommandsTest,
	"DIGETHER.Game.Dev.Commands", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherDevCommandsTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Shipping이 아닌 빌드에서 사용 가능"), DigetherDev::IsAvailable());

	TSet<FName> Seen;
	for (const DigetherDev::FCommandInfo& Info : DigetherDev::GetPlannedCommands())
	{
		TestTrue(FString::Printf(TEXT("%s 접두사"), *Info.Name.ToString()), Info.Name.ToString().StartsWith(TEXT("Digether")));
		TestTrue(FString::Printf(TEXT("%s 중복 없음"), *Info.Name.ToString()), !Seen.Contains(Info.Name));
		Seen.Add(Info.Name);
		TestTrue(FString::Printf(TEXT("%s 담당 패키지 표기"), *Info.Name.ToString()), FCString::Strlen(Info.OwnerPackage) > 0);
		// 예약 명령은 각 Exec 함수가 있어야 한다.
		TestNotNull(FString::Printf(TEXT("%s Exec 함수"), *Info.Name.ToString()),
			UDigetherCheatManager::StaticClass()->FindFunctionByName(Info.Name));
	}
	TestEqual(TEXT("예약 명령 개수"), Seen.Num(), 7);

	const FName Advance(TEXT("DigetherAdvanceTime"));
	TestFalse(TEXT("처음엔 연결 안 됨"), DigetherDev::IsConnected(Advance));
	TestEqual(TEXT("미연결 응답"), DigetherDev::Execute(Advance, {}), DigetherDev::MakeNotConnectedMessage(Advance));

	TestFalse(TEXT("예약 외 이름은 등록 거절"), DigetherDev::Register(TEXT("DigetherUnknown"), [](const TArray<FString>&) { return FString(); }));
	TestFalse(TEXT("빈 처리기는 등록 거절"), DigetherDev::Register(Advance, DigetherDev::FCommandHandler()));
	TestTrue(TEXT("예약 외 명령 응답"), DigetherDev::Execute(TEXT("DigetherUnknown"), {}).Contains(TEXT("알 수 없는 명령")));

	TestTrue(TEXT("등록"), DigetherDev::Register(Advance, [](const TArray<FString>& Args) { return FString::Printf(TEXT("ok:%d"), Args.Num()); }));
	TestTrue(TEXT("등록 뒤 연결됨"), DigetherDev::IsConnected(Advance));
	TestEqual(TEXT("처리기 응답과 인자 전달"), DigetherDev::Execute(Advance, { TEXT("5") }), FString(TEXT("ok:1")));

	DigetherDev::Unregister(Advance);
	TestFalse(TEXT("해제 뒤 연결 안 됨"), DigetherDev::IsConnected(Advance));
	TestEqual(TEXT("해제 뒤 다시 미연결 응답"), DigetherDev::Execute(Advance, {}), DigetherDev::MakeNotConnectedMessage(Advance));
	return true;
}

// 개발 로그 카테고리 이름이 문서(LOG_CATEGORIES.md)와 같은지 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherDevLogTest,
	"DIGETHER.Game.Dev.LogCategory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherDevLogTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("개발 로그 카테고리 이름"), LogDigetherDev.GetCategoryName().ToString(), FString(TEXT("LogDigetherDev")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
