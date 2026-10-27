#include "Tools/SWorkbenchT11PlacementPanel.h"

#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "ScopedTransaction.h"
#include "Selection.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Internationalization/Text.h"

#define LOCTEXT_NAMESPACE "WorkbenchT11"

namespace
{
	using namespace WorkbenchToolsAlgo;

	FText IssueText(const FPlacementIssue& Issue, const TArray<FPlacementActorState>& States)
	{
		const FText Name = FText::FromString(States.IsValidIndex(Issue.ActorIndex) ? States[Issue.ActorIndex].Label : FString());
		switch (Issue.Kind)
		{
		case EPlacementIssueKind::NoSelection:
			return LOCTEXT("NoSelection", "선택된 액터가 2개 미만이다. 시드 1개와 이동 대상 1개 이상을 선택한다.");
		case EPlacementIssueKind::PlayInEditorActive:
			return LOCTEXT("Pie", "PIE 실행 중에는 배치를 적용하지 않는다.");
		case EPlacementIssueKind::Locked:
			return FText::Format(LOCTEXT("Locked", "{0}: 위치가 잠겨 있어 이동할 수 없다."), Name);
		case EPlacementIssueKind::Attached:
			return FText::Format(LOCTEXT("Attached", "{0}: 부모에 부착되었거나 부착된 자식이 있어 월드 변환 의미가 달라질 수 있다."), Name);
		case EPlacementIssueKind::NegativeScale:
			return FText::Format(LOCTEXT("NegScale", "{0}: 계산된 스케일이 0 이하이거나 유한하지 않다. 스케일 단계를 줄인다."), Name);
		case EPlacementIssueKind::ExternalActor:
			return FText::Format(LOCTEXT("External", "{0}: 외부 액터 패키지이며 적용하면 해당 패키지가 변경 상태가 된다(자동 저장 안 함)."), Name);
		case EPlacementIssueKind::DifferentLevel:
			return FText::Format(LOCTEXT("DiffLevel", "{0}: 시드와 다른 레벨 소속이다."), Name);
		}
		return FText::GetEmpty();
	}

	FPlacementActorState ReadActorState(const AActor* Actor, const ULevel* SeedLevel)
	{
		TArray<AActor*> Children;
		Actor->GetAttachedActors(Children);

		FPlacementActorState State;
		State.Label = Actor->GetActorNameOrLabel();
		State.bLocked = Actor->IsLockLocation();
		State.bAttachedToParent = Actor->GetAttachParentActor() != nullptr;
		State.bHasAttachedChildren = Children.Num() > 0;
		State.bExternalPackage = Actor->IsPackageExternal();
		State.bInDifferentLevelThanSeed = Actor->GetLevel() != SeedLevel;
		return State;
	}

	template <typename NumType>
	TSharedRef<SWidget> MakeLabeledSpin(const FText& Label, NumType Value, NumType Min, NumType Max, TFunction<void(NumType)> OnChanged)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
			[
				SNew(STextBlock).Text(Label)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(90.f)
				[
					SNew(SSpinBox<NumType>)
					.MinValue(Min).MaxValue(Max).Value(Value)
					.OnValueChanged_Lambda([OnChanged](NumType V) { OnChanged(V); })
				]
			];
	}
}

