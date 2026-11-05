#include "DigetherDepositWidget.h"

#include "DigetherPrototypePlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"

namespace
{
	UTextBlock* MakeLabel(UWidgetTree* Tree, const FName Name, const FString& Text, int32 FontSize, const FLinearColor& Color)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Label->SetText(FText::FromString(Text));
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = FontSize;
		Label->SetFont(Font);
		Label->SetColorAndOpacity(FSlateColor(Color));
		Label->SetAutoWrapText(true);
		return Label;
	}

	UButton* MakeButton(UWidgetTree* Tree, const FName Name, const FString& Caption)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetBackgroundColor(FLinearColor(0.12f, 0.09f, 0.04f, 1.f));
		UTextBlock* CaptionText = MakeLabel(Tree, FName(*(Name.ToString() + TEXT("Label"))), Caption, 16, FLinearColor::White);
		CaptionText->SetJustification(ETextJustify::Center);
		Button->AddChild(CaptionText);
		return Button;
	}
}

void UDigetherDepositWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultTree();
	}
	if (AmountInput) { AmountInput->OnTextChanged.AddDynamic(this, &UDigetherDepositWidget::HandleTextChanged); }
	if (ConfirmButton) { ConfirmButton->OnClicked.AddDynamic(this, &UDigetherDepositWidget::HandleConfirmClicked); }
	if (CancelButton) { CancelButton->OnClicked.AddDynamic(this, &UDigetherDepositWidget::HandleCancelClicked); }
}

void UDigetherDepositWidget::BuildDefaultTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DepositRoot"));
	WidgetTree->RootWidget = Root;

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DepositFrame"));
	Frame->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.9f));
	Frame->SetPadding(FMargin(24.f));
	if (UCanvasPanelSlot* FrameSlot = Root->AddChildToCanvas(Frame))
	{
		FrameSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		FrameSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		FrameSlot->SetAutoSize(true);
		FrameSlot->SetPosition(FVector2D::ZeroVector);
	}

	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DepositSize"));
	Size->SetWidthOverride(460.f);
	Frame->SetContent(Size);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DepositColumn"));
	Size->AddChild(Column);

	auto AddRow = [Column](UWidget* Child, float TopPadding)
	{
		if (UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Child))
		{
			RowSlot->SetPadding(FMargin(0.f, TopPadding, 0.f, 0.f));
		}
	};

	AddRow(MakeLabel(WidgetTree, TEXT("DepositTitle"), TEXT("납부"), 22, FLinearColor(1.f, 0.85f, 0.45f)), 0.f);

	DescriptionText = MakeLabel(WidgetTree, TEXT("DescriptionText"),
		TEXT("납부할 금액을 숫자로 입력하세요. 목표 잔여보다 크게 입력하면 남은 만큼만 납부됩니다."), 15, FLinearColor::White);
	AddRow(DescriptionText, 8.f);

	BalanceText = MakeLabel(WidgetTree, TEXT("BalanceText"), TEXT("보유금: -"), 16, FLinearColor(0.7f, 1.f, 0.75f));
	AddRow(BalanceText, 12.f);

	RemainingText = MakeLabel(WidgetTree, TEXT("RemainingText"), TEXT("목표 잔여: -"), 16, FLinearColor(1.f, 0.85f, 0.45f));
	AddRow(RemainingText, 4.f);

	AmountInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("AmountInput"));
	// Slate uses a separate focused foreground while typing; setting only the
	// normal foreground leaves pale text on the default pale focused background.
	FEditableTextBoxStyle InputStyle = AmountInput->GetWidgetStyle();
	const FSlateColor InputInk(FLinearColor(0.015f, 0.015f, 0.015f, 1.f));
	InputStyle.ForegroundColor = InputInk;
	InputStyle.FocusedForegroundColor = InputInk;
	InputStyle.ReadOnlyForegroundColor = InputInk;
	InputStyle.TextStyle.SetColorAndOpacity(InputInk);
	AmountInput->SetWidgetStyle(InputStyle);
	AmountInput->SetHintText(FText::FromString(TEXT("납부 금액 (숫자만)")));
	AddRow(AmountInput, 12.f);

	ErrorText = MakeLabel(WidgetTree, TEXT("ErrorText"), FString(), 14, FLinearColor(1.f, 0.4f, 0.35f));
	AddRow(ErrorText, 6.f);

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DepositButtons"));
	ConfirmButton = MakeButton(WidgetTree, TEXT("ConfirmButton"), TEXT("확인 (Enter)"));
	CancelButton = MakeButton(WidgetTree, TEXT("CancelButton"), TEXT("취소 (ESC)"));
	if (UHorizontalBoxSlot* ConfirmSlot = Buttons->AddChildToHorizontalBox(ConfirmButton))
	{
		ConfirmSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ConfirmSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
	}
	if (UHorizontalBoxSlot* CancelSlot = Buttons->AddChildToHorizontalBox(CancelButton))
	{
		CancelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CancelSlot->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
	}
	AddRow(Buttons, 14.f);
}

