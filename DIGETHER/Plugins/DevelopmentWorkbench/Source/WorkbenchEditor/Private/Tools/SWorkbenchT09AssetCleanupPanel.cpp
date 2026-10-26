#include "Tools/SWorkbenchT09AssetCleanupPanel.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IAssetTools.h"
#include "Internationalization/Text.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/ObjectRedirector.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "WorkbenchT09"

using namespace WorkbenchToolsAlgo;

namespace
{
	constexpr int32 MaxScanAssets = 5000;
	constexpr int32 MaxApplyCount = 200;

	FString KeyOf(const FString& PackagePath, const FString& Name)
	{
		return PackagePath / Name;
	}

	// 계획 작성 이후 외부 이름 변경·삭제, 새 이름 점유, 읽기 전용, 참조 수 변화를 확인한다. 문제가 있으면 사유를 반환한다.
	FString RecheckEntry(IAssetRegistry& Registry, const FRenamePlanEntry& Entry, const FSoftObjectPath& OldPath, const FRenameCandidate* Original)
	{
		const FString Key = KeyOf(Entry.PackagePath, Entry.OldName);
		const FAssetData OldAsset = Registry.GetAssetByObjectPath(OldPath);
		if (!OldAsset.IsValid())
		{
			return FString::Printf(TEXT("%s: 계획 이후 이름이 바뀌었거나 삭제되었다."), *Key);
		}
		const FSoftObjectPath NewPath(FString::Printf(TEXT("%s/%s.%s"), *Entry.PackagePath, *Entry.NewName, *Entry.NewName));
		if (Registry.GetAssetByObjectPath(NewPath).IsValid())
		{
			return FString::Printf(TEXT("%s: 새 이름 %s을(를) 계획 이후 다른 에셋이 사용하고 있다."), *Key, *Entry.NewName);
		}
		FString Filename;
		if (FPackageName::TryConvertLongPackageNameToFilename(OldAsset.PackageName.ToString(), Filename, FPackageName::GetAssetPackageExtension())
			&& IFileManager::Get().FileExists(*Filename) && IFileManager::Get().IsReadOnly(*Filename))
		{
			return FString::Printf(TEXT("%s: 계획 이후 읽기 전용이 되었다."), *Key);
		}
		TArray<FName> Referencers;
		Registry.GetReferencers(OldAsset.PackageName, Referencers);
		Referencers.Remove(OldAsset.PackageName);
		if (Original && Referencers.Num() != Original->ReferencerCount)
		{
			return FString::Printf(TEXT("%s: 계획 이후 참조 수가 %d에서 %d로 바뀌었다."), *Key, Original->ReferencerCount, Referencers.Num());
		}
		return FString();
	}

	FText StatusLabel(ERenamePlanStatus Status)
	{
		switch (Status)
		{
		case ERenamePlanStatus::Ok: return LOCTEXT("StOk", "변경");
		case ERenamePlanStatus::NoChange: return LOCTEXT("StNo", "변경 없음");
		case ERenamePlanStatus::Collision: return LOCTEXT("StCol", "이름 충돌");
		case ERenamePlanStatus::ReadOnly: return LOCTEXT("StRo", "읽기 전용");
		}
		return FText::GetEmpty();
	}
}

