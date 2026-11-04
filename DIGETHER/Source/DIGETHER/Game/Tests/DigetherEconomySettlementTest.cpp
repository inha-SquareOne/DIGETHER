#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Data/DigetherTuningProfile.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherEconomySettlementTest,
	"DIGETHER.Game.Economy.Settlement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherEconomySettlementTest::RunTest(const FString& Parameters)
{
	// 기본값 없는 런 스냅샷에 기존 시험 입력을 명시한다.
	FDigetherRunTuning RunTuning;
	RunTuning.DeathLossPercent = 100;
	RunTuning.CorpseRefundPercent = 50;
	RunTuning.InitialQuotaCredits = 100;
	// 고정된 프로토타입 Q01~Q09의 산식 입력이며 최종 기획 수치가 아니다.
	struct FDigetherSettlementCase
	{
		const TCHAR* Name;
		int32 CargoCredits;
		int32 Departures;
		int32 Survivors;
		int32 ShareCredits;
		int32 LostCredits;
	};
	const FDigetherSettlementCase Cases[] =
	{
		{ TEXT("Q01: 4인 중 3인 생존"), 900, 4, 3, 225, 225 },
		{ TEXT("Q02: 내려놓은 물품 재회수"), 1020, 4, 3, 255, 255 },
		{ TEXT("Q03: 긴급 탈출 순서 무관"), 900, 4, 3, 225, 225 },
		{ TEXT("Q04: 사망자 몫 소멸"), 600, 4, 1, 150, 450 },
		{ TEXT("Q05: 전멸"), 600, 4, 0, 0, 600 },
		{ TEXT("Q06: 전원 생존"), 900, 4, 4, 225, 0 },
		{ TEXT("Q07: 마지막 편 생존자"), 300, 4, 2, 75, 150 },
		{ TEXT("Q08: 먼저 긴급 탈출한 사람만 생존"), 600, 4, 1, 150, 450 },
		{ TEXT("Q09: 정수 나머지 소멸"), 100, 3, 3, 33, 1 }
	};
	for (const FDigetherSettlementCase& Case : Cases)
	{
		TArray<FDigetherPlayerSettlementInput> Players;
		for (int32 Index = 0; Index < Case.Departures; ++Index)
		{
			FDigetherPlayerSettlementInput Player;
			Player.bSurvived = Index < Case.Survivors;
			Player.bDied = !Player.bSurvived;
			Players.Add(Player);
		}
		const FDigetherRunSettlementResult Result = FDigetherEconomyRules::CalculateRunSettlement(
			Case.CargoCredits, Case.Departures, Players, RunTuning.CorpseRefundPercent);
		TestTrue(FString::Printf(TEXT("%s: 정산 성공"), Case.Name), Result.Result == EDigetherEconomyResult::Success);
		TestEqual(FString::Printf(TEXT("%s: 출발 인원으로 나눈 몫"), Case.Name), Result.GeneralShareCredits, Case.ShareCredits);
		TestEqual(FString::Printf(TEXT("%s: 소멸 일반 회수액"), Case.Name), Result.LostGeneralCredits, Case.LostCredits);
		TestEqual(FString::Printf(TEXT("%s: 입력 순서의 인원 수"), Case.Name), Result.Players.Num(), Players.Num());
		for (int32 Index = 0; Index < Players.Num(); ++Index)
		{
			if (Result.Players.IsValidIndex(Index))
			{
				TestEqual(FString::Printf(TEXT("%s: %d번 지급액"), Case.Name, Index), Result.Players[Index].PayoutCredits,
					Players[Index].bSurvived ? Case.ShareCredits : 0);
			}
		}
	}

	const auto CheckPlayer = [this](const TCHAR* Name, const FDigetherPlayerSettlement& Actual,
		int32 GeneralCredits, int32 HiddenCredits, int32 PayoutCredits, int32 RefundCredits)
	{
		TestEqual(FString::Printf(TEXT("%s: 일반 몫"), Name), Actual.GeneralCredits, GeneralCredits);
		TestEqual(FString::Printf(TEXT("%s: 개인 히든"), Name), Actual.HiddenCredits, HiddenCredits);
		TestEqual(FString::Printf(TEXT("%s: 런 지급액"), Name), Actual.PayoutCredits, PayoutCredits);
		TestEqual(FString::Printf(TEXT("%s: 시체 복구액"), Name), Actual.CorpseRefundCredits, RefundCredits);
	};
	FDigetherPlayerSettlementInput Dead;
	Dead.bDied = true;
	Dead.PreDeathBalanceCredits = 101;
	Dead.ReturnedHiddenCredits = 77;
	Dead.bCorpseRecovered = true;
	FDigetherPlayerSettlementInput FirstSurvivor;
	FirstSurvivor.bSurvived = true;
	FirstSurvivor.ReturnedHiddenCredits = 31;
	FDigetherPlayerSettlementInput SecondSurvivor = FirstSurvivor;
	SecondSurvivor.ReturnedHiddenCredits = 7;
	const FDigetherRunSettlementResult Ordered = FDigetherEconomyRules::CalculateRunSettlement(
		901, 4, { Dead, FirstSurvivor, SecondSurvivor }, RunTuning.CorpseRefundPercent);
	TestTrue(TEXT("누락 참가자가 있어도 출발 인원 기준 정산 성공"), Ordered.Result == EDigetherEconomyResult::Success);
	TestEqual(TEXT("누락 참가자의 몫도 재분배하지 않음"), Ordered.GeneralShareCredits, 225);
	TestEqual(TEXT("사망자·누락 참가자 몫과 나머지 소멸"), Ordered.LostGeneralCredits, 451);
	TestEqual(TEXT("결과 배열은 입력 인원 수 유지"), Ordered.Players.Num(), 3);
	if (Ordered.Players.Num() == 3)
	{
		CheckPlayer(TEXT("첫 입력 사망자"), Ordered.Players[0], 0, 0, 0, RunTuning.CorpseRefundPercent);
		CheckPlayer(TEXT("둘째 입력 생존자"), Ordered.Players[1], 225, 31, 256, 0);
		CheckPlayer(TEXT("셋째 입력 생존자"), Ordered.Players[2], 225, 7, 232, 0);
	}

	FirstSurvivor.ReturnedHiddenCredits = -20;
	Dead.PreDeathBalanceCredits = -101;
	const FDigetherRunSettlementResult NegativeAmounts = FDigetherEconomyRules::CalculateRunSettlement(
		-10, 2, { FirstSurvivor, Dead }, RunTuning.CorpseRefundPercent);
	TestTrue(TEXT("음수 금액 입력은 0으로 정리"), NegativeAmounts.Result == EDigetherEconomyResult::Success);
	TestEqual(TEXT("음수 일반 회수액은 몫 0"), NegativeAmounts.GeneralShareCredits, 0);
	TestEqual(TEXT("음수 일반 회수액은 소멸액 0"), NegativeAmounts.LostGeneralCredits, 0);
	TestEqual(TEXT("음수 금액 정리 후 인원 수"), NegativeAmounts.Players.Num(), 2);
	if (NegativeAmounts.Players.Num() == 2)
	{
		CheckPlayer(TEXT("음수 히든 정리"), NegativeAmounts.Players[0], 0, 0, 0, 0);
		CheckPlayer(TEXT("음수 사망 직전 잔고 정리"), NegativeAmounts.Players[1], 0, 0, 0, 0);
	}

	const FDigetherRunSettlementResult Empty = FDigetherEconomyRules::CalculateRunSettlement(100, 0, {}, RunTuning.CorpseRefundPercent);
	TestTrue(TEXT("출발 0명과 빈 입력은 성공"), Empty.Result == EDigetherEconomyResult::Success);
	TestEqual(TEXT("출발 0명은 일반 몫 0"), Empty.GeneralShareCredits, 0);
	TestEqual(TEXT("출발 0명은 일반 회수액 전부 소멸"), Empty.LostGeneralCredits, 100);
	TestEqual(TEXT("출발 0명은 지급 대상 없음"), Empty.Players.Num(), 0);

	const auto CheckInvalid = [this](const TCHAR* Name, const FDigetherRunSettlementResult& Actual)
	{
		TestTrue(FString::Printf(TEXT("%s: 잘못된 입력"), Name), Actual.Result == EDigetherEconomyResult::InvalidInput);
		TestEqual(FString::Printf(TEXT("%s: 몫 없음"), Name), Actual.GeneralShareCredits, 0);
		TestEqual(FString::Printf(TEXT("%s: 소멸 집계 없음"), Name), Actual.LostGeneralCredits, 0);
		TestEqual(FString::Printf(TEXT("%s: 부분 지급 결과 없음"), Name), Actual.Players.Num(), 0);
	};
	CheckInvalid(TEXT("음수 출발 인원"), FDigetherEconomyRules::CalculateRunSettlement(100, -1, {}, RunTuning.CorpseRefundPercent));
	CheckInvalid(TEXT("출발 0명인데 지급 대상 존재"), FDigetherEconomyRules::CalculateRunSettlement(100, 0, { FirstSurvivor }, RunTuning.CorpseRefundPercent));
	CheckInvalid(TEXT("출발 인원보다 많은 지급 대상"), FDigetherEconomyRules::CalculateRunSettlement(100, 1, { FirstSurvivor, Dead }, RunTuning.CorpseRefundPercent));
	CheckInvalid(TEXT("출발 1명인데 생존자 2명"), FDigetherEconomyRules::CalculateRunSettlement(100, 1, { FirstSurvivor, SecondSurvivor }, RunTuning.CorpseRefundPercent));
	FDigetherPlayerSettlementInput Contradiction = FirstSurvivor;
	Contradiction.bDied = true;
	CheckInvalid(TEXT("생존과 사망 동시 지정"), FDigetherEconomyRules::CalculateRunSettlement(100, 1, { Contradiction }, RunTuning.CorpseRefundPercent));
	Contradiction.bDied = false;
	Contradiction.bCorpseRecovered = true;
	CheckInvalid(TEXT("사망 없이 시체 회수 지정"), FDigetherEconomyRules::CalculateRunSettlement(100, 1, { Contradiction }, RunTuning.CorpseRefundPercent));

	Dead.PreDeathBalanceCredits = 101;
	const FDigetherRunSettlementResult Wipeout = FDigetherEconomyRules::CalculateRunSettlement(600, 2, { Dead, Dead }, RunTuning.CorpseRefundPercent);
	TestTrue(TEXT("전멸 정산 자체는 성공"), Wipeout.Result == EDigetherEconomyResult::Success);
	TestEqual(TEXT("전멸 일반 몫은 0"), Wipeout.GeneralShareCredits, 0);
	TestEqual(TEXT("전멸 일반 회수액 전부 소멸"), Wipeout.LostGeneralCredits, 600);
	TestEqual(TEXT("전멸 결과 인원 수"), Wipeout.Players.Num(), 2);
	if (Wipeout.Players.Num() == 2)
	{
		CheckPlayer(TEXT("전멸 첫 사망자"), Wipeout.Players[0], 0, 0, 0, 0);
		CheckPlayer(TEXT("전멸 둘째 사망자"), Wipeout.Players[1], 0, 0, 0, 0);
	}

	FirstSurvivor.ReturnedHiddenCredits = MAX_int32;
	const FDigetherRunSettlementResult Saturated = FDigetherEconomyRules::CalculateRunSettlement(
		MAX_int32, 1, { FirstSurvivor }, RunTuning.CorpseRefundPercent);
	TestTrue(TEXT("큰 지급액 정산 성공"), Saturated.Result == EDigetherEconomyResult::Success);
	TestEqual(TEXT("큰 일반 회수액의 몫"), Saturated.GeneralShareCredits, MAX_int32);
	TestEqual(TEXT("전원 지급된 일반 회수액은 소멸 없음"), Saturated.LostGeneralCredits, 0);
	TestEqual(TEXT("큰 지급액 결과 인원 수"), Saturated.Players.Num(), 1);
	if (Saturated.Players.Num() == 1)
	{
		CheckPlayer(TEXT("일반 몫과 히든 합계 포화"), Saturated.Players[0], MAX_int32, MAX_int32, MAX_int32, 0);
	}

	TestEqual(TEXT("일반 금액 덧셈"), FDigetherEconomyRules::AddCredits(10, 20), 30);
	TestEqual(TEXT("왼쪽 음수는 0"), FDigetherEconomyRules::AddCredits(-1, 5), 5);
	TestEqual(TEXT("오른쪽 음수는 0"), FDigetherEconomyRules::AddCredits(5, -1), 5);
	TestEqual(TEXT("양쪽 음수는 0"), FDigetherEconomyRules::AddCredits(-1, -2), 0);
	TestEqual(TEXT("상한을 한 원 넘는 덧셈 포화"), FDigetherEconomyRules::AddCredits(MAX_int32 - 1, 2), MAX_int32);
	TestEqual(TEXT("최대 금액끼리 덧셈 포화"), FDigetherEconomyRules::AddCredits(MAX_int32, MAX_int32), MAX_int32);
	return true;
}

#endif
