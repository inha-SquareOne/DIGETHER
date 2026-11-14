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
	if (Text_FarmingCredits)
	{
		Text_FarmingCredits->SetText(FText::Format(LOCTEXT("Farming", "이번 탐사 파밍액  {0}"), FText::AsNumber(Result.FarmingCredits)));
	}
	if (Text_DeadPlayers)
	{
		Text_DeadPlayers->SetText(FText::Format(LOCTEXT("DeadPlayers", "사망자  {0}"), JoinPlayerNames(Result.DeadPlayerNames)));
	}
	if (Text_MissingPlayers)
	{
		Text_MissingPlayers->SetText(FText::Format(LOCTEXT("MissingPlayers", "실종자  {0}"), JoinPlayerNames(Result.MissingPlayerNames)));
	}
	if (Text_ExcludedCredits)
	{
		Text_ExcludedCredits->SetText(FText::Format(LOCTEXT("Excluded", "제외된 금액  {0}"), FText::AsNumber(Result.ExcludedCredits)));
	}
	if (Text_PreviousBalance)
	{
		Text_PreviousBalance->SetText(FText::Format(LOCTEXT("PreviousBalance", "이전 잔고  {0}"), FText::AsNumber(Result.PreviousBalanceCredits)));
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
		Text_DeathLoss->SetText(MakeDeathLossText(Result));
	}
	if (Text_CorpseRefund)
	{
		// 시체 복구금 폐지, 기존 WBP 배치에 남은 칸은 빈 문구로 접음
		Text_CorpseRefund->SetText(FText::GetEmpty());
		Text_CorpseRefund->SetVisibility(ESlateVisibility::Collapsed);
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

FText UDigetherRunResultWidget::MakeDeathLossText(const FDigetherRunResult& Result)
{
	if (Result.Outcome == EDigetherRunReturnOutcome::Returned)
	{
		return FText::Format(LOCTEXT("DeathLossNone", "사망 손실  {0}"), FText::AsNumber(0));
	}
	const FText Recovery = Result.bCorpseRecovered
		? LOCTEXT("CorpseRecovered", "시체 회수") : LOCTEXT("CorpseNotRecovered", "시체 미회수");
	return FText::Format(LOCTEXT("DeathLoss", "사망 손실  -{0} ({1}, {2}%)"),
		FText::AsNumber(Result.DeathLossCredits), Recovery, FText::AsNumber(Result.DeathLossPercent));
}

FText UDigetherRunResultWidget::JoinPlayerNames(const TArray<FString>& Names)
{
	if (Names.IsEmpty())
	{
		return LOCTEXT("NoPlayers", "없음");
	}
	// 플레이어 표시 이름은 번역 대상이 아닌 고유 문자열
	return FText::FromString(FString::Join(Names, TEXT(", ")));
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
