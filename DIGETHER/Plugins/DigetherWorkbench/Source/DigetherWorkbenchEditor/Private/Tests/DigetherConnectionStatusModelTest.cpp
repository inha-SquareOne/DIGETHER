// 'DIGETHER 연결 상태' 패널의 순수 값 모델에 대한 Automation Test.
// UI나 Core 모듈 로드 없이 BuildDigetherConnectionStatusRows()의 계약을 검증한다.
#include "DigetherConnectionStatusModel.h"
#include "DigetherWorkbenchEditorModule.h"
#include "WorkbenchModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/AutomationTest.h"
#include "Containers/Set.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDigetherConnectionStatusModelTest,
	"DIGETHER.Workbench.ConnectionStatus.AllDomainsUnconnected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDigetherConnectionStatusModelTest::RunTest(const FString& Parameters)
{
	const TArray<FDigetherConnectionStatusRow> Rows = BuildDigetherConnectionStatusRows();

	TestEqual(
		TEXT("연결 상태 행 수는 아이템/경제/스폰/룸/런타임/기획 개정 6개여야 한다"),
		Rows.Num(), 6);

	TSet<FName> UniqueIds;
	for (const FDigetherConnectionStatusRow& Row : Rows)
	{
		UniqueIds.Add(Row.DomainId);

		TestFalse(
			FString::Printf(TEXT("%s 도메인은 실게임 provider가 등록되지 않아 연결 상태가 false여야 한다"), *Row.DomainId.ToString()),
			Row.bConnected);

		TestFalse(
			FString::Printf(TEXT("%s DomainLabel이 비어 있지 않아야 한다"), *Row.DomainId.ToString()),
			Row.DomainLabel.IsEmpty());

		TestFalse(
			FString::Printf(TEXT("%s StatusLabel이 비어 있지 않아야 한다"), *Row.DomainId.ToString()),
			Row.StatusLabel.IsEmpty());

		TestFalse(
			FString::Printf(TEXT("%s DetailText가 비어 있지 않아야 한다"), *Row.DomainId.ToString()),
			Row.DetailText.IsEmpty());
	}

	TestEqual(
		TEXT("DomainId는 행마다 고유해야 한다 (중복 도메인 금지)"),
		UniqueIds.Num(), Rows.Num());

	const bool bHasItemDomain = UniqueIds.Contains(FName(TEXT("Item")));
	const bool bHasEconomyDomain = UniqueIds.Contains(FName(TEXT("Economy")));
	const bool bHasSpawnDomain = UniqueIds.Contains(FName(TEXT("Spawn")));
	const bool bHasRoomDomain = UniqueIds.Contains(FName(TEXT("Room")));
	const bool bHasRuntimeDomain = UniqueIds.Contains(FName(TEXT("Runtime")));
	const bool bHasPlanningDomain = UniqueIds.Contains(FName(TEXT("PlanningRevision")));

	TestTrue(TEXT("아이템 도메인이 포함되어야 한다"), bHasItemDomain);
	TestTrue(TEXT("경제 도메인이 포함되어야 한다"), bHasEconomyDomain);
	TestTrue(TEXT("스폰 도메인이 포함되어야 한다"), bHasSpawnDomain);
	TestTrue(TEXT("룸 도메인이 포함되어야 한다"), bHasRoomDomain);
	TestTrue(TEXT("런타임 도메인이 포함되어야 한다"), bHasRuntimeDomain);
	TestTrue(TEXT("기획 승인 개정 도메인이 포함되어야 한다"), bHasPlanningDomain);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDigetherConnectionStatusRegistrationTest,
	"DIGETHER.Workbench.ConnectionStatus.CoreRegistration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDigetherConnectionStatusRegistrationTest::RunTest(const FString& Parameters)
{
	const FWorkbenchToolDescriptor Descriptor = MakeDigetherConnectionStatusDescriptor();

	TestTrue(TEXT("설명자는 Core 계약상 유효해야 한다 (Id 지정, MakeWidget 존재)"), Descriptor.IsValid());
	TestEqual(TEXT("설명자 Id는 등록 ID와 같아야 한다"), Descriptor.Id, GetDigetherConnectionStatusToolId());
	TestFalse(TEXT("제목이 비어 있지 않아야 한다"), Descriptor.Title.IsEmpty());
	TestFalse(TEXT("설명이 비어 있지 않아야 한다"), Descriptor.Description.IsEmpty());

	if (!FModuleManager::Get().IsModuleLoaded(TEXT("WorkbenchEditor")))
	{
		AddWarning(TEXT("WorkbenchEditor 모듈이 로드되지 않아 Core 등록 확인은 건너뛴다."));
		return true;
	}

	const IWorkbenchModule& Core = FModuleManager::GetModuleChecked<IWorkbenchModule>(TEXT("WorkbenchEditor"));
	int32 RegisteredCount = 0;
	for (const FWorkbenchToolDescriptor& Registered : Core.GetRegisteredTools())
	{
		if (Registered.Id == Descriptor.Id)
		{
			++RegisteredCount;
		}
	}
	TestEqual(TEXT("모듈 시작 후 Core에 연결 상태 도구가 정확히 한 번 등록되어야 한다"), RegisteredCount, 1);

	return true;
}
