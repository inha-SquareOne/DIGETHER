#include "Game/UI/Hud/DigetherCrosshairWidget.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Game/Interaction/DigetherInteractionComponent.h"
#include "Game/Player/DigetherPlayerCharacter.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"

namespace
{
	// 점 지름(px)·색, 시험값. 테두리는 밝은 배경에서도 보이게 하는 반투명 검정
	constexpr float NormalDotSize = 12.f;
	constexpr float FocusedDotSize = 20.f;
	const FLinearColor NormalDotColor(1.f, 1.f, 1.f, 0.85f);
	const FLinearColor FocusedDotColor(1.f, 0.82f, 0.25f, 1.f);
	const FLinearColor DotOutlineColor(0.f, 0.f, 0.f, 0.6f);
	constexpr float DotOutlineWidth = 2.f;

	FSlateBrush MakeDotBrush(const float Size, const FLinearColor& Color)
	{
		return FSlateRoundedBoxBrush(Color, Size * 0.5f, DotOutlineColor, DotOutlineWidth, FVector2f(Size, Size));
	}
}

TSharedRef<SWidget> UDigetherCrosshairWidget::RebuildWidget()
{
	NormalBrush = MakeDotBrush(NormalDotSize, NormalDotColor);
	FocusedBrush = MakeDotBrush(FocusedDotSize, FocusedDotColor);
	bShowingFocused = false;
	// 입력을 가로채지 않음
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SAssignNew(DotImage, SImage)
			.Image(&NormalBrush)
		];
}

void UDigetherCrosshairWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	DotImage.Reset();
}

void UDigetherCrosshairWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!DotImage.IsValid())
	{
		return;
	}
	const ADigetherPlayerCharacter* Character = Cast<ADigetherPlayerCharacter>(GetOwningPlayerPawn());
	if (!IsValid(Character) || Character->IsDead())
	{
		DotImage->SetVisibility(EVisibility::Collapsed);
		return;
	}
	DotImage->SetVisibility(EVisibility::HitTestInvisible);
	const UDigetherInteractionComponent* Interaction = Character->GetInteractionComponent();
	const bool bFocused = IsValid(Interaction) && IsValid(Interaction->GetFocusedTarget());
	if (bFocused != bShowingFocused)
	{
		bShowingFocused = bFocused;
		DotImage->SetImage(bFocused ? &FocusedBrush : &NormalBrush);
	}
}
