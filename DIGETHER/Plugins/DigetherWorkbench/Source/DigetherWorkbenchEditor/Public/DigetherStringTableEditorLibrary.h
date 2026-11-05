#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DigetherStringTableEditorLibrary.generated.h"

class UStringTable;

// 에디터 Python에서 String Table 원문을 CSV로 다시 가져온다. 엔진의 CSV 가져오기는 C++에만 있어 이 함수로 노출한다.
// 가져오기는 기존 항목을 CSV 내용으로 바꾼다. CSV에는 기존 키를 모두 포함해야 한다(현지화 시트 절차의 table 출력).
UCLASS()
class DIGETHERWORKBENCHEDITOR_API UDigetherStringTableEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 성공하면 에셋을 변경 상태로 표시한다. 저장은 호출한 쪽이 한다(예: unreal.EditorAssetLibrary.save_loaded_asset).
	UFUNCTION(BlueprintCallable, Category = "DIGETHER|Localization")
	static bool ImportStringTableFromCsvFile(UStringTable* StringTable, const FString& CsvFilePath);
};
