#pragma once

#include "CoreMinimal.h"

namespace DigetherPrototypeRules
{
	inline int32 GeneralShare(int32 CargoValue, int32 Departures, int32 Survivors)
	{
		return CargoValue >= 0 && Departures > 0 && Survivors > 0 ? CargoValue / Departures : 0;
	}

	inline int32 CorpseRefund(int32 DeathBalance)
	{
		return FMath::Max(0, DeathBalance) / 2;
	}
}
