#include "Game/UI/Lobby/DigetherLobbyScreenWidget.h"

#include "CommonTextBlock.h"
#include "Components/DynamicEntryBox.h"
#include "Engine/World.h"
#include "Game/Frontend/Lobby/DigetherLobbyGameState.h"
#include "Game/Frontend/Lobby/DigetherLobbyPlayerController.h"
#include "Game/Frontend/Lobby/DigetherLobbyPlayerState.h"
#include "Game/Online/DigetherRoomProtocol.h"
#include "Game/Online/DigetherSessionSubsystem.h"
#include "Game/UI/Common/DigetherButtonWidget.h"
#include "Game/UI/Lobby/DigetherLobbyPlayerSlotWidget.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Input/CommonUIInputTypes.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "DigetherLobby"

namespace
{
	// 클라이언트는 화면이 GameState 복제보다 먼저 뜰 수 있어 짧게 다시 확인한다.
	constexpr float GameStateWaitSeconds = 0.2f;
}

ADigetherLobbyGameState* UDigetherLobbyScreenWidget::GetLobbyGameState() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetGameState<ADigetherLobbyGameState>() : nullptr;
}

ADigetherLobbyPlayerController* UDigetherLobbyScreenWidget::GetLobbyController() const
{
	return Cast<ADigetherLobbyPlayerController>(GetOwningPlayer());
}

ADigetherLobbyPlayerState* UDigetherLobbyScreenWidget::GetLocalLobbyState() const
{
	const APlayerController* Controller = GetOwningPlayer();
	return Controller ? Controller->GetPlayerState<ADigetherLobbyPlayerState>() : nullptr;
}

void UDigetherLobbyScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Btn_Primary->OnClicked().AddUObject(this, &ThisClass::HandlePrimaryAction);
	Btn_Invite->OnClicked().AddUObject(this, &ThisClass::HandleInvite);
	Btn_CopyCode->OnClicked().AddUObject(this, &ThisClass::HandleCopyCode);
	Btn_Leave->OnClicked().AddUObject(this, &ThisClass::HandleLeave);
}

// CommonUI 스택은 화면 인스턴스를 다시 쓸 수 있으므로 NativeOnInitialized가 아니라 Construct/Destruct 짝에서 구독한다.
void UDigetherLobbyScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this))
	{
		StatusChangedHandle = Session->OnStatusChanged.AddUObject(this, &ThisClass::HandleSessionStatusChanged);
	}
}

void UDigetherLobbyScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	BindGameState();
	Refresh();
}

void UDigetherLobbyScreenWidget::NativeDestruct()
{
	if (UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this))
	{
		Session->OnStatusChanged.Remove(StatusChangedHandle);
	}
	StatusChangedHandle.Reset();
	if (ADigetherLobbyGameState* LobbyState = BoundGameState.Get())
	{
		LobbyState->OnLobbyChanged.Remove(LobbyChangedHandle);
	}
	// 다시 열릴 때 NativeOnActivated의 BindGameState가 새로 구독하도록 비운다.
	BoundGameState.Reset();
	LobbyChangedHandle.Reset();
	LocalNotice = FText::GetEmpty();
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NoticeTimer);
		World->GetTimerManager().ClearTimer(GameStateWaitTimer);
	}
	Super::NativeDestruct();
}

UWidget* UDigetherLobbyScreenWidget::NativeGetDesiredFocusTarget() const
{
	return Btn_Primary;
}

TOptional<FUIInputConfig> UDigetherLobbyScreenWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void UDigetherLobbyScreenWidget::BindGameState()
{
	if (BoundGameState.IsValid())
	{
		return;
	}
	ADigetherLobbyGameState* LobbyState = GetLobbyGameState();
	if (!LobbyState)
	{
		GetWorld()->GetTimerManager().SetTimer(GameStateWaitTimer, this, &ThisClass::BindGameState, GameStateWaitSeconds, false);
		return;
	}
	BoundGameState = LobbyState;
	LobbyChangedHandle = LobbyState->OnLobbyChanged.AddUObject(this, &ThisClass::HandleLobbyChanged);
	Refresh();
}

void UDigetherLobbyScreenWidget::HandleSessionStatusChanged()
{
	Refresh();
}

void UDigetherLobbyScreenWidget::HandleLobbyChanged()
{
	Refresh();
}

