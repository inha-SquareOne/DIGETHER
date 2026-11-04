#pragma once

#include "Game/Economy/DigetherEconomyTypes.h"

class DIGETHER_API FDigetherDepositInput
{
public:
	// 앞뒤 공백을 제외한 ASCII 숫자만 허용하며 실패 시 출력 금액을 0으로 지운다.
	static EDigetherEconomyResult TryParseAmount(const FString& Input, int32& OutAmount);
};
