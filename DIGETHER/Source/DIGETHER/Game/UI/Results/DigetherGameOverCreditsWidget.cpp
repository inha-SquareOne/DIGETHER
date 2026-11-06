#include "Game/UI/Results/DigetherGameOverCreditsWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Run/DigetherGameOverPublicStateComponent.h"

#define LOCTEXT_NAMESPACE "DigetherGameOverCredits"

namespace
{
	FText MakePlayerName(const FDigetherGameResultPlayer& Player)
	{
		return Player.DisplayName.IsEmpty() ? LOCTEXT("UnknownPlayer", "이름 없음") : FText::FromString(Player.DisplayName);
	}

	template <typename TValue>
	FText MakeBlockText(const FText& Title, const FDigetherGameResult& Result, TValue SelectAmount)
	{
		TArray<FText> Lines;
		Lines.Add(Title);
		for (const FDigetherGameResultPlayer& Player : Result.Players)
		{
			// 참가 순서 유지·순위 없음, 0원도 표시(S33 236행)
			Lines.Add(FText::Format(LOCTEXT("PlayerAmount", "{0}: {1}"), MakePlayerName(Player),
				FText::AsNumber(SelectAmount(Player))));
		}
		return FText::Join(FText::FromString(TEXT("\n")), Lines);
	}
}

FText UDigetherGameOverCreditsWidget::MakePeriodText(const FDigetherGameResult& Result)
{
	return FText::Format(LOCTEXT("Period", "합산 기간: 1일차 ~ {0}일차"), FText::AsNumber(FMath::Max(1, Result.FinalTotalDay)));
}

FText UDigetherGameOverCreditsWidget::MakeTotalEarnedText(const FDigetherGameResult& Result)
{
	return FText::Format(LOCTEXT("TotalEarned", "플레이어 전체 획득 총액: {0}"), FText::AsNumber(Result.TotalEarnedCredits));
}

FText UDigetherGameOverCreditsWidget::MakeEarnedBlockText(const FDigetherGameResult& Result)
{
	return MakeBlockText(LOCTEXT("EarnedTitle", "획득 금액"), Result,
		[](const FDigetherGameResultPlayer& Player) { return Player.EarnedCredits; });
}

FText UDigetherGameOverCreditsWidget::MakeHiddenBlockText(const FDigetherGameResult& Result)
{
	return MakeBlockText(LOCTEXT("HiddenTitle", "히든 슬롯으로 얻은 금액"), Result,
		[](const FDigetherGameResultPlayer& Player) { return Player.HiddenCredits; });
}

FText UDigetherGameOverCreditsWidget::MakeDeathLossBlockText(const FDigetherGameResult& Result)
{
	return MakeBlockText(LOCTEXT("DeathLossTitle", "사망으로 잃은 금액"), Result,
		[](const FDigetherGameResultPlayer& Player) { return Player.DeathLossCredits; });
}

void UDigetherGameOverCreditsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_FastForward)
	{
		Button_FastForward->OnClicked.AddDynamic(this, &ThisClass::HandleFastForwardClicked);
	}
}

void UDigetherGameOverCreditsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// 접힌 위젯은 Tick하지 않으므로 단계 알림으로 표시, 늦은 GameState도 재연결
	bShown = false;
	SetVisibility(ESlateVisibility::Collapsed);
	if (UWorld* World = GetWorld())
	{
		BoundWorld = World;
		GameStateSetHandle = World->GameStateSetEvent.AddUObject(this, &ThisClass::HandleGameStateSet);
		BindPublicState(World->GetGameState());
	}
}

void UDigetherGameOverCreditsWidget::NativeDestruct()
{
	if (UWorld* World = BoundWorld.Get())
	{
		World->GameStateSetEvent.Remove(GameStateSetHandle);
	}
	BoundWorld.Reset();
	GameStateSetHandle.Reset();
	UnbindPublicState();
	Super::NativeDestruct();
}

void UDigetherGameOverCreditsWidget::HandleGameStateSet(AGameStateBase* GameState)
{
	BindPublicState(GameState);
}

void UDigetherGameOverCreditsWidget::BindPublicState(AGameStateBase* GameState)
{
	const ADigetherGameplayGameState* State = Cast<ADigetherGameplayGameState>(GameState);
	UDigetherGameOverPublicStateComponent* Public = IsValid(State) ? State->GetGameOverPublicStateComponent() : nullptr;
	if (BoundPublicState.Get() != Public)
	{
		UnbindPublicState();
		BoundPublicState = Public;
		if (Public)
		{
			Public->OnGameOverPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandlePhaseChanged);
		}
	}
	// 위젯 재사용·크레딧 중 도착에도 현재 공개 스냅샷부터 반영
	HandlePhaseChanged(Public ? Public->GetPhase() : EDigetherGameOverPhase::None);
}

void UDigetherGameOverCreditsWidget::UnbindPublicState()
{
	if (UDigetherGameOverPublicStateComponent* Public = BoundPublicState.Get())
	{
		Public->OnGameOverPhaseChanged.RemoveDynamic(this, &ThisClass::HandlePhaseChanged);
	}
	BoundPublicState.Reset();
}

