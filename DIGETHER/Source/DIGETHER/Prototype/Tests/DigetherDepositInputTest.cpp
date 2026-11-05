#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Prototype/Player/DigetherPrototypePlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherDepositInputTest,
	"DIGETHER.Prototype.DepositInput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherDepositInputTest::RunTest(const FString& Parameters)
{
	const auto ExpectAccepted = [this](const TCHAR* Label, const FString& Input, int32 ExpectedAmount)
	{
		int32 Amount = -123;
		FString Error = TEXT("previous error");
		const bool bAccepted = ADigetherPrototypePlayerController::TryParseDepositAmount(Input, Amount, Error);
		TestTrue(FString::Printf(TEXT("%s: accepted"), Label), bAccepted);
		TestEqual(FString::Printf(TEXT("%s: exact amount"), Label), Amount, ExpectedAmount);
		TestTrue(FString::Printf(TEXT("%s: clears previous error"), Label), Error.IsEmpty());
	};
	const auto ExpectRejected = [this](const TCHAR* Label, const FString& Input)
	{
		int32 Amount = 123;
		FString Error = TEXT("previous error");
		const bool bAccepted = ADigetherPrototypePlayerController::TryParseDepositAmount(Input, Amount, Error);
		TestFalse(FString::Printf(TEXT("%s: rejected"), Label), bAccepted);
		TestEqual(FString::Printf(TEXT("%s: clears output amount"), Label), Amount, 0);
		TestFalse(FString::Printf(TEXT("%s: supplies error"), Label), Error.IsEmpty());
		TestTrue(FString::Printf(TEXT("%s: replaces previous error"), Label), Error != TEXT("previous error"));
	};

	ExpectRejected(TEXT("empty"), TEXT(""));
	ExpectRejected(TEXT("whitespace only"), TEXT(" \t\r\n"));
	ExpectRejected(TEXT("zero"), TEXT("0"));
	ExpectRejected(TEXT("leading zeroes without positive value"), TEXT("0000"));
	ExpectRejected(TEXT("negative"), TEXT("-1"));
	ExpectRejected(TEXT("explicit plus sign"), TEXT("+1"));
	ExpectRejected(TEXT("decimal"), TEXT("1.5"));
	ExpectRejected(TEXT("embedded whitespace"), TEXT("1 2"));
	ExpectRejected(TEXT("letters"), TEXT("abc"));
	ExpectRejected(TEXT("int32 overflow"), TEXT("2147483648"));
	ExpectRejected(TEXT("very long overflowing digits"), FString::ChrN(4096, TEXT('9')));
	ExpectRejected(TEXT("full-width non-ASCII digits"), TEXT("\uFF11\uFF12"));
	ExpectRejected(TEXT("Arabic-Indic non-ASCII digits"), TEXT("\u0661\u0662"));

	ExpectAccepted(TEXT("minimum positive ASCII amount"), TEXT("1"), 1);
	ExpectAccepted(TEXT("ASCII amount"), TEXT("12345"), 12345);
	ExpectAccepted(TEXT("surrounding whitespace"), TEXT(" \t42\r\n"), 42);
	ExpectAccepted(TEXT("leading zeroes"), TEXT("00042"), 42);
	ExpectAccepted(TEXT("int32 maximum"), TEXT("2147483647"), MAX_int32);
	ExpectAccepted(TEXT("int32 maximum with leading zeroes"), TEXT("0002147483647"), MAX_int32);
	return true;
}

#endif
