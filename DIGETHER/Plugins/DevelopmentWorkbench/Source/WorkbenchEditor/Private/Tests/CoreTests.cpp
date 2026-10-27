#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "AssetRegistry/AssetData.h"
#include "Core/WorkbenchCsvPatchManifest.h"
#include "Core/WorkbenchDataTableEditAdapter.h"
#include "Core/WorkbenchDraftSession.h"
#include "Core/WorkbenchItemFixture.h"
#include "Core/WorkbenchItemRowValidator.h"
#include "Core/WorkbenchItemValidation.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "Misc/DataValidation.h"
#include "UObject/Package.h"
#include "Widgets/SNullWidget.h"
#include "WorkbenchModule.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace WorkbenchCoreTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	FWorkbenchSyntheticItemRow MakeRow(const TCHAR* Id, const TCHAR* Name, int32 Price, float Weight)
	{
		FWorkbenchSyntheticItemRow Row;
		Row.DefinitionId = FName(Id);
		Row.DisplayName = Name;
		Row.Category = TEXT("Test");
		Row.Price = Price;
		Row.Weight = Weight;
		Row.RarityId = TEXT("R1");
		return Row;
	}

	UDataTable* MakeTable()
	{
		UDataTable* Table = NewObject<UDataTable>(GetTransientPackage(), NAME_None, RF_Transient);
		Table->RowStruct = FWorkbenchSyntheticItemRow::StaticStruct();
		Table->AddRow(TEXT("A"), MakeRow(TEXT("A"), TEXT("Alpha"), 10, 1.5f));
		Table->AddRow(TEXT("B"), MakeRow(TEXT("B"), TEXT("Beta"), 20, 2.5f));
		return Table;
	}

	FWorkbenchSyntheticItemRow ReadRow(const UDataTable* Table, FName Name)
	{
		FWorkbenchSyntheticItemRow Row;
		WorkbenchDataTableEditAdapter::TryReadRow(Table, Name, Row);
		return Row;
	}

	const FWorkbenchCsvCellPatch* FindPatch(const FWorkbenchCsvPatchManifest& Manifest, FName Row, FName Column)
	{
		return Manifest.Patches.FindByPredicate([&](const FWorkbenchCsvCellPatch& Patch) { return Patch.RowId == Row && Patch.Column == Column; });
	}

	FWorkbenchCsvPatchManifest Build(const FString& Csv)
	{
		return WorkbenchCsvPatchManifest::BuildFromCsv(Csv, WorkbenchItemFixture::GetColumns());
	}

	// 내보낸 CSV의 메타 줄(기준 해시)을 유지하고, 지정 행의 가격 열만 담은 CSV를 만든다.
	FString MakeBaselineCsv(const FString& Exported, const TCHAR* RowId, int32 Price)
	{
		FString MetaLine;
		FString Rest;
		if (!Exported.Split(TEXT("\n"), &MetaLine, &Rest) || !MetaLine.StartsWith(TEXT("#WorkbenchCsv")))
		{
			return FString();
		}
		return MetaLine + TEXT("\nRowId,Price\n") + FString::Printf(TEXT("%s,%d\n"), RowId, Price);
	}

	bool HasRule(const TArray<FWorkbenchValidationMessage>& Messages, const TCHAR* Rule, bool bBlocking)
	{
		return Messages.ContainsByPredicate([&](const FWorkbenchValidationMessage& Message)
		{
			return Message.RuleId == FName(Rule) && Message.IsBlocking() == bBlocking;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchCoreValidationRulesTest, "DevelopmentWorkbench.Core.Validation.Rules", WorkbenchCoreTests::Flags)
bool FWorkbenchCoreValidationRulesTest::RunTest(const FString& Parameters)
{
	using namespace WorkbenchCoreTests;

	TMap<FName, FWorkbenchSyntheticItemRow> Rows;
	Rows.Add(TEXT("Good"), MakeRow(TEXT("Good"), TEXT("Good"), 5, 1.f));
	Rows.Add(TEXT("EmptyId"), MakeRow(TEXT(""), TEXT("x"), 5, 1.f));
	Rows.Add(TEXT("DupA"), MakeRow(TEXT("Dup"), TEXT("x"), 5, 1.f));
	Rows.Add(TEXT("DupB"), MakeRow(TEXT("Dup"), TEXT("y"), 5, 1.f));
	Rows.Add(TEXT("NoName"), MakeRow(TEXT("NoName"), TEXT(""), 5, 1.f));
	Rows.Add(TEXT("Neg"), MakeRow(TEXT("Neg"), TEXT("x"), -1, 1.f));
	Rows.Add(TEXT("Zero"), MakeRow(TEXT("Zero"), TEXT("x"), 0, 1.f));
	Rows.Add(TEXT("BadWeight"), MakeRow(TEXT("BadWeight"), TEXT("x"), 5, -2.f));

	const TArray<FWorkbenchValidationMessage> Messages = WorkbenchItemValidation::ValidateTable(Rows);
	TestTrue(TEXT("빈 DefinitionId는 차단"), HasRule(Messages, TEXT("Item.DefinitionIdEmpty"), true));
	TestTrue(TEXT("중복 DefinitionId는 차단"), HasRule(Messages, TEXT("Item.DefinitionIdDuplicate"), true));
	TestTrue(TEXT("빈 이름은 차단"), HasRule(Messages, TEXT("Item.DisplayNameEmpty"), true));
	TestTrue(TEXT("음수 가격은 차단"), HasRule(Messages, TEXT("Item.PriceNegative"), true));
	TestTrue(TEXT("잘못된 중량은 차단"), HasRule(Messages, TEXT("Item.WeightInvalid"), true));
	TestTrue(TEXT("가격 0은 결정 대기이며 차단하지 않음"), HasRule(Messages, TEXT("Item.PriceZero"), false));
	TestFalse(TEXT("가격 0 메시지는 차단이 아니다"), HasRule(Messages, TEXT("Item.PriceZero"), true));
	TestTrue(TEXT("전역 ID 인덱스는 미검사 안내로 남는다"), HasRule(Messages, TEXT("Item.GlobalDefinitionIdIndex"), false));
	TestTrue(TEXT("차단 메시지가 있으면 HasBlocking"), WorkbenchItemValidation::HasBlocking(Messages));

	TMap<FName, FWorkbenchSyntheticItemRow> Clean;
	Clean.Add(TEXT("Good"), MakeRow(TEXT("Good"), TEXT("Good"), 5, 1.f));
	TestFalse(TEXT("정상 행만 있으면 차단 없음"), WorkbenchItemValidation::HasBlocking(WorkbenchItemValidation::ValidateTable(Clean)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchCoreCsvManifestTest, "DevelopmentWorkbench.Core.Csv.Manifest", WorkbenchCoreTests::Flags)
bool FWorkbenchCoreCsvManifestTest::RunTest(const FString& Parameters)
{
	using namespace WorkbenchCoreTests;

	{
		const FWorkbenchCsvPatchManifest Manifest = Build(TEXT("RowId,Price,Weight\nA,30,\nB,,4.5\n"));
		TestTrue(TEXT("정상 CSV는 수용"), Manifest.IsAcceptable());
		const FWorkbenchCsvCellPatch* Price = FindPatch(Manifest, TEXT("A"), TEXT("Price"));
		TestNotNull(TEXT("값이 있는 셀은 패치가 된다"), Price);
		if (Price)
		{
			TestEqual(TEXT("가격 값"), Price->Value.IntValue, 30);
		}
		TestNull(TEXT("빈 셀은 변경 없음"), FindPatch(Manifest, TEXT("A"), TEXT("Weight")));
		TestNull(TEXT("빈 셀은 변경 없음(가격)"), FindPatch(Manifest, TEXT("B"), TEXT("Price")));
		TestNull(TEXT("헤더에 없는 열은 패치에 없다"), FindPatch(Manifest, TEXT("A"), TEXT("DisplayName")));
		TestEqual(TEXT("패치 수"), Manifest.Patches.Num(), 2);
	}

	{
		const FWorkbenchCsvPatchManifest Manifest = Build(TEXT("RowId,Price\nA,1\nA,2\n"));
		TestFalse(TEXT("중복 RowId는 거부"), Manifest.IsAcceptable());
		TestTrue(TEXT("중복 ID 기록"), Manifest.RejectedDuplicateIds.Contains(FName(TEXT("A"))));
	}

	{
		const FWorkbenchCsvPatchManifest Manifest = Build(TEXT("RowId,Bogus\nA,1\n"));
		TestFalse(TEXT("알 수 없는 열은 거부"), Manifest.IsAcceptable());
		TestTrue(TEXT("알 수 없는 열 기록"), Manifest.UnknownColumns.Contains(FName(TEXT("Bogus"))));
	}

	TestFalse(TEXT("로케일 소수점 쉼표 거부"), Build(TEXT("RowId,Weight\nA,\"1,5\"\n")).IsAcceptable());
	TestFalse(TEXT("NaN 거부"), Build(TEXT("RowId,Weight\nA,NaN\n")).IsAcceptable());
	TestFalse(TEXT("정수 열의 소수 거부"), Build(TEXT("RowId,Price\nA,1.5\n")).IsAcceptable());
	TestFalse(TEXT("미래 스키마 버전 거부"), Build(TEXT("#WorkbenchCsv,99,0\nRowId,Price\nA,1\n")).IsAcceptable());

	{
		const FWorkbenchCsvPatchManifest Manifest = Build(TEXT("RowId,DisplayName,Category\nA,<BLANK>,<NULL>\n"));
		TestTrue(TEXT("명시 토큰은 수용"), Manifest.IsAcceptable());
		const FWorkbenchCsvCellPatch* Display = FindPatch(Manifest, TEXT("A"), TEXT("DisplayName"));
		const FWorkbenchCsvCellPatch* Category = FindPatch(Manifest, TEXT("A"), TEXT("Category"));
		TestNotNull(TEXT("<BLANK> 패치"), Display);
		TestNotNull(TEXT("<NULL> 패치"), Category);
		if (Display)
		{
			TestTrue(TEXT("<BLANK>는 빈 문자열 설정"), Display->Value.StringValue.IsEmpty());
		}
		if (Category)
		{
			TestTrue(TEXT("<NULL>은 None 설정"), Category->Value.NameValue.IsNone());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchCoreCsvRoundTripTest, "DevelopmentWorkbench.Core.Csv.RoundTrip", WorkbenchCoreTests::Flags)
bool FWorkbenchCoreCsvRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace WorkbenchCoreTests;

	const FString Tricky = TEXT("골드 \"코인\", 두 줄\n둘째 줄");
	const FString Formula = TEXT("=1+1");
	TMap<FName, FWorkbenchRowValues> Rows;
	TArray<FName> Order;
	Order.Add(TEXT("A"));
	Order.Add(TEXT("B"));
	Rows.Add(TEXT("A"), WorkbenchItemFixture::RowToValues(MakeRow(TEXT("A"), *Tricky, 7, 0.1f)));
	Rows.Add(TEXT("B"), WorkbenchItemFixture::RowToValues(MakeRow(TEXT("B"), *Formula, -3, 123.456f)));

	const FString Csv = WorkbenchCsvPatchManifest::ExportToCsv(Order, Rows, WorkbenchItemFixture::GetColumns(), 0xABCD);
	const FWorkbenchCsvPatchManifest Manifest = Build(Csv);
	TestTrue(TEXT("내보낸 CSV는 다시 수용된다"), Manifest.IsAcceptable());
	TestTrue(TEXT("기준 해시를 읽는다"), Manifest.bHasBaselineHash && Manifest.BaselineHash == 0xABCD);
	TestEqual(TEXT("행 순서 보존"), Manifest.RowOrder, Order);

	const FWorkbenchCsvCellPatch* Name = FindPatch(Manifest, TEXT("A"), TEXT("DisplayName"));
	TestNotNull(TEXT("한글·따옴표·줄바꿈 셀"), Name);
	if (Name)
	{
		TestEqual(TEXT("한글·따옴표·줄바꿈 왕복"), Name->Value.StringValue, Tricky);
	}
	const FWorkbenchCsvCellPatch* Escaped = FindPatch(Manifest, TEXT("B"), TEXT("DisplayName"));
	TestNotNull(TEXT("수식 문자열 셀"), Escaped);
	if (Escaped)
	{
		TestEqual(TEXT("수식 접두 왕복"), Escaped->Value.StringValue, Formula);
	}
	TestTrue(TEXT("수식 문자열은 내보낼 때 접두가 붙는다"), Csv.Contains(TEXT("'=1+1")));

	const FWorkbenchCsvCellPatch* Weight = FindPatch(Manifest, TEXT("A"), TEXT("Weight"));
	TestNotNull(TEXT("실수 셀"), Weight);
	if (Weight)
	{
		TestEqual(TEXT("실수 왕복"), Weight->Value.FloatValue, 0.1f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchCoreApplyTest, "DevelopmentWorkbench.Core.Adapter.ApplyCsv", WorkbenchCoreTests::Flags)
bool FWorkbenchCoreApplyTest::RunTest(const FString& Parameters)
{
	using namespace WorkbenchCoreTests;
	if (!GEditor)
	{
		AddInfo(TEXT("GEditor가 없어 Undo 트랜잭션을 쓰는 적용 시험을 건너뛴다."));
		return true;
	}

	UDataTable* Table = MakeTable();
	FText Reason;
	TestTrue(TEXT("합성 테이블은 지원 대상"), WorkbenchDataTableEditAdapter::IsSupportedTable(Table, Reason));

	FString Exported;
	FText ExportError;
	TestTrue(TEXT("기준 해시가 든 CSV를 내보낸다"), WorkbenchDataTableEditAdapter::ExportTableToCsv(Table, Exported, ExportError));
	const FWorkbenchCsvPatchManifest Manifest = Build(MakeBaselineCsv(Exported, TEXT("A"), 99));
	const FWorkbenchEditPreview Preview = WorkbenchDataTableEditAdapter::PreviewCsv(Table, Manifest);
	TestTrue(TEXT("미리보기는 적용 가능"), Preview.CanApply());
	TestEqual(TEXT("변경 1건"), Preview.Changes.Num(), 1);
	TestEqual(TEXT("미리보기는 값을 바꾸지 않는다"), ReadRow(Table, TEXT("A")).Price, 10);

	const FWorkbenchApplyResult Result = WorkbenchDataTableEditAdapter::ApplyCsv(Table, Manifest);
	TestTrue(TEXT("적용 성공"), Result.bSuccess);
	const FWorkbenchSyntheticItemRow A = ReadRow(Table, TEXT("A"));
	TestEqual(TEXT("가격 변경"), A.Price, 99);
	TestEqual(TEXT("빠진 열(표시 이름) 보존"), A.DisplayName, FString(TEXT("Alpha")));
	TestEqual(TEXT("빠진 열(중량) 보존"), A.Weight, 1.5f);
	TestEqual(TEXT("다른 행 보존"), ReadRow(Table, TEXT("B")).Price, 20);
	TestTrue(TEXT("메모리 테이블은 저장 불가이므로 저장 시도는 실패한다"), !WorkbenchDataTableEditAdapter::SaveTablePackage(Table).bSaved);

	TestTrue(TEXT("Undo 성공"), GEditor->UndoTransaction());
	TestEqual(TEXT("Undo 후 원래 값"), ReadRow(Table, TEXT("A")).Price, 10);
	TestTrue(TEXT("Redo 성공"), GEditor->RedoTransaction());
	TestEqual(TEXT("Redo 후 적용 값"), ReadRow(Table, TEXT("A")).Price, 99);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchCoreRejectTest, "DevelopmentWorkbench.Core.Adapter.Rejections", WorkbenchCoreTests::Flags)
bool FWorkbenchCoreRejectTest::RunTest(const FString& Parameters)
{
	using namespace WorkbenchCoreTests;

	UDataTable* Table = MakeTable();

	FString Baseline;
	FText BaselineError;
	TestTrue(TEXT("기준 CSV 내보내기"), WorkbenchDataTableEditAdapter::ExportTableToCsv(Table, Baseline, BaselineError));

	{
		const FWorkbenchCsvPatchManifest NoBaseline = Build(TEXT("RowId,Price\nA,99\n"));
		TestTrue(TEXT("기준 해시 없는 CSV도 파서는 읽는다"), NoBaseline.IsAcceptable());
		TestFalse(TEXT("기준 해시 없음"), NoBaseline.bHasBaselineHash);
		const FWorkbenchEditPreview Preview = WorkbenchDataTableEditAdapter::PreviewCsv(Table, NoBaseline);
		TestFalse(TEXT("기준 해시 없는 CSV는 적용 거부"), Preview.CanApply());
		TestFalse(TEXT("거부 사유가 오류로 남는다"), Preview.Errors.IsEmpty());
		TestEqual(TEXT("거부 시 값은 그대로"), ReadRow(Table, TEXT("A")).Price, 10);
	}

	{
		const FWorkbenchEditPreview Preview = WorkbenchDataTableEditAdapter::PreviewCsv(Table, Build(MakeBaselineCsv(Baseline, TEXT("ZZZ"), 1)));
		TestFalse(TEXT("없는 행 ID는 적용 불가"), Preview.CanApply());
	}

	{
		const FWorkbenchEditPreview Preview = WorkbenchDataTableEditAdapter::PreviewCsv(Table, Build(MakeBaselineCsv(Baseline, TEXT("A"), -5)));
		TestFalse(TEXT("차단 검증 오류가 있으면 적용 불가"), Preview.CanApply());
		TestTrue(TEXT("차단 메시지가 포함된다"), WorkbenchItemValidation::HasBlocking(Preview.Messages));
	}

	{
		FString Exported;
		FText Error;
		TestTrue(TEXT("내보내기 성공"), WorkbenchDataTableEditAdapter::ExportTableToCsv(Table, Exported, Error));
		FWorkbenchSyntheticItemRow* Row = Table->FindRow<FWorkbenchSyntheticItemRow>(TEXT("B"), TEXT(""));
		if (TestNotNull(TEXT("B 행"), Row))
		{
			Row->Price = 21;
		}
		const FWorkbenchCsvPatchManifest Manifest = Build(Exported);
		const FWorkbenchEditPreview Preview = WorkbenchDataTableEditAdapter::PreviewCsv(Table, Manifest);
		TestFalse(TEXT("내보낸 뒤 원본이 바뀌면 기준 해시 불일치로 거부"), Preview.CanApply());
		TestEqual(TEXT("거부 시 값은 그대로"), ReadRow(Table, TEXT("B")).Price, 21);
	}

	{
		TMap<FName, FWorkbenchRowValues> Edits;
		FWorkbenchRowValues Values;
		Values.Add(TEXT("Price"), FWorkbenchScalarValue::MakeInt32(1));
		Edits.Add(TEXT("A"), Values);
		TMap<FName, uint32> Expected;
		Expected.Add(TEXT("A"), 0x1234u);
		const FWorkbenchEditPreview Preview = WorkbenchDataTableEditAdapter::PreviewEdits(Table, Edits, Expected);
		TestFalse(TEXT("행 기준 해시가 다르면 적용 불가"), Preview.CanApply());
	}

	{
		TMap<FName, FWorkbenchRowValues> Edits;
		FWorkbenchRowValues Values;
		Values.Add(TEXT("Price"), FWorkbenchScalarValue::MakeFloat(1.f));
		Edits.Add(TEXT("A"), Values);
		const FWorkbenchEditPreview Preview = WorkbenchDataTableEditAdapter::PreviewEdits(Table, Edits, TMap<FName, uint32>());
		TestFalse(TEXT("타입이 다른 값은 거부"), Preview.CanApply());
	}

	{
		TMap<FName, FWorkbenchRowValues> Edits;
		FWorkbenchRowValues Values;
		Values.Add(TEXT("NotAColumn"), FWorkbenchScalarValue::MakeInt32(1));
		Edits.Add(TEXT("A"), Values);
		const FWorkbenchEditPreview Preview = WorkbenchDataTableEditAdapter::PreviewEdits(Table, Edits, TMap<FName, uint32>());
		TestFalse(TEXT("허용 목록에 없는 열은 거부"), Preview.CanApply());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchCoreDraftSessionTest, "DevelopmentWorkbench.Core.DraftSession", WorkbenchCoreTests::Flags)
bool FWorkbenchCoreDraftSessionTest::RunTest(const FString& Parameters)
{
	using namespace WorkbenchCoreTests;

	UDataTable* Table = MakeTable();
	FWorkbenchItemDraftSession Session(Table, TEXT("A"));
	if (!TestTrue(TEXT("세션이 행을 읽는다"), Session.IsValid()))
	{
		return false;
	}
	TestFalse(TEXT("처음에는 변경 없음"), Session.IsDirty());
	TestFalse(TEXT("처음에는 오래되지 않음"), Session.IsStale());

	UWorkbenchItemDraftObject* Draft = Session.GetDraftObject();
	if (!TestNotNull(TEXT("초안 객체"), Draft))
	{
		return false;
	}
	Draft->Price = 77;
	TestTrue(TEXT("초안을 바꾸면 dirty"), Session.IsDirty());
	TestEqual(TEXT("바뀐 필드 1개"), Session.GetChanges().Num(), 1);
	TestEqual(TEXT("원본은 아직 그대로"), ReadRow(Table, TEXT("A")).Price, 10);

	Session.Revert();
	TestFalse(TEXT("되돌리면 dirty 아님"), Session.IsDirty());

	Draft->Price = 77;
	if (FWorkbenchSyntheticItemRow* Row = Table->FindRow<FWorkbenchSyntheticItemRow>(TEXT("A"), TEXT("")))
	{
		Row->DisplayName = TEXT("외부에서 변경");
	}
	TestTrue(TEXT("원본이 바뀌면 stale"), Session.RecomputeStale());
	const FWorkbenchApplyResult Blocked = Session.Apply();
	TestFalse(TEXT("stale 상태의 적용은 거부"), Blocked.bSuccess);
	TestEqual(TEXT("거부된 적용은 값을 바꾸지 않는다"), ReadRow(Table, TEXT("A")).Price, 10);

	TestTrue(TEXT("재조회 성공"), Session.Requery());
	TestFalse(TEXT("재조회 후 stale 해소"), Session.IsStale());
	TestEqual(TEXT("재조회가 새 원본 값을 반영"), Session.GetBaseline().DisplayName, FString(TEXT("외부에서 변경")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchCoreModuleTest, "DevelopmentWorkbench.Core.Module.Registry", WorkbenchCoreTests::Flags)
bool FWorkbenchCoreModuleTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("모듈이 로드되어 있다"), IWorkbenchModule::IsAvailable());
	IWorkbenchModule& Module = IWorkbenchModule::Get();

	const TArray<FWorkbenchToolDescriptor> Tools = Module.GetRegisteredTools();
	for (const TCHAR* Prefix : { TEXT("T01"), TEXT("T02"), TEXT("T03"), TEXT("T04"), TEXT("T05"), TEXT("T06"), TEXT("T07"), TEXT("T08"), TEXT("T09"), TEXT("T10"), TEXT("T11") })
	{
		TestTrue(FString::Printf(TEXT("%s 등록됨"), Prefix), Tools.ContainsByPredicate([Prefix](const FWorkbenchToolDescriptor& Tool) { return Tool.Id.ToString().StartsWith(Prefix) && Tool.IsValid(); }));
	}

	const FName TestId(TEXT("Test.CoreRegistry"));
	const FWorkbenchToolDescriptor Descriptor(TestId, FText::FromString(TEXT("시험")), FText::FromString(TEXT("시험")), []() { return SNullWidget::NullWidget; });
	TestTrue(TEXT("새 도구 등록"), Module.RegisterTool(Descriptor));
	TestFalse(TEXT("같은 ID 재등록은 거부"), Module.RegisterTool(Descriptor));
	TestFalse(TEXT("유효하지 않은 설명자는 거부"), Module.RegisterTool(FWorkbenchToolDescriptor()));
	Module.UnregisterTool(TestId);
	TestFalse(TEXT("해제 후 목록에 없다"), Module.GetRegisteredTools().ContainsByPredicate([TestId](const FWorkbenchToolDescriptor& Tool) { return Tool.Id == TestId; }));
	TestEqual(TEXT("내장 도구 수는 그대로"), Module.GetRegisteredTools().Num(), Tools.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchCoreEngineValidatorTest, "DevelopmentWorkbench.Core.Validation.EngineValidator", WorkbenchCoreTests::Flags)
bool FWorkbenchCoreEngineValidatorTest::RunTest(const FString& Parameters)
{
	using namespace WorkbenchCoreTests;

	// 엔진 UEditorValidatorBase::ValidateLoadedAsset은 공개 비가상 진입점이고 CanValidateAsset_Implementation은 서브클래스에서 공개한다.
	UWorkbenchItemRowValidator* Validator = NewObject<UWorkbenchItemRowValidator>(GetTransientPackage());
	UDataTable* Good = WorkbenchItemFixture::CreateTableFromRows(GetTransientPackage(), NAME_None, WorkbenchItemFixture::MakeCleanRows(), RF_Transient);
	UDataTable* Bad = WorkbenchItemFixture::CreateTableFromRows(GetTransientPackage(), NAME_None, WorkbenchItemFixture::MakeInvalidRows(), RF_Transient);
	if (!TestNotNull(TEXT("정상 fixture 테이블"), Good) || !TestNotNull(TEXT("문제 fixture 테이블"), Bad))
	{
		return false;
	}

	FDataValidationContext GoodContext;
	TestTrue(TEXT("합성 행 구조의 일반 테이블은 검증 대상"), Validator->CanValidateAsset_Implementation(FAssetData(Good), Good, GoodContext));
	TestEqual(TEXT("정상 fixture는 Valid"), Validator->ValidateLoadedAsset(FAssetData(Good), Good, GoodContext), EDataValidationResult::Valid);

	FDataValidationContext BadContext;
	TestTrue(TEXT("문제 fixture도 일반 테이블이면 검증 대상"), Validator->CanValidateAsset_Implementation(FAssetData(Bad), Bad, BadContext));
	TestEqual(TEXT("문제 fixture는 Invalid"), Validator->ValidateLoadedAsset(FAssetData(Bad), Bad, BadContext), EDataValidationResult::Invalid);

	UDataTable* Plain = NewObject<UDataTable>(GetTransientPackage(), NAME_None, RF_Transient);
	FDataValidationContext PlainContext;
	TestFalse(TEXT("행 구조가 다른 테이블은 대상 아님"), Validator->CanValidateAsset_Implementation(FAssetData(Plain), Plain, PlainContext));

	const FString SyntheticPath = WorkbenchItemFixture::GetSyntheticAssetPackagePath();
	FAssetData SyntheticData;
	SyntheticData.PackageName = FName(*SyntheticPath);
	FDataValidationContext SyntheticContext;
	TestFalse(TEXT("의도적 문제 행이 든 합성 시험 에셋은 에셋 검증에서 제외"), Validator->CanValidateAsset_Implementation(SyntheticData, Good, SyntheticContext));
	return true;
}

#endif
