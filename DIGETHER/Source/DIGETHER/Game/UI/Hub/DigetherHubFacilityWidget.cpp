#include "Game/UI/Hub/DigetherHubFacilityWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"

void UDigetherHubFacilityWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	bIsBackHandler = true;
	// 재활성화 중복 구독 방지용 최초 1회 바인딩
	Button_Option1->OnClicked.AddDynamic(this, &ThisClass::HandleOption1);
	Button_Option2->OnClicked.AddDynamic(this, &ThisClass::HandleOption2);
	Button_Option3->OnClicked.AddDynamic(this, &ThisClass::HandleOption3);
	Button_Close->OnClicked.AddDynamic(this, &ThisClass::HandleClose);
}

void UDigetherHubFacilityWidget::SetView(const FDigetherHubView& View)
{
	Text_Title->SetText(View.Title);
	Text_Status->SetText(View.Message);
	Text_Balance->SetText(FText::Format(NSLOCTEXT("DigetherHub", "Balance", "내 잔고: {0}"), FText::AsNumber(View.Balance)));
	Text_Detail->SetText(View.Detail);
	UButton* Buttons[] = {Button_Option1, Button_Option2, Button_Option3};
	UTextBlock* Labels[] = {Text_Option1, Text_Option2, Text_Option3};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const bool bPresent = View.Options.IsValidIndex(Index);
		Buttons[Index]->SetVisibility(bPresent ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Buttons[Index]->SetIsEnabled(bPresent && View.Options[Index].bEnabled);
		Labels[Index]->SetText(bPresent ? View.Options[Index].Label : FText::GetEmpty());
		Buttons[Index]->SetToolTipText(bPresent ? View.Options[Index].Detail : FText::GetEmpty());
	}
}

void UDigetherHubFacilityWidget::SetPending()
{
	// 거래 응답 전 추가 선택 차단
	Text_Status->SetText(NSLOCTEXT("DigetherHub", "Pending", "처리 중입니다."));
	Button_Option1->SetIsEnabled(false);
	Button_Option2->SetIsEnabled(false);
	Button_Option3->SetIsEnabled(false);
}

void UDigetherHubFacilityWidget::FocusDefaultButton()
{
	Button_Close->SetKeyboardFocus();
}

void UDigetherHubFacilityWidget::Submit(const int32 Index)
{
	// 옵션 유효성 확인과 서버 요청은 소유 Controller 담당
	if (ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(GetOwningPlayer()))
	{
		Controller->SubmitHubOption(Index);
	}
}

void UDigetherHubFacilityWidget::HandleOption1() { Submit(0); }
void UDigetherHubFacilityWidget::HandleOption2() { Submit(1); }
void UDigetherHubFacilityWidget::HandleOption3() { Submit(2); }

void UDigetherHubFacilityWidget::HandleClose()
{
	if (ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(GetOwningPlayer()))
	{
		Controller->CloseHubInteraction();
	}
}

bool UDigetherHubFacilityWidget::NativeOnHandleBackAction()
{
	HandleClose();
	return true;
}