ADigetherPrototypePlayerController* UDigetherDepositWidget::GetDepositController() const
{
	return Cast<ADigetherPrototypePlayerController>(GetOwningPlayer());
}

void UDigetherDepositWidget::SetDepositInfo(int32 Balance, int32 Remaining)
{
	if (BalanceText) { BalanceText->SetText(FText::FromString(FString::Printf(TEXT("보유금: %d원"), Balance))); }
	if (RemainingText) { RemainingText->SetText(FText::FromString(FString::Printf(TEXT("목표 잔여: %d원"), Remaining))); }
}

void UDigetherDepositWidget::SetInputText(const FString& Text)
{
	if (AmountInput && AmountInput->GetText().ToString() != Text)
	{
		AmountInput->SetText(FText::FromString(Text));
	}
}

FString UDigetherDepositWidget::GetInputText() const
{
	return AmountInput ? AmountInput->GetText().ToString() : FString();
}

void UDigetherDepositWidget::SetErrorMessage(const FString& Message)
{
	if (ErrorText) { ErrorText->SetText(FText::FromString(Message)); }
}

void UDigetherDepositWidget::SetSubmitting(bool bSubmitting)
{
	if (ConfirmButton) { ConfirmButton->SetIsEnabled(!bSubmitting); }
	if (AmountInput) { AmountInput->SetIsReadOnly(bSubmitting); }
}

TSharedPtr<SWidget> UDigetherDepositWidget::GetInputSlateWidget()
{
	return AmountInput ? TSharedPtr<SWidget>(AmountInput->TakeWidget()) : TSharedPtr<SWidget>();
}

void UDigetherDepositWidget::FocusInput()
{
	if (AmountInput) { AmountInput->SetKeyboardFocus(); }
}

FReply UDigetherDepositWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (ADigetherPrototypePlayerController* PC = GetDepositController())
	{
		const FKey Key = InKeyEvent.GetKey();
		if (Key == EKeys::Escape)
		{
			PC->CloseDepositInput();
			return FReply::Handled();
		}
		if (Key == EKeys::Enter)
		{
			PC->SubmitDepositInput();
			return FReply::Handled();
		}
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UDigetherDepositWidget::HandleTextChanged(const FText& Text)
{
	if (ADigetherPrototypePlayerController* PC = GetDepositController())
	{
		PC->SetDepositInputText(Text.ToString());
	}
}

void UDigetherDepositWidget::HandleConfirmClicked()
{
	if (ADigetherPrototypePlayerController* PC = GetDepositController())
	{
		PC->SubmitDepositInput();
	}
}

void UDigetherDepositWidget::HandleCancelClicked()
{
	if (ADigetherPrototypePlayerController* PC = GetDepositController())
	{
		PC->CloseDepositInput();
	}
}
