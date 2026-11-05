#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Online/DigetherSessionText.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Misc/AutomationTest.h"

// 코드가 쓰는 상태 문구 키의 ST_Session 존재와 넘기는 인자 자리의 문구 내 유지 확인
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSessionTextTest,
	"DIGETHER.Game.Online.SessionText", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSessionTextTest::RunTest(const FString& Parameters)
{
	const UStringTable* Table = LoadObject<UStringTable>(nullptr, DigetherSessionText::TableId);
	if (!TestNotNull(TEXT("ST_Session 에셋"), Table))
	{
		return false;
	}
	const FStringTableConstRef StringTable = Table->GetStringTable();

	const UEnum* Keys = StaticEnum<EDigetherSessionText>();
	for (int32 Index = 0; Index < Keys->NumEnums() - 1; ++Index)
	{
		const FString Key = Keys->GetNameStringByIndex(Index);
		FString Source;
		if (TestTrue(FString::Printf(TEXT("키 %s"), *Key), StringTable->GetSourceString(Key, Source)))
		{
			TestFalse(FString::Printf(TEXT("키 %s 문구가 비어 있음"), *Key), Source.IsEmpty());
		}
	}

	struct FExpectedArgument
	{
		EDigetherSessionText Key;
		const TCHAR* Argument;
	};
	static const FExpectedArgument ExpectedArguments[] = {
		{ EDigetherSessionText::SteamConnected, TEXT("PlayerName") },
		{ EDigetherSessionText::InvalidRoomCode, TEXT("MaxLength") },
		{ EDigetherSessionText::Created, TEXT("RoomCode") },
		{ EDigetherSessionText::SearchingCode, TEXT("RoomCode") },
		{ EDigetherSessionText::RoomsFound, TEXT("Count") },
		{ EDigetherSessionText::WaitingForRoom, TEXT("RoomCode") },
		{ EDigetherSessionText::CodeNotFound, TEXT("RoomCode") },
		{ EDigetherSessionText::RoomOpen, TEXT("RoomCode") },
		{ EDigetherSessionText::RoomOpen, TEXT("MaxPlayers") },
		{ EDigetherSessionText::TravelFailed, TEXT("Error") },
		{ EDigetherSessionText::ConnectionFailed, TEXT("Error") },
		{ EDigetherSessionText::DefaultRoomName, TEXT("PlayerName") },
	};
	for (const FExpectedArgument& Expected : ExpectedArguments)
	{
		TArray<FString> Arguments;
		FText::GetFormatPatternParameters(DigetherSessionText::Get(Expected.Key), Arguments);
		TestTrue(FString::Printf(TEXT("%s에 {%s} 자리"), *DigetherSessionText::GetKey(Expected.Key), Expected.Argument), Arguments.Contains(Expected.Argument));
	}
	return true;
}

#endif
