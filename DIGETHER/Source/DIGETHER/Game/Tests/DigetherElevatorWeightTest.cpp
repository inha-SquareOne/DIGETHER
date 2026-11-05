#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Elevator/DigetherElevatorRules.h"
#include "Misc/AutomationTest.h"

#include <cmath>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherElevatorWeightTest,
	"DIGETHER.Game.Elevator.Weight", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherElevatorWeightTest::RunTest(const FString& Parameters)
{
	using namespace DigetherElevatorRules;
	// 테스트 픽스처이며 최종 기획 무게·용량·단계 비율이 아니다.
	FDigetherElevatorTuning Tuning;
	Tuning.LightMaxCapacityRatio = 0.25f;
	Tuning.MediumMaxCapacityRatio = 0.75f;
	constexpr float CapacityKg = 100.f;
	TestTrue(TEXT("명시한 시험 튜닝 유효"), IsValidTuning(Tuning));
	TestTrue(TEXT("양수 용량·운행 횟수·시간 유효"), IsValidRunParameters(CapacityKg, 2, 4.f));

	FDigetherElevatorLoadEntry Entry;
	Entry.Kind = EDigetherElevatorLoadKind::Player;
	Entry.WeightKg = 80.f;
	TestEqual(TEXT("플레이어 몸무게 제외"), SanitizeWeight(Entry), 0.f);
	Entry.Kind = EDigetherElevatorLoadKind::Corpse;
	TestEqual(TEXT("시체 몸무게 제외"), SanitizeWeight(Entry), 0.f);
	Entry.Kind = EDigetherElevatorLoadKind::CarriedItem;
	Entry.WeightKg = 7.5f;
	TestEqual(TEXT("플레이어 소지품 무게 포함"), SanitizeWeight(Entry), 7.5f);
	TArray<FDigetherElevatorLoadEntry> Entries;
	Entries.Add(Entry);
	Entry.Kind = EDigetherElevatorLoadKind::Cargo;
	Entry.WeightKg = 12.5f;
	Entries.Add(Entry);
	Entry.Kind = EDigetherElevatorLoadKind::Player;
	Entry.WeightKg = 80.f;
	Entries.Add(Entry);
	Entry.Kind = EDigetherElevatorLoadKind::Corpse;
	Entries.Add(Entry);
	const float InvalidWeights[] = { -1.f, NAN, INFINITY, -INFINITY };
	for (const EDigetherElevatorLoadKind Kind : { EDigetherElevatorLoadKind::CarriedItem, EDigetherElevatorLoadKind::Cargo })
	{
		Entry.Kind = Kind;
		for (const float Weight : InvalidWeights)
		{
			Entry.WeightKg = Weight;
			TestEqual(TEXT("음수·NaN·무한대 적재 무게는 0"), SanitizeWeight(Entry), 0.f);
			Entries.Add(Entry);
		}
	}
	TestEqual(TEXT("몸무게·손상값을 제외하고 소지품과 화물만 합산"), SumWeight(Entries), 20.0);
	Entry.Kind = static_cast<EDigetherElevatorLoadKind>(MAX_uint8);
	Entry.WeightKg = 50.f;
	TestEqual(TEXT("정의되지 않은 적재 종류의 무게 제외"), SanitizeWeight(Entry), 0.f);
	const FDigetherElevatorWeightSummary Summary = EvaluateLoad(Entries, CapacityKg, Tuning);
	TestEqual(TEXT("적재 요약 무게 일치"), Summary.TotalWeightKg, 20.0);
	TestEqual(TEXT("적재 요약 단계 일치"), Summary.Stage, EDigetherElevatorWeightStage::Light);
	TestEqual(TEXT("빈 적재 단계"), CalculateWeightStage(0.0, CapacityKg, Tuning), EDigetherElevatorWeightStage::Empty);

	const double LightBoundary = 25.0;
	TestEqual(TEXT("가벼움 경계 바로 아래"), CalculateWeightStage(std::nextafter(LightBoundary, 0.0), CapacityKg, Tuning), EDigetherElevatorWeightStage::Light);
	TestEqual(TEXT("가벼움 경계 정확히 같음"), CalculateWeightStage(LightBoundary, CapacityKg, Tuning), EDigetherElevatorWeightStage::Light);
	TestEqual(TEXT("가벼움 경계 바로 위"), CalculateWeightStage(std::nextafter(LightBoundary, INFINITY), CapacityKg, Tuning), EDigetherElevatorWeightStage::Medium);
	const double MediumBoundary = 75.0;
	TestEqual(TEXT("보통 경계 바로 아래"), CalculateWeightStage(std::nextafter(MediumBoundary, 0.0), CapacityKg, Tuning), EDigetherElevatorWeightStage::Medium);
	TestEqual(TEXT("보통 경계 정확히 같음"), CalculateWeightStage(MediumBoundary, CapacityKg, Tuning), EDigetherElevatorWeightStage::Medium);
	TestEqual(TEXT("보통 경계 바로 위"), CalculateWeightStage(std::nextafter(MediumBoundary, INFINITY), CapacityKg, Tuning), EDigetherElevatorWeightStage::Heavy);
	TestEqual(TEXT("용량 바로 아래는 무거움"), CalculateWeightStage(std::nextafter(100.0, 0.0), CapacityKg, Tuning), EDigetherElevatorWeightStage::Heavy);
	TestFalse(TEXT("용량과 같으면 출발 허용"), IsOverCapacity(100.0, CapacityKg));
	TestEqual(TEXT("용량과 같으면 무거움"), CalculateWeightStage(100.0, CapacityKg, Tuning), EDigetherElevatorWeightStage::Heavy);
	TestTrue(TEXT("용량 바로 위면 초과"), IsOverCapacity(std::nextafter(100.0, INFINITY), CapacityKg));
	TestEqual(TEXT("용량 바로 위는 초과 단계"), CalculateWeightStage(std::nextafter(100.0, INFINITY), CapacityKg, Tuning), EDigetherElevatorWeightStage::Over);

	for (const float InvalidValue : { 0.f, -1.f, NAN, INFINITY, -INFINITY })
	{
		TestFalse(TEXT("유효하지 않은 용량 거절"), IsValidRunParameters(InvalidValue, 2, 4.f));
		TestFalse(TEXT("유효하지 않은 운행 시간 거절"), IsValidRunParameters(CapacityKg, 2, InvalidValue));
	}
	TestFalse(TEXT("0회 운행 거절"), IsValidRunParameters(CapacityKg, 0, 4.f));
	// ELV_ASCENT_LIMIT는 2회 확정이다. 1회·3회 비교 설정은 받지 않는다.
	TestFalse(TEXT("1회 운행 거절"), IsValidRunParameters(CapacityKg, 1, 4.f));
	TestFalse(TEXT("3회 운행 거절"), IsValidRunParameters(CapacityKg, 3, 4.f));
	TestFalse(TEXT("음수 운행 횟수 거절"), IsValidRunParameters(CapacityKg, -1, 4.f));
	FDigetherElevatorTuning InvalidTuning = Tuning;
	InvalidTuning.LightMaxCapacityRatio = 0.f;
	TestFalse(TEXT("가벼움 비율은 0 초과"), IsValidTuning(InvalidTuning));
	InvalidTuning = Tuning;
	InvalidTuning.MediumMaxCapacityRatio = Tuning.LightMaxCapacityRatio;
	TestFalse(TEXT("단계 비율은 엄격히 증가"), IsValidTuning(InvalidTuning));
	InvalidTuning = Tuning;
	InvalidTuning.MediumMaxCapacityRatio = 1.f;
	TestFalse(TEXT("보통 비율은 1 미만"), IsValidTuning(InvalidTuning));
	InvalidTuning = Tuning;
	InvalidTuning.AscentHoldSeconds = NAN;
	TestFalse(TEXT("NaN 홀드 시간 거절"), IsValidTuning(InvalidTuning));
	InvalidTuning = Tuning;
	InvalidTuning.DepartureHoldSeconds = 0.f;
	TestFalse(TEXT("0 출발 홀드 시간 거절"), IsValidTuning(InvalidTuning));
	InvalidTuning = Tuning;
	InvalidTuning.UpperStopSeconds = -1.f;
	TestFalse(TEXT("음수 상부 정지 시간 거절"), IsValidTuning(InvalidTuning));
	InvalidTuning = Tuning;
	InvalidTuning.WarningDayNumber = 3;
	TestFalse(TEXT("보드 경고 일차 4 고정"), IsValidTuning(InvalidTuning));
	InvalidTuning = Tuning;
	InvalidTuning.WarningDayHoldMultiplier = 3.f;
	TestFalse(TEXT("보드 경고 배율 2 고정"), IsValidTuning(InvalidTuning));
	return true;
}

#endif
