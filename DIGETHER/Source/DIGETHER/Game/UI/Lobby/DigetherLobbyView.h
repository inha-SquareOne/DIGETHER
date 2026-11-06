#pragma once

#include "CoreMinimal.h"

// 대기실 표시값 계산 순수 함수, UObject 비의존이라 자동 테스트(UI.LobbyView) 대상
// 위젯은 GameState·PlayerState·세션 입력 수집과 결과 적용만 담당
namespace DigetherLobbyView
{
	struct FDigetherLobbyPlayerInput
	{
		bool bHost = false;
	};

	struct FDigetherLobbyViewInput
	{
		// 대기실 GameState 없음(클라이언트 복제 전)이면 방 이름·코드 표시 유지
		bool bHasLobbyState = false;
		FString RoomName;
		FString RoomCode;
		// GameState.PlayerArray 중 대기실 PlayerState만, 원래 순서 유지
		TArray<FDigetherLobbyPlayerInput> Players;
		int32 MaxPlayers = 0;
		bool bGuestsReady = false;
		// 로컬 플레이어의 대기실 PlayerState 연결 여부
		bool bHasLocalState = false;
		bool bLocalHost = false;
		bool bLocalReady = false;
		bool bSessionBusy = false;
		bool bSteamMode = false;
		// 코드 복사 등 화면 안내, 비어 있지 않으면 세션 상태 문구보다 우선
		FText LocalNotice;
		FText SessionStatus;
	};

	struct FDigetherLobbyView
	{
		bool bShowRoomInfo = false;
		FText RoomName;
		FText RoomCode;
		FText PlayerCount;
		// 칸 수는 MaxPlayers, 값은 입력 Players 위치(빈 칸 INDEX_NONE), 호스트 맨 앞·나머지 입장 순서
		TArray<int32> SlotPlayerIndices;
		FText PrimaryText;
		bool bPrimaryEnabled = false;
		FText Hint;
		bool bInviteEnabled = false;
		bool bCopyCodeEnabled = false;
		bool bLeaveEnabled = false;
		FText Status;
	};

	DIGETHER_API FDigetherLobbyView BuildLobbyView(const FDigetherLobbyViewInput& Input);
}
