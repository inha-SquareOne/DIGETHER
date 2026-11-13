#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Player/DigetherPlayerHealthRules.h"
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherPlayerHealthRulesTest,
	"DIGETHER.Game.Player.Health.Rules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherPlayerHealthRulesTest::RunTest(const FString& Parameters)
{
	// 시작: D1 기준 시험값 100으로 가득
	FDigetherPlayerHealthState State;
	TestTrue(TEXT("최대 100으로 생성"), FDigetherPlayerHealthRules::TryMakeFull(100.f, State));
	TestEqual(TEXT("시작은 최대치"), State.Current, 100.f);
	TestFalse(TEXT("시작은 생존"), State.bDead);
	TestTrue(TEXT("시작 상태는 올바름"), FDigetherPlayerHealthRules::IsValidState(State));

	// 잘못된 최대치
	FDigetherPlayerHealthState Bad;
	TestFalse(TEXT("최대 0은 거절"), FDigetherPlayerHealthRules::TryMakeFull(0.f, Bad));
	TestFalse(TEXT("최대 음수는 거절"), FDigetherPlayerHealthRules::TryMakeFull(-5.f, Bad));
	TestFalse(TEXT("최대 NaN은 거절"), FDigetherPlayerHealthRules::TryMakeFull(std::numeric_limits<float>::quiet_NaN(), Bad));
	TestTrue(TEXT("미초기화 상태 피해는 무시"), FDigetherPlayerHealthRules::ApplyDamage(Bad, 10.f) == EDigetherPlayerDamageResult::IgnoredInvalidState);

	// 피해: 기본 공격 12.5 가정
	TestTrue(TEXT("12.5 피해는 Damaged"), FDigetherPlayerHealthRules::ApplyDamage(State, 12.5f) == EDigetherPlayerDamageResult::Damaged);
	TestEqual(TEXT("12.5 피해 후 87.5"), State.Current, 87.5f);

	// 잘못된 피해량은 상태를 바꾸지 않음
	TestTrue(TEXT("0 피해 무시"), FDigetherPlayerHealthRules::ApplyDamage(State, 0.f) == EDigetherPlayerDamageResult::IgnoredInvalidAmount);
	TestTrue(TEXT("음수 피해(회복) 무시"), FDigetherPlayerHealthRules::ApplyDamage(State, -20.f) == EDigetherPlayerDamageResult::IgnoredInvalidAmount);
	TestTrue(TEXT("NaN 피해 무시"), FDigetherPlayerHealthRules::ApplyDamage(State, std::numeric_limits<float>::quiet_NaN()) == EDigetherPlayerDamageResult::IgnoredInvalidAmount);
	TestEqual(TEXT("무시 후 체력 그대로"), State.Current, 87.5f);

	// 초과 피해: 0 아래로 내려가지 않고 사망
	TestTrue(TEXT("초과 피해는 Killed"), FDigetherPlayerHealthRules::ApplyDamage(State, 500.f) == EDigetherPlayerDamageResult::Killed);
	TestEqual(TEXT("사망 시 0"), State.Current, 0.f);
	TestTrue(TEXT("사망 표시"), State.bDead);
	TestTrue(TEXT("사망 상태도 올바름"), FDigetherPlayerHealthRules::IsValidState(State));
	TestTrue(TEXT("사망 후 피해 무시"), FDigetherPlayerHealthRules::ApplyDamage(State, 10.f) == EDigetherPlayerDamageResult::IgnoredDead);

	// 정확히 0이 되는 피해도 사망
	FDigetherPlayerHealthState Exact;
	FDigetherPlayerHealthRules::TryMakeFull(100.f, Exact);
	TestTrue(TEXT("정확히 100 피해는 Killed"), FDigetherPlayerHealthRules::ApplyDamage(Exact, 100.f) == EDigetherPlayerDamageResult::Killed);

	// 8회 x 12.5 = 100: 8번째에 사망
	FDigetherPlayerHealthState Eight;
	FDigetherPlayerHealthRules::TryMakeFull(100.f, Eight);
	for (int32 Hit = 1; Hit <= 7; ++Hit)
	{
		FDigetherPlayerHealthRules::ApplyDamage(Eight, 12.5f);
	}
	TestFalse(TEXT("7회까지 생존"), Eight.bDead);
	TestTrue(TEXT("8회째 사망"), FDigetherPlayerHealthRules::ApplyDamage(Eight, 12.5f) == EDigetherPlayerDamageResult::Killed);

	// 형태가 깨진 상태 거절
	FDigetherPlayerHealthState Broken;
	Broken.Max = 100.f;
	Broken.Current = 150.f;
	TestFalse(TEXT("최대 초과 체력은 잘못된 상태"), FDigetherPlayerHealthRules::IsValidState(Broken));
	TestTrue(TEXT("잘못된 상태 피해 무시"), FDigetherPlayerHealthRules::ApplyDamage(Broken, 10.f) == EDigetherPlayerDamageResult::IgnoredInvalidState);
	Broken.Current = 0.f;
	TestFalse(TEXT("생존인데 0은 잘못된 상태"), FDigetherPlayerHealthRules::IsValidState(Broken));

	return true;
}

#endif
