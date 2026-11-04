#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Monster/DigetherMonsterDefinition.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

#include <cmath>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherMonsterDefinitionTest,
	"DIGETHER.Game.Monster.Definition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherMonsterDefinitionTest::RunTest(const FString& Parameters)
{
	UDigetherMonsterDefinition* Definition = NewObject<UDigetherMonsterDefinition>(GetTransientPackage(), NAME_None);
	if (!TestNotNull(TEXT("임시 몬스터 정의 생성"), Definition))
	{
		return false;
	}

	TArray<FText> Problems;
	TestFalse(TEXT("기본 정의는 불사"), Definition->bKillable);
	TestEqual(TEXT("미정 체력 기본값은 0"), Definition->MaxHealth, 0.f);
	TestTrue(TEXT("기본 불사 정의는 유효"), Definition->CollectProblems(Problems));
	const FPrimaryAssetId AssetId = Definition->GetPrimaryAssetId();
	TestEqual(TEXT("Primary Asset 타입 이름"), UDigetherMonsterDefinition::PrimaryAssetType, FPrimaryAssetType(TEXT("DigetherMonster")));
	TestEqual(TEXT("Primary Asset 타입 유지"), AssetId.PrimaryAssetType, UDigetherMonsterDefinition::PrimaryAssetType);
	TestEqual(TEXT("Primary Asset 이름은 정의 이름"), AssetId.PrimaryAssetName, Definition->GetFName());

	// 테스트 픽스처이며 몬스터의 최종 기획 체력이 아니다.
	constexpr float FixtureMaxHealth = 100.f;
	Definition->bKillable = true;
	TestFalse(TEXT("처치 가능한 정의의 0 체력 거부"), Definition->CollectProblems(Problems));
	Definition->MaxHealth = FixtureMaxHealth;
	Problems.Reset();
	TestTrue(TEXT("처치 가능한 정의의 양수 유한 체력 허용"), Definition->CollectProblems(Problems));
	TestEqual(TEXT("정상 정의는 문제를 추가하지 않음"), Problems.Num(), 0);

	const FText ExistingProblem = FText::FromString(TEXT("호출자가 이미 수집한 문제"));
	Problems.Add(ExistingProblem);
	TestTrue(TEXT("기존 문제가 있어도 현재 정의가 유효하면 통과"), Definition->CollectProblems(Problems));
	TestEqual(TEXT("기존 문제 개수 보존"), Problems.Num(), 1);
	TestTrue(TEXT("기존 문제 본문 보존"), Problems[0].EqualTo(ExistingProblem));

	const float InvalidMaxHealth[] = { -FixtureMaxHealth, NAN, INFINITY, -INFINITY };
	const bool KillableValues[] = { false, true };
	for (const bool bKillable : KillableValues)
	{
		Definition->bKillable = bKillable;
		for (const float MaxHealth : InvalidMaxHealth)
		{
			Definition->MaxHealth = MaxHealth;
			Problems.Reset();
			Problems.Add(ExistingProblem);
			TestFalse(TEXT("음수·NaN·무한대 최대 체력 거부"), Definition->CollectProblems(Problems));
			TestEqual(TEXT("잘못된 체력 문제는 기존 목록 뒤에 추가"), Problems.Num(), 2);
			TestTrue(TEXT("검사 실패도 기존 문제 본문 보존"), Problems[0].EqualTo(ExistingProblem));
		}
	}

	Definition->bKillable = false;
	Definition->MaxHealth = FixtureMaxHealth;
	Problems.Reset();
	TestTrue(TEXT("불사 정의의 미사용 양수 체력은 오류가 아님"), Definition->CollectProblems(Problems));
	TestEqual(TEXT("불사의 미사용 체력은 문제를 추가하지 않음"), Problems.Num(), 0);
	return true;
}

#endif
