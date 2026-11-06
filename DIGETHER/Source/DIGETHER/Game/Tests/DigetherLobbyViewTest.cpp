#if WITH_DEV_AUTOMATION_TESTS

#include "Game/UI/Lobby/DigetherLobbyView.h"
#include "Misc/AutomationTest.h"

namespace
{
	// 입력으로 주입하는 정원 시험값, 실제 정원은 DigetherRoomProtocol::MaxPlayers
	constexpr int32 LobbyViewFixtureMaxPlayers = 4;

	DigetherLobbyView::FDigetherLobbyViewInput MakeLobbyViewInput(const TArray<bool>& HostFlags)
	{
		DigetherLobbyView::FDigetherLobbyViewInput Input;
		Input.bHasLobbyState = true;
		Input.RoomName = TEXT("시험 방");
		Input.RoomCode = TEXT("DG-TEST01");
		Input.MaxPlayers = LobbyViewFixtureMaxPlayers;
		for (const bool bHost : HostFlags)
		{
			Input.Players.AddDefaulted_GetRef().bHost = bHost;
		}
		Input.bHasLocalState = true;
		Input.SessionStatus = FText::FromString(TEXT("세션 상태"));
		return Input;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherLobbyViewTest,
	"DIGETHER.Game.UI.LobbyView", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherLobbyViewTest::RunTest(const FString& Parameters)
{
	using namespace DigetherLobbyView;

	// 참가자 정렬: 호스트가 맨 앞, 나머지는 입장 순서, 빈 칸은 INDEX_NONE
	{
		const FDigetherLobbyView View = BuildLobbyView(MakeLobbyViewInput({ false, true, false }));
		TestEqual(TEXT("칸 수 = 정원"), View.SlotPlayerIndices.Num(), LobbyViewFixtureMaxPlayers);
		TestEqual(TEXT("1칸 호스트"), View.SlotPlayerIndices[0], 1);
		TestEqual(TEXT("2칸 먼저 들어온 참가자"), View.SlotPlayerIndices[1], 0);
		TestEqual(TEXT("3칸 다음 참가자"), View.SlotPlayerIndices[2], 2);
		TestEqual(TEXT("4칸 빈 칸"), View.SlotPlayerIndices[3], static_cast<int32>(INDEX_NONE));
		TestEqual(TEXT("참가자 수 문구"), View.PlayerCount.ToString(), FText::Format(NSLOCTEXT("DigetherLobby", "PlayerCount", "참가자 {0} / {1}"), 3, LobbyViewFixtureMaxPlayers).ToString());
	}

	// 방 정보
	{
		FDigetherLobbyViewInput Input = MakeLobbyViewInput({ true });
		FDigetherLobbyView View = BuildLobbyView(Input);
		TestTrue(TEXT("GameState 있으면 방 정보 표시"), View.bShowRoomInfo);
		TestEqual(TEXT("방 이름"), View.RoomName.ToString(), FString(TEXT("시험 방")));
		TestEqual(TEXT("방 코드 문구"), View.RoomCode.ToString(), FText::Format(NSLOCTEXT("DigetherLobby", "RoomCode", "방 코드  {0}"), FText::FromString(TEXT("DG-TEST01"))).ToString());
		TestTrue(TEXT("코드 있으면 복사 가능"), View.bCopyCodeEnabled);

		Input.RoomName.Reset();
		Input.RoomCode.Reset();
		View = BuildLobbyView(Input);
		TestEqual(TEXT("이름 없는 방"), View.RoomName.ToString(), NSLOCTEXT("DigetherLobby", "NoName", "이름 없는 방").ToString());
		TestFalse(TEXT("코드 없으면 복사 불가"), View.bCopyCodeEnabled);
	}

	// GameState 복제 전(클라이언트)
	{
		FDigetherLobbyViewInput Input;
		Input.MaxPlayers = LobbyViewFixtureMaxPlayers;
		Input.bGuestsReady = true;
		Input.bHasLocalState = true;
		Input.bLocalHost = true;
		const FDigetherLobbyView View = BuildLobbyView(Input);
		TestFalse(TEXT("GameState 없으면 방 정보 유지"), View.bShowRoomInfo);
		TestEqual(TEXT("GameState 없으면 참가자 0"), View.PlayerCount.ToString(), FText::Format(NSLOCTEXT("DigetherLobby", "PlayerCount", "참가자 {0} / {1}"), 0, LobbyViewFixtureMaxPlayers).ToString());
		TestFalse(TEXT("GameState 없으면 시작 불가"), View.bPrimaryEnabled);
		TestFalse(TEXT("GameState 없으면 복사 불가"), View.bCopyCodeEnabled);
	}

	// 호스트 분기
	{
		FDigetherLobbyViewInput Input = MakeLobbyViewInput({ true, false });
		Input.bLocalHost = true;
		FDigetherLobbyView View = BuildLobbyView(Input);
		TestEqual(TEXT("호스트 버튼"), View.PrimaryText.ToString(), NSLOCTEXT("DigetherLobby", "StartGame", "게임 시작").ToString());
		TestFalse(TEXT("참가자 미준비면 시작 불가"), View.bPrimaryEnabled);
		TestTrue(TEXT("미준비 안내"), View.Hint.EqualTo(NSLOCTEXT("DigetherLobby", "HintWaiting", "참가자가 모두 준비하면 게임을 시작할 수 있습니다. 친구 초대(Steam 오버레이)나 방 코드로 참가자를 부르세요.")));

		Input.bGuestsReady = true;
		View = BuildLobbyView(Input);
		TestTrue(TEXT("모두 준비면 시작 가능"), View.bPrimaryEnabled);
		TestTrue(TEXT("준비 완료 안내"), View.Hint.EqualTo(NSLOCTEXT("DigetherLobby", "HintReady", "모두 준비되었습니다. 게임 시작을 누르면 탐사를 시작합니다. 혼자서도 시작할 수 있습니다.")));

		Input.bSessionBusy = true;
		View = BuildLobbyView(Input);
		TestFalse(TEXT("작업 중이면 시작 불가"), View.bPrimaryEnabled);
		TestFalse(TEXT("작업 중이면 나가기 불가"), View.bLeaveEnabled);
	}

	// 참가자 분기
	{
		FDigetherLobbyViewInput Input = MakeLobbyViewInput({ true, false });
		FDigetherLobbyView View = BuildLobbyView(Input);
		TestEqual(TEXT("미준비 버튼"), View.PrimaryText.ToString(), NSLOCTEXT("DigetherLobby", "Ready", "준비하기").ToString());
		TestTrue(TEXT("참가자는 준비 전환 가능"), View.bPrimaryEnabled);
		TestTrue(TEXT("참가자 안내"), View.Hint.EqualTo(NSLOCTEXT("DigetherLobby", "HintGuest", "준비하기를 누른 뒤 호스트가 게임을 시작하기를 기다리세요.")));

		Input.bLocalReady = true;
		View = BuildLobbyView(Input);
		TestEqual(TEXT("준비 상태 버튼"), View.PrimaryText.ToString(), NSLOCTEXT("DigetherLobby", "Unready", "준비 취소").ToString());

		// 호스트 표시가 있어도 로컬 PlayerState가 없으면 참가자 화면, 버튼 비활성
		Input.bHasLocalState = false;
		Input.bLocalHost = true;
		View = BuildLobbyView(Input);
		TestEqual(TEXT("로컬 상태 없으면 참가자 버튼"), View.PrimaryText.ToString(), NSLOCTEXT("DigetherLobby", "Ready", "준비하기").ToString());
		TestFalse(TEXT("로컬 상태 없으면 버튼 꺼짐"), View.bPrimaryEnabled);
	}

	// 초대·나가기·상태 문구
	{
		FDigetherLobbyViewInput Input = MakeLobbyViewInput({ true });
		FDigetherLobbyView View = BuildLobbyView(Input);
		TestFalse(TEXT("LAN 모드는 초대 불가"), View.bInviteEnabled);
		TestTrue(TEXT("작업 없으면 나가기 가능"), View.bLeaveEnabled);
		TestEqual(TEXT("안내 없으면 세션 상태"), View.Status.ToString(), FString(TEXT("세션 상태")));

		Input.bSteamMode = true;
		Input.LocalNotice = FText::FromString(TEXT("복사했습니다"));
		View = BuildLobbyView(Input);
		TestTrue(TEXT("Steam 모드는 초대 가능"), View.bInviteEnabled);
		TestEqual(TEXT("화면 안내가 먼저"), View.Status.ToString(), FString(TEXT("복사했습니다")));

		Input.bSessionBusy = true;
		View = BuildLobbyView(Input);
		TestFalse(TEXT("작업 중이면 초대 불가"), View.bInviteEnabled);
	}
	return true;
}

#endif
