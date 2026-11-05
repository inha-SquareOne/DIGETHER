#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Monster/DigetherMonsterHealthRules.h"
#include "Game/Tests/DigetherTestValues.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherMonsterHealthRulesTest,
	"DIGETHER.Game.Monster.HealthRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherMonsterHealthRulesTest::RunTest(const FString& Parameters)
{
	using namespace DigetherMonsterHealthRules;

	// 테스트 픽스처, 몬스터 기획 체력·피해량 아님
	constexpr float FixtureMaxHealth = 100.f;
	constexpr float FixtureDamage = 25.f;
	FDigetherMonsterHealthState State;
	TestTrue(TEXT("처치 가능 상태 초기화"), TryCreateState(true, FixtureMaxHealth, State));
	TestTrue(TEXT("처치 가능 여부 유지"), State.bKillable);
	TestEqual(TEXT("최대 체력 복사"), State.MaxHealth, FixtureMaxHealth);
	TestEqual(TEXT("초기 체력은 최대 체력"), State.CurrentHealth, FixtureMaxHealth);
	TestTrue(TEXT("초기 생명 상태는 생존"), State.Life == EDigetherMonsterLife::Alive);

	TestTrue(TEXT("일반 피해 결과"), ApplyDamage(State, FixtureDamage) == EDigetherMonsterDamageResult::Damaged);
	TestEqual(TEXT("일반 피해 차감"), State.CurrentHealth, FixtureMaxHealth - FixtureDamage);
	TestTrue(TEXT("체력이 남으면 생존"), State.Life == EDigetherMonsterLife::Alive);
	TestTrue(TEXT("정확히 남은 체력만큼 피해로 사망"),
		ApplyDamage(State, State.CurrentHealth) == EDigetherMonsterDamageResult::Killed);
	TestEqual(TEXT("정확히 치명적인 피해 후 체력 0"), State.CurrentHealth, 0.f);
	TestTrue(TEXT("생명 상태가 사망으로 전이"), State.Life == EDigetherMonsterLife::Dead);
	TestTrue(TEXT("사망 후 피해는 다시 사망시키지 않음"),
		ApplyDamage(State, FixtureDamage) == EDigetherMonsterDamageResult::IgnoredAlreadyDead);
	TestTrue(TEXT("사망 여부를 잘못된 피해량보다 먼저 검사"),
		ApplyDamage(State, DigetherTestValues::NaN()) == EDigetherMonsterDamageResult::IgnoredAlreadyDead);
	TestEqual(TEXT("사망 후 체력 유지"), State.CurrentHealth, 0.f);

	TryCreateState(true, FixtureMaxHealth, State);
	TestTrue(TEXT("과잉 피해도 사망 한 번"),
		ApplyDamage(State, FixtureMaxHealth + FixtureDamage) == EDigetherMonsterDamageResult::Killed);
	TestEqual(TEXT("과잉 피해는 음수 체력이 되지 않음"), State.CurrentHealth, 0.f);

	TryCreateState(true, FixtureMaxHealth, State);
	int32 DeathTransitions = 0;
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		if (ApplyDamage(State, FixtureDamage) == EDigetherMonsterDamageResult::Killed)
		{
			++DeathTransitions;
		}
	}
	TestEqual(TEXT("연속 피해에서 사망 전이는 한 번"), DeathTransitions, 1);
	TestEqual(TEXT("연속 피해 후 체력 0"), State.CurrentHealth, 0.f);
	TestTrue(TEXT("연속 피해 후 사망 상태 유지"), State.Life == EDigetherMonsterLife::Dead);

	const float InvalidAmounts[] = { 0.f, -FixtureDamage, DigetherTestValues::NaN(), DigetherTestValues::Infinity(), -DigetherTestValues::Infinity() };
	TryCreateState(true, FixtureMaxHealth, State);
	for (const float Amount : InvalidAmounts)
	{
		TestTrue(TEXT("0·음수·NaN·무한대 피해 무시"),
			ApplyDamage(State, Amount) == EDigetherMonsterDamageResult::IgnoredInvalidAmount);
		TestEqual(TEXT("잘못된 피해는 회복이나 차감을 하지 않음"), State.CurrentHealth, FixtureMaxHealth);
		TestTrue(TEXT("잘못된 피해는 생명 상태를 바꾸지 않음"), State.Life == EDigetherMonsterLife::Alive);
	}

	TestTrue(TEXT("불사는 양수 최대 체력도 초기화 가능"), TryCreateState(false, FixtureMaxHealth, State));
	TestFalse(TEXT("불사의 처치 가능 여부"), State.bKillable);
	TestEqual(TEXT("불사의 최대 체력 정규화"), State.MaxHealth, 0.f);
	TestEqual(TEXT("불사의 현재 체력 정규화"), State.CurrentHealth, 0.f);
	TestTrue(TEXT("불사는 유효한 피해도 무시"),
		ApplyDamage(State, FixtureDamage) == EDigetherMonsterDamageResult::IgnoredNotKillable);
	for (const float Amount : InvalidAmounts)
	{
		TestTrue(TEXT("처치 가능 여부를 잘못된 피해량보다 먼저 검사"),
			ApplyDamage(State, Amount) == EDigetherMonsterDamageResult::IgnoredNotKillable);
	}
	TestTrue(TEXT("불사는 피해 후에도 생존"), State.Life == EDigetherMonsterLife::Alive);
	TestEqual(TEXT("불사는 피해 후에도 체력 0"), State.CurrentHealth, 0.f);
	TestTrue(TEXT("체력 0의 불사 초기화"), TryCreateState(false, 0.f, State));

	const float InvalidMaxHealth[] = { 0.f, -FixtureMaxHealth, DigetherTestValues::NaN(), DigetherTestValues::Infinity(), -DigetherTestValues::Infinity() };
	for (const float MaxHealth : InvalidMaxHealth)
	{
		TryCreateState(true, FixtureMaxHealth, State);
		TestFalse(TEXT("잘못된 처치 가능 최대 체력은 초기화 실패"), TryCreateState(true, MaxHealth, State));
		TestFalse(TEXT("초기화 실패는 처치 불가로 복원"), State.bKillable);
		TestEqual(TEXT("초기화 실패는 현재 체력 0"), State.CurrentHealth, 0.f);
		TestEqual(TEXT("초기화 실패는 최대 체력 0"), State.MaxHealth, 0.f);
		TestTrue(TEXT("초기화 실패는 생존 상태"), State.Life == EDigetherMonsterLife::Alive);
		TestTrue(TEXT("초기화 실패 상태는 피해로 사망하지 않음"),
			ApplyDamage(State, FixtureDamage) == EDigetherMonsterDamageResult::IgnoredNotKillable);
	}
	const float InvalidNonKillableMaxHealth[] = { -FixtureMaxHealth, DigetherTestValues::NaN(), DigetherTestValues::Infinity(), -DigetherTestValues::Infinity() };
	for (const float MaxHealth : InvalidNonKillableMaxHealth)
	{
		TestFalse(TEXT("불사도 손상된 최대 체력은 초기화 실패"), TryCreateState(false, MaxHealth, State));
		TestFalse(TEXT("불사 초기화 실패도 처치 불가 유지"), State.bKillable);
		TestEqual(TEXT("불사 초기화 실패도 안전한 체력 유지"), State.CurrentHealth, 0.f);
	}

	const float InvalidCurrentHealth[] = { 0.f, -FixtureDamage, FixtureMaxHealth + FixtureDamage, DigetherTestValues::NaN(), DigetherTestValues::Infinity(), -DigetherTestValues::Infinity() };
	for (const float CurrentHealth : InvalidCurrentHealth)
	{
		TryCreateState(true, FixtureMaxHealth, State);
		State.CurrentHealth = CurrentHealth;
		TestTrue(TEXT("손상된 현재 체력 상태에는 피해 적용 거부"),
			ApplyDamage(State, FixtureDamage) == EDigetherMonsterDamageResult::IgnoredInvalidState);
		TestTrue(TEXT("손상된 상태 거부는 사망 전이를 만들지 않음"), State.Life == EDigetherMonsterLife::Alive);
	}
	for (const float MaxHealth : InvalidMaxHealth)
	{
		TryCreateState(true, FixtureMaxHealth, State);
		State.MaxHealth = MaxHealth;
		TestTrue(TEXT("손상된 최대 체력 상태에는 피해 적용 거부"),
			ApplyDamage(State, FixtureDamage) == EDigetherMonsterDamageResult::IgnoredInvalidState);
		TestEqual(TEXT("손상된 상태 거부는 현재 체력 보존"), State.CurrentHealth, FixtureMaxHealth);
	}
	TryCreateState(true, FixtureMaxHealth, State);
	State.Life = static_cast<EDigetherMonsterLife>(MAX_uint8);
	TestTrue(TEXT("정의되지 않은 생명 상태에는 피해 적용 거부"),
		ApplyDamage(State, FixtureDamage) == EDigetherMonsterDamageResult::IgnoredInvalidState);
	return true;
}

#endif
