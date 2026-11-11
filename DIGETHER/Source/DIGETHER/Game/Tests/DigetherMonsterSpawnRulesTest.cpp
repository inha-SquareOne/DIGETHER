#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Monster/DigetherMonsterSpawnRules.h"
#include "Game/Tests/DigetherTestValues.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherMonsterSpawnRulesTest,
	"DIGETHER.Game.Monster.SpawnRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherMonsterSpawnRulesTest::RunTest(const FString& Parameters)
{
	using namespace DigetherMonsterSpawnRules;

	// 곡선 픽스처: 개발 시험값 문서의 예시(유예 2분, 5%→60%, 8분 도달), 기획 확정값 아님
	FCurveParams Curve;
	Curve.GraceSeconds = 120.f;
	Curve.StartChance = 0.05f;
	Curve.MaxChance = 0.6f;
	Curve.MaxChanceAtSeconds = 480.f;
	TestEqual(TEXT("유예 중 확률 0"), EvaluateChance(Curve, 119.f), 0.f);
	TestEqual(TEXT("유예 직후 시작 확률"), EvaluateChance(Curve, 120.f), 0.05f, 1e-5f);
	// 5분: 0.05 × 12^(180/360) ≈ 0.1732
	TestEqual(TEXT("중간은 지수형"), EvaluateChance(Curve, 300.f), 0.05f * FMath::Sqrt(12.f), 1e-4f);
	TestEqual(TEXT("도달 시각 최종 확률"), EvaluateChance(Curve, 480.f), 0.6f, 1e-5f);
	TestEqual(TEXT("도달 후 최종 확률 유지"), EvaluateChance(Curve, 2000.f), 0.6f, 1e-5f);
	TestEqual(TEXT("비유한 시간은 0"), EvaluateChance(Curve, DigetherTestValues::NaN()), 0.f);
	FCurveParams Linear = Curve;
	Linear.StartChance = 0.f;
	TestEqual(TEXT("시작 0이면 선형"), EvaluateChance(Linear, 300.f), 0.3f, 1e-4f);
	FCurveParams Off = Curve;
	Off.MaxChance = 0.f;
	TestEqual(TEXT("최종 0이면 항상 0"), EvaluateChance(Off, 1000.f), 0.f);

	// 후보 거르기
	TArray<FCandidate> Candidates;
	FCandidate Box;
	Box.SourceIndex = 0;
	Box.Weight = 1.f;
	Box.PowerCost = 3;
	Box.RunSpawnCap = 1;
	Candidates.Add(Box);
	FCandidate Small;
	Small.SourceIndex = 1;
	Small.Weight = 2.f;
	Small.PowerCost = 1;
	Candidates.Add(Small);
	TestEqual(TEXT("파워 충분하면 둘 다"), FilterCandidates(Candidates, 3).Num(), 2);
	TArray<FCandidate> PowerFiltered = FilterCandidates(Candidates, 2);
	TestTrue(TEXT("파워 부족 제외"), PowerFiltered.Num() == 1 && PowerFiltered[0].SourceIndex == 1);
	TestEqual(TEXT("파워 미적용(갱도)"), FilterCandidates(Candidates, -1).Num(), 2);
	Candidates[0].RunSpawned = 1;
	TestEqual(TEXT("탐사당 상한 도달 제외"), FilterCandidates(Candidates, 10).Num(), 1);
	Candidates[1].MaxAlive = 2;
	Candidates[1].Alive = 2;
	TestEqual(TEXT("종별 최대 수 도달 제외, 후보 없음"), FilterCandidates(Candidates, 10).Num(), 0);
	Candidates[1].Alive = 1;
	Candidates[1].Weight = 0.f;
	TestEqual(TEXT("가중치 0 제외"), FilterCandidates(Candidates, 10).Num(), 0);

	// 가중치 뽑기
	TArray<FCandidate> Pool;
	FCandidate A;
	A.Weight = 1.f;
	Pool.Add(A);
	FCandidate B;
	B.Weight = 3.f;
	Pool.Add(B);
	const FRandomStream Stream(1234);
	int32 PickedB = 0;
	constexpr int32 Draws = 20000;
	for (int32 Draw = 0; Draw < Draws; ++Draw)
	{
		const int32 Pick = PickWeighted(Pool, Stream);
		TestTrue(TEXT("범위 안 결과"), Pick == 0 || Pick == 1);
		PickedB += Pick == 1 ? 1 : 0;
	}
	TestTrue(TEXT("가중치 비율 1:3 근사"), FMath::Abs(static_cast<float>(PickedB) / Draws - 0.75f) < 0.02f);
	TestEqual(TEXT("빈 후보"), PickWeighted(TArray<FCandidate>(), Stream), static_cast<int32>(INDEX_NONE));
	Pool[0].Weight = 0.f;
	Pool[1].Weight = 0.f;
	TestEqual(TEXT("가중치 합 0"), PickWeighted(Pool, Stream), static_cast<int32>(INDEX_NONE));

	// 파워 한도
	TestEqual(TEXT("기본 한도"), EvaluatePowerLimit(3, 1, 6, 0), 3);
	TestEqual(TEXT("목표 달성마다 증가"), EvaluatePowerLimit(3, 1, 6, 2), 5);
	TestEqual(TEXT("최대에서 멈춤"), EvaluatePowerLimit(3, 1, 6, 10), 6);
	TestEqual(TEXT("음수 달성 수 무시"), EvaluatePowerLimit(3, 1, 6, -4), 3);

	// 장부: 던전별 파워, 소멸 시 반환, 탐사당 생성 수 유지
	FLedger Ledger;
	const FName Species(TEXT("Box"));
	const FName DungeonA(TEXT("A"));
	const FName DungeonB(TEXT("B"));
	Ledger.RecordSpawn(Species, DungeonA, 3);
	TestEqual(TEXT("던전 A 파워 사용"), Ledger.GetPowerUsed(DungeonA), 3);
	TestEqual(TEXT("던전 B 영향 없음"), Ledger.GetPowerUsed(DungeonB), 0);
	TestEqual(TEXT("생존 수"), Ledger.GetAlive(Species), 1);
	TestTrue(TEXT("소멸 반환"), Ledger.RecordRemoval(Species, DungeonA, 3));
	TestEqual(TEXT("파워 반환"), Ledger.GetPowerUsed(DungeonA), 0);
	TestEqual(TEXT("생존 수 감소"), Ledger.GetAlive(Species), 0);
	TestEqual(TEXT("탐사당 생성 수 유지"), Ledger.GetRunSpawned(Species), 1);
	TestFalse(TEXT("기록 없는 반환 거절"), Ledger.RecordRemoval(Species, DungeonA, 3));
	TestEqual(TEXT("음수 파워 없음"), Ledger.GetPowerUsed(DungeonA), 0);
	Ledger.RecordSpawn(Species, DungeonB, 3);
	TestTrue(TEXT("예약 취소"), Ledger.RecordCancelledSpawn(Species, DungeonB, 3));
	TestEqual(TEXT("취소는 생성 수까지 되돌림"), Ledger.GetRunSpawned(Species), 1);
	TestEqual(TEXT("취소 후 파워 0"), Ledger.GetPowerUsed(DungeonB), 0);
	Ledger.RecordSpawn(TEXT("Mole"), NAME_None, 0);
	TestEqual(TEXT("갱도 종은 파워 미사용"), Ledger.DungeonPowerUsed.Num(), 2);
	return true;
}

#endif
