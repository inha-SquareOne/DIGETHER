#include "Game/Economy/DigetherDepositInput.h"

namespace
{
	constexpr int32 DecimalBase = 10;
}

EDigetherEconomyResult FDigetherDepositInput::TryParseAmount(const FString& Input, int32& OutAmount)
{
	OutAmount = 0;
	const FString Trimmed = Input.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return EDigetherEconomyResult::DepositInputEmpty;
	}

	int32 Value = 0;
	for (int32 Index = 0; Index < Trimmed.Len(); ++Index)
	{
		const TCHAR Character = Trimmed[Index];
		if (Character < TEXT('0') || Character > TEXT('9'))
		{
			return EDigetherEconomyResult::DepositInputInvalidCharacter;
		}

		const int32 Digit = static_cast<int32>(Character - TEXT('0'));
		if (Value > (MAX_int32 - Digit) / DecimalBase)
		{
			return EDigetherEconomyResult::DepositInputOverflow;
		}
		Value = Value * DecimalBase + Digit;
	}

	if (Value <= 0)
	{
		return EDigetherEconomyResult::InvalidAmount;
	}

	OutAmount = Value;
	return EDigetherEconomyResult::Success;
}
