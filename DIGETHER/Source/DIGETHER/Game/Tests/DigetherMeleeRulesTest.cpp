#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Combat/DigetherMeleeRules.h"
#include "Game/Tests/DigetherTestValues.h"
#include "Misc/AutomationTest.h"

namespace
{
	// 규칙 계산 확인용 값, 삽 BP의 실제 값과 무관하다
	FDigetherMeleeHitParams MakeParams()
	{
		FDigetherMeleeHitParams Params;
		Params.Damage = 10.f;
		Params.ForwardOffsetCm = 20.f;
		Params.RangeCm = 100.f;
		Params.HalfWidthCm = 30.f;
		Params.HalfHeightCm = 40.f;
		return Params;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherMeleeRulesTest,
	"DIGETHER.Game.Combat.Melee.Rules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherMeleeRulesTest::RunTest(const FString& Parameters)
{
	const FDigetherMeleeHitParams Params = MakeParams();

	// 사용 가능 여부
	TestTrue(TEXT("양수 값은 사용 가능"), DigetherMeleeRules::IsUsable(Params));
	TestFalse(TEXT("코드 기본값(0)은 사용 불가"), DigetherMeleeRules::IsUsable(FDigetherMeleeHitParams()));
	TestEqual(TEXT("기본 대상 종류는 Pawn·PhysicsBody 두 개"), FDigetherMeleeHitParams().TargetObjectTypes.Num(), 2);
	{
		FDigetherMeleeHitParams NoTargets = Params;
		NoTargets.TargetObjectTypes.Reset();
		TArray<FString> Problems;
		TestFalse(TEXT("대상 종류가 없으면 사용 불가"), DigetherMeleeRules::IsUsable(NoTargets, &Problems));
		TestEqual(TEXT("문제 한 개 보고"), Problems.Num(), 1);
	}
	{
		FDigetherMeleeHitParams Negative = Params;
		Negative.ForwardOffsetCm = -1.f;
		TestFalse(TEXT("음수 시작 거리는 사용 불가"), DigetherMeleeRules::IsUsable(Negative));
	}

	// 상자 위치: 정면(+X)을 보면 중심은 시작 거리 + 사거리 절반만큼 앞
	{
		const FDigetherMeleeHitBox Box = DigetherMeleeRules::ComputeHitBox(FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, Params);
		TestTrue(TEXT("정면 중심"), Box.Center.Equals(FVector(70.f, 0.f, 100.f), 0.01f));
		TestTrue(TEXT("반크기"), Box.HalfExtent.Equals(FVector(50.f, 30.f, 40.f), 0.01f));
	}
	// 오른쪽(요 90도)을 보면 +Y 방향으로 놓인다
	{
		const FDigetherMeleeHitBox Box = DigetherMeleeRules::ComputeHitBox(FVector::ZeroVector, FRotator(0.f, 90.f, 0.f), Params);
		TestTrue(TEXT("요 90도 중심"), Box.Center.Equals(FVector(0.f, 70.f, 0.f), 0.01f));
		TestTrue(TEXT("상자 X축이 시선 방향"), Box.Rotation.GetForwardVector().Equals(FVector(0.f, 1.f, 0.f), 0.001f));
	}
	// 아래(피치 -90도)를 보면 아래로 놓인다(바닥 근처 대상 판정)
	{
		const FDigetherMeleeHitBox Box = DigetherMeleeRules::ComputeHitBox(FVector::ZeroVector, FRotator(-90.f, 0.f, 0.f), Params);
		TestTrue(TEXT("아래 중심"), Box.Center.Equals(FVector(0.f, 0.f, -70.f), 0.01f));
	}

	// 피해 배율
	TestEqual(TEXT("배율 1"), DigetherMeleeRules::ComputeDamage(Params, 1.f), 10.f);
	TestEqual(TEXT("배율 1.5"), DigetherMeleeRules::ComputeDamage(Params, 1.5f), 15.f);
	TestEqual(TEXT("배율 0"), DigetherMeleeRules::ComputeDamage(Params, 0.f), 0.f);
	TestEqual(TEXT("음수 배율은 0"), DigetherMeleeRules::ComputeDamage(Params, -2.f), 0.f);
	TestEqual(TEXT("NaN 배율은 0"), DigetherMeleeRules::ComputeDamage(Params, DigetherTestValues::NaN()), 0.f);

	return true;
}

#endif
