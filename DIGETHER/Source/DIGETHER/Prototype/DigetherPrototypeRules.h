#pragma once

#include "CoreMinimal.h"

namespace DigetherPrototypeRules
{
	inline int32 GeneralShare(int32 CargoValue, int32 Departures, int32 Survivors)
	{
		return CargoValue >= 0 && Departures > 0 && Survivors > 0 ? CargoValue / Departures : 0;
	}

	// 잔고의 Percent%를 내림한 값. 비율은 튜닝 프로필에서 받는다(D25 시험값은 DA에 있다).
	inline int32 PercentOf(int32 Balance, int32 Percent)
	{
		const int64 Scaled = static_cast<int64>(FMath::Max(0, Balance)) * FMath::Clamp(Percent, 0, 100);
		return static_cast<int32>(Scaled / 100);
	}

	inline int32 DeathLoss(int32 Balance, int32 LossPercent)
	{
		return PercentOf(Balance, LossPercent);
	}

	// 기준은 사망 직전 잔고다. 손실액 기준으로 바꿀지는 PQ-14에서 정한다.
	inline int32 CorpseRefund(int32 DeathBalance, int32 RefundPercent)
	{
		return PercentOf(DeathBalance, RefundPercent);
	}
}
