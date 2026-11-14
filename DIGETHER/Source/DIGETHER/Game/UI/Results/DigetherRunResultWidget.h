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

	// 사망자는 차감액·시체 회수 여부·적용 비율, 귀환자는 0
	static FText MakeDeathLossText(const FDigetherRunResult& Result);
	// 이름 목록 표시, 빈 목록은 '없음'
	static FText JoinPlayerNames(const TArray<FString>& Names);

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

	// 화면 위쪽 탐사 공통 값(런 플로우 9.5), WBP에 같은 이름 텍스트를 추가해야 표시
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_FarmingCredits;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_DeadPlayers;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MissingPlayers;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ExcludedCredits;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_PreviousBalance;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_GeneralCredits;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_HiddenCredits;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_DeathLoss;

	// 사용 중단(시체 복구금 폐지): 기존 WBP 배치 호환용, 항상 접어서 숨김, WBP에서 제거 후 삭제 예정
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