void SWorkbenchT09AssetCleanupPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8)
		[
			SNew(STextBlock).AutoWrapText(true)
			.Text(LOCTEXT("Intro", "에셋 레지스트리에 등록된 경로를 조사한다. 참조 없음은 레지스트리에 기록된 참조가 없다는 뜻일 뿐, 코드·문자열·소프트 경로 동적 사용은 확인하지 못하므로 삭제를 보장하지 않는다. 이 도구에는 삭제 기능이 없다."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0, 0, 8, 0)
			[
				SNew(SEditableTextBox).Text(FText::FromString(RootPath)).HintText(LOCTEXT("PathHint", "/Game/폴더"))
				.OnTextChanged_Lambda([this](const FText& T) { RootPath = T.ToString().TrimStartAndEnd(); bScanned = false; InvalidatePlan(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(SCheckBox).IsChecked(ECheckBoxState::Checked)
				.OnCheckStateChanged_Lambda([this](ECheckBoxState S) { bRecursive = S == ECheckBoxState::Checked; bScanned = false; InvalidatePlan(); })
				[SNew(STextBlock).Text(LOCTEXT("Recursive", "하위 폴더 포함"))]
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[SNew(SButton).Text(LOCTEXT("Scan", "조사")).OnClicked(this, &SWorkbenchT09AssetCleanupPanel::OnScanClicked)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0, 0, 8, 0)
			[
				SNew(SEditableTextBox).HintText(LOCTEXT("PrefixHint", "붙일 접두사(예시: T_) - 영문·숫자·밑줄 32자 이하"))
				.OnTextChanged_Lambda([this](const FText& T) { Prefix = T.ToString(); InvalidatePlan(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
			[SNew(SButton).Text(LOCTEXT("MakePlan", "계획 만들기")).OnClicked(this, &SWorkbenchT09AssetCleanupPanel::OnPlanClicked)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
			[SNew(SButton).Text(LOCTEXT("Copy", "복구 목록 복사")).IsEnabled_Lambda([this]() { return bPlanFresh; }).OnClicked(this, &SWorkbenchT09AssetCleanupPanel::OnCopyRecoveryClicked)]
			+ SHorizontalBox::Slot().AutoWidth()
			[SNew(SButton).Text(LOCTEXT("Apply", "대상 확인 후 적용")).IsEnabled(this, &SWorkbenchT09AssetCleanupPanel::IsApplyEnabled).OnClicked(this, &SWorkbenchT09AssetCleanupPanel::OnApplyClicked)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 4)
		[
			SNew(STextBlock).AutoWrapText(true).Text(this, &SWorkbenchT09AssetCleanupPanel::GetStatusText)
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(8, 2)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("PlanTitle", "이름 변경 계획"))]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SAssignNew(PlanView, SListView<FRow>).ListItemsSource(&PlanRows).OnGenerateRow(this, &SWorkbenchT09AssetCleanupPanel::OnGenerateRow)
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(8, 2)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("UnrefTitle", "참조 없음 후보(삭제 보장 아님, 삭제 기능 없음)"))]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SAssignNew(UnreferencedView, SListView<FRow>).ListItemsSource(&UnreferencedRows).OnGenerateRow(this, &SWorkbenchT09AssetCleanupPanel::OnGenerateRow)
			]
		]
	];
}

bool SWorkbenchT09AssetCleanupPanel::IsPlayInEditorActive() const
{
	return GEditor && GEditor->PlayWorld != nullptr;
}

void SWorkbenchT09AssetCleanupPanel::InvalidatePlan()
{
	Plan.Reset();
	PlanRows.Reset();
	bPlanFresh = false;
	if (PlanView.IsValid())
	{
		PlanView->RequestListRefresh();
	}
}

FReply SWorkbenchT09AssetCleanupPanel::OnScanClicked()
{
	InvalidatePlan();
	Candidates.Reset();
	ObjectPathByKey.Reset();
	UnreferencedRows.Reset();
	bScanned = false;
	bScanTruncated = false;

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	if (Registry.IsLoadingAssets())
	{
		LastMessage = LOCTEXT("RegLoading", "에셋 레지스트리가 아직 에셋을 읽는 중이다. 끝난 뒤 다시 조사한다.");
		UnreferencedView->RequestListRefresh();
		return FReply::Handled();
	}
	if (!FPackageName::IsValidPath(RootPath) || !RootPath.StartsWith(TEXT("/")))
	{
		LastMessage = LOCTEXT("BadPath", "경로가 올바르지 않다. /Game/폴더 형식으로 입력한다.");
		UnreferencedView->RequestListRefresh();
		return FReply::Handled();
	}

	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(*RootPath), Assets, bRecursive);
	if (Assets.Num() > MaxScanAssets)
	{
		Assets.SetNum(MaxScanAssets);
		bScanTruncated = true;
	}

	const FTopLevelAssetPath RedirectorClass = UObjectRedirector::StaticClass()->GetClassPathName();
	const FTopLevelAssetPath WorldClass = UWorld::StaticClass()->GetClassPathName();

	for (const FAssetData& Asset : Assets)
	{
		FRenameCandidate Candidate;
		Candidate.PackagePath = Asset.PackagePath.ToString();
		Candidate.AssetName = Asset.AssetName.ToString();
		Candidate.bOccupyOnly = Asset.AssetClassPath == RedirectorClass || Asset.AssetClassPath == WorldClass;

		if (!Candidate.bOccupyOnly)
		{
			FString Filename;
			if (FPackageName::TryConvertLongPackageNameToFilename(Asset.PackageName.ToString(), Filename, FPackageName::GetAssetPackageExtension()))
			{
				Candidate.bReadOnly = IFileManager::Get().FileExists(*Filename) && IFileManager::Get().IsReadOnly(*Filename);
			}

			TArray<FName> Referencers;
			Registry.GetReferencers(Asset.PackageName, Referencers);
			Referencers.Remove(Asset.PackageName);
			Candidate.ReferencerCount = Referencers.Num();

			ObjectPathByKey.Add(KeyOf(Candidate.PackagePath, Candidate.AssetName), Asset.GetSoftObjectPath());
			if (Candidate.ReferencerCount == 0)
			{
				UnreferencedRows.Add(MakeShared<FString>(FString::Printf(TEXT("%s (레지스트리 참조 0)"), *KeyOf(Candidate.PackagePath, Candidate.AssetName))));
			}
		}
		Candidates.Add(Candidate);
	}

	bScanned = true;
	LastMessage = FText::Format(LOCTEXT("Scanned", "{0}개 에셋을 조사했다."), FText::AsNumber(Candidates.Num()));
	UnreferencedView->RequestListRefresh();
	return FReply::Handled();
}

FReply SWorkbenchT09AssetCleanupPanel::OnPlanClicked()
{
	InvalidatePlan();
	if (!bScanned)
	{
		LastMessage = LOCTEXT("NeedScan", "먼저 조사한다.");
		return FReply::Handled();
	}
	if (!IsValidAssetNamePrefix(Prefix))
	{
		LastMessage = LOCTEXT("BadPrefix", "접두사는 영문·숫자·밑줄만 쓸 수 있고 1~32자여야 한다.");
		return FReply::Handled();
	}

	Plan = BuildRenamePlan(Candidates, Prefix);
	for (const FRenamePlanEntry& Entry : Plan)
	{
		PlanRows.Add(MakeShared<FString>(FString::Printf(TEXT("[%s] %s  ->  %s%s"), *StatusLabel(Entry.Status).ToString(),
			*KeyOf(Entry.PackagePath, Entry.OldName), *Entry.NewName, Entry.bNoReferencersFound ? TEXT("  (참조 0)") : TEXT(""))));
	}
	bPlanFresh = true;
	PlanView->RequestListRefresh();
	LastMessage = FText::GetEmpty();
	return FReply::Handled();
}

FReply SWorkbenchT09AssetCleanupPanel::OnCopyRecoveryClicked()
{
	FPlatformApplicationMisc::ClipboardCopy(*FormatRenameRecoveryList(Plan));
	LastMessage = LOCTEXT("Copied", "복구 목록(경로, 현재 이름, 새 이름)을 클립보드에 복사했다.");
	return FReply::Handled();
}

bool SWorkbenchT09AssetCleanupPanel::IsApplyEnabled() const
{
	return bPlanFresh && IsRenamePlanApplyable(Plan) && !IsPlayInEditorActive();
}

FReply SWorkbenchT09AssetCleanupPanel::OnApplyClicked()
{
	if (!IsApplyEnabled())
	{
		return FReply::Handled();
	}

	TArray<const FRenamePlanEntry*> Targets;
	for (const FRenamePlanEntry& Entry : Plan)
	{
		if (Entry.Status == ERenamePlanStatus::Ok)
		{
			Targets.Add(&Entry);
		}
	}
	if (Targets.Num() > MaxApplyCount)
	{
		LastMessage = FText::Format(LOCTEXT("TooMany", "변경 대상이 {0}개로 {1}개를 넘어 계획만 제공한다. 폴더를 나눠 조사한다."), FText::AsNumber(Targets.Num()), FText::AsNumber(MaxApplyCount));
		return FReply::Handled();
	}

	FString Listing;
	for (int32 Index = 0; Index < FMath::Min(Targets.Num(), 15); ++Index)
	{
		Listing += FString::Printf(TEXT("\n%s -> %s"), *KeyOf(Targets[Index]->PackagePath, Targets[Index]->OldName), *Targets[Index]->NewName);
	}
	if (Targets.Num() > 15)
	{
		Listing += FString::Printf(TEXT("\n... 외 %d개"), Targets.Num() - 15);
	}
	const FText Question = FText::Format(LOCTEXT("Confirm", "{0}개 에셋의 이름을 변경한다. 변경 전 복구 목록을 Saved/Workbench/RenameRecovery에 저장한다. 참조는 갱신되지만 리다이렉터가 남을 수 있고 저장은 하지 않는다.{1}\n\n진행할까?"),
		FText::AsNumber(Targets.Num()), FText::FromString(Listing));
	if (FMessageDialog::Open(EAppMsgType::YesNo, Question) != EAppReturnType::Yes)
	{
		LastMessage = LOCTEXT("Declined", "사용자가 적용하지 않았다.");
		return FReply::Handled();
	}

	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		FString Problems;
		for (const FRenamePlanEntry* Entry : Targets)
		{
			const FString Key = KeyOf(Entry->PackagePath, Entry->OldName);
			const FSoftObjectPath* OldPath = ObjectPathByKey.Find(Key);
			const FRenameCandidate* Original = Candidates.FindByPredicate([Entry](const FRenameCandidate& C) { return C.PackagePath == Entry->PackagePath && C.AssetName == Entry->OldName; });
			const FString Reason = OldPath ? RecheckEntry(Registry, *Entry, *OldPath, Original) : FString::Printf(TEXT("%s: 조사 기록이 없다."), *Key);
			if (!Reason.IsEmpty())
			{
				Problems += TEXT("\n") + Reason;
			}
		}
		if (!Problems.IsEmpty())
		{
			InvalidatePlan();
			bScanned = false;
			LastMessage = FText::Format(LOCTEXT("Recheck", "적용 직전 재검사에서 계획 이후의 변화가 발견되어 아무것도 변경하지 않았다. 다시 조사한다.{0}"), FText::FromString(Problems));
			return FReply::Handled();
		}
	}

	const FString RecoveryFile = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Workbench/RenameRecovery") / (FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")) + TEXT(".tsv")));
	if (!FFileHelper::SaveStringToFile(FormatRenameRecoveryList(Plan), *RecoveryFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		LastMessage = LOCTEXT("RecoveryFail", "복구 목록을 저장하지 못해 적용하지 않았다.");
		return FReply::Handled();
	}

	TArray<FAssetRenameData> Renames;
	for (const FRenamePlanEntry* Entry : Targets)
	{
		const FSoftObjectPath* Path = ObjectPathByKey.Find(KeyOf(Entry->PackagePath, Entry->OldName));
		UObject* Asset = Path ? Path->TryLoad() : nullptr;
		if (!Asset)
		{
			LastMessage = FText::Format(LOCTEXT("LoadFail", "{0}를 불러오지 못해 아무것도 변경하지 않았다."), FText::FromString(KeyOf(Entry->PackagePath, Entry->OldName)));
			return FReply::Handled();
		}
		Renames.Emplace(Asset, Entry->PackagePath, Entry->NewName);
	}

	const bool bSucceeded = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().RenameAssets(Renames);
	LastMessage = FText::Format(bSucceeded
		? LOCTEXT("Renamed", "이름 변경을 요청한 {0}개를 처리했다. 에셋은 변경 상태이며 저장하지 않았다. 복구 목록: {1}")
		: LOCTEXT("RenamedPartial", "일부 또는 전체 이름 변경이 실패했거나 취소되었다(요청 {0}개). 실제 상태는 콘텐츠 브라우저에서 확인한다. 복구 목록: {1}"),
		FText::AsNumber(Targets.Num()), FText::FromString(RecoveryFile));

	InvalidatePlan();
	bScanned = false;
	return FReply::Handled();
}

