#include "SDigetherConnectionStatusPanel.h"
#include "DigetherConnectionStatusModel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "SDigetherConnectionStatusPanel"

void SDigetherConnectionStatusPanel::Construct(const FArguments& InArgs)
{
	const TArray<FDigetherConnectionStatusRow> Rows = BuildDigetherConnectionStatusRows();

	TSharedRef<SVerticalBox> RowsBox = SNew(SVerticalBox);

	for (const FDigetherConnectionStatusRow& Row : Rows)
	{
		const FSlateColor StatusColor = Row.bConnected
			? FSlateColor(FLinearColor(0.2f, 0.75f, 0.2f))
			: FSlateColor(FLinearColor(0.85f, 0.3f, 0.2f));

		RowsBox->AddSlot()
		.AutoHeight()
		.Padding(4.f, 2.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(8.f, 6.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.28f)
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(Row.DomainLabel)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.14f)
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(Row.StatusLabel)
					.ColorAndOpacity(StatusColor)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.58f)
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(Row.DetailText)
					.AutoWrapText(true)
				]
			]
		];
	}

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.f, 8.f, 8.f, 2.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PanelTitle", "DIGETHER 연결 상태 — 합성 예제 패널"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.f, 0.f, 8.f, 8.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PanelDisclaimer",
					"이 패널은 합성 예제 도구입니다. 아래 실게임 item/economy/spawn/room/runtime provider는 아직 연결되지 않았고, "
					"승인된 기획 개정도 등록되지 않았습니다. 이 도구가 보여주는 값은 실게임 결과와 다릅니다."))
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.6f, 0.1f)))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.f, 0.f, 8.f, 8.f)
			[
				RowsBox
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
