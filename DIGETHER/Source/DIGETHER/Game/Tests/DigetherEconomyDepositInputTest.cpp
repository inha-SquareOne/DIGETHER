#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Economy/DigetherDepositInput.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherEconomyDepositInputTest,
	"DIGETHER.Game.Economy.DepositInput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherEconomyDepositInputTest::RunTest(const FString& Parameters)
{
	const auto ExpectRejected = [this](const TCHAR* Label, const FString& Input, EDigetherEconomyResult ExpectedResult)
	{
		int32 Amount = 123;
		const EDigetherEconomyResult Result = FDigetherDepositInput::TryParseAmount(Input, Amount);
		TestEqual(FString::Printf(TEXT("%s: 실패 원인"), Label), Result, ExpectedResult);
		TestEqual(FString::Printf(TEXT("%s: 이전 출력 금액 제거"), Label), Amount, 0);
	};
	const auto ExpectAccepted = [this](const TCHAR* Label, const FString& Input, int32 ExpectedAmount)
	{
		int32 Amount = -123;
		const EDigetherEconomyResult Result = FDigetherDepositInput::TryParseAmount(Input, Amount);
		TestEqual(FString::Printf(TEXT("%s: 해석 성공"), Label), Result, EDigetherEconomyResult::Success);
		TestEqual(FString::Printf(TEXT("%s: 정확한 금액"), Label), Amount, ExpectedAmount);
	};

	ExpectRejected(TEXT("빈 입력"), TEXT(""), EDigetherEconomyResult::DepositInputEmpty);
	ExpectRejected(TEXT("공백만 있는 입력"), TEXT(" \t\r\n"), EDigetherEconomyResult::DepositInputEmpty);
	ExpectRejected(TEXT("0"), TEXT("0"), EDigetherEconomyResult::InvalidAmount);
	ExpectRejected(TEXT("0만 여러 개"), TEXT("0000"), EDigetherEconomyResult::InvalidAmount);
	ExpectRejected(TEXT("음수 부호"), TEXT("-1"), EDigetherEconomyResult::DepositInputInvalidCharacter);
	ExpectRejected(TEXT("양수 부호"), TEXT("+1"), EDigetherEconomyResult::DepositInputInvalidCharacter);
	ExpectRejected(TEXT("소수점"), TEXT("1.5"), EDigetherEconomyResult::DepositInputInvalidCharacter);
	ExpectRejected(TEXT("중간 공백"), TEXT("1 2"), EDigetherEconomyResult::DepositInputInvalidCharacter);
	ExpectRejected(TEXT("영문자"), TEXT("abc"), EDigetherEconomyResult::DepositInputInvalidCharacter);
	ExpectRejected(TEXT("int32 최대값 초과"), TEXT("2147483648"), EDigetherEconomyResult::DepositInputOverflow);
	ExpectRejected(TEXT("4096자리 9"), FString::ChrN(4096, TEXT('9')), EDigetherEconomyResult::DepositInputOverflow);
	ExpectRejected(TEXT("전각 숫자"), TEXT("\uFF11\uFF12"), EDigetherEconomyResult::DepositInputInvalidCharacter);
	ExpectRejected(TEXT("아랍-인도 숫자"), TEXT("\u0661\u0662"), EDigetherEconomyResult::DepositInputInvalidCharacter);

	ExpectAccepted(TEXT("최소 양수"), TEXT("1"), 1);
	ExpectAccepted(TEXT("일반 숫자"), TEXT("12345"), 12345);
	ExpectAccepted(TEXT("앞뒤 공백"), TEXT(" \t42\r\n"), 42);
	ExpectAccepted(TEXT("선행 0"), TEXT("00042"), 42);
	ExpectAccepted(TEXT("int32 최대값"), TEXT("2147483647"), MAX_int32);
	ExpectAccepted(TEXT("선행 0과 int32 최대값"), TEXT("0002147483647"), MAX_int32);
	return true;
}

#endif
