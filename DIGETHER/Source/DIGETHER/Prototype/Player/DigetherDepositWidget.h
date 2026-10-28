#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DigetherDepositWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;
class ADigetherPrototypePlayerController;

// 납부 금액 입력창. 루트가 비어 있으면 코드가 기본 트리를 만들고, BP 파생은 같은 이름의 위젯으로 대체할 수 있다.
// 입력 해석·제출은 소유 PlayerController가 맡고 이 위젯은 표시와 키 전달만 한다.
UCLASS(Blueprintable)
class DIGETHER_API UDigetherDepositWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetDepositInfo(int32 Balance, int32 Remaining);
	void SetInputText(const FString& Text);
	FString GetInputText() const;
	void SetErrorMessage(const FString& Message);
	void SetSubmitting(bool bSubmitting);
	TSharedPtr<SWidget> GetInputSlateWidget();
	void FocusInput();

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintReadOnly, Category = "Prototype|Deposit", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY(BlueprintReadOnly, Category = "Prototype|Deposit", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BalanceText;

	UPROPERTY(BlueprintReadOnly, Category = "Prototype|Deposit", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RemainingText;

	UPROPERTY(BlueprintReadOnly, Category = "Prototype|Deposit", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ErrorText;

	UPROPERTY(BlueprintReadOnly, Category = "Prototype|Deposit", meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> AmountInput;

	UPROPERTY(BlueprintReadOnly, Category = "Prototype|Deposit", meta = (BindWidgetOptional))
	TObjectPtr<UButton> ConfirmButton;

	UPROPERTY(BlueprintReadOnly, Category = "Prototype|Deposit", meta = (BindWidgetOptional))
	TObjectPtr<UButton> CancelButton;

private:
	UFUNCTION()
	void HandleTextChanged(const FText& Text);

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

	void BuildDefaultTree();
	ADigetherPrototypePlayerController* GetDepositController() const;
};