void UDigetherLobbyScreenWidget::Refresh()
{
	const ADigetherLobbyGameState* LobbyState = GetLobbyGameState();
	const ADigetherLobbyPlayerState* LocalState = GetLocalLobbyState();
	const UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this);
	const bool bHost = LocalState && LocalState->IsHost();
	const bool bBusy = Session && Session->IsBusy();

	if (LobbyState)
	{
		Text_RoomName->SetText(LobbyState->GetRoomName().IsEmpty() ? LOCTEXT("NoName", "이름 없는 방") : FText::FromString(LobbyState->GetRoomName()));
		Text_RoomCode->SetText(FText::Format(LOCTEXT("RoomCode", "방 코드  {0}"), FText::FromString(LobbyState->GetRoomCode())));
	}

	// 호스트를 맨 위에 두고 나머지는 입장 순서대로 보여 준다.
	TArray<const ADigetherLobbyPlayerState*> Players;
	if (LobbyState)
	{
		for (const APlayerState* Player : LobbyState->PlayerArray)
		{
			if (const ADigetherLobbyPlayerState* LobbyPlayer = Cast<ADigetherLobbyPlayerState>(Player))
			{
				Players.Add(LobbyPlayer);
			}
		}
	}
	Players.StableSort([](const ADigetherLobbyPlayerState& A, const ADigetherLobbyPlayerState& B) { return A.IsHost() && !B.IsHost(); });
	Text_PlayerCount->SetText(FText::Format(LOCTEXT("PlayerCount", "참가자 {0} / {1}"), Players.Num(), DigetherRoomProtocol::MaxPlayers));

	Box_Players->Reset();
	for (int32 Index = 0; Index < DigetherRoomProtocol::MaxPlayers; ++Index)
	{
		if (UDigetherLobbyPlayerSlotWidget* PlayerSlot = Box_Players->CreateEntry<UDigetherLobbyPlayerSlotWidget>())
		{
			const ADigetherLobbyPlayerState* Player = Players.IsValidIndex(Index) ? Players[Index] : nullptr;
			PlayerSlot->ShowPlayer(Index + 1, Player, Player && Player == LocalState);
		}
	}

	const bool bGuestsReady = LobbyState && LobbyState->AreGuestsReady();
	if (bHost)
	{
		Btn_Primary->SetButtonText(LOCTEXT("StartGame", "게임 시작"));
		Btn_Primary->SetIsEnabled(!bBusy && bGuestsReady);
		Text_Hint->SetText(bGuestsReady
			? LOCTEXT("HintReady", "모두 준비되었습니다. 게임 시작을 누르면 탐사를 시작합니다. 혼자서도 시작할 수 있습니다.")
			: LOCTEXT("HintWaiting", "참가자가 모두 준비하면 게임을 시작할 수 있습니다. 친구 초대(Steam 오버레이)나 방 코드로 참가자를 부르세요."));
	}
	else
	{
		const bool bReady = LocalState && LocalState->IsReady();
		Btn_Primary->SetButtonText(bReady ? LOCTEXT("Unready", "준비 취소") : LOCTEXT("Ready", "준비하기"));
		Btn_Primary->SetIsEnabled(!bBusy && LocalState != nullptr);
		Text_Hint->SetText(LOCTEXT("HintGuest", "준비하기를 누른 뒤 호스트가 게임을 시작하기를 기다리세요."));
	}
	Btn_Invite->SetIsEnabled(!bBusy && Session && Session->IsSteamMode());
	Btn_CopyCode->SetIsEnabled(LobbyState && !LobbyState->GetRoomCode().IsEmpty());
	Btn_Leave->SetIsEnabled(!bBusy);

	Text_Status->SetText(!LocalNotice.IsEmpty() ? LocalNotice : (Session ? Session->GetStatus() : FText::GetEmpty()));
}

void UDigetherLobbyScreenWidget::HandlePrimaryAction()
{
	ADigetherLobbyPlayerController* Controller = GetLobbyController();
	const ADigetherLobbyPlayerState* LocalState = GetLocalLobbyState();
	if (!Controller || !LocalState)
	{
		return;
	}
	if (LocalState->IsHost())
	{
		Controller->RequestStartGame();
	}
	else
	{
		Controller->RequestSetReady(!LocalState->IsReady());
	}
}

void UDigetherLobbyScreenWidget::HandleInvite()
{
	if (UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this))
	{
		Session->ShowInviteUI();
	}
}

void UDigetherLobbyScreenWidget::HandleCopyCode()
{
	const ADigetherLobbyGameState* LobbyState = GetLobbyGameState();
	if (!LobbyState || LobbyState->GetRoomCode().IsEmpty())
	{
		return;
	}
	FPlatformApplicationMisc::ClipboardCopy(*LobbyState->GetRoomCode());
	LocalNotice = FText::Format(LOCTEXT("Copied", "방 코드 {0}을(를) 복사했습니다."), FText::FromString(LobbyState->GetRoomCode()));
	GetWorld()->GetTimerManager().SetTimer(NoticeTimer, this, &ThisClass::ClearLocalNotice, LocalNoticeSeconds, false);
	Refresh();
}

void UDigetherLobbyScreenWidget::ClearLocalNotice()
{
	LocalNotice = FText::GetEmpty();
	Refresh();
}

void UDigetherLobbyScreenWidget::HandleLeave()
{
	if (ADigetherLobbyPlayerController* Controller = GetLobbyController())
	{
		Controller->RequestLeave();
	}
}

#undef LOCTEXT_NAMESPACE
