#include "Game/UI/Deposit/DigetherDepositPanelWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"

void UDigetherDepositPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// 재구성 중복 구독 방지용 최초 1회 바인딩
	if (Button_Submit)
	{
		Button_Submit->OnClicked.AddDynamic(this, &ThisClass::HandleSubmitClicked);
	}
	if (Button_Cancel)
	{
		Button_Cancel->OnClicked.AddDynamic(this, &ThisClass::HandleCancelClicked);
	}
}

void UDigetherDepositPanelWidget::HandleSubmitClicked()
{
	// 미사용 금액 입력 API의 호환 호출
	if (ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(GetOwningPlayer()))
	{
		Controller->SubmitDepositInput(Editable_Amount ? Editable_Amount->GetText().ToString() : FString());
	}
}

void UDigetherDepositPanelWidget::HandleCancelClicked()
{
	if (ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(GetOwningPlayer()))
	{
		Controller->CloseDepositInteraction();
	}
}
