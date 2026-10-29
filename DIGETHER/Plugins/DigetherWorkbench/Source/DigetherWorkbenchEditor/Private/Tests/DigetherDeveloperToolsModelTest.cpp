// 'DIGETHER 개발 도구' 허브의 판정 경계와 등록 소유권에 대한 Automation Test.
// 가짜 환경만 쓰며 개인 설정을 바꾸거나 엔진 UI를 열지 않는다.
#include "DigetherDeveloperToolsModel.h"
#include "DigetherWorkbenchEditorModule.h"
#include "WorkbenchModule.h"
#include "Misc/AutomationTest.h"

namespace DigetherDevToolsTestPrivate
{
	FDigetherDevToolsEnvironment MakeAllPresentEnvironment()
	{
		FDigetherDevToolsEnvironment Environment;
		Environment.GetPluginState = [](FName) { return EDigetherPluginState::Enabled; };
		Environment.HasGlobalTab = [](FName) { return true; };
		Environment.HasLevelEditorTab = [](FName) { return true; };
		Environment.HasSettingsSection = [](FName, FName, FName) { return true; };
		return Environment;
	}

	const FDigetherShortcut* FindShortcut(const TArray<FDigetherShortcut>& Shortcuts, const TCHAR* Id)
	{
		return Shortcuts.FindByPredicate([Id](const FDigetherShortcut& Shortcut)
		{
			return Shortcut.Id == FName(Id);
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDigetherDeveloperToolsDescriptorTest,
	"DIGETHER.Workbench.DeveloperTools.Descriptor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDigetherDeveloperToolsDescriptorTest::RunTest(const FString& Parameters)
{
	const FWorkbenchToolDescriptor Descriptor = MakeDigetherDeveloperToolsDescriptor();

	TestTrue(TEXT("개발 도구 설명자가 유효해야 한다"), Descriptor.IsValid());
	TestEqual(TEXT("개발 도구 ID"), Descriptor.Id, FName(TEXT("Digether.DeveloperTools")));
	TestEqual(TEXT("개발 도구 제목"), Descriptor.Title.ToString(), FString(TEXT("DIGETHER 개발 도구")));
	TestFalse(TEXT("개발 도구 설명이 비어 있지 않아야 한다"), Descriptor.Description.IsEmpty());
	TestNotEqual(TEXT("연결 상태 도구와 ID가 달라야 한다"), Descriptor.Id, GetDigetherConnectionStatusToolId());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDigetherDeveloperToolsShortcutFailureTest,
	"DIGETHER.Workbench.DeveloperTools.ShortcutFailureBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDigetherDeveloperToolsShortcutFailureTest::RunTest(const FString& Parameters)
{
	using namespace DigetherDevToolsTestPrivate;

	const TArray<FDigetherShortcut> Shortcuts = GetDigetherShortcuts();
	const FDigetherShortcut* AssetSearch = FindShortcut(Shortcuts, TEXT("AssetSearch"));
	const FDigetherShortcut* RewindDebugger = FindShortcut(Shortcuts, TEXT("RewindDebugger"));
	const FDigetherShortcut* AutoSave = FindShortcut(Shortcuts, TEXT("AutoSaveSettings"));
	const FDigetherShortcut* OutputLog = FindShortcut(Shortcuts, TEXT("OutputLog"));
	if (!TestNotNull(TEXT("AssetSearch 바로가기"), AssetSearch)
		|| !TestNotNull(TEXT("RewindDebugger 바로가기"), RewindDebugger)
		|| !TestNotNull(TEXT("AutoSaveSettings 바로가기"), AutoSave)
		|| !TestNotNull(TEXT("OutputLog 바로가기"), OutputLog))
	{
		return false;
	}

	{
		const FDigetherShortcutAvailability Result = EvaluateDigetherShortcut(*AssetSearch, MakeAllPresentEnvironment());
		TestTrue(TEXT("모든 조건이 갖춰지면 사용 가능"), Result.bAvailable);
	}

	{
		FDigetherDevToolsEnvironment Environment = MakeAllPresentEnvironment();
		Environment.GetPluginState = [](FName) { return EDigetherPluginState::NotFound; };
		const FDigetherShortcutAvailability Result = EvaluateDigetherShortcut(*AssetSearch, Environment);
		TestFalse(TEXT("플러그인이 없으면 탭이 등록돼 있어도 사용 불가"), Result.bAvailable);
		TestFalse(TEXT("플러그인 부재 이유가 있어야 한다"), Result.FailureReason.IsEmpty());
	}

	{
		FDigetherDevToolsEnvironment Environment = MakeAllPresentEnvironment();
		Environment.GetPluginState = [](FName) { return EDigetherPluginState::Disabled; };
		const FDigetherShortcutAvailability Result = EvaluateDigetherShortcut(*AssetSearch, Environment);
		TestFalse(TEXT("플러그인이 비활성이면 사용 불가"), Result.bAvailable);
		TestFalse(TEXT("비활성 이유가 있어야 한다"), Result.FailureReason.IsEmpty());
	}

	{
		FDigetherDevToolsEnvironment Environment = MakeAllPresentEnvironment();
		Environment.HasGlobalTab = [](FName) { return false; };
		TestFalse(TEXT("전역 탭이 미등록이면 사용 불가"), EvaluateDigetherShortcut(*AssetSearch, Environment).bAvailable);
		TestFalse(TEXT("전역 탭 미등록 이유"), EvaluateDigetherShortcut(*OutputLog, Environment).FailureReason.IsEmpty());
	}

	{
		FDigetherDevToolsEnvironment Environment = MakeAllPresentEnvironment();
		Environment.HasGlobalTab = [](FName) { return true; };
		Environment.HasLevelEditorTab = [](FName) { return false; };
		TestFalse(TEXT("전역 탭이 있어도 레벨 에디터 탭이 없으면 되감기 디버거 사용 불가"),
			EvaluateDigetherShortcut(*RewindDebugger, Environment).bAvailable);
	}

	{
		FDigetherDevToolsEnvironment Environment = MakeAllPresentEnvironment();
		Environment.HasSettingsSection = [](FName, FName, FName) { return false; };
		const FDigetherShortcutAvailability Result = EvaluateDigetherShortcut(*AutoSave, Environment);
		TestFalse(TEXT("설정 구획이 없으면 사용 불가"), Result.bAvailable);
		TestFalse(TEXT("설정 구획 부재 이유"), Result.FailureReason.IsEmpty());
	}

	{
		const FDigetherDevToolsEnvironment EmptyEnvironment;
		for (const FDigetherShortcut& Shortcut : Shortcuts)
		{
			TestFalse(
				FString::Printf(TEXT("%s: 조회 수단이 없으면 성공으로 간주하지 않는다"), *Shortcut.Id.ToString()),
				EvaluateDigetherShortcut(Shortcut, EmptyEnvironment).bAvailable);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDigetherDeveloperToolsSettingsSeverityTest,
	"DIGETHER.Workbench.DeveloperTools.SettingsSeverity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDigetherDeveloperToolsSettingsSeverityTest::RunTest(const FString& Parameters)
{
	{
		FDigetherAutoSaveValues Values;
		TestEqual(TEXT("자동 저장 설정을 읽지 못하면 문제"),
			DescribeDigetherAutoSave(Values).Severity, EDigetherDevToolsSeverity::Problem);

		Values.bAvailable = true;
		Values.IntervalMinutes = 10;
		Values.bSaveMaps = true;
		TestEqual(TEXT("자동 저장이 꺼져 있으면 다른 항목이 켜져 있어도 경고"),
			DescribeDigetherAutoSave(Values).Severity, EDigetherDevToolsSeverity::Warning);

		Values.bEnabled = true;
		Values.bSaveMaps = false;
		Values.bSaveContent = false;
		TestEqual(TEXT("켜져 있어도 저장 대상이 없으면 경고"),
			DescribeDigetherAutoSave(Values).Severity, EDigetherDevToolsSeverity::Warning);

		Values.bSaveMaps = true;
		Values.IntervalMinutes = 0;
		TestEqual(TEXT("간격이 1분 미만이면 경고"),
			DescribeDigetherAutoSave(Values).Severity, EDigetherDevToolsSeverity::Warning);

		Values.IntervalMinutes = 10;
		const FDigetherDevToolsRow Row = DescribeDigetherAutoSave(Values);
		TestEqual(TEXT("정상 설정값은 중립 등급"), Row.Severity, EDigetherDevToolsSeverity::Neutral);
		TestTrue(TEXT("설정값 조회가 동작 검증이 아님을 밝혀야 한다"),
			Row.Detail.ToString().Contains(TEXT("검증하지 않았습니다")));
	}

	{
		FDigetherAssetManagerValues Values;
		TestEqual(TEXT("에셋 매니저 설정을 읽지 못하면 문제"),
			DescribeDigetherAssetManagerRule(Values).Severity, EDigetherDevToolsSeverity::Problem);

		Values.bAvailable = true;
		Values.PrimaryAssetTypeCount = 3;
		TestEqual(TEXT("GameFeatureData 규칙이 없으면 문제"),
			DescribeDigetherAssetManagerRule(Values).Severity, EDigetherDevToolsSeverity::Problem);

		Values.bHasGameFeatureDataRule = true;
		const FDigetherDevToolsRow Row = DescribeDigetherAssetManagerRule(Values);
		TestEqual(TEXT("규칙이 있어도 로딩 검증 성공이 아니므로 중립 등급"), Row.Severity, EDigetherDevToolsSeverity::Neutral);
		TestTrue(TEXT("로딩 동작 미검증을 밝혀야 한다"), Row.Detail.ToString().Contains(TEXT("검증하지 않았습니다")));
	}

	TestEqual(TEXT("미발견 플러그인은 문제"),
		DescribeDigetherPlugin(FName(TEXT("AssetSearch")), EDigetherPluginState::NotFound).Severity, EDigetherDevToolsSeverity::Problem);
	TestEqual(TEXT("비활성 플러그인은 경고"),
		DescribeDigetherPlugin(FName(TEXT("AssetSearch")), EDigetherPluginState::Disabled).Severity, EDigetherDevToolsSeverity::Warning);
	TestEqual(TEXT("활성 플러그인도 기능 검증이 아니므로 중립 등급"),
		DescribeDigetherPlugin(FName(TEXT("AssetSearch")), EDigetherPluginState::Enabled).Severity, EDigetherDevToolsSeverity::Neutral);

	TestEqual(TEXT("프로젝트 기준과 같은 엔진 버전은 중립 등급"),
		DescribeDigetherEngineVersion(5, 8, 2).Severity, EDigetherDevToolsSeverity::Neutral);
	TestEqual(TEXT("패치 버전만 달라도 문제"),
		DescribeDigetherEngineVersion(5, 8, 1).Severity, EDigetherDevToolsSeverity::Problem);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDigetherDeveloperToolsRegistrationOwnershipTest,
	"DIGETHER.Workbench.DeveloperTools.RegistrationOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDigetherDeveloperToolsRegistrationOwnershipTest::RunTest(const FString& Parameters)
{
	// 연결 상태 ID가 이미 다른 곳에 등록된 상황: 이쪽은 거부되고 개발 도구만 등록된다.
	{
		TArray<FName> Registered;
		FDigetherRegisteredTools Owned = RegisterDigetherTools(
			[&Registered](const FWorkbenchToolDescriptor& Descriptor)
			{
				if (Descriptor.Id == GetDigetherConnectionStatusToolId())
				{
					return false;
				}
				Registered.Add(Descriptor.Id);
				return true;
			});

		TestFalse(TEXT("거부된 연결 상태 도구는 소유하지 않는다"), Owned.bConnectionStatus);
		TestTrue(TEXT("한 도구의 거부가 개발 도구 등록을 막지 않는다"), Owned.bDeveloperTools);
		TestEqual(TEXT("실제 등록된 도구는 하나"), Registered.Num(), 1);

		TArray<FName> Unregistered;
		UnregisterDigetherTools(Owned, [&Unregistered](FName Id) { Unregistered.Add(Id); });
		TestEqual(TEXT("해제는 소유한 도구 하나뿐"), Unregistered.Num(), 1);
		TestTrue(TEXT("소유하지 않은 연결 상태 도구는 해제하지 않는다"),
			!Unregistered.Contains(GetDigetherConnectionStatusToolId()));
		TestTrue(TEXT("해제 뒤 소유 기록이 비워진다"), !Owned.bConnectionStatus && !Owned.bDeveloperTools);

		Unregistered.Reset();
		UnregisterDigetherTools(Owned, [&Unregistered](FName Id) { Unregistered.Add(Id); });
		TestEqual(TEXT("두 번째 해제는 아무것도 호출하지 않는다"), Unregistered.Num(), 0);
	}

	// 개발 도구만 거부된 반대 경우.
	{
		FDigetherRegisteredTools Owned = RegisterDigetherTools(
			[](const FWorkbenchToolDescriptor& Descriptor)
			{
				return Descriptor.Id != GetDigetherDeveloperToolsToolId();
			});
		TestTrue(TEXT("연결 상태는 등록된다"), Owned.bConnectionStatus);
		TestFalse(TEXT("거부된 개발 도구는 소유하지 않는다"), Owned.bDeveloperTools);

		TArray<FName> Unregistered;
		UnregisterDigetherTools(Owned, [&Unregistered](FName Id) { Unregistered.Add(Id); });
		TestEqual(TEXT("해제 대상은 연결 상태 하나"), Unregistered.Num(), 1);
		TestTrue(TEXT("소유하지 않은 개발 도구는 해제하지 않는다"),
			!Unregistered.Contains(GetDigetherDeveloperToolsToolId()));
	}

	// 둘 다 거부되면 아무것도 해제하지 않는다.
	{
		FDigetherRegisteredTools Owned = RegisterDigetherTools([](const FWorkbenchToolDescriptor&) { return false; });
		int32 UnregisterCalls = 0;
		UnregisterDigetherTools(Owned, [&UnregisterCalls](FName) { ++UnregisterCalls; });
		TestEqual(TEXT("모두 거부되면 해제 호출이 없다"), UnregisterCalls, 0);
	}

	return true;
}
