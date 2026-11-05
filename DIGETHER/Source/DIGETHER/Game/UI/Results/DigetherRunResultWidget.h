#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/Run/DigetherRunResult.h"
#include "DigetherRunResultWidget.generated.h"

class UButton;
class UTextBlock;

// 탐사 상태와 분리된 본인 정산 스냅샷 표시
UCLASS()
class DIGETHER_API UDigetherRunResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetResult(const FDigetherRunResult& Result);
	void FocusCloseButton();

	UFUNCTION(BlueprintPure, Category="Digether|Run Result")
	FDigetherRunResult GetResult() const { return RunResult; }

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY(Transient)
	FDigetherRunResult RunResult;

	// 정산 항목별 생략 가능한 선택 바인딩
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Title;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Outcome;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_GeneralCredits;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_HiddenCredits;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_DeathLoss;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CorpseRefund;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Payout;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_FinalBalance;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_Close;

	UFUNCTION()
	void HandleCloseClicked();
};
