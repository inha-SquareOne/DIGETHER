#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "InputCoreTypes.h"
#include "DigetherKeyBindingRowWidget.generated.h"

class UCommonTextBlock;
class UDigetherButtonWidget;

DECLARE_DELEGATE_OneParam(FDigetherKeyRowClicked, FName /*MappingName*/);

// 키 재설정 한 줄: 동작 이름 + 현재 키 버튼, 누르면 설정 패널이 새 키 입력 대기
UCLASS(Abstract)
class DIGETHER_API UDigetherKeyBindingRowWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void Setup(FName InMappingName, const FText& InDisplayName, const FKey& InKey);
	void SetKey(const FKey& InKey);
	void SetListening(bool bListening);
	FName GetMappingName() const { return MappingName; }
	UDigetherButtonWidget* GetKeyButton() const { return Btn_Key; }

	FDigetherKeyRowClicked OnClicked;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_Action;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_Key;

private:
	void HandleClicked();

	FName MappingName;
	FKey Key;
};
