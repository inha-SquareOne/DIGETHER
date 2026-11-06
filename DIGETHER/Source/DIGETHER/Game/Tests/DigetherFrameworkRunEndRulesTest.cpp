#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Framework/DigetherFrameworkRules.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkRunPlayerLocationTest,
	"DIGETHER.Game.Framework.RunEnd.PlayerLocation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkRunPlayerLocationTest::RunTest(const FString& Parameters)
{
	using DigetherFrameworkRules::MakeRunPlayerLocation;
	// 기획 A3: 문 닫히는 중에도 탑승 가능
	const EDigetherElevatorPhase BoardingPhases[] = {
		EDigetherElevatorPhase::InitialDescent, EDigetherElevatorPhase::BottomWaiting, EDigetherElevatorPhase::DoorClosing };
	const EDigetherElevatorPhase LeftPhases[] = {
		EDigetherElevatorPhase::HubWaiting, EDigetherElevatorPhase::Ascending, EDigetherElevatorPhase::UpperStop,
		EDigetherElevatorPhase::ReturnDescent, EDigetherElevatorPhase::FinalAscent, EDigetherElevatorPhase::Finished };

	for (const EDigetherElevatorPhase Phase : BoardingPhases)
	{
		const FString Name = UEnum::GetValueAsString(Phase);
		TestEqual(Name + TEXT(" active in volume boards"),
			MakeRunPlayerLocation(EDigetherLifeState::Active, true, Phase), EDigetherRunPlayerLocation::Elevator);
		TestEqual(Name + TEXT(" active outside volume stays in mine"),
			MakeRunPlayerLocation(EDigetherLifeState::Active, false, Phase), EDigetherRunPlayerLocation::MineOrDungeon);
	}
	// 상승 확정 뒤 고정 볼륨에 들어온 활동 참가자는 탑승자 아님
	for (const EDigetherElevatorPhase Phase : LeftPhases)
	{
		const FString Name = UEnum::GetValueAsString(Phase);
		TestEqual(Name + TEXT(" active in volume stays in mine"),
			MakeRunPlayerLocation(EDigetherLifeState::Active, true, Phase), EDigetherRunPlayerLocation::MineOrDungeon);
		TestEqual(Name + TEXT(" escaped is at base"),
			MakeRunPlayerLocation(EDigetherLifeState::Escaped, true, Phase), EDigetherRunPlayerLocation::Base);
	}
	TestEqual(TEXT("Escaped wins over boarding phase"),
		MakeRunPlayerLocation(EDigetherLifeState::Escaped, true, EDigetherElevatorPhase::BottomWaiting),
		EDigetherRunPlayerLocation::Base);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkHiddenRevealsTest,
	"DIGETHER.Game.Framework.RunEnd.HiddenReveals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkHiddenRevealsTest::RunTest(const FString& Parameters)
{
	// 숫자·식별자는 테스트 픽스처, 기획 값 아님
	FDigetherWalletState First;
	First.HiddenLifetimeCredits = 40;
	First.BalanceCredits = 999;
	FDigetherWalletState Second;
	Second.HiddenLifetimeCredits = 0;

	TArray<FDigetherHiddenRevealEntry> Reveals;
	TestTrue(TEXT("Valid reveals build"),
		DigetherFrameworkRules::TryMakeHiddenReveals({ TEXT("256"), TEXT("257") }, { First, Second }, Reveals));
	if (TestEqual(TEXT("One entry per player"), Reveals.Num(), 2))
	{
		TestEqual(TEXT("First key"), Reveals[0].PlayerKey, FString(TEXT("256")));
		TestEqual(TEXT("First hidden"), Reveals[0].HiddenLifetimeCredits, 40);
		TestEqual(TEXT("Second hidden"), Reveals[1].HiddenLifetimeCredits, 0);
	}

	TArray<FDigetherHiddenRevealEntry> Preserved = Reveals;
	TestFalse(TEXT("Length mismatch rejected"),
		DigetherFrameworkRules::TryMakeHiddenReveals({ TEXT("256") }, { First, Second }, Preserved));
	TestFalse(TEXT("Empty key rejected"),
		DigetherFrameworkRules::TryMakeHiddenReveals({ TEXT("") }, { First }, Preserved));
	TestFalse(TEXT("Duplicate key rejected"),
		DigetherFrameworkRules::TryMakeHiddenReveals({ TEXT("256"), TEXT("256") }, { First, Second }, Preserved));
	FDigetherWalletState Negative;
	Negative.HiddenLifetimeCredits = -1;
	TestFalse(TEXT("Negative hidden rejected"),
		DigetherFrameworkRules::TryMakeHiddenReveals({ TEXT("1") }, { Negative }, Preserved));
	TestEqual(TEXT("Output preserved on failure"), Preserved.Num(), 2);
	return true;
}

#endif
