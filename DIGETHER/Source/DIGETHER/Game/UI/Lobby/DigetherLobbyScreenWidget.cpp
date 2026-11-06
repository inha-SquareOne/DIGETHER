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
#include "Game/UI/Lobby/DigetherLobbyView.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Input/CommonUIInputTypes.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "DigetherLobby"

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

// CommonUI 스택의 화면 인스턴스 재사용 대비, NativeOnInitialized 대신 Construct/Destruct 짝에서 구독
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
	// 재활성화 시 NativeOnActivated의 BindGameState 재구독용 초기화
	BoundGameState.Reset();
	LobbyChangedHandle.Reset();
	LocalNotice = FText::GetEmpty();
	StopWaitingForGameState();
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NoticeTimer);
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
		UWorld* World = GetWorld();
		if (World && !GameStateSetHandle.IsValid())
		{
			GameStateWaitWorld = World;
			GameStateSetHandle = World->GameStateSetEvent.AddUObject(this, &ThisClass::HandleGameStateSet);
		}
		return;
	}
	StopWaitingForGameState();
	BoundGameState = LobbyState;
	LobbyChangedHandle = LobbyState->OnLobbyChanged.AddUObject(this, &ThisClass::HandleLobbyChanged);
}

void UDigetherLobbyScreenWidget::HandleGameStateSet(AGameStateBase* GameState)
{
	// 대기실 GameState가 아니면 대기 유지, 연결 시 BindGameState가 대기 구독 해제
	if (Cast<ADigetherLobbyGameState>(GameState))
	{
		BindGameState();
		Refresh();
	}
}

void UDigetherLobbyScreenWidget::StopWaitingForGameState()
{
	if (UWorld* World = GameStateWaitWorld.Get())
	{
		World->GameStateSetEvent.Remove(GameStateSetHandle);
	}
	GameStateWaitWorld.Reset();
	GameStateSetHandle.Reset();
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

	// 표시값 계산은 DigetherLobbyView 담당, 여기서는 입력 수집·결과 적용만
	TArray<const ADigetherLobbyPlayerState*> Players;
	DigetherLobbyView::FDigetherLobbyViewInput Input;
	if (LobbyState)
	{
		Input.bHasLobbyState = true;
		Input.RoomName = LobbyState->GetRoomName();
		Input.RoomCode = LobbyState->GetRoomCode();
		Input.bGuestsReady = LobbyState->AreGuestsReady();
		for (const APlayerState* Player : LobbyState->PlayerArray)
		{
			if (const ADigetherLobbyPlayerState* LobbyPlayer = Cast<ADigetherLobbyPlayerState>(Player))
			{
				Players.Add(LobbyPlayer);
				Input.Players.AddDefaulted_GetRef().bHost = LobbyPlayer->IsHost();
			}
		}
	}
	Input.MaxPlayers = DigetherRoomProtocol::MaxPlayers;
	Input.bHasLocalState = LocalState != nullptr;
	Input.bLocalHost = LocalState && LocalState->IsHost();
	Input.bLocalReady = LocalState && LocalState->IsReady();
	Input.bSessionBusy = Session && Session->IsBusy();
	Input.bSteamMode = Session && Session->IsSteamMode();
	Input.LocalNotice = LocalNotice;
	Input.SessionStatus = Session ? Session->GetStatus() : FText::GetEmpty();

	const DigetherLobbyView::FDigetherLobbyView View = DigetherLobbyView::BuildLobbyView(Input);

	if (View.bShowRoomInfo)
	{
		Text_RoomName->SetText(View.RoomName);
		Text_RoomCode->SetText(View.RoomCode);
	}
	Text_PlayerCount->SetText(View.PlayerCount);

	Box_Players->Reset();
	for (int32 SlotIndex = 0; SlotIndex < View.SlotPlayerIndices.Num(); ++SlotIndex)
	{
		if (UDigetherLobbyPlayerSlotWidget* PlayerSlot = Box_Players->CreateEntry<UDigetherLobbyPlayerSlotWidget>())
		{
			const int32 PlayerIndex = View.SlotPlayerIndices[SlotIndex];
			const ADigetherLobbyPlayerState* Player = Players.IsValidIndex(PlayerIndex) ? Players[PlayerIndex] : nullptr;
			PlayerSlot->ShowPlayer(SlotIndex + 1, Player, Player && Player == LocalState);
		}
	}

	Btn_Primary->SetButtonText(View.PrimaryText);
	Btn_Primary->SetIsEnabled(View.bPrimaryEnabled);
	Text_Hint->SetText(View.Hint);
	Btn_Invite->SetIsEnabled(View.bInviteEnabled);
	Btn_CopyCode->SetIsEnabled(View.bCopyCodeEnabled);
	Btn_Leave->SetIsEnabled(View.bLeaveEnabled);
	Text_Status->SetText(View.Status);
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
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(NoticeTimer, this, &ThisClass::ClearLocalNotice, LocalNoticeSeconds, false);
	}
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
