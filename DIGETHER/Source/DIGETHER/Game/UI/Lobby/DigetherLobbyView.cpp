#include "Game/UI/Lobby/DigetherLobbyView.h"

// 번역 키 보존용, 대기실 화면과 같은 네임스페이스·키 사용
#define LOCTEXT_NAMESPACE "DigetherLobby"

namespace DigetherLobbyView
{
	FDigetherLobbyView BuildLobbyView(const FDigetherLobbyViewInput& Input)
	{
		FDigetherLobbyView View;

		View.bShowRoomInfo = Input.bHasLobbyState;
		if (Input.bHasLobbyState)
		{
			View.RoomName = Input.RoomName.IsEmpty() ? LOCTEXT("NoName", "이름 없는 방") : FText::FromString(Input.RoomName);
			View.RoomCode = FText::Format(LOCTEXT("RoomCode", "방 코드  {0}"), FText::FromString(Input.RoomCode));
		}

		// 호스트 맨 위, 나머지는 입장 순서
		TArray<int32> Order;
		Order.Reserve(Input.Players.Num());
		for (int32 Index = 0; Index < Input.Players.Num(); ++Index)
		{
			Order.Add(Index);
		}
		Order.StableSort([&Input](const int32 A, const int32 B) { return Input.Players[A].bHost && !Input.Players[B].bHost; });
		View.PlayerCount = FText::Format(LOCTEXT("PlayerCount", "참가자 {0} / {1}"), Input.Players.Num(), Input.MaxPlayers);

		View.SlotPlayerIndices.Init(INDEX_NONE, FMath::Max(0, Input.MaxPlayers));
		for (int32 Slot = 0; Slot < View.SlotPlayerIndices.Num() && Order.IsValidIndex(Slot); ++Slot)
		{
			View.SlotPlayerIndices[Slot] = Order[Slot];
		}

		const bool bHost = Input.bHasLocalState && Input.bLocalHost;
		const bool bGuestsReady = Input.bHasLobbyState && Input.bGuestsReady;
		if (bHost)
		{
			View.PrimaryText = LOCTEXT("StartGame", "게임 시작");
			View.bPrimaryEnabled = !Input.bSessionBusy && bGuestsReady;
			View.Hint = bGuestsReady
				? LOCTEXT("HintReady", "모두 준비되었습니다. 게임 시작을 누르면 탐사를 시작합니다. 혼자서도 시작할 수 있습니다.")
				: LOCTEXT("HintWaiting", "참가자가 모두 준비하면 게임을 시작할 수 있습니다. 친구 초대(Steam 오버레이)나 방 코드로 참가자를 부르세요.");
		}
		else
		{
			const bool bReady = Input.bHasLocalState && Input.bLocalReady;
			View.PrimaryText = bReady ? LOCTEXT("Unready", "준비 취소") : LOCTEXT("Ready", "준비하기");
			View.bPrimaryEnabled = !Input.bSessionBusy && Input.bHasLocalState;
			View.Hint = LOCTEXT("HintGuest", "준비하기를 누른 뒤 호스트가 게임을 시작하기를 기다리세요.");
		}
		View.bInviteEnabled = !Input.bSessionBusy && Input.bSteamMode;
		View.bCopyCodeEnabled = Input.bHasLobbyState && !Input.RoomCode.IsEmpty();
		View.bLeaveEnabled = !Input.bSessionBusy;

		View.Status = !Input.LocalNotice.IsEmpty() ? Input.LocalNotice : Input.SessionStatus;
		return View;
	}
}

#undef LOCTEXT_NAMESPACE
