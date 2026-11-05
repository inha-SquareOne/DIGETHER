#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DigetherRunHudWidget.generated.h"

class UTextBlock;

// 공개 런·목표 상태와 본인 지갑 표시
UCLASS(Abstract)
class DIGETHER_API UDigetherRunHudWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 화면별 생략 가능한 선택 바인딩
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Day;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Quota;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Balance;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RunPhase;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Interaction;

private:
	void RefreshText();

	FTimerHandle RefreshTimer;
};