FText SWorkbenchT09AssetCleanupPanel::GetStatusText() const
{
	FTextBuilder Builder;
	if (IsPlayInEditorActive())
	{
		Builder.AppendLine(LOCTEXT("Pie", "PIE 실행 중에는 이름 변경을 적용하지 않는다."));
	}
	if (bScanTruncated)
	{
		Builder.AppendLine(FText::Format(LOCTEXT("Truncated", "에셋이 {0}개를 넘어 앞부분만 조사했다. 폴더를 좁힌다."), FText::AsNumber(MaxScanAssets)));
	}
	if (bPlanFresh)
	{
		int32 Ok = 0, Collision = 0, ReadOnly = 0;
		for (const FRenamePlanEntry& Entry : Plan)
		{
			Ok += Entry.Status == ERenamePlanStatus::Ok;
			Collision += Entry.Status == ERenamePlanStatus::Collision;
			ReadOnly += Entry.Status == ERenamePlanStatus::ReadOnly;
		}
		Builder.AppendLine(FText::Format(LOCTEXT("PlanSummary", "계획: 변경 {0}, 충돌 {1}, 읽기 전용 {2}"), FText::AsNumber(Ok), FText::AsNumber(Collision), FText::AsNumber(ReadOnly)));
		if (!IsRenamePlanApplyable(Plan))
		{
			Builder.AppendLine(LOCTEXT("PlanOnly", "충돌·읽기 전용이 있거나 변경 항목이 없어 계획만 제공한다. 해결 후 다시 계획한다."));
		}
	}
	if (!LastMessage.IsEmpty())
	{
		Builder.AppendLine(LastMessage);
	}
	return Builder.ToText();
}

TSharedRef<ITableRow> SWorkbenchT09AssetCleanupPanel::OnGenerateRow(FRow Row, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<FRow>, OwnerTable)[SNew(STextBlock).Text(FText::FromString(*Row))];
}

#undef LOCTEXT_NAMESPACE
