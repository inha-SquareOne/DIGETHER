#pragma once

#include "CoreMinimal.h"

// 대기실 화면에 보일 값을 계산하는 순수 함수. UObject에 의존하지 않아 자동 테스트로 확인한다.
// 위젯은 GameState·PlayerState·세션에서 입력을 모으고 결과를 그대로 적용만 한다.
namespace DigetherLobbyView
{
	struct FDigetherLobbyPlayerInput
	{
		bool bHost = false;
	};

	struct FDigetherLobbyViewInput
	{
		// 대기실 GameState가 아직 없으면(클라이언트 복제 전) 방 이름·코드 표시는 건드리지 않는다.
		bool bHasLobbyState = false;
		FString RoomName;
		FString RoomCode;
		// GameState.PlayerArray 중 대기실 PlayerState만, 원래 순서대로.
		TArray<FDigetherLobbyPlayerInput> Players;
		int32 MaxPlayers = 0;
		bool bGuestsReady = false;
		// 로컬 플레이어의 대기실 PlayerState가 연결됐는지.
		bool bHasLocalState = false;
		bool bLocalHost = false;
		bool bLocalReady = false;
		bool bSessionBusy = false;
		bool bSteamMode = false;
		// '코드 복사' 같은 화면 안내. 비어 있지 않으면 세션 상태 문구보다 먼저 보인다.
		FText LocalNotice;
		FText SessionStatus;
	};

	struct FDigetherLobbyView
	{
		bool bShowRoomInfo = false;
		FText RoomName;
		FText RoomCode;
		FText PlayerCount;
		// 칸 수는 MaxPlayers. 값은 입력 Players의 위치이며 빈 칸은 INDEX_NONE이다. 호스트가 맨 앞, 나머지는 입장 순서.
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
