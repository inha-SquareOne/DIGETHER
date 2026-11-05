#include "DigetherStringTableEditorLibrary.h"

#include "HAL/FileManager.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"

DEFINE_LOG_CATEGORY_STATIC(LogDigetherStringTableEditor, Log, All);

bool UDigetherStringTableEditorLibrary::ImportStringTableFromCsvFile(UStringTable* StringTable, const FString& CsvFilePath)
{
	if (!IsValid(StringTable))
	{
		UE_LOG(LogDigetherStringTableEditor, Error, TEXT("String Table이 없어 가져오기를 중단했다."));
		return false;
	}
	if (!IFileManager::Get().FileExists(*CsvFilePath))
	{
		UE_LOG(LogDigetherStringTableEditor, Error, TEXT("CSV 파일이 없다: %s"), *CsvFilePath);
		return false;
	}

	// 실행 취소 기록과 패키지 변경 표시를 가져오기 전에 남긴다.
	StringTable->Modify();
	if (!StringTable->GetMutableStringTable()->ImportStringsFromCSVFile(CsvFilePath))
	{
		UE_LOG(LogDigetherStringTableEditor, Error, TEXT("CSV 가져오기에 실패했다: %s -> %s"), *CsvFilePath,
			*StringTable->GetPathName());
		return false;
	}
	StringTable->MarkPackageDirty();
	UE_LOG(LogDigetherStringTableEditor, Display, TEXT("CSV를 가져왔다: %s -> %s"), *CsvFilePath,
		*StringTable->GetPathName());
	return true;
}
