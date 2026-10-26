#include "Core/SWorkbenchCoreWidgets.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Core/WorkbenchItemFixture.h"
#include "Core/WorkbenchItemValidation.h"
#include "Engine/DataTable.h"
#include "Modules/ModuleManager.h"
#include "UObject/SoftObjectPath.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "WorkbenchTablePicker"

namespace WorkbenchCoreUi
{
	FLinearColor ColorForMessage(const FWorkbenchValidationMessage& Message)
	{
		switch (Message.State)
		{
		case EWorkbenchValidationState::Invalid:
			return Message.Severity == EWorkbenchValidationSeverity::Error ? FLinearColor(1.f, 0.4f, 0.4f) : FLinearColor(1.f, 0.85f, 0.3f);
		case EWorkbenchValidationState::PendingDecision:
			return FLinearColor(1.f, 0.7f, 0.3f);
		case EWorkbenchValidationState::Valid:
			return FLinearColor(0.5f, 0.9f, 0.5f);
		default:
			return FLinearColor(0.7f, 0.7f, 0.7f);
		}
	}

	TSharedRef<FWorkbenchUiLine> MakeLine(const FText& Text, const FLinearColor& Color)
	{
		TSharedRef<FWorkbenchUiLine> Line = MakeShared<FWorkbenchUiLine>();
		Line->Text = Text;
		Line->Color = Color;
		return Line;
	}

	FText MessageToText(const FWorkbenchValidationMessage& Message)
	{
		return FText::Format(
			LOCTEXT("MessageLine", "[{0}] {1} · {2} — {3}"),
			WorkbenchItemValidation::StateToText(Message.State),
			FText::FromName(Message.RuleId),
			FText::FromName(Message.TargetId),
			Message.Description);
	}

	TSharedRef<SWidget> MakeLineList(TArray<TSharedPtr<FWorkbenchUiLine>>* Source, TSharedPtr<SListView<TSharedPtr<FWorkbenchUiLine>>>& OutList)
	{
		SAssignNew(OutList, SListView<TSharedPtr<FWorkbenchUiLine>>)
			.ListItemsSource(Source)
			.SelectionMode(ESelectionMode::None)
			.OnGenerateRow_Lambda([](TSharedPtr<FWorkbenchUiLine> Line, const TSharedRef<STableViewBase>& Owner) -> TSharedRef<ITableRow>
			{
				return SNew(STableRow<TSharedPtr<FWorkbenchUiLine>>, Owner)
				[
					SNew(STextBlock)
					.Text(Line->Text)
					.ColorAndOpacity(Line->Color)
					.AutoWrapText(true)
				];
			});
		return OutList.ToSharedRef();
	}
}

void SWorkbenchTablePicker::Construct(const FArguments& InArgs)
{
	OnTableChosen = InArgs._OnTableChosen;
	Status = LOCTEXT("StatusInitial", "대상 DataTable을 선택하세요. 지원 대상은 행 구조가 합성 아이템(FWorkbenchSyntheticItemRow)인 테이블입니다.");
	RefreshOptions();

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(2.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(STextBlock).Text(LOCTEXT("TableLabel", "대상 테이블"))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SAssignNew(Combo, SComboBox<TSharedPtr<FOption>>)
				.OptionsSource(&Options)
				.OnGenerateWidget(this, &SWorkbenchTablePicker::GenerateOption)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FOption> Option, ESelectInfo::Type)
				{
					if (Option.IsValid())
					{
						ChooseOption(Option);
					}
				})
				[
					SNew(STextBlock).Text(this, &SWorkbenchTablePicker::GetSelectedLabel)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Refresh", "목록 새로고침"))
				.ToolTipText(LOCTEXT("RefreshTip", "AssetRegistry에서 지원 DataTable을 다시 찾습니다."))
				.OnClicked_Lambda([this]() { RefreshOptions(); if (Combo.IsValid()) { Combo->RefreshOptions(); } return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("MemoryFixture", "메모리 합성 테이블"))
				.ToolTipText(LOCTEXT("MemoryFixtureTip", "저장되지 않는 합성 테이블을 만듭니다. 검증 규칙을 보여 주기 위한 문제 행이 포함됩니다."))
				.OnClicked_Lambda([this]() { UseMemoryFixture(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("DiskAsset", "합성 에셋 만들기/열기"))
				.ToolTipText(LOCTEXT("DiskAssetTip", "/Game/Editor/DevelopmentWorkbench/Synthetic 아래 합성 DataTable 에셋을 만들거나 불러옵니다. 만든 직후에는 저장 전 상태입니다."))
				.OnClicked_Lambda([this]() { CreateOrLoadDiskAsset(); return FReply::Handled(); })
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(2.f)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return Status; })
			.AutoWrapText(true)
			.ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f))
		]
	];
}

