#include "Game/UI/Results/DigetherRunResultWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"

#define LOCTEXT_NAMESPACE "DigetherRunResult"

void UDigetherRunResultWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// 재표시 중복 구독 방지용 최초 1회 바인딩
	if (Button_Close)
	{
		Button_Close->OnClicked.AddDynamic(this, &ThisClass::HandleCloseClicked);
	}
}

void UDigetherRunResultWidget::SetResult(const FDigetherRunResult& Result)
{
	RunResult = Result;
	if (Text_Title)
	{
		Text_Title->SetText(FText::Format(LOCTEXT("Title", "{0}일차 탐사 결과"), FText::AsNumber(Result.CompletedDay)));
	}
	if (Text_Outcome)
	{
		FText Outcome = LOCTEXT("Returned", "귀환 완료");
		if (Result.Outcome == EDigetherRunReturnOutcome::Died)
		{
			Outcome = LOCTEXT("Died", "사망 후 거점에서 부활했습니다.");
		}
		else if (Result.Outcome == EDigetherRunReturnOutcome::Missing)
		{
			Outcome = LOCTEXT("Missing", "실종 처리 후 거점에서 부활했습니다.");
		}
		Text_Outcome->SetText(Outcome);
	}
	if (Text_GeneralCredits)
	{
		Text_GeneralCredits->SetText(FText::Format(LOCTEXT("General", "일반 몫  {0}"), FText::AsNumber(Result.GeneralCredits)));
	}
	if (Text_HiddenCredits)
	{
		Text_HiddenCredits->SetText(FText::Format(LOCTEXT("Hidden", "히든 수익  {0}"), FText::AsNumber(Result.HiddenCredits)));
	}
	if (Text_DeathLoss)
	{
		Text_DeathLoss->SetText(FText::Format(LOCTEXT("DeathLoss", "사망 손실  -{0}"), FText::AsNumber(Result.DeathLossCredits)));
	}
	if (Text_CorpseRefund)
	{
		Text_CorpseRefund->SetText(FText::Format(LOCTEXT("CorpseRefund", "시체 복구  {0}"), FText::AsNumber(Result.CorpseRefundCredits)));
	}
	if (Text_Payout)
	{
		Text_Payout->SetText(FText::Format(LOCTEXT("Payout", "탐사 지급액  {0}"), FText::AsNumber(Result.PayoutCredits)));
	}
	if (Text_FinalBalance)
	{
		Text_FinalBalance->SetText(FText::Format(LOCTEXT("FinalBalance", "최종 잔고  {0}"), FText::AsNumber(Result.FinalBalanceCredits)));
	}
}

void UDigetherRunResultWidget::FocusCloseButton()
{
	if (Button_Close)
	{
		Button_Close->SetKeyboardFocus();
	}
}

void UDigetherRunResultWidget::HandleCloseClicked()
{
	// 화면만 닫고 정산·탐사 상태 유지
	if (ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(GetOwningPlayer()))
	{
		Controller->CloseRunResult();
	}
}

#undef LOCTEXT_NAMESPACE
