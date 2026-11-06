#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/Run/DigetherGameOverTypes.h"
#include "DigetherGameOverCreditsWidget.generated.h"

class UButton;
class UTextBlock;
class UWidget;
class UDigetherGameOverPublicStateComponent;
class AGameStateBase;

// 기획 S33 함께 보는 결과 크레딧 화면 C++ 기반, 공개 상태 Credits 단계에서만 자체 표시
// 진행은 모두 같은 서버 시각 기준점, 원근(뒤로 기울어진 평면)·배경·글꼴·음악은 BP 담당
// 건너뛰기·일시정지·다시 보기 없음, 빨리감기 버튼은 호스트 전용·서버 재확인
UCLASS()
class DIGETHER_API UDigetherGameOverCreditsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 첫 블록: 합산 기간(이번 게임 범위)
	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	static FText MakePeriodText(const FDigetherGameResult& Result);

	// 첫 블록: 플레이어 전체 획득 총액
	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	static FText MakeTotalEarnedText(const FDigetherGameResult& Result);

	// 항목 블록(획득 금액 → 히든 슬롯으로 얻은 금액 → 사망으로 잃은 금액), 참가 순서·0원 포함
	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	static FText MakeEarnedBlockText(const FDigetherGameResult& Result);

	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	static FText MakeHiddenBlockText(const FDigetherGameResult& Result);

	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	static FText MakeDeathLossBlockText(const FDigetherGameResult& Result);

	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	FDigetherGameResult GetResult() const { return ShownResult; }

	// 스크롤 시작 뒤 이동 거리(화면 높이 단위)·원근 기울기(도), BP 원근 표현용
	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	float GetScrollOffset() const;

	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	float GetTiltDegrees() const;

	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	bool IsFastForward() const;

	// 화면 주인이 방 호스트(리슨 서버 로컬 플레이어)인지, 빨리감기 버튼 표시 전용
	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	bool IsLocalHost() const;

	UFUNCTION(BlueprintCallable, Category="Digether|Game Over")
	void ToggleFastForward();

	UFUNCTION(BlueprintImplementableEvent, Category="Digether|Game Over", meta=(DisplayName="On Credits Shown"))
	void BP_OnCreditsShown(const FDigetherGameResult& Result);

	// 매 프레임 같은 서버 시각 기준 진행 통지, 원근 스크롤 입력값
	UFUNCTION(BlueprintImplementableEvent, Category="Digether|Game Over", meta=(DisplayName="On Credit Scroll"))
	void BP_OnCreditScroll(float ScrollOffset, float ProgressSeconds, float TotalSeconds);

	UFUNCTION(BlueprintImplementableEvent, Category="Digether|Game Over", meta=(DisplayName="On Credits Hidden"))
	void BP_OnCreditsHidden();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 화면별 생략 가능한 선택 바인딩
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

	// 있으면 기본 평면 스크롤(아래→위) 적용, 원근 표현은 BP가 이 위젯 또는 별도 머티리얼로 덮어씀
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Scroll;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_FastForward;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_FastForward;

private:
	void HandleGameStateSet(AGameStateBase* GameState);
	void BindPublicState(AGameStateBase* GameState);
	void UnbindPublicState();
	UFUNCTION()
	void HandlePhaseChanged(EDigetherGameOverPhase Phase);

	UDigetherGameOverPublicStateComponent* ResolvePublicState() const;
	void Show(const FDigetherGameResult& Result);
	void Hide();
	void RefreshFastForwardLabel(bool bFastForward);

	UFUNCTION()
	void HandleFastForwardClicked();

	UPROPERTY(Transient)
	FDigetherGameResult ShownResult;

	TWeakObjectPtr<UWorld> BoundWorld;
	FDelegateHandle GameStateSetHandle;
	TWeakObjectPtr<UDigetherGameOverPublicStateComponent> BoundPublicState;
	bool bShown = false;
};
