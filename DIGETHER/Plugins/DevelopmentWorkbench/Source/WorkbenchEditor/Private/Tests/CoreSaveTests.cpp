#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "Core/WorkbenchCsvPatchManifest.h"
#include "Core/WorkbenchDataTableEditAdapter.h"
#include "Core/WorkbenchItemFixture.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PackageTools.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace WorkbenchCoreSaveTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	// 시험 전용 임시 패키지 환경: 프로젝트 Saved 아래의 새 폴더만 /WorkbenchTest/ 마운트로 쓰고,
	// 백업·저널 루트도 그 아래로 돌린다. 기존 사용자 에셋·저널은 건드리지 않는다.
	struct FTempEnv
	{
		FString Root;
		FString Content;
		FString Storage;
		TArray<FString> ProtectedFiles;
		TArray<TWeakObjectPtr<UDataTable>> Tables;

		FTempEnv()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("WorkbenchAutomation"), FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			Content = FPaths::Combine(Root, TEXT("Content"));
			Storage = FPaths::Combine(Root, TEXT("Storage"));
			IFileManager::Get().MakeDirectory(*Content, true);
			IFileManager::Get().MakeDirectory(*Storage, true);
			FPackageName::RegisterMountPoint(TEXT("/WorkbenchTest/"), Content);
			WorkbenchDataTableEditAdapter::ResetOwnershipStateForTesting();
			WorkbenchDataTableEditAdapter::SetStorageRootOverrideForTesting(Storage);
		}

		~FTempEnv()
		{
			for (const FString& File : ProtectedFiles)
			{
				FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*File, false);
			}
			for (const TWeakObjectPtr<UDataTable>& Weak : Tables)
			{
				if (UDataTable* Table = Weak.Get())
				{
					UPackage* Package = Table->GetOutermost();
					Package->SetDirtyFlag(false);
					Table->ClearFlags(RF_Public | RF_Standalone);
					Table->MarkAsGarbage();
				}
			}
			WorkbenchDataTableEditAdapter::SetStorageRootOverrideForTesting(FString());
			WorkbenchDataTableEditAdapter::ResetOwnershipStateForTesting();
			FPackageName::UnRegisterMountPoint(TEXT("/WorkbenchTest/"), Content);
			if (Root.Contains(TEXT("WorkbenchAutomation")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		UDataTable* MakeDiskTable(const TCHAR* ShortName)
		{
			const FString PackageName = FString(TEXT("/WorkbenchTest/")) + ShortName;
			UPackage* Package = CreatePackage(*PackageName);
			UDataTable* Table = WorkbenchItemFixture::CreateTableFromRows(Package, FName(ShortName), WorkbenchItemFixture::MakeCleanRows(), RF_Public | RF_Standalone | RF_Transactional);
			if (Table)
			{
				WorkbenchDataTableEditAdapter::MarkPackageOwnedByWorkbench(Table);
				Tables.Add(Table);
			}
			return Table;
		}

		FString FileOf(const UDataTable* Table) const
		{
			return FPackageName::LongPackageNameToFilename(Table->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		}
	};

	FString WithBaseline(const FString& Exported, FName RowId, int32 Price)
	{
		FString MetaLine;
		FString Rest;
		if (!Exported.Split(TEXT("\n"), &MetaLine, &Rest) || !MetaLine.StartsWith(TEXT("#WorkbenchCsv")))
		{
			return FString();
		}
		return MetaLine + TEXT("\nRowId,Price\n") + FString::Printf(TEXT("%s,%d\n"), *RowId.ToString(), Price);
	}

	FName FirstRow(const UDataTable* Table)
	{
		TArray<FName> Names = Table->GetRowNames();
		Names.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
		return Names.IsEmpty() ? NAME_None : Names[0];
	}

	int32 PriceOf(const UDataTable* Table, FName Row)
	{
		FWorkbenchSyntheticItemRow Value;
		WorkbenchDataTableEditAdapter::TryReadRow(Table, Row, Value);
		return Value.Price;
	}

	FWorkbenchApplyResult SetPrice(UDataTable* Table, FName Row, int32 Price)
	{
		FString Csv;
		FText Error;
		WorkbenchDataTableEditAdapter::ExportTableToCsv(Table, Csv, Error);
		return WorkbenchDataTableEditAdapter::ApplyCsv(Table, WorkbenchCsvPatchManifest::BuildFromCsv(WithBaseline(Csv, Row, Price), WorkbenchItemFixture::GetColumns()));
	}

	bool ExternalPriceEdit(UDataTable* Table, FName Row, int32 Delta)
	{
		if (FWorkbenchSyntheticItemRow* Value = Table->FindRow<FWorkbenchSyntheticItemRow>(Row, TEXT("")))
		{
			Value->Price += Delta;
			return true;
		}
		return false;
	}

	// 정상 저장 한 번으로 디스크 파일을 만든다(이후 시험의 "기존 파일이 있는 패키지" 전제).
	bool SeedSavedFile(FAutomationTestBase& Test, UDataTable* Table, FName Row, int32 Price)
	{
		if (!Test.TestTrue(TEXT("초기 적용"), SetPrice(Table, Row, Price).bSuccess))
		{
			return false;
		}
		const FWorkbenchSaveResult Saved = WorkbenchDataTableEditAdapter::SaveTablePackage(Table);
		return Test.TestTrue(TEXT("초기 저장"), Saved.bSaved);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchCoreStableHashTest, "DevelopmentWorkbench.Core.Hash.Stable", WorkbenchCoreSaveTests::Flags)
bool FWorkbenchCoreStableHashTest::RunTest(const FString& Parameters)
{
	// FNV-1a 32비트 공개 시험 벡터: 실행·플랫폼·FName 인덱스와 무관해야 한다.
	TestEqual(TEXT("FNV-1a 빈 문자열"), WorkbenchItemFixture::Fnv1a32(TEXT("")), 0x811C9DC5u);
	TestEqual(TEXT("FNV-1a \"a\""), WorkbenchItemFixture::Fnv1a32(TEXT("a")), 0xE40C292Cu);
	TestEqual(TEXT("FNV-1a \"foobar\""), WorkbenchItemFixture::Fnv1a32(TEXT("foobar")), 0xBF9CF968u);

	FWorkbenchSyntheticItemRow Row;
	Row.DefinitionId = TEXT("A");
	Row.DisplayName = TEXT("Alpha");
	Row.Category = TEXT("Test");
	Row.Price = 10;
	Row.Weight = 1.5f;
	Row.RarityId = TEXT("R1");

	const FString Expected = TEXT("R1;1:A;5:Alpha;4:Test;2:10;3:1.5;2:R1;");
	TestEqual(TEXT("행 정규 문자열은 고정 형식"), WorkbenchItemFixture::SerializeRowCanonical(Row), Expected);
	TestEqual(TEXT("행 해시는 정규 문자열의 FNV-1a"), WorkbenchItemFixture::ComputeRowHash(Row), WorkbenchItemFixture::Fnv1a32(Expected));

	FWorkbenchSyntheticItemRow NegZero = Row;
	NegZero.Weight = 0.f;
	FWorkbenchSyntheticItemRow Zero = Row;
	Zero.Weight = -0.f;
	TestEqual(TEXT("-0과 0은 같은 해시"), WorkbenchItemFixture::ComputeRowHash(NegZero), WorkbenchItemFixture::ComputeRowHash(Zero));

	FWorkbenchSyntheticItemRow Other = Row;
	Other.Price = 11;
	TestNotEqual(TEXT("값이 다르면 해시도 다르다"), WorkbenchItemFixture::ComputeRowHash(Row), WorkbenchItemFixture::ComputeRowHash(Other));

	// 필드 경계가 모호하지 않아야 한다(길이 접두).
	FWorkbenchSyntheticItemRow Left = Row;
	Left.DisplayName = TEXT("ab");
	Left.Category = TEXT("c");
	FWorkbenchSyntheticItemRow Right = Row;
	Right.DisplayName = TEXT("a");
	Right.Category = TEXT("bc");
	TestNotEqual(TEXT("필드 경계가 다르면 해시도 다르다"), WorkbenchItemFixture::ComputeRowHash(Left), WorkbenchItemFixture::ComputeRowHash(Right));

	// 삽입 순서와 무관한 테이블 해시.
	TMap<FName, FWorkbenchSyntheticItemRow> Forward;
	Forward.Add(TEXT("A"), Row);
	Forward.Add(TEXT("B"), Other);
	TMap<FName, FWorkbenchSyntheticItemRow> Backward;
	Backward.Add(TEXT("B"), Other);
	Backward.Add(TEXT("A"), Row);
	TestEqual(TEXT("삽입 순서가 달라도 테이블 해시는 같다"), WorkbenchItemFixture::ComputeTableHash(Forward), WorkbenchItemFixture::ComputeTableHash(Backward));
	TestTrue(TEXT("테이블 정규 문자열은 T1 머리글로 시작"), WorkbenchItemFixture::SerializeTableCanonical(Forward).StartsWith(TEXT("T1;2;")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchCoreRowIdRoundTripTest, "DevelopmentWorkbench.Core.Csv.RowIdRoundTrip", WorkbenchCoreSaveTests::Flags)
bool FWorkbenchCoreRowIdRoundTripTest::RunTest(const FString& Parameters)
{
	const TArray<FName> Ids = {
		FName(TEXT("Plain")),
		FName(TEXT("=SUM(A1)")),
		FName(TEXT("+Plus")),
		FName(TEXT("-Minus")),
		FName(TEXT("@At")),
		FName(TEXT("<NULL>")),
		FName(TEXT("<BLANK>")),
		FName(TEXT(" Leading")),
		FName(TEXT("Trailing ")),
		FName(TEXT("쉼표,따옴표\"")),
	};

	TMap<FName, FWorkbenchRowValues> Rows;
	for (const FName& Id : Ids)
	{
		FWorkbenchSyntheticItemRow Row;
		Row.DefinitionId = Id;
		Row.DisplayName = TEXT("x");
		Row.Price = 1;
		Rows.Add(Id, WorkbenchItemFixture::RowToValues(Row));
	}

	const FString Csv = WorkbenchCsvPatchManifest::ExportToCsv(Ids, Rows, WorkbenchItemFixture::GetColumns(), 1u);
	const FWorkbenchCsvPatchManifest Manifest = WorkbenchCsvPatchManifest::BuildFromCsv(Csv, WorkbenchItemFixture::GetColumns());
	TestTrue(TEXT("특수 RowId를 내보낸 CSV도 수용된다"), Manifest.IsAcceptable());
	TestEqual(TEXT("RowId 왕복(순서 포함)"), Manifest.RowOrder, Ids);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchCoreOwnershipTest, "DevelopmentWorkbench.Core.Ownership.Lifecycle", WorkbenchCoreSaveTests::Flags)
bool FWorkbenchCoreOwnershipTest::RunTest(const FString& Parameters)
{
	using namespace WorkbenchCoreSaveTests;
	if (!GEditor)
	{
		AddInfo(TEXT("GEditor가 없어 Undo 트랜잭션을 쓰는 소유권 시험을 건너뛴다."));
		return true;
	}

	FTempEnv Env;
	UDataTable* Table = Env.MakeDiskTable(TEXT("CoreOwnership"));
	if (!TestNotNull(TEXT("임시 테이블"), Table))
	{
		return false;
	}
	const FName Row = FirstRow(Table);
	const int32 Original = PriceOf(Table, Row);

	TestEqual(TEXT("처음에는 소유 상태"), WorkbenchDataTableEditAdapter::GetOwnership(Table), EWorkbenchOwnership::Owned);
	TestTrue(TEXT("적용 성공"), SetPrice(Table, Row, Original + 7).bSuccess);
	TestEqual(TEXT("적용 뒤 값"), PriceOf(Table, Row), Original + 7);
	TestEqual(TEXT("적용 뒤 소유"), WorkbenchDataTableEditAdapter::GetOwnership(Table), EWorkbenchOwnership::Owned);
	TestTrue(TEXT("미저장 변경이 기록된다"), WorkbenchDataTableEditAdapter::GetPendingChangeCount(Table) > 0);

	// 외부 변경: 적용·저장이 모두 막히고, 새 적용으로 소유권을 되찾을 수 없다.
	TestTrue(TEXT("외부 변경"), ExternalPriceEdit(Table, Row, 5));
	TestEqual(TEXT("외부 변경 뒤 Broken"), WorkbenchDataTableEditAdapter::GetOwnership(Table), EWorkbenchOwnership::Broken);
	FText Reason;
	TestFalse(TEXT("Broken이면 편집 불가"), WorkbenchDataTableEditAdapter::EvaluateEditability(Table, Reason));
	const int32 ExternalPrice = PriceOf(Table, Row);
	TestFalse(TEXT("Broken에서 새 적용은 거부"), SetPrice(Table, Row, Original + 100).bSuccess);
	TestEqual(TEXT("거부된 적용은 값을 바꾸지 않는다"), PriceOf(Table, Row), ExternalPrice);
	TestEqual(TEXT("거부된 적용 뒤에도 Broken(소유권 세탁 없음)"), WorkbenchDataTableEditAdapter::GetOwnership(Table), EWorkbenchOwnership::Broken);
	const FWorkbenchSaveResult BlockedSave = WorkbenchDataTableEditAdapter::SaveTablePackage(Table);
	TestFalse(TEXT("Broken에서 저장은 거부"), BlockedSave.bSaved);
	TestFalse(TEXT("저장 거부 사유가 있다"), BlockedSave.Message.IsEmpty());
	TestFalse(TEXT("거부된 저장은 파일을 만들지 않는다"), IFileManager::Get().FileExists(*Env.FileOf(Table)));

	// 외부 변경을 되돌리면 이력과 일치하므로 다시 Owned.
	TestTrue(TEXT("외부 변경 원복"), ExternalPriceEdit(Table, Row, -5));
	TestEqual(TEXT("원복 뒤 Owned"), WorkbenchDataTableEditAdapter::GetOwnership(Table), EWorkbenchOwnership::Owned);

	// Undo로 기준 상태까지 돌아오면 이력과 일치(Owned)하고 미저장 목록은 비어야 한다.
	GEditor->UndoTransaction();
	TestEqual(TEXT("Undo 뒤 값"), PriceOf(Table, Row), Original);
	TestEqual(TEXT("Undo 뒤 Owned"), WorkbenchDataTableEditAdapter::GetOwnership(Table), EWorkbenchOwnership::Owned);
	TestEqual(TEXT("Undo 뒤 미저장 변경 없음"), WorkbenchDataTableEditAdapter::GetPendingChangeCount(Table), 0);
	TestFalse(TEXT("미저장 변경이 없으면 저장하지 않는다"), WorkbenchDataTableEditAdapter::SaveTablePackage(Table).bSaved);

	// 복구 경로: Broken에서 소유권 해제 후 깨끗한 패키지는 다시 편집할 수 있다.
	TestTrue(TEXT("다시 외부 변경"), ExternalPriceEdit(Table, Row, 3));
	TestEqual(TEXT("다시 Broken"), WorkbenchDataTableEditAdapter::GetOwnership(Table), EWorkbenchOwnership::Broken);
	Table->GetOutermost()->SetDirtyFlag(false);
	FText ReleaseMessage;
	TestTrue(TEXT("소유권 해제"), WorkbenchDataTableEditAdapter::ReleaseWorkbenchOwnership(Table, ReleaseMessage));
	TestEqual(TEXT("해제 뒤 None"), WorkbenchDataTableEditAdapter::GetOwnership(Table), EWorkbenchOwnership::None);
	TestTrue(TEXT("해제 뒤 깨끗한 패키지는 편집 가능"), WorkbenchDataTableEditAdapter::EvaluateEditability(Table, Reason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchCoreSaveNormalTest, "DevelopmentWorkbench.Core.Save.NormalAndReload", WorkbenchCoreSaveTests::Flags)
bool FWorkbenchCoreSaveNormalTest::RunTest(const FString& Parameters)
{
	using namespace WorkbenchCoreSaveTests;
	if (!GEditor)
	{
		AddInfo(TEXT("GEditor가 없어 적용 트랜잭션을 쓰는 저장 시험을 건너뛴다."));
		return true;
	}

	FTempEnv Env;
	UDataTable* Table = Env.MakeDiskTable(TEXT("CoreSaveNormal"));
	if (!TestNotNull(TEXT("임시 테이블"), Table))
	{
		return false;
	}
	const FName Row = FirstRow(Table);
	const int32 NewPrice = PriceOf(Table, Row) + 13;
	const FString PackageName = Table->GetOutermost()->GetName();
	const FString Filename = Env.FileOf(Table);

	TestTrue(TEXT("적용"), SetPrice(Table, Row, NewPrice).bSuccess);
	const FWorkbenchSaveResult First = WorkbenchDataTableEditAdapter::SaveTablePackage(Table);
	TestTrue(TEXT("저장 성공"), First.bSaved);
	TestFalse(TEXT("작업 ID가 있다"), First.JobId.IsEmpty());
	TestTrue(TEXT("저널 기록 성공"), First.bJournalOk);
	TestTrue(TEXT("패키지 파일이 생겼다"), IFileManager::Get().FileExists(*Filename));
	TestTrue(TEXT("새 패키지라 백업은 없다"), First.BackupPath.IsEmpty());
	FString Journal;
	TestTrue(TEXT("저널 파일을 읽는다"), FFileHelper::LoadFileToString(Journal, *First.JournalPath));
	TestTrue(TEXT("저널에 작업 ID가 남는다"), Journal.Contains(First.JobId));
	TestTrue(TEXT("저널이 임시 저장소 아래에 있다"), First.JournalPath.StartsWith(Env.Storage));

	// 저장 뒤 다시 로드해 디스크 값이 저장한 값인지 확인한다.
	UPackage* Package = Table->GetOutermost();
	const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackageName, *Table->GetName());
	FText UnloadError;
	if (TestTrue(TEXT("패키지 언로드"), UPackageTools::UnloadPackages(TArray<UPackage*>{ Package }, UnloadError)))
	{
		UDataTable* Reloaded = LoadObject<UDataTable>(nullptr, *ObjectPath);
		if (TestNotNull(TEXT("다시 로드한 테이블"), Reloaded))
		{
			Env.Tables.Add(Reloaded);
			TestEqual(TEXT("재로드한 값이 저장한 값"), PriceOf(Reloaded, Row), NewPrice);

			// 두 번째 저장: 기존 파일이 백업되고 작업 ID·백업 폴더가 첫 번째와 다르다.
			WorkbenchDataTableEditAdapter::MarkPackageOwnedByWorkbench(Reloaded);
			TestTrue(TEXT("두 번째 적용"), SetPrice(Reloaded, Row, NewPrice + 1).bSuccess);
			const FWorkbenchSaveResult Second = WorkbenchDataTableEditAdapter::SaveTablePackage(Reloaded);
			TestTrue(TEXT("두 번째 저장 성공"), Second.bSaved);
			TestNotEqual(TEXT("작업 ID는 저장마다 고유"), Second.JobId, First.JobId);
			TestFalse(TEXT("기존 파일이 있으면 백업 경로가 있다"), Second.BackupPath.IsEmpty());
			TestTrue(TEXT("백업 파일이 있다"), IFileManager::Get().FileExists(*Second.BackupPath));
			TestTrue(TEXT("백업이 작업 ID 폴더 아래에 있다"), Second.BackupPath.Contains(Second.JobId));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchCoreSaveGuardTest, "DevelopmentWorkbench.Core.Save.Guards", WorkbenchCoreSaveTests::Flags)
bool FWorkbenchCoreSaveGuardTest::RunTest(const FString& Parameters)
{
	using namespace WorkbenchCoreSaveTests;
	if (!GEditor)
	{
		AddInfo(TEXT("GEditor가 없어 적용 트랜잭션을 쓰는 저장 시험을 건너뛴다."));
		return true;
	}

	IFileManager& Files = IFileManager::Get();

	// 디스크가 편집 시작 뒤 바뀌면 덮어쓰지 않는다.
	{
		FTempEnv Env;
		UDataTable* Table = Env.MakeDiskTable(TEXT("CoreSaveDiskChanged"));
		if (!TestNotNull(TEXT("임시 테이블"), Table) || !SeedSavedFile(*this, Table, FirstRow(Table), PriceOf(Table, FirstRow(Table)) + 1))
		{
			return false;
		}
		const FName Row = FirstRow(Table);
		const FString Filename = Env.FileOf(Table);
		WorkbenchDataTableEditAdapter::MarkPackageOwnedByWorkbench(Table);
		TestTrue(TEXT("재적용"), SetPrice(Table, Row, PriceOf(Table, Row) + 1).bSuccess);
		const FDateTime Forged = Files.GetTimeStamp(*Filename) + FTimespan::FromSeconds(30);
		Files.SetTimeStamp(*Filename, Forged);
		const FWorkbenchSaveResult Result = WorkbenchDataTableEditAdapter::SaveTablePackage(Table);
		TestFalse(TEXT("디스크 변경 시 저장 거부"), Result.bSaved);
		TestFalse(TEXT("거부 사유가 있다"), Result.Message.IsEmpty());
		TestEqual(TEXT("거부된 저장은 파일을 건드리지 않는다"), Files.GetTimeStamp(*Filename), Forged);
		TestTrue(TEXT("메모리 변경은 그대로"), WorkbenchDataTableEditAdapter::GetPendingChangeCount(Table) > 0);
	}

	// 읽기 전용 파일은 덮어쓰지 않는다.
	{
		FTempEnv Env;
		UDataTable* Table = Env.MakeDiskTable(TEXT("CoreSaveReadOnly"));
		if (!TestNotNull(TEXT("임시 테이블"), Table) || !SeedSavedFile(*this, Table, FirstRow(Table), PriceOf(Table, FirstRow(Table)) + 1))
		{
			return false;
		}
		const FName Row = FirstRow(Table);
		const FString Filename = Env.FileOf(Table);
		WorkbenchDataTableEditAdapter::MarkPackageOwnedByWorkbench(Table);
		TestTrue(TEXT("재적용"), SetPrice(Table, Row, PriceOf(Table, Row) + 1).bSuccess);
		Env.ProtectedFiles.Add(Filename);
		FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*Filename, true);
		const FDateTime Before = Files.GetTimeStamp(*Filename);
		const FWorkbenchSaveResult Result = WorkbenchDataTableEditAdapter::SaveTablePackage(Table);
		TestFalse(TEXT("읽기 전용이면 저장 거부"), Result.bSaved);
		TestFalse(TEXT("거부 사유가 있다"), Result.Message.IsEmpty());
		TestEqual(TEXT("읽기 전용 파일은 바뀌지 않는다"), Files.GetTimeStamp(*Filename), Before);
	}

	// 백업을 만들 수 없으면 저장을 중단한다(백업 루트 자리에 일반 파일을 둔다).
	{
		FTempEnv Env;
		UDataTable* Table = Env.MakeDiskTable(TEXT("CoreSaveBackupFail"));
		if (!TestNotNull(TEXT("임시 테이블"), Table) || !SeedSavedFile(*this, Table, FirstRow(Table), PriceOf(Table, FirstRow(Table)) + 1))
		{
			return false;
		}
		const FName Row = FirstRow(Table);
		const FString Filename = Env.FileOf(Table);
		WorkbenchDataTableEditAdapter::MarkPackageOwnedByWorkbench(Table);
		TestTrue(TEXT("재적용"), SetPrice(Table, Row, PriceOf(Table, Row) + 1).bSuccess);
		Files.DeleteDirectory(*WorkbenchDataTableEditAdapter::GetBackupDirectory(), false, true);
		FFileHelper::SaveStringToFile(TEXT("blocker"), *WorkbenchDataTableEditAdapter::GetBackupDirectory());
		const FDateTime Before = Files.GetTimeStamp(*Filename);
		const FWorkbenchSaveResult Result = WorkbenchDataTableEditAdapter::SaveTablePackage(Table);
		TestFalse(TEXT("백업 실패 시 저장 중단"), Result.bSaved);
		TestTrue(TEXT("백업 경로가 비어 있다"), Result.BackupPath.IsEmpty());
		TestEqual(TEXT("디스크 파일은 그대로"), Files.GetTimeStamp(*Filename), Before);
		TestTrue(TEXT("메모리 변경은 그대로"), WorkbenchDataTableEditAdapter::GetPendingChangeCount(Table) > 0);
	}

	// 저널 시작 기록을 쓸 수 없으면 저장을 중단한다(저널 폴더 자리에 일반 파일을 둔다).
	{
		FTempEnv Env;
		UDataTable* Table = Env.MakeDiskTable(TEXT("CoreSaveJournalFail"));
		if (!TestNotNull(TEXT("임시 테이블"), Table))
		{
			return false;
		}
		const FName Row = FirstRow(Table);
		TestTrue(TEXT("적용"), SetPrice(Table, Row, PriceOf(Table, Row) + 1).bSuccess);
		const FString JournalDir = FPaths::GetPath(WorkbenchDataTableEditAdapter::GetJournalPath());
		Files.DeleteDirectory(*JournalDir, false, true);
		FFileHelper::SaveStringToFile(TEXT("blocker"), *JournalDir);
		const FWorkbenchSaveResult Result = WorkbenchDataTableEditAdapter::SaveTablePackage(Table);
		TestFalse(TEXT("저널 기록 실패 시 저장 중단"), Result.bSaved);
		TestFalse(TEXT("저널 실패가 결과에 표시된다"), Result.bJournalOk);
		TestFalse(TEXT("저널 경고 문구가 있다"), Result.JournalWarning.IsEmpty());
		TestFalse(TEXT("패키지 파일이 만들어지지 않았다"), Files.FileExists(*Env.FileOf(Table)));
		TestTrue(TEXT("메모리 변경은 그대로"), WorkbenchDataTableEditAdapter::GetPendingChangeCount(Table) > 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchCoreUndoRedoJournalTest, "DevelopmentWorkbench.Core.Save.UndoRedoJournal", WorkbenchCoreSaveTests::Flags)
bool FWorkbenchCoreUndoRedoJournalTest::RunTest(const FString& Parameters)
{
	using namespace WorkbenchCoreSaveTests;
	if (!GEditor)
	{
		AddInfo(TEXT("GEditor가 없어 Undo·Redo 시험을 건너뛴다."));
		return true;
	}

	FTempEnv Env;
	UDataTable* Table = Env.MakeDiskTable(TEXT("CoreUndoRedoJournal"));
	if (!TestNotNull(TEXT("임시 테이블"), Table))
	{
		return false;
	}
	const FName Row = FirstRow(Table);
	const int32 Original = PriceOf(Table, Row);

	TestTrue(TEXT("첫 적용"), SetPrice(Table, Row, Original + 7).bSuccess);
	TestTrue(TEXT("둘째 적용"), SetPrice(Table, Row, Original + 9).bSuccess);
	TestEqual(TEXT("적용 2건"), WorkbenchDataTableEditAdapter::GetPendingChangeCount(Table), 2);

	GEditor->UndoTransaction();
	TestEqual(TEXT("Undo 뒤 값"), PriceOf(Table, Row), Original + 7);
	TestEqual(TEXT("Undo 뒤 미저장 1건"), WorkbenchDataTableEditAdapter::GetPendingChangeCount(Table), 1);

	GEditor->RedoTransaction();
	TestEqual(TEXT("Redo 뒤 값"), PriceOf(Table, Row), Original + 9);
	TestEqual(TEXT("Redo 뒤 미저장 2건"), WorkbenchDataTableEditAdapter::GetPendingChangeCount(Table), 2);

	const FWorkbenchSaveResult Saved = WorkbenchDataTableEditAdapter::SaveTablePackage(Table);
	TestTrue(TEXT("저장 성공"), Saved.bSaved);
	FString Journal;
	TestTrue(TEXT("저널을 읽는다"), FFileHelper::LoadFileToString(Journal, *Saved.JournalPath));
	TestTrue(TEXT("저널에 첫 변경의 전후값이 남는다"), Journal.Contains(FString::Printf(TEXT("Price: %d -> %d"), Original, Original + 7)));
	TestTrue(TEXT("저널에 Redo된 변경의 전후값이 남는다"), Journal.Contains(FString::Printf(TEXT("Price: %d -> %d"), Original + 7, Original + 9)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchCoreCreatedAssetSaveTest, "DevelopmentWorkbench.Core.Save.CreatedAssetFirstSave", WorkbenchCoreSaveTests::Flags)
bool FWorkbenchCoreCreatedAssetSaveTest::RunTest(const FString& Parameters)
{
	using namespace WorkbenchCoreSaveTests;

	FTempEnv Env;
	UPackage* Package = CreatePackage(TEXT("/WorkbenchTest/CoreCreatedAsset"));
	UDataTable* Table = WorkbenchItemFixture::CreateTableFromRows(Package, TEXT("CoreCreatedAsset"), WorkbenchItemFixture::MakeCleanRows(), RF_Public | RF_Standalone | RF_Transactional);
	if (!TestNotNull(TEXT("새 테이블"), Table))
	{
		return false;
	}
	Env.Tables.Add(Table);

	WorkbenchDataTableEditAdapter::MarkPackageCreatedByWorkbench(Table);
	TestEqual(TEXT("생성 자체가 미저장 변경 1건"), WorkbenchDataTableEditAdapter::GetPendingChangeCount(Table), 1);
	const FWorkbenchSaveResult Saved = WorkbenchDataTableEditAdapter::SaveTablePackage(Table);
	TestTrue(TEXT("별도 편집 없이 최초 저장된다"), Saved.bSaved);
	TestTrue(TEXT("패키지 파일이 생겼다"), IFileManager::Get().FileExists(*Env.FileOf(Table)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkbenchCoreSyntheticOccupiedTest, "DevelopmentWorkbench.Core.Fixture.SyntheticPathOccupied", WorkbenchCoreSaveTests::Flags)
bool FWorkbenchCoreSyntheticOccupiedTest::RunTest(const FString& Parameters)
{
	const FString PackagePath = WorkbenchItemFixture::GetSyntheticAssetPackagePath();
	if (FindPackage(nullptr, *PackagePath) || FPackageName::DoesPackageExist(PackagePath))
	{
		AddInfo(TEXT("합성 에셋 경로가 이미 사용 중이라 점유 시험을 건너뛴다."));
		return true;
	}

	UPackage* Package = CreatePackage(*PackagePath);
	UDataTable* Other = NewObject<UDataTable>(Package, FName(*FPackageName::GetShortName(PackagePath)), RF_Public | RF_Standalone);
	Other->RowStruct = FTableRowBase::StaticStruct();

	FText Error;
	UDataTable* Result = WorkbenchItemFixture::FindOrCreateSyntheticAsset(Error);
	TestNull(TEXT("다른 RowStruct 테이블이 있으면 거부"), Result);
	TestFalse(TEXT("거부 사유가 있다"), Error.IsEmpty());
	TestTrue(TEXT("기존 테이블의 RowStruct를 덮어쓰지 않는다"), Other->RowStruct == FTableRowBase::StaticStruct());
	TestTrue(TEXT("기존 테이블에 행을 채우지 않는다"), Other->GetRowMap().IsEmpty());

	Other->ClearFlags(RF_Public | RF_Standalone);
	Other->MarkAsGarbage();
	Package->SetDirtyFlag(false);
	return true;
}

#endif
