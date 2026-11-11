#if WITH_DEV_AUTOMATION_TESTS

#include "DigetherDevRegistry.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"

// 등록부 계약: 이름·설명 필수, 콘솔 명령 자동 등록, 기존 명령 이름 유지, 인자 검사, 실행 조건 거절
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherDevRegistryContractTest,
	"DIGETHER.Dev.Registry.Contract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherDevRegistryContractTest::RunTest(const FString& Parameters)
{
	TSet<FName> Seen;
	for (const DigetherDev::FCommandDesc* Desc : DigetherDev::GetCommands())
	{
		const FString Name = Desc->Name.ToString();
		TestTrue(FString::Printf(TEXT("%s 접두사"), *Name), Name.StartsWith(TEXT("Digether")));
		TestFalse(FString::Printf(TEXT("%s 중복 없음"), *Name), Seen.Contains(Desc->Name));
		Seen.Add(Desc->Name);
		TestFalse(FString::Printf(TEXT("%s 분류"), *Name), Desc->Category.IsEmpty());
		TestFalse(FString::Printf(TEXT("%s 설명"), *Name), Desc->Summary.IsEmpty());
		TestNotNull(FString::Printf(TEXT("%s 콘솔 명령 등록"), *Name), IConsoleManager::Get().FindConsoleObject(*Name));
	}

	// 기존 자동화 인자(-DigetherAutoCmds)·다인 검증이 쓰는 이름 유지
	const TCHAR* LegacyNames[] = {
		TEXT("DigetherCheatList"), TEXT("DigetherAdvanceTime"), TEXT("DigetherSpawnItem"), TEXT("DigetherAftershock"),
		TEXT("DigetherRerunSettlement"), TEXT("DigetherSave"), TEXT("DigetherLoad"), TEXT("DigetherNewGame"),
		TEXT("DigetherMonsterSpawn"), TEXT("DigetherMonsterForceHabitat"), TEXT("DigetherMonsterAdvanceSpawnTimer"),
		TEXT("DigetherMonsterEvaluate"), TEXT("DigetherHubTeleport"), TEXT("DigetherHubAction"), TEXT("DigetherShowRunResult"),
		TEXT("DigetherPour"), TEXT("DigetherPourStop"), TEXT("DigetherSetQuotaProgress"), TEXT("DigetherCreditFastForward"),
		TEXT("DigetherLever"), TEXT("DigetherRunSmoke"), TEXT("DigetherGrantCredits"), TEXT("DigetherDumpState"),
		TEXT("DigetherTeleportOut"), TEXT("DigetherTeleportToDeposit")
	};
	for (const TCHAR* Name : LegacyNames)
	{
		TestNotNull(FString::Printf(TEXT("기존 명령 %s 유지"), Name), DigetherDev::Find(Name));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherDevRegistryExecuteTest,
	"DIGETHER.Dev.Registry.Execute", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherDevRegistryExecuteTest::RunTest(const FString& Parameters)
{
	using namespace DigetherDev;
	const FName Name(TEXT("DigetherRegistryTestCommand"));
	FCommandDesc Desc;
	Desc.Name = Name;
	Desc.Category = TEXT("도구");
	Desc.Summary = TEXT("등록부 시험");
	Desc.Params = { FParam{ TEXT("정수"), EParamType::Int }, FParam{ TEXT("선택"), EParamType::Enum, true, FString(), { TEXT("A"), TEXT("B") } } };
	Desc.Params[0].Min = 0;
	Desc.Params[0].Max = 10;
	Desc.Scope = EScope::Local;
	Desc.bRequiresGameplayWorld = false;
	int32 Calls = 0;
	TestTrue(TEXT("등록"), Register(Desc, [&Calls](const FContext&, const TArray<FString>& Args)
	{
		++Calls;
		return FResult::Ok(FString::Printf(TEXT("ok:%d"), Args.Num()));
	}));
	TestNotNull(TEXT("콘솔 명령도 등록"), IConsoleManager::Get().FindConsoleObject(*Name.ToString()));

	FContext Context;
	Context.Source = ESource::Test;
	TestTrue(TEXT("정상 실행"), Execute(Name, { TEXT("5"), TEXT("b") }, Context).bOk);
	TestFalse(TEXT("필수 인자 누락 거절"), Execute(Name, {}, Context).bOk);
	TestFalse(TEXT("정수 자리 문자 거절"), Execute(Name, { TEXT("x") }, Context).bOk);
	TestFalse(TEXT("정수 자리 소수 거절"), Execute(Name, { TEXT("1.5") }, Context).bOk);
	TestFalse(TEXT("범위 밖 거절"), Execute(Name, { TEXT("11") }, Context).bOk);
	TestFalse(TEXT("선택지 밖 거절"), Execute(Name, { TEXT("1"), TEXT("C") }, Context).bOk);
	TestFalse(TEXT("인자 초과 거절"), Execute(Name, { TEXT("1"), TEXT("A"), TEXT("extra") }, Context).bOk);
	TestEqual(TEXT("거절은 처리기를 부르지 않음"), Calls, 1);

	AddExpectedMessage(TEXT("개발 명령 등록 거절: NoPrefix"), EAutomationExpectedMessageFlags::Contains, 1);
	TestFalse(TEXT("접두사 없는 이름 거절"), Register(FCommandDesc{ TEXT("NoPrefix"), TEXT("도구"), FString(), TEXT("x") },
		[](const FContext&, const TArray<FString>&) { return FResult::Ok(FString()); }));
	TestFalse(TEXT("알 수 없는 명령 거절"), Execute(TEXT("DigetherNoSuchCommand"), {}, Context).bOk);

	// 호스트 명령은 월드가 없으면 거절(참가자·메뉴 거절 경로와 같은 분기)
	FCommandDesc HostDesc = Desc;
	HostDesc.Name = TEXT("DigetherRegistryTestHost");
	HostDesc.Params.Reset();
	HostDesc.Scope = EScope::Host;
	HostDesc.bRequiresGameplayWorld = true;
	Register(HostDesc, [&Calls](const FContext&, const TArray<FString>&) { ++Calls; return FResult::Ok(FString()); });
	TestFalse(TEXT("월드 없는 호스트 명령 거절"), Execute(HostDesc.Name, {}, Context).bOk);

	FCommandDesc Unavailable = Desc;
	Unavailable.Name = TEXT("DigetherRegistryTestUnavailable");
	Unavailable.Params.Reset();
	Unavailable.UnavailableReason = TEXT("선행 시스템 없음");
	Register(Unavailable, [&Calls](const FContext&, const TArray<FString>&) { ++Calls; return FResult::Ok(FString()); });
	const FResult Blocked = Execute(Unavailable.Name, {}, Context);
	TestFalse(TEXT("사용 불가 명령 거절"), Blocked.bOk);
	TestTrue(TEXT("사용 불가 이유 전달"), Blocked.Message.Contains(TEXT("선행 시스템 없음")));
	TestEqual(TEXT("거절된 호스트·사용 불가 명령은 처리기 미호출"), Calls, 1);

	Unregister(Name);
	Unregister(HostDesc.Name);
	Unregister(Unavailable.Name);
	TestNull(TEXT("해제 뒤 등록부에서 제거"), Find(Name));
	TestNull(TEXT("해제 뒤 콘솔 명령 제거"), IConsoleManager::Get().FindConsoleObject(*Name.ToString()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherDevLogTest,
	"DIGETHER.Dev.LogCategory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherDevLogTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("개발 로그 카테고리 이름"), LogDigetherDev.GetCategoryName().ToString(), FString(TEXT("LogDigetherDev")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
