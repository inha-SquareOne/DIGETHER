#include "SDigetherDeveloperToolsPanel.h"
#include "Input/Reply.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDigetherDeveloperToolsPanel"

namespace DigetherDevToolsPanelPrivate
{
	FSlateColor SeverityColor(EDigetherDevToolsSeverity Severity)
	{
		switch (Severity)
		{
		case EDigetherDevToolsSeverity::Warning:
			return FSlateColor(FLinearColor(0.9f, 0.65f, 0.1f));
		case EDigetherDevToolsSeverity::Problem:
			return FSlateColor(FLinearColor(0.85f, 0.3f, 0.2f));
		case EDigetherDevToolsSeverity::Neutral:
		default:
			return FSlateColor::UseForeground();
		}
	}

	TSharedRef<SWidget> MakeSectionTitle(const FText& Title)
	{
		return SNew(STextBlock)
			.Text(Title)
			.Font(FAppStyle::GetFontStyle("BoldFont"));
	}

	TSharedRef<SWidget> MakeRow(const FDigetherDevToolsRow& Row)
	{
		return SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(8.f, 4.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.28f)
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock).Text(Row.Label).AutoWrapText(true)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.2f)
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(Row.Value)
					.ColorAndOpacity(SeverityColor(Row.Severity))
					.AutoWrapText(true)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.52f)
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock).Text(Row.Detail).AutoWrapText(true)
				]
			];
	}
}

void SDigetherDeveloperToolsPanel::Construct(const FArguments& InArgs)
{
	ContentBox = SNew(SVerticalBox);

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(8.f)
		[
			ContentBox.ToSharedRef()
		]
	];

	Rebuild();
}

void SDigetherDeveloperToolsPanel::Rebuild()
{
	using namespace DigetherDevToolsPanelPrivate;

	ContentBox->ClearChildren();

	const FEngineVersion& EngineVersion = FEngineVersion::Current();
	const FDigetherDevToolsEnvironment Environment = MakeDigetherLiveEnvironment();

	ContentBox->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SButton)
			.Text(LOCTEXT("Refresh", "새로고침"))
			.ToolTipText(LOCTEXT("RefreshTip", "엔진 버전, 플러그인, 설정값을 다시 읽습니다. 아무것도 저장하지 않습니다."))
			.OnClicked(this, &SDigetherDeveloperToolsPanel::HandleRefreshClicked)
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(8.f, 0.f)
		[
			SNew(STextBlock)
			.Text(FText::Format(
				LOCTEXT("QueriedAt", "조회 시점 {0} · 다른 창에서 바뀐 값은 새로고침해야 반영됩니다. 값은 설정 존재 여부이며 기능 검증 결과가 아닙니다."),
				FText::AsTime(FDateTime::UtcNow())))
			.AutoWrapText(true)
		]
	];

	ContentBox->AddSlot().AutoHeight().Padding(0.f, 6.f, 0.f, 2.f)
	[
		MakeSectionTitle(LOCTEXT("EnvironmentTitle", "현재 환경"))
	];

	ContentBox->AddSlot().AutoHeight().Padding(0.f, 1.f)
	[
		MakeRow(DescribeDigetherEngineVersion(EngineVersion.GetMajor(), EngineVersion.GetMinor(), EngineVersion.GetPatch()))
	];
	ContentBox->AddSlot().AutoHeight().Padding(0.f, 1.f)
	[
		MakeRow(DescribeDigetherAutoSave(ReadDigetherAutoSaveValues()))
	];
	ContentBox->AddSlot().AutoHeight().Padding(0.f, 1.f)
	[
		MakeRow(DescribeDigetherAssetManagerRule(ReadDigetherAssetManagerValues()))
	];
	for (const FName PluginName : GetDigetherDevToolsPluginNames())
	{
		ContentBox->AddSlot().AutoHeight().Padding(0.f, 1.f)
		[
			MakeRow(DescribeDigetherPlugin(PluginName, Environment.GetPluginState(PluginName)))
		];
	}

	ContentBox->AddSlot().AutoHeight().Padding(0.f, 10.f, 0.f, 2.f)
	[
		MakeSectionTitle(LOCTEXT("ShortcutsTitle", "바로 열기"))
	];
	ContentBox->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 2.f)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("ShortcutsNote", "엔진 화면을 열기만 합니다. 저장, 설정 변경, 검사 실행은 열린 화면에서 직접 합니다. 누를 때 다시 확인합니다."))
		.AutoWrapText(true)
	];

	for (const FDigetherShortcut& Shortcut : GetDigetherShortcuts())
	{
		const FDigetherShortcutAvailability Availability = EvaluateDigetherShortcut(Shortcut, Environment);

		ContentBox->AddSlot().AutoHeight().Padding(0.f, 2.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.28f).VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text(Shortcut.Label)
				.IsEnabled(Availability.bAvailable)
				.ToolTipText(Shortcut.Hint)
				.OnClicked(this, &SDigetherDeveloperToolsPanel::HandleShortcutClicked, Shortcut)
			]
			+ SHorizontalBox::Slot().FillWidth(0.72f).VAlign(VAlign_Center).Padding(8.f, 0.f)
			[
				SNew(STextBlock)
				.Text(Availability.bAvailable ? Shortcut.Hint : Availability.FailureReason)
				.ColorAndOpacity(Availability.bAvailable
					? FSlateColor::UseForeground()
					: SeverityColor(EDigetherDevToolsSeverity::Problem))
				.AutoWrapText(true)
			]
		];
	}

	ContentBox->AddSlot().AutoHeight().Padding(0.f, 4.f)
	[
		SNew(STextBlock)
		.Text(this, &SDigetherDeveloperToolsPanel::GetActionMessage)
		.ColorAndOpacity(SeverityColor(EDigetherDevToolsSeverity::Problem))
		.AutoWrapText(true)
	];

	ContentBox->AddSlot().AutoHeight().Padding(0.f, 10.f, 0.f, 2.f)
	[
		MakeSectionTitle(LOCTEXT("GuidanceTitle", "메뉴 안내 (이 도구가 실행하지 않음)"))
	];
	for (const FText& Guidance : GetDigetherMenuGuidance())
	{
		ContentBox->AddSlot().AutoHeight().Padding(0.f, 1.f)
		[
			SNew(STextBlock).Text(Guidance).AutoWrapText(true)
		];
	}
}

FReply SDigetherDeveloperToolsPanel::HandleRefreshClicked()
{
	LastActionMessage = FText::GetEmpty();
	Rebuild();
	return FReply::Handled();
}

FReply SDigetherDeveloperToolsPanel::HandleShortcutClicked(FDigetherShortcut Shortcut)
{
	FText FailureReason;
	if (TryOpenDigetherShortcut(Shortcut, FailureReason))
	{
		LastActionMessage = FText::GetEmpty();
	}
	else
	{
		LastActionMessage = FText::Format(LOCTEXT("OpenFailed", "'{0}' 열기 실패: {1}"), Shortcut.Label, FailureReason);
		Rebuild();
	}
	return FReply::Handled();
}

FText SDigetherDeveloperToolsPanel::GetActionMessage() const
{
	return LastActionMessage;
}

#undef LOCTEXT_NAMESPACE