void SWorkbenchT11PlacementPanel::Construct(const FArguments& InArgs)
{
	auto Spin = [this](const FText& Label, float Initial, float Min, float Max, TFunction<void(float)> Setter)
	{
		return MakeLabeledSpin<float>(Label, Initial, Min, Max, [this, Setter](float V) { Setter(V); ClearPreview(); });
	};

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8)
		[
			SNew(STextBlock).AutoWrapText(true)
			.Text(LOCTEXT("Intro", "선택 읽기 시점의 첫 선택 액터를 시드로 삼는다. 시드는 움직이지 않고 나머지 선택 액터를 간격·회전·스케일 단계로 배치한다. 기본값은 예시이며 게임 규칙이 아니다."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)[Spin(LOCTEXT("SpX", "간격 X"), Spacing.X, -100000.f, 100000.f, [this](float V) { Spacing.X = V; })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)[Spin(LOCTEXT("SpY", "Y"), Spacing.Y, -100000.f, 100000.f, [this](float V) { Spacing.Y = V; })]
			+ SHorizontalBox::Slot().AutoWidth()[Spin(LOCTEXT("SpZ", "Z"), Spacing.Z, -100000.f, 100000.f, [this](float V) { Spacing.Z = V; })]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)[Spin(LOCTEXT("RotYaw", "회전 단계 Yaw"), RotationStep.Yaw, -360.f, 360.f, [this](float V) { RotationStep.Yaw = V; })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)[Spin(LOCTEXT("RotPitch", "Pitch"), RotationStep.Pitch, -360.f, 360.f, [this](float V) { RotationStep.Pitch = V; })]
			+ SHorizontalBox::Slot().AutoWidth()[Spin(LOCTEXT("RotRoll", "Roll"), RotationStep.Roll, -360.f, 360.f, [this](float V) { RotationStep.Roll = V; })]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)[Spin(LOCTEXT("ScaleStep", "균등 스케일 단계"), UniformScaleStep, -10.f, 10.f, [this](float V) { UniformScaleStep = V; })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)[Spin(LOCTEXT("Jitter", "위치 지터(±)"), PositionJitter, 0.f, 100000.f, [this](float V) { PositionJitter = V; })]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				MakeLabeledSpin<int32>(LOCTEXT("Seed", "난수 시드"), RandomSeed, 0, 2000000000, [this](int32 V) { RandomSeed = V; ClearPreview(); })
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
			[
				SNew(SButton).Text(LOCTEXT("Read", "선택 읽기"))
				.OnClicked(this, &SWorkbenchT11PlacementPanel::OnReadSelectionClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
			[
				SNew(SButton).Text(LOCTEXT("Preview", "미리보기 계산"))
				.OnClicked(this, &SWorkbenchT11PlacementPanel::OnPreviewClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
			[
				SNew(SButton).Text(LOCTEXT("Apply", "적용(트랜잭션)"))
				.IsEnabled(this, &SWorkbenchT11PlacementPanel::IsApplyEnabled)
				.OnClicked(this, &SWorkbenchT11PlacementPanel::OnApplyClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(LOCTEXT("Cancel", "미리보기 취소"))
				.OnClicked(this, &SWorkbenchT11PlacementPanel::OnCancelClicked)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 4)
		[
			SNew(STextBlock).AutoWrapText(true).Text(this, &SWorkbenchT11PlacementPanel::GetStatusText)
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(8, 4)
		[
			SAssignNew(RowsView, SListView<TSharedPtr<FPreviewRow>>)
			.ListItemsSource(&Rows)
			.OnGenerateRow(this, &SWorkbenchT11PlacementPanel::OnGenerateRow)
		]
	];
}

bool SWorkbenchT11PlacementPanel::IsPlayInEditorActive() const
{
	return GEditor && GEditor->PlayWorld != nullptr;
}

FReply SWorkbenchT11PlacementPanel::OnReadSelectionClicked()
{
	Targets.Reset();
	States.Reset();
	ClearPreview();
	LastResult = FText::GetEmpty();

	if (!GEditor)
	{
		return FReply::Handled();
	}

	TArray<AActor*> Selected;
	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(*It))
		{
			Selected.Add(Actor);
		}
	}

	const ULevel* SeedLevel = Selected.Num() > 0 ? Selected[0]->GetLevel() : nullptr;
	CapturedOriginals.Reset();
	for (AActor* Actor : Selected)
	{
		Targets.Add(Actor);
		States.Add(ReadActorState(Actor, SeedLevel));
		CapturedOriginals.Add(Actor->GetActorTransform());
	}

	if (Selected.Num() > 0)
	{
		CapturedSeed = Selected[0]->GetActorTransform();
	}
	return FReply::Handled();
}

void SWorkbenchT11PlacementPanel::ClearPreview()
{
	Preview.Reset();
	Issues.Reset();
	ParamErrors.Reset();
	Rows.Reset();
	bPreviewValid = false;
	if (RowsView.IsValid())
	{
		RowsView->RequestListRefresh();
	}
}

void SWorkbenchT11PlacementPanel::RebuildPreview()
{
	ClearPreview();

	FPlacementParams Params;
	Params.Seed = CapturedSeed;
	Params.Count = Targets.Num();
	Params.Spacing = Spacing;
	Params.RotationStep = RotationStep;
	Params.UniformScaleStep = UniformScaleStep;
	Params.RandomSeed = RandomSeed;
	Params.PositionJitter = PositionJitter;

	if (Targets.Num() > 0)
	{
		ParamErrors = ValidatePlacementParams(Params);
	}
	if (ParamErrors.Num() > 0)
	{
		RowsView->RequestListRefresh();
		return;
	}

	Preview = BuildPlacementPreview(Params);
	Issues = CheckPlacement(States, Preview, IsPlayInEditorActive());
	if (Targets.Num() < 2)
	{
		FPlacementIssue NoSelection;
		NoSelection.Kind = EPlacementIssueKind::NoSelection;
		NoSelection.bBlocking = true;
		Issues.Insert(NoSelection, 0);
	}

	for (int32 Index = 0; Index < Preview.Num(); ++Index)
	{
		const FTransform& T = Preview[Index];
		const FVector L = T.GetLocation();
		const FRotator R = T.Rotator();
		const FVector S = T.GetScale3D();
		TSharedPtr<FPreviewRow> Row = MakeShared<FPreviewRow>();
		Row->Label = FString::Printf(TEXT("%d. %s%s"), Index, *States[Index].Label, Index == 0 ? TEXT(" (시드, 이동 안 함)") : TEXT(""));
		Row->Detail = FString::Printf(TEXT("위치 (%.1f, %.1f, %.1f)  회전 (P%.1f Y%.1f R%.1f)  스케일 (%.2f, %.2f, %.2f)"),
			L.X, L.Y, L.Z, R.Pitch, R.Yaw, R.Roll, S.X, S.Y, S.Z);
		Rows.Add(Row);
	}

	bPreviewValid = !HasBlockingIssue(Issues);
	RowsView->RequestListRefresh();
}

FReply SWorkbenchT11PlacementPanel::OnPreviewClicked()
{
	LastResult = FText::GetEmpty();
	RebuildPreview();
	return FReply::Handled();
}

FReply SWorkbenchT11PlacementPanel::OnCancelClicked()
{
	ClearPreview();
	LastResult = LOCTEXT("Cancelled", "미리보기를 취소했다. 월드는 변경되지 않았다.");
	return FReply::Handled();
}

bool SWorkbenchT11PlacementPanel::IsApplyEnabled() const
{
	return bPreviewValid && !IsPlayInEditorActive();
}

FReply SWorkbenchT11PlacementPanel::OnApplyClicked()
{
	if (!IsApplyEnabled())
	{
		return FReply::Handled();
	}

	// 적용 직전 재검사: 입력 범위, 대상 유효성, 읽은 뒤 외부 변경, 잠금·부착·레벨 상태.
	const bool bCountMatches = Targets.Num() == Preview.Num() && Targets.Num() == CapturedOriginals.Num() && Targets.Num() >= 2;
	TArray<FPlacementActorState> FreshStates;
	bool bStale = !bCountMatches;
	FText Reason = LOCTEXT("Stale", "선택 대상이나 변환이 바뀌어 미리보기가 낡았다. 선택을 다시 읽는다.");
	if (!bStale)
	{
		FPlacementParams Params;
		Params.Seed = CapturedSeed;
		Params.Count = Targets.Num();
		Params.Spacing = Spacing;
		Params.RotationStep = RotationStep;
		Params.UniformScaleStep = UniformScaleStep;
		Params.RandomSeed = RandomSeed;
		Params.PositionJitter = PositionJitter;
		if (ValidatePlacementParams(Params).Num() > 0 || HasInvalidScale(Preview))
		{
			bStale = true;
			Reason = LOCTEXT("BadInput", "입력값이 유효 범위를 벗어났다(비유한 값, 개수 상한, 0 이하 스케일). 적용하지 않았다.");
		}
	}
	if (!bStale)
	{
		const AActor* Seed = Targets[0].Get();
		const ULevel* SeedLevel = Seed ? Seed->GetLevel() : nullptr;
		for (int32 Index = 0; Index < Targets.Num() && !bStale; ++Index)
		{
			const AActor* Actor = Targets[Index].Get();
			if (!Actor || !IsTransformUnchanged(CapturedOriginals[Index], Actor->GetActorTransform()))
			{
				bStale = true;
				break;
			}
			FreshStates.Add(ReadActorState(Actor, SeedLevel));
		}
		if (!bStale && HasBlockingIssue(CheckPlacement(FreshStates, Preview, IsPlayInEditorActive())))
		{
			bStale = true;
			Reason = LOCTEXT("StateChanged", "읽은 뒤 잠금·부착·레벨 상태가 바뀌어 적용하지 않았다. 선택을 다시 읽는다.");
		}
	}
	if (bStale)
	{
		ClearPreview();
		LastResult = Reason;
		return FReply::Handled();
	}

	{
		const FScopedTransaction Transaction(LOCTEXT("ApplyTx", "개발 도구 배치 적용"));
		for (int32 Index = 1; Index < Targets.Num(); ++Index)
		{
			AActor* Actor = Targets[Index].Get();
			Actor->Modify();
			Actor->SetActorTransform(Preview[Index]);
		}
	}

	LastResult = FText::Format(LOCTEXT("Applied", "{0}개 액터에 적용했다. Ctrl+Z로 되돌릴 수 있다. 레벨과 외부 액터 패키지는 저장하지 않았다."), FText::AsNumber(Targets.Num() - 1));
	ClearPreview();
	return FReply::Handled();
}

FText SWorkbenchT11PlacementPanel::GetStatusText() const
{
	FTextBuilder Builder;
	if (IsPlayInEditorActive())
	{
		Builder.AppendLine(LOCTEXT("PieState", "PIE 실행 중: 적용이 차단된다."));
	}
	Builder.AppendLine(FText::Format(LOCTEXT("Count", "읽은 액터 {0}개"), FText::AsNumber(Targets.Num())));
	for (const FString& Error : ParamErrors)
	{
		Builder.AppendLine(FText::Format(LOCTEXT("ParamError", "[차단] {0}"), FText::FromString(Error)));
	}
	for (const FPlacementIssue& Issue : Issues)
	{
		Builder.AppendLine(FText::Format(LOCTEXT("IssueLine", "{0} {1}"),
			Issue.bBlocking ? LOCTEXT("Block", "[차단]") : LOCTEXT("Warn", "[경고]"),
			IssueText(Issue, States)));
	}
	if (bPreviewValid)
	{
		Builder.AppendLine(LOCTEXT("Ready", "미리보기가 유효하다. 적용하면 하나의 트랜잭션으로 반영된다."));
	}
	if (!LastResult.IsEmpty())
	{
		Builder.AppendLine(LastResult);
	}
	return Builder.ToText();
}

TSharedRef<ITableRow> SWorkbenchT11PlacementPanel::OnGenerateRow(TSharedPtr<FPreviewRow> Row, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FPreviewRow>>, OwnerTable)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Row->Label))]
			+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Row->Detail))]
		];
}

#undef LOCTEXT_NAMESPACE