void SWorkbenchTablePicker::RefreshOptions()
{
	Options.Reset();
	const FString StructName = FWorkbenchSyntheticItemRow::StaticStruct()->GetName();

	FAssetRegistryModule& RegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> Assets;
	RegistryModule.Get().GetAssetsByClass(UDataTable::StaticClass()->GetClassPathName(), Assets);
	for (const FAssetData& Asset : Assets)
	{
		FString RowStructure;
		if (!Asset.GetTagValue(TEXT("RowStructure"), RowStructure) || !RowStructure.Contains(StructName))
		{
			continue;
		}
		TSharedPtr<FOption> Option = MakeShared<FOption>();
		Option->Label = FString::Printf(TEXT("%s  (%s)"), *Asset.AssetName.ToString(), *Asset.PackageName.ToString());
		Option->ObjectPath = Asset.GetObjectPathString();
		Options.Add(Option);
	}
}

TSharedRef<SWidget> SWorkbenchTablePicker::GenerateOption(TSharedPtr<FOption> Option)
{
	return SNew(STextBlock).Text(FText::FromString(Option->Label));
}

FText SWorkbenchTablePicker::GetSelectedLabel() const
{
	if (UDataTable* Table = Current.Get())
	{
		return FText::FromString(Selected.IsValid() ? Selected->Label : Table->GetName());
	}
	return LOCTEXT("NoSelection", "(선택 안 됨)");
}

void SWorkbenchTablePicker::SetCurrent(UDataTable* Table, const FText& Message)
{
	Current = Table;
	KeepAlive.Reset(Table);
	Status = Message;
	OnTableChosen.ExecuteIfBound(Table);
}

void SWorkbenchTablePicker::ChooseOption(TSharedPtr<FOption> Option)
{
	Selected = Option;
	UDataTable* Table = Cast<UDataTable>(FSoftObjectPath(Option->ObjectPath).TryLoad());
	if (!Table)
	{
		SetCurrent(nullptr, FText::Format(LOCTEXT("LoadFailed", "에셋을 불러오지 못했습니다: {0}"), FText::FromString(Option->ObjectPath)));
		return;
	}
	FText Reason;
	if (!WorkbenchDataTableEditAdapter::IsSupportedTable(Table, Reason))
	{
		SetCurrent(nullptr, FText::Format(LOCTEXT("Unsupported", "지원하지 않는 테이블입니다: {0}"), Reason));
		return;
	}
	SetCurrent(Table, FText::Format(LOCTEXT("Loaded", "불러옴: {0}"), FText::FromString(Option->ObjectPath)));
}

void SWorkbenchTablePicker::UseMemoryFixture()
{
	Selected.Reset();
	UDataTable* Table = WorkbenchItemFixture::CreateSyntheticTable();
	SetCurrent(
		Table,
		LOCTEXT("MemoryLoaded", "메모리 합성 테이블입니다(합성 시험 데이터, 게임 스키마 아님). 디스크에 저장할 수 없습니다. 적용 결과 확인과 Undo만 가능합니다."));
}

void SWorkbenchTablePicker::CreateOrLoadDiskAsset()
{
	FText Error;
	UDataTable* Table = WorkbenchItemFixture::FindOrCreateSyntheticAsset(Error);
	if (!Table)
	{
		SetCurrent(nullptr, FText::Format(LOCTEXT("DiskFailed", "합성 에셋을 만들지 못했습니다: {0}"), Error));
		return;
	}

	const FString PackagePath = WorkbenchItemFixture::GetSyntheticAssetPackagePath();
	RefreshOptions();
	Selected = nullptr;
	for (const TSharedPtr<FOption>& Option : Options)
	{
		if (Option->ObjectPath.StartsWith(PackagePath))
		{
			Selected = Option;
			break;
		}
	}
	if (!Selected.IsValid())
	{
		TSharedPtr<FOption> Option = MakeShared<FOption>();
		Option->Label = FString::Printf(TEXT("%s  (%s)"), *Table->GetName(), *PackagePath);
		Option->ObjectPath = Table->GetPathName();
		Options.Add(Option);
		Selected = Option;
	}
	if (Combo.IsValid())
	{
		Combo->RefreshOptions();
	}
	SetCurrent(
		Table,
		FText::Format(
			LOCTEXT("DiskLoaded", "합성 에셋: {0}. 의도적인 문제 행이 들어 있어 엔진 DataValidation 대상에서 제외됩니다. 만든 직후에는 저장 전 상태입니다."),
			FText::FromString(PackagePath)));
}

#undef LOCTEXT_NAMESPACE
