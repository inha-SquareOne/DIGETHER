#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Online/DigetherRoomProtocol.h"
#include "Misc/AutomationTest.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemTypes.h"

namespace
{
	// 검색 결과 '유효' 판정에 세션 정보 필요, 시험용 최소 구현
	class FTestSessionInfo final : public FOnlineSessionInfo
	{
	public:
		FTestSessionInfo() : SessionId(FUniqueNetIdString::Create(TEXT("test-session"), TEXT("Test"))) {}
		virtual const uint8* GetBytes() const override { return nullptr; }
		virtual int32 GetSize() const override { return 0; }
		virtual bool IsValid() const override { return true; }
		virtual FString ToString() const override { return SessionId->ToString(); }
		virtual FString ToDebugString() const override { return ToString(); }
		virtual const FUniqueNetId& GetSessionId() const override { return *SessionId; }

	private:
		FUniqueNetIdStringRef SessionId;
	};

	FOnlineSessionSearchResult MakeRoomResult(const FString& RoomCode, const FString& RoomName)
	{
		FOnlineSessionSearchResult Result;
		Result.Session.OwningUserId = FUniqueNetIdString::Create(TEXT("host"), TEXT("Test"));
		Result.Session.OwningUserName = TEXT("Host");
		Result.Session.SessionInfo = MakeShared<FTestSessionInfo>();
		Result.Session.SessionSettings.NumPublicConnections = DigetherRoomProtocol::MaxPlayers;
		DigetherRoomProtocol::WriteRoom(Result.Session.SessionSettings, RoomCode, RoomName);
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherRoomProtocolTest,
	"DIGETHER.Game.Online.RoomProtocol", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherRoomProtocolTest::RunTest(const FString& Parameters)
{
	using namespace DigetherRoomProtocol;

	// 방 코드 규칙
	TestFalse(TEXT("빈 코드"), IsValidRoomCode(FString()));
	TestTrue(TEXT("최대 길이 코드"), IsValidRoomCode(FString::ChrN(MaxRoomCodeLength, TEXT('A'))));
	TestFalse(TEXT("최대 길이 초과"), IsValidRoomCode(FString::ChrN(MaxRoomCodeLength + 1, TEXT('A'))));
	TestTrue(TEXT("대문자·숫자·하이픈·밑줄"), IsValidRoomCode(TEXT("AB-9_Z")));
	// 2026-11-05 사용자 결정 "항상 대문자 정규화": 소문자는 정규화 전 입력으로 보고 거절
	TestFalse(TEXT("정규화 전 소문자"), IsValidRoomCode(TEXT("Ab-9_z")));
	TestFalse(TEXT("한글"), IsValidRoomCode(TEXT("방코드")));
	TestFalse(TEXT("공백"), IsValidRoomCode(TEXT("A B")));
	TestFalse(TEXT("점"), IsValidRoomCode(TEXT("A.B")));

	// 방 코드 정규화: 앞뒤 공백 제거 + 대문자
	TestEqual(TEXT("소문자 → 대문자"), NormalizeRoomCode(TEXT("dg-ab12")), FString(TEXT("DG-AB12")));
	TestEqual(TEXT("앞뒤 공백 제거"), NormalizeRoomCode(TEXT("  DG-AB12 \t")), FString(TEXT("DG-AB12")));
	TestEqual(TEXT("공백 + 대소문자 섞임"), NormalizeRoomCode(TEXT(" Dg-aB_12\n")), FString(TEXT("DG-AB_12")));
	TestEqual(TEXT("이미 정식 형태는 그대로"), NormalizeRoomCode(TEXT("DG-AB12")), FString(TEXT("DG-AB12")));
	TestEqual(TEXT("공백만 있으면 빈 코드"), NormalizeRoomCode(TEXT("   ")), FString());
	TestTrue(TEXT("정규화한 소문자 코드는 유효"), IsValidRoomCode(NormalizeRoomCode(TEXT(" ab-9_z "))));
	TestFalse(TEXT("가운데 공백은 정규화 후에도 무효"), IsValidRoomCode(NormalizeRoomCode(TEXT("a b"))));
	TestFalse(TEXT("공백만 있는 입력은 정규화 후 무효"), IsValidRoomCode(NormalizeRoomCode(TEXT("   "))));

	// 자동 생성 코드
	const FString Prefix(GeneratedCodePrefix);
	for (int32 Attempt = 0; Attempt < 20; ++Attempt)
	{
		const FString Code = MakeRoomCode();
		TestTrue(TEXT("생성 코드 접두"), Code.StartsWith(Prefix, ESearchCase::CaseSensitive));
		TestEqual(TEXT("생성 코드 길이"), Code.Len(), Prefix.Len() + GeneratedCodeRandomLength);
		TestTrue(TEXT("생성 코드는 규칙을 지킨다"), IsValidRoomCode(Code));
		const FString RandomPart = Code.RightChop(Prefix.Len());
		TestFalse(TEXT("혼동 문자 제외"), RandomPart.Contains(TEXT("0")) || RandomPart.Contains(TEXT("O")) || RandomPart.Contains(TEXT("1")) || RandomPart.Contains(TEXT("I")));
	}

	// 방 이름 정리
	TestEqual(TEXT("앞뒤 공백·줄바꿈 정리"), CleanRoomName(TEXT("  a\nb\r ")), FString(TEXT("a b")));
	TestEqual(TEXT("최대 길이로 자름"), CleanRoomName(FString::ChrN(MaxRoomNameLength + 8, TEXT('x'))).Len(), MaxRoomNameLength);

	// 쓰기 → 검색 결과 일치 확인
	const FOnlineSessionSearchResult Room = MakeRoomResult(TEXT("DG-TEST01"), TEXT("시험 방"));
	TestTrue(TEXT("같은 코드"), Matches(Room, TEXT("DG-TEST01")));
	TestTrue(TEXT("코드 생략(목록 검색)"), Matches(Room, FString()));
	TestFalse(TEXT("다른 코드"), Matches(Room, TEXT("DG-OTHER")));
	TestFalse(TEXT("대소문자 다른 코드"), Matches(Room, TEXT("dg-test01")));
	TestTrue(TEXT("정규화한 소문자·공백 입력은 같은 방"), Matches(Room, NormalizeRoomCode(TEXT("  dg-test01 "))));

	const FOnlineSessionSearchResult LowercaseAdvertised = MakeRoomResult(TEXT("dg-test01"), TEXT("정규화 전 광고"));
	TestFalse(TEXT("정규화되지 않은 광고 코드는 제외"), Matches(LowercaseAdvertised, FString()));

	FOnlineSessionSearchResult OtherBuild = Room;
	OtherBuild.Session.SessionSettings.BuildUniqueId += 1;
	TestFalse(TEXT("다른 빌드"), Matches(OtherBuild, FString()));

	FOnlineSessionSearchResult OtherProtocol = Room;
	OtherProtocol.Session.SessionSettings.Set(FName(TEXT("DIGETHER_GAME")), FString(TEXT("lobby-v0")), EOnlineDataAdvertisementType::ViaOnlineService);
	TestFalse(TEXT("다른 프로토콜 버전"), Matches(OtherProtocol, FString()));

	FOnlineSessionSearchResult NoSessionInfo = Room;
	NoSessionInfo.Session.SessionInfo.Reset();
	TestFalse(TEXT("세션 정보 없는 결과"), Matches(NoSessionInfo, FString()));

	// 목록 표시 값
	FOnlineSessionSearchResult TwoOpen = Room;
	TwoOpen.Session.NumOpenPublicConnections = 2;
	TwoOpen.PingInMs = 42;
	const FDigetherRoomInfo Info = ReadRoomInfo(TwoOpen, 3);
	TestEqual(TEXT("목록 위치"), Info.Index, 3);
	TestEqual(TEXT("코드"), Info.RoomCode, FString(TEXT("DG-TEST01")));
	TestEqual(TEXT("이름"), Info.RoomName, FString(TEXT("시험 방")));
	TestEqual(TEXT("최대 인원"), Info.MaxPlayers, MaxPlayers);
	TestEqual(TEXT("현재 인원 = 최대 - 빈자리"), Info.CurrentPlayers, MaxPlayers - 2);
	TestEqual(TEXT("핑"), Info.PingMs, 42);
	TestFalse(TEXT("대기실 상태"), Info.bInGame);

	FOnlineSessionSearchResult AllOpen = Room;
	AllOpen.Session.NumOpenPublicConnections = MaxPlayers + 3;
	TestEqual(TEXT("빈자리가 너무 많아도 호스트 1명"), ReadRoomInfo(AllOpen, 0).CurrentPlayers, 1);

	FOnlineSessionSearchResult NoSlots = Room;
	NoSlots.Session.SessionSettings.NumPublicConnections = 0;
	NoSlots.Session.NumOpenPublicConnections = 0;
	TestEqual(TEXT("최대 인원은 최소 1"), ReadRoomInfo(NoSlots, 0).MaxPlayers, 1);

	FOnlineSessionSearchResult InGame = Room;
	WriteInGame(InGame.Session.SessionSettings, true);
	TestTrue(TEXT("탐사 중 상태"), ReadRoomInfo(InGame, 0).bInGame);

	const FOnlineSessionSearchResult Unnamed = MakeRoomResult(TEXT("DG-NONAME"), FString());
	TestEqual(TEXT("이름 없으면 호스트 이름"), ReadRoomInfo(Unnamed, 0).RoomName, FString(TEXT("Host")));

	FString Code, Name;
	ReadRoomIdentity(Room, Code, Name);
	TestEqual(TEXT("참가용 코드"), Code, FString(TEXT("DG-TEST01")));
	TestEqual(TEXT("참가용 이름"), Name, FString(TEXT("시험 방")));
	return true;
}

#endif
