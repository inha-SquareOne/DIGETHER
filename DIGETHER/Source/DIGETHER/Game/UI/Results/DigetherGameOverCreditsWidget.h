#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/Run/DigetherGameOverTypes.h"
#include "DigetherGameOverCreditsWidget.generated.h"

class UButton;
class UTextBlock;
class UWidget;
class UDigetherGameOverPublicStateComponent;

// 기획 S33 함께 보는 결과 크레딧 화면의 C++ 기반이다. 공개 상태의 크레딧 단계에서만 스스로 보이고,
// 진행은 모두 같은 서버 시각 기준점으로 계산한다. 원근(뒤로 기울어진 평면) 표현·배경·글꼴·음악은 BP가 맡는다.
// 건너뛰기·일시정지·다시 보기는 없다. 빨리감기 버튼은 호스트에게만 보이며 서버가 다시 확인한다.
UCLASS()
class DIGETHER_API UDigetherGameOverCreditsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 첫 블록: 합산 기간(이번 게임 범위)이다.
	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	static FText MakePeriodText(const FDigetherGameResult& Result);

	// 첫 블록: 플레이어 전체 획득 총액이다.
	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	static FText MakeTotalEarnedText(const FDigetherGameResult& Result);

	// 항목 블록(획득 금액 → 히든 슬롯으로 얻은 금액 → 사망으로 잃은 금액)이다. 참가 순서대로, 0원도 생략하지 않는다.
	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	static FText MakeEarnedBlockText(const FDigetherGameResult& Result);

	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	static FText MakeHiddenBlockText(const FDigetherGameResult& Result);

	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	static FText MakeDeathLossBlockText(const FDigetherGameResult& Result);

	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	FDigetherGameResult GetResult() const { return ShownResult; }

	// 스크롤 시작 뒤 올라간 거리(화면 높이 단위)와 원근 기울기(도)다. BP 원근 표현에 쓴다.
	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	float GetScrollOffset() const;

	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	float GetTiltDegrees() const;

	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	bool IsFastForward() const;

	// 이 화면의 주인이 방의 호스트(리슨 서버 로컬 플레이어)인지다. 빨리감기 버튼 표시에만 쓴다.
	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	bool IsLocalHost() const;

	UFUNCTION(BlueprintCallable, Category="Digether|Game Over")
	void ToggleFastForward();

	UFUNCTION(BlueprintImplementableEvent, Category="Digether|Game Over", meta=(DisplayName="On Credits Shown"))
	void BP_OnCreditsShown(const FDigetherGameResult& Result);

	// 매 프레임 같은 서버 시각 기준의 진행을 알린다. 원근 스크롤은 이 값으로 그린다.
	UFUNCTION(BlueprintImplementableEvent, Category="Digether|Game Over", meta=(DisplayName="On Credit Scroll"))
	void BP_OnCreditScroll(float ScrollOffset, float ProgressSeconds, float TotalSeconds);

	UFUNCTION(BlueprintImplementableEvent, Category="Digether|Game Over", meta=(DisplayName="On Credits Hidden"))
	void BP_OnCreditsHidden();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 화면별 생략 가능한 선택 바인딩이다.
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Period;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_TotalEarned;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_EarnedBlock;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_HiddenBlock;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_DeathLossBlock;

	// 있으면 기본 평면 스크롤(아래에서 위로)을 적용한다. 원근 표현은 BP가 이 위젯이나 별도 머티리얼로 덮어쓴다.
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Scroll;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_FastForward;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_FastForward;

private:
	UDigetherGameOverPublicStateComponent* ResolvePublicState() const;
	void Show(const FDigetherGameResult& Result);
	void Hide();
	void RefreshFastForwardLabel();

	UFUNCTION()
	void HandleFastForwardClicked();

	UPROPERTY(Transient)
	FDigetherGameResult ShownResult;

	bool bShown = false;
};
