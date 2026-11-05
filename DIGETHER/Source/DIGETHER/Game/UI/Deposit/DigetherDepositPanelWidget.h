#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DigetherDepositPanelWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

// 폐기된 금액 입력 방식의 위젯 참조 호환용
UCLASS()
class DIGETHER_API UDigetherDepositPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetStatus(const FText& Message);
	void SetCanSubmit(bool bEnabled);
	void FocusAmountInput();

protected:
	virtual void NativeOnInitialized() override;

private:
	// 구형 WBP 호환을 위한 선택 바인딩
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEditableTextBox> Editable_Amount;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_Submit;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_Cancel;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Status;

	UFUNCTION()
	void HandleSubmitClicked();

	UFUNCTION()
	void HandleCancelClicked();
};
