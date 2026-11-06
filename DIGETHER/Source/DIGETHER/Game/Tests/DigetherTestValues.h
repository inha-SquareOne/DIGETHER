#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

// 테스트용 비유한 입력값
// NAN·INFINITY 매크로는 상수 오버플로 식이라 MSVC C4756(경고=오류), volatile 읽기로 상수 접기 차단
namespace DigetherTestValues
{
	template <typename T = float>
	T NaN()
	{
		volatile T Value = std::numeric_limits<T>::quiet_NaN();
		return Value;
	}

	template <typename T = float>
	T Infinity()
	{
		volatile T Value = std::numeric_limits<T>::infinity();
		return Value;
	}
}

#endif
