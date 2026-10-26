// 엔진 DataValidation에 거는 네이티브 검증기. UEditorValidatorSubsystem이 네이티브 서브클래스를 찾아 등록하므로
// 이 모듈에서 수동 등록하지 않는다. UI·자동화 테스트와 같은 WorkbenchItemValidation 함수를 호출한다.
// 합성 시험 에셋(의도적 문제 행 포함)은 검증 대상에서 제외한다.
#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "WorkbenchItemRowValidator.generated.h"

UCLASS()
class WORKBENCHEDITOR_API UWorkbenchItemRowValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	virtual bool CanValidateAsset_Implementation(
		const FAssetData& InAssetData,
		UObject* InAsset,
		FDataValidationContext& InContext) const override;

	virtual EDataValidationResult ValidateLoadedAsset_Implementation(
		const FAssetData& InAssetData,
		UObject* InAsset,
		FDataValidationContext& InContext) override;
};
