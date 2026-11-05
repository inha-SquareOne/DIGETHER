#include "Game/UI/MainMenu/DigetherCreateRoomPanelWidget.h"

#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Game/Online/DigetherSessionSubsystem.h"
#include "Game/Save/DigetherSaveSubsystem.h"
#include "Game/UI/Common/DigetherButtonWidget.h"

#define LOCTEXT_NAMESPACE "DigetherCreateRoomPanel"

UDigetherCreateRoomPanelWidget::UDigetherCreateRoomPanelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
}

void UDigetherCreateRoomPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Btn_Create->OnClicked().AddUObject(this, &ThisClass::CreateRoom);
	Btn_Close->OnClicked().AddUObject(this, &ThisClass::DeactivateWidget);
	if (Combo_SaveSlot)
	{
		Combo_SaveSlot->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleSaveSlotChanged);
	}
}

// CommonUI 스택은 닫힌 패널 인스턴스를 다시 쓰므로 NativeOnInitialized가 아니라 Construct/Destruct 짝에서 구독한다.
void UDigetherCreateRoomPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this))
	{
		StatusChangedHandle = Session->OnStatusChanged.AddUObject(this, &ThisClass::HandleSessionStatusChanged);
	}
}

void UDigetherCreateRoomPanelWidget::NativeDestruct()
{
	if (UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this))
	{
		Session->OnStatusChanged.Remove(StatusChangedHandle);
	}
	StatusChangedHandle.Reset();
	Super::NativeDestruct();
}

void UDigetherCreateRoomPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	const UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this);
	if (Session && Input_RoomName->GetText().IsEmpty())
	{
		Input_RoomName->SetText(FText::FromString(Session->MakeDefaultRoomName()));
	}
	RefreshSaveSlots();
	RefreshState();
}

UWidget* UDigetherCreateRoomPanelWidget::NativeGetDesiredFocusTarget() const
{
	return Input_RoomName;
}

void UDigetherCreateRoomPanelWidget::HandleSessionStatusChanged()
{
	RefreshState();
}

void UDigetherCreateRoomPanelWidget::HandleSaveSlotChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	RefreshSaveSlotSummary();
}

void UDigetherCreateRoomPanelWidget::CreateRoom()
{
	if (UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this))
	{
		Session->HostRoom(Input_RoomName->GetText().ToString(), Check_Public->IsChecked(), FString(),
			MakeSaveSelection());
	}
}

void UDigetherCreateRoomPanelWidget::RefreshState()
{
	const UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this);
	Btn_Create->SetIsEnabled(Session && Session->CanStartRoomRequest());
}

void UDigetherCreateRoomPanelWidget::RefreshSaveSlots()
{
	SlotSummaries.Reset();
	const UGameInstance* Instance = GetGameInstance();
	if (const UDigetherSaveSubsystem* Save = IsValid(Instance) ? Instance->GetSubsystem<UDigetherSaveSubsystem>() : nullptr)
	{
		SlotSummaries = Save->ListSlots();
	}
	if (Combo_SaveSlot)
	{
		const int32 PreviousIndex = Combo_SaveSlot->GetSelectedIndex();
		Combo_SaveSlot->ClearOptions();
		const int32 SlotCount = UDigetherSaveSubsystem::GetMaxSlotCount();
		for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
		{
			// 화면 번호는 1부터다. 내부 슬롯 번호(0부터)와 섞지 않는다.
			Combo_SaveSlot->AddOption(FText::Format(LOCTEXT("SlotOption", "슬롯 {0}"),
				FText::AsNumber(SlotIndex + 1)).ToString());
		}
		Combo_SaveSlot->SetSelectedIndex(PreviousIndex >= 0 && PreviousIndex < SlotCount ? PreviousIndex : 0);
	}
	RefreshSaveSlotSummary();
}

void UDigetherCreateRoomPanelWidget::RefreshSaveSlotSummary()
{
	if (!Text_SaveSlotSummary)
	{
		return;
	}
	const int32 SlotIndex = MakeSaveSelection().SlotIndex;
	const FDigetherSaveSlotSummary* Summary = SlotSummaries.FindByPredicate(
		[SlotIndex](const FDigetherSaveSlotSummary& Entry) { return Entry.SlotIndex == SlotIndex; });
	FText Message = LOCTEXT("SlotUnknown", "슬롯 정보를 읽지 못했습니다.");
	if (Summary)
	{
		switch (Summary->Status)
		{
		case EDigetherSaveSlotStatus::Empty:
			Message = LOCTEXT("SlotEmpty", "비어 있음 — 새 게임으로 시작합니다.");
			break;
		case EDigetherSaveSlotStatus::Ready:
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("TotalDay"), FText::AsNumber(Summary->TotalDay));
			Arguments.Add(TEXT("PeriodDay"), FText::AsNumber(Summary->PeriodDay));
			Arguments.Add(TEXT("Target"), FText::AsNumber(Summary->TargetCredits));
			Arguments.Add(TEXT("SavedAt"), Summary->bHasSavedAt
				? FText::AsDateTime(Summary->SavedAtUtc, EDateTimeStyle::Short, EDateTimeStyle::Short)
				: LOCTEXT("SavedAtUnknown", "저장 시각 모름"));
			Message = FText::Format(LOCTEXT("SlotReady",
				"전체 {TotalDay}일차 · 기간 {PeriodDay}일차 · 목표 {Target} · {SavedAt}"), Arguments);
			break;
		}
		case EDigetherSaveSlotStatus::FutureVersion:
			Message = LOCTEXT("SlotFuture", "새 버전에서 만든 저장이라 이 버전에서 열 수 없습니다. 저장은 보존됩니다.");
			break;
		case EDigetherSaveSlotStatus::Unreadable:
			Message = LOCTEXT("SlotUnreadable", "저장을 읽을 수 없습니다. 이어하기는 거절되며 새 게임만 시작할 수 있습니다.");
			break;
		case EDigetherSaveSlotStatus::IdentityUnavailable:
			Message = LOCTEXT("SlotIdentity", "계정 정보를 확인하지 못해 슬롯을 읽지 못했습니다.");
			break;
		}
	}
	Text_SaveSlotSummary->SetText(Message);
}

FDigetherSaveSlotSelection UDigetherCreateRoomPanelWidget::MakeSaveSelection() const
{
	FDigetherSaveSlotSelection Selection;
	if (Combo_SaveSlot && Combo_SaveSlot->GetSelectedIndex() >= 0)
	{
		Selection.SlotIndex = Combo_SaveSlot->GetSelectedIndex();
	}
	if (Check_NewGame && Check_NewGame->IsChecked())
	{
		Selection.StartMode = EDigetherSaveStartMode::NewGame;
	}
	return Selection;
}

#undef LOCTEXT_NAMESPACE