void UDigetherGameOverCreditsWidget::HandlePhaseChanged(const EDigetherGameOverPhase Phase)
{
	const UDigetherGameOverPublicStateComponent* Public = BoundPublicState.Get();
	if (Phase == EDigetherGameOverPhase::Credits && IsValid(Public))
	{
		Show(Public->GetPublishedResult());
	}
	else if (bShown)
	{
		Hide();
	}
}

void UDigetherGameOverCreditsWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const UDigetherGameOverPublicStateComponent* Public = ResolvePublicState();
	const bool bCredits = IsValid(Public) && Public->GetPhase() == EDigetherGameOverPhase::Credits;
	if (!bCredits)
	{
		if (bShown)
		{
			Hide();
		}
		return;
	}
	const FDigetherGameResult Result = Public->GetPublishedResult();
	if (!bShown || Result.ResultId != ShownResult.ResultId)
	{
		Show(Result);
	}
	const float ScrollOffset = Public->GetCreditScrollOffset();
	if (Panel_Scroll)
	{
		// 기본 평면 스크롤, 화면 아래(1)에서 위로 이동, 원근은 BP가 덧입힘
		const float Height = MyGeometry.GetLocalSize().Y;
		Panel_Scroll->SetRenderTranslation(FVector2D(0.0, (1.0 - ScrollOffset) * Height));
	}
	// 같은 프레임의 Public 재사용, 월드·GameState 재조회 생략
	RefreshFastForwardLabel(Public->IsCreditFastForward());
	BP_OnCreditScroll(ScrollOffset, Public->GetCreditProgressSeconds(), Public->GetCreditTotalSeconds());
}

UDigetherGameOverPublicStateComponent* UDigetherGameOverCreditsWidget::ResolvePublicState() const
{
	const UWorld* World = GetWorld();
	const ADigetherGameplayGameState* State = World != nullptr ? World->GetGameState<ADigetherGameplayGameState>() : nullptr;
	return IsValid(State) ? State->GetGameOverPublicStateComponent() : nullptr;
}

void UDigetherGameOverCreditsWidget::Show(const FDigetherGameResult& Result)
{
	ShownResult = Result;
	bShown = true;
	if (Text_Period)
	{
		Text_Period->SetText(MakePeriodText(Result));
	}
	if (Text_TotalEarned)
	{
		Text_TotalEarned->SetText(MakeTotalEarnedText(Result));
	}
	if (Text_EarnedBlock)
	{
		Text_EarnedBlock->SetText(MakeEarnedBlockText(Result));
	}
	if (Text_HiddenBlock)
	{
		Text_HiddenBlock->SetText(MakeHiddenBlockText(Result));
	}
	if (Text_DeathLossBlock)
	{
		Text_DeathLossBlock->SetText(MakeDeathLossBlockText(Result));
	}
	if (Button_FastForward)
	{
		Button_FastForward->SetVisibility(IsLocalHost() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BP_OnCreditsShown(Result);
}

void UDigetherGameOverCreditsWidget::Hide()
{
	bShown = false;
	ShownResult = FDigetherGameResult();
	SetVisibility(ESlateVisibility::Collapsed);
	BP_OnCreditsHidden();
}

void UDigetherGameOverCreditsWidget::RefreshFastForwardLabel(const bool bFastForward)
{
	if (Text_FastForward)
	{
		Text_FastForward->SetText(bFastForward ? LOCTEXT("FastForwardOn", "빨리감기 중") : LOCTEXT("FastForwardOff", "빨리감기"));
	}
}

float UDigetherGameOverCreditsWidget::GetScrollOffset() const
{
	const UDigetherGameOverPublicStateComponent* Public = ResolvePublicState();
	return IsValid(Public) ? Public->GetCreditScrollOffset() : 0.f;
}

float UDigetherGameOverCreditsWidget::GetTiltDegrees() const
{
	const UDigetherGameOverPublicStateComponent* Public = ResolvePublicState();
	return IsValid(Public) ? Public->GetPublicState().Tuning.CreditTiltDegrees : 0.f;
}

bool UDigetherGameOverCreditsWidget::IsFastForward() const
{
	const UDigetherGameOverPublicStateComponent* Public = ResolvePublicState();
	return IsValid(Public) && Public->IsCreditFastForward();
}

bool UDigetherGameOverCreditsWidget::IsLocalHost() const
{
	const APlayerController* Controller = GetOwningPlayer();
	return IsValid(Controller) && Controller->IsLocalController() && Controller->GetNetMode() != NM_Client;
}

void UDigetherGameOverCreditsWidget::ToggleFastForward()
{
	if (ADigetherGameplayPlayerController* Controller = Cast<ADigetherGameplayPlayerController>(GetOwningPlayer()))
	{
		Controller->RequestCreditFastForward(!IsFastForward());
	}
}

void UDigetherGameOverCreditsWidget::HandleFastForwardClicked()
{
	ToggleFastForward();
}

#undef LOCTEXT_NAMESPACE
