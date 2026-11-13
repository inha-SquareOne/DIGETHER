#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Player/DigetherStaminaRules.h"
#include "Misc/AutomationTest.h"

namespace
{
	// 프로토타입 시험값과 같은 수치, 규칙 계산 확인용이며 표의 실제 값과 무관함
	FDigetherStaminaParams MakeStaminaParams()
	{
		FDigetherStaminaParams Params;
		Params.MaxStamina = 100.f;
		Params.SprintDrainPerSec = 20.f;
		Params.JumpStaminaCost = 10.f;
		Params.StaminaRegenPerSec = 12.f;
		Params.StaminaRegenDelaySec = 1.f;
		Params.SprintResumeThreshold = 20.f;
		return Params;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherStaminaRulesTest,
	"DIGETHER.Game.Player.Stamina.Rules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherStaminaRulesTest::RunTest(const FString& Parameters)
{
	const FDigetherStaminaParams Params = MakeStaminaParams();

	// 시작 상태
	FDigetherStaminaState State = FDigetherStaminaRules::MakeFull(Params);
	TestEqual(TEXT("시작은 최대치"), State.Current, 100.f);
	TestTrue(TEXT("시작 상태는 달리기 가능"), FDigetherStaminaRules::CanSprint(State));

	// 달리기 소모: 초당 20
	TestTrue(TEXT("1초 달리기는 계속 가능"), FDigetherStaminaRules::Advance(State, Params, 1.f, true));
	TestEqual(TEXT("1초 달리기 후 80"), State.Current, 80.f);
	TestEqual(TEXT("소모하면 회복 대기 시간이 0으로 돌아간다"), State.SecondsSinceUse, 0.f);

	// 회복 대기: 소모 직후 1초 동안은 회복하지 않음
	FDigetherStaminaRules::Advance(State, Params, 0.5f, false);
	TestEqual(TEXT("대기 중에는 회복하지 않는다"), State.Current, 80.f);
	FDigetherStaminaRules::Advance(State, Params, 0.5f, false);
	TestEqual(TEXT("대기가 정확히 끝난 순간에도 회복은 아직 0"), State.Current, 80.f);
	FDigetherStaminaRules::Advance(State, Params, 1.f, false);
	TestEqual(TEXT("대기 후 1초 회복 12"), State.Current, 92.f);

	// 큰 DeltaTime: 대기 시간을 건너뛰지 않고 최대치를 넘지 않음
	FDigetherStaminaState Big = FDigetherStaminaRules::MakeFull(Params);
	FDigetherStaminaRules::Advance(Big, Params, 2.f, true);
	TestEqual(TEXT("2초 달리기 후 60"), Big.Current, 60.f);
	FDigetherStaminaRules::Advance(Big, Params, 2.f, false);
	TestEqual(TEXT("2초 쉬면 대기 1초 + 회복 1초분(12)"), Big.Current, 72.f);
	FDigetherStaminaRules::Advance(Big, Params, 100.f, false);
	TestEqual(TEXT("오래 쉬어도 최대치 초과 없음"), Big.Current, 100.f);

	// 0 도달: 달리기 종료와 잠금
	FDigetherStaminaState Empty = FDigetherStaminaRules::MakeFull(Params);
	TestTrue(TEXT("4.9초까지는 계속 가능"), FDigetherStaminaRules::Advance(Empty, Params, 4.9f, true));
	TestFalse(TEXT("0에 닿으면 계속 불가"), FDigetherStaminaRules::Advance(Empty, Params, 1.f, true));
	TestEqual(TEXT("0 아래로 내려가지 않는다"), Empty.Current, 0.f);
	TestTrue(TEXT("0에 닿으면 달리기 잠금"), Empty.bSprintLocked);
	TestFalse(TEXT("잠기면 달리기 불가"), FDigetherStaminaRules::CanSprint(Empty));

	// 재달리기 임계: 20 이상 회복해야 잠금이 풀림
	TestFalse(TEXT("잠긴 채 달리기를 요청하면 거절"), FDigetherStaminaRules::Advance(Empty, Params, 0.1f, true));
	FDigetherStaminaRules::Advance(Empty, Params, 1.9f, false);
	TestEqual(TEXT("요청 거절 0.1초 + 1.9초 쉼 = 대기 1초 후 1초 회복 12"), Empty.Current, 12.f);
	TestTrue(TEXT("임계 미만이면 아직 잠김"), Empty.bSprintLocked);
	FDigetherStaminaRules::Advance(Empty, Params, 1.f, false);
	TestEqual(TEXT("24로 임계 이상"), Empty.Current, 24.f);
	TestFalse(TEXT("임계 이상이면 잠금 해제"), Empty.bSprintLocked);
	TestTrue(TEXT("잠금 해제 후 달리기 가능"), FDigetherStaminaRules::CanSprint(Empty));

	// 점프
	FDigetherStaminaState Jumper = FDigetherStaminaRules::MakeFull(Params);
	TestTrue(TEXT("가득 차면 점프 가능"), FDigetherStaminaRules::TryConsumeJump(Jumper, Params));
	TestEqual(TEXT("점프 1회 10 소모"), Jumper.Current, 90.f);
	TestEqual(TEXT("점프도 회복 대기를 초기화"), Jumper.SecondsSinceUse, 0.f);
	Jumper.Current = 9.f;
	TestFalse(TEXT("부족하면 점프 불가"), FDigetherStaminaRules::CanJump(Jumper, Params));
	TestFalse(TEXT("부족하면 소모 시도도 거절"), FDigetherStaminaRules::TryConsumeJump(Jumper, Params));
	TestEqual(TEXT("거절되면 소모 없음"), Jumper.Current, 9.f);
	Jumper.Current = 10.f;
	TestTrue(TEXT("정확히 소모량이면 점프 가능"), FDigetherStaminaRules::TryConsumeJump(Jumper, Params));
	TestEqual(TEXT("정확히 0이 된다"), Jumper.Current, 0.f);

	// 잘못된 DeltaTime
	FDigetherStaminaState Safe = FDigetherStaminaRules::MakeFull(Params);
	FDigetherStaminaRules::Advance(Safe, Params, 0.f, true);
	FDigetherStaminaRules::Advance(Safe, Params, -1.f, true);
	TestEqual(TEXT("0·음수 DeltaTime은 상태를 바꾸지 않는다"), Safe.Current, 100.f);

	return true;
}

#endif
