#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Voice/DigetherVoiceRules.h"
#include "Misc/AutomationTest.h"

namespace
{
	FDigetherVoiceParticipant MakeSurvivor(const TCHAR* Key, const int32 Zone = 0)
	{
		FDigetherVoiceParticipant Participant;
		Participant.PlayerKey = Key;
		Participant.ZoneId = Zone;
		return Participant;
	}

	FDigetherVoiceParticipant MakeSpectator(const TCHAR* Key, const TCHAR* Target)
	{
		FDigetherVoiceParticipant Participant;
		Participant.PlayerKey = Key;
		Participant.Role = EDigetherVoiceRole::Spectator;
		Participant.SpectateTargetPlayerKey = Target;
		return Participant;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherVoiceRulesHearingTest,
	"DIGETHER.Game.Voice.Hearing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherVoiceRulesHearingTest::RunTest(const FString& Parameters)
{
	// 1·2: 갱도(0), 3: 던전(1), 4: 1을 관전, 5: 3을 관전, 6: 대상 없음
	const FDigetherVoiceParticipant MineA = MakeSurvivor(TEXT("1"), 0);
	const FDigetherVoiceParticipant MineB = MakeSurvivor(TEXT("2"), 0);
	const FDigetherVoiceParticipant Dungeon = MakeSurvivor(TEXT("3"), 1);
	const FDigetherVoiceParticipant WatchMine = MakeSpectator(TEXT("4"), TEXT("1"));
	const FDigetherVoiceParticipant WatchDungeon = MakeSpectator(TEXT("5"), TEXT("3"));
	const FDigetherVoiceParticipant NoTarget = MakeSpectator(TEXT("6"), TEXT(""));
	const TArray<FDigetherVoiceParticipant> All = { MineA, MineB, Dungeon, WatchMine, WatchDungeon, NoTarget };
	constexpr EDigetherVoiceStage Separated = EDigetherVoiceStage::Separated;

	TestTrue(TEXT("같은 구역 생존자끼리 들림"), FDigetherVoiceRules::CanHear(MineA, MineB, All, Separated));
	TestFalse(TEXT("갱도→던전 차단"), FDigetherVoiceRules::CanHear(Dungeon, MineA, All, Separated));
	TestFalse(TEXT("던전→갱도 차단"), FDigetherVoiceRules::CanHear(MineA, Dungeon, All, Separated));
	TestFalse(TEXT("관전자→생존자 차단"), FDigetherVoiceRules::CanHear(MineA, WatchMine, All, Separated));
	TestTrue(TEXT("관전자끼리 대상·구역 무관하게 들림"), FDigetherVoiceRules::CanHear(WatchMine, WatchDungeon, All, Separated));
	TestTrue(TEXT("대상 없는 관전자도 관전자 대화 들림"), FDigetherVoiceRules::CanHear(NoTarget, WatchMine, All, Separated));
	TestTrue(TEXT("관전자는 대상 본인 목소리 들림"), FDigetherVoiceRules::CanHear(WatchMine, MineA, All, Separated));
	TestTrue(TEXT("관전자는 대상과 같은 구역 생존자 들림"), FDigetherVoiceRules::CanHear(WatchMine, MineB, All, Separated));
	TestFalse(TEXT("관전자는 대상과 다른 구역 생존자 못 들음"), FDigetherVoiceRules::CanHear(WatchMine, Dungeon, All, Separated));
	TestTrue(TEXT("던전 관전자는 던전 생존자 들림"), FDigetherVoiceRules::CanHear(WatchDungeon, Dungeon, All, Separated));
	TestFalse(TEXT("대상 없는 관전자는 생존자 못 들음"), FDigetherVoiceRules::CanHear(NoTarget, MineA, All, Separated));

	const FDigetherVoiceParticipant WatchSpectator = MakeSpectator(TEXT("7"), TEXT("4"));
	const TArray<FDigetherVoiceParticipant> WithSpectatorTarget = { MineA, WatchMine, WatchSpectator };
	TestFalse(TEXT("대상이 관전자면 생존자 못 들음"),
		FDigetherVoiceRules::CanHear(WatchSpectator, MineA, WithSpectatorTarget, Separated));

	constexpr EDigetherVoiceStage AllOpen = EDigetherVoiceStage::AllOpen;
	TestTrue(TEXT("크레딧: 관전자→생존자 들림"), FDigetherVoiceRules::CanHear(MineA, WatchMine, All, AllOpen));
	TestTrue(TEXT("크레딧: 구역 무관"), FDigetherVoiceRules::CanHear(MineA, Dungeon, All, AllOpen));

	const TSet<FString> BlockedForMineA = FDigetherVoiceRules::CollectBlockedTalkers(MineA, All, Separated);
	TestEqual(TEXT("갱도 생존자 차단 수"), BlockedForMineA.Num(), 4);
	TestFalse(TEXT("자기 자신은 차단 목록 제외"), BlockedForMineA.Contains(TEXT("1")));
	TestFalse(TEXT("같은 구역 동료는 차단 안 함"), BlockedForMineA.Contains(TEXT("2")));
	TestTrue(TEXT("크레딧 차단 없음"), FDigetherVoiceRules::CollectBlockedTalkers(MineA, All, AllOpen).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherVoiceRulesDiffTest,
	"DIGETHER.Game.Voice.MuteDiff", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherVoiceRulesDiffTest::RunTest(const FString& Parameters)
{
	const TSet<FString> Empty;
	const TSet<FString> AB = { TEXT("A"), TEXT("B") };
	const TSet<FString> BC = { TEXT("B"), TEXT("C") };

	const FDigetherVoiceMuteDiff First = FDigetherVoiceRules::Diff(Empty, AB);
	TestEqual(TEXT("처음엔 모두 음소거"), First.ToMute, TArray<FString>{ TEXT("A"), TEXT("B") });
	TestTrue(TEXT("처음엔 해제 없음"), First.ToUnmute.IsEmpty());

	const FDigetherVoiceMuteDiff Change = FDigetherVoiceRules::Diff(AB, BC);
	TestEqual(TEXT("새로 막을 대상만 음소거"), Change.ToMute, TArray<FString>{ TEXT("C") });
	TestEqual(TEXT("빠진 대상만 해제"), Change.ToUnmute, TArray<FString>{ TEXT("A") });

	const FDigetherVoiceMuteDiff Same = FDigetherVoiceRules::Diff(BC, BC);
	TestTrue(TEXT("같으면 변경 없음"), Same.ToMute.IsEmpty() && Same.ToUnmute.IsEmpty());
	return true;
}

#endif
