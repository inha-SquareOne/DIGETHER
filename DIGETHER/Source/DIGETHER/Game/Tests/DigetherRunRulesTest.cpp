#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Run/DigetherRunRules.h"

#include "Game/Economy/DigetherEconomyTypes.h"
#include "Game/Run/DigetherRunRuleTuning.h"
#include "Game/Tests/DigetherTestValues.h"
#include "Misc/AutomationTest.h"

#include <limits>

// 숫자는 테스트 픽스처, 기획 값 아님

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherRunTimeRulesTest,
	"DIGETHER.Game.Run.Rules.Time", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherRunTimeRulesTest::RunTest(const FString& Parameters)
{
	// 테스트 픽스처, 기획 값 아님
	constexpr double FixtureRunSeconds = 600.0;
	constexpr double FixtureElapsedSeconds = 100.0;
	constexpr double FixturePenaltySeconds = 30.0;
	const FDigetherRunTimeResult Initial = FDigetherRunRules::CalculateRemainingTime(FixtureRunSeconds, 0.0, 0,
		FixturePenaltySeconds);
	TestTrue(TEXT("초기 남은 시간 계산 성공"), Initial.Result == EDigetherRunResult::Success);
	TestEqual(TEXT("초기에는 전체 제한 시간이 남음"), Initial.RemainingSeconds, FixtureRunSeconds);
	TestFalse(TEXT("초기에는 미만료"), Initial.bExpired);

	const FDigetherRunTimeResult FirstAscent = FDigetherRunRules::CalculateRemainingTime(FixtureRunSeconds,
		FixtureElapsedSeconds, 1, FixturePenaltySeconds);
	const FDigetherRunTimeResult SecondAscent = FDigetherRunRules::CalculateRemainingTime(FixtureRunSeconds,
		FixtureElapsedSeconds, 2, FixturePenaltySeconds);
	TestEqual(TEXT("첫 상승 확정 페널티"), FirstAscent.RemainingSeconds,
		FixtureRunSeconds - FixtureElapsedSeconds - FixturePenaltySeconds);
	TestEqual(TEXT("두 상승 확정은 페널티 두 번"), SecondAscent.RemainingSeconds,
		FixtureRunSeconds - FixtureElapsedSeconds - 2.0 * FixturePenaltySeconds);
	TestFalse(TEXT("양수 남은 시간은 미만료"), SecondAscent.bExpired);

	const FDigetherRunTimeResult Expired[] =
	{
		FDigetherRunRules::CalculateRemainingTime(FixtureRunSeconds, FixtureRunSeconds, 0, FixturePenaltySeconds),
		FDigetherRunRules::CalculateRemainingTime(FixtureRunSeconds, FixtureRunSeconds + 1.0, 0, FixturePenaltySeconds),
		FDigetherRunRules::CalculateRemainingTime(FixtureRunSeconds, 0.0, 20, FixturePenaltySeconds),
		FDigetherRunRules::CalculateRemainingTime(FixtureRunSeconds, FixtureElapsedSeconds, MAX_int32,
			std::numeric_limits<double>::max()),
		FDigetherRunRules::CalculateRemainingTime(0.0, 0.0, 0, 0.0)
	};
	for (const FDigetherRunTimeResult& Result : Expired)
	{
		TestTrue(TEXT("만료·초과 시간도 계산 성공"), Result.Result == EDigetherRunResult::Success);
		TestEqual(TEXT("음수 남은 시간 없이 0으로 제한"), Result.RemainingSeconds, 0.0);
		TestTrue(TEXT("0 남은 시간은 만료"), Result.bExpired);
	}

	const double InvalidSeconds[] = { -1.0, DigetherTestValues::NaN<double>(), DigetherTestValues::Infinity<double>(), -DigetherTestValues::Infinity<double>() };
	for (const double Invalid : InvalidSeconds)
	{
		const FDigetherRunTimeResult Results[] =
		{
			FDigetherRunRules::CalculateRemainingTime(Invalid, 0.0, 0, 0.0),
			FDigetherRunRules::CalculateRemainingTime(FixtureRunSeconds, Invalid, 0, 0.0),
			FDigetherRunRules::CalculateRemainingTime(FixtureRunSeconds, 0.0, 0, Invalid)
		};
		for (const FDigetherRunTimeResult& Result : Results)
		{
			TestTrue(TEXT("음수·비유한 시간 입력 거절"), Result.Result == EDigetherRunResult::InvalidInput);
			TestEqual(TEXT("실패 결과에는 사용 가능한 남은 시간 없음"), Result.RemainingSeconds, 0.0);
			TestFalse(TEXT("실패를 만료로 잘못 확정하지 않음"), Result.bExpired);
		}
	}
	TestTrue(TEXT("음수 확정 상승 횟수 거절"),
		FDigetherRunRules::CalculateRemainingTime(FixtureRunSeconds, 0.0, -1, FixturePenaltySeconds).Result
		== EDigetherRunResult::InvalidInput);
	const FDigetherRunTimeResult LargeFinite = FDigetherRunRules::CalculateRemainingTime(
		std::numeric_limits<double>::max(), 0.0, MAX_int32, 0.0);
	TestTrue(TEXT("큰 유한 시간은 유한하게 유지"), FMath::IsFinite(LargeFinite.RemainingSeconds));
	TestEqual(TEXT("0 페널티는 큰 시간도 보존"), LargeFinite.RemainingSeconds, std::numeric_limits<double>::max());
	const double SmallFiniteSeconds = std::numeric_limits<double>::min();
	TestEqual(TEXT("0 페널티는 작은 유한 시간도 만료시키지 않음"),
		FDigetherRunRules::CalculateRemainingTime(SmallFiniteSeconds, 0.0, MAX_int32, 0.0).RemainingSeconds,
		SmallFiniteSeconds);
	TestTrue(TEXT("페널티 나눗셈 반올림으로 양수 잔여 시간을 조기 만료하지 않음"),
		FDigetherRunRules::CalculateRemainingTime(1.0, 0.0, 49, 1.0 / 49.0).RemainingSeconds > 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherRunAftershockRulesTest,
	"DIGETHER.Game.Run.Rules.Aftershock", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherRunAftershockRulesTest::RunTest(const FString& Parameters)
{
	// 단조성·범위 검증용 테스트 픽스처, 기획 값 아님
	FDigetherRunRuleTuning Tuning;
	Tuning.AftershockStageCount = 3;
	Tuning.AftershockMaxIntervalSeconds = 30.0;
	Tuning.AftershockMinIntervalSeconds = 5.0;
	constexpr double FixtureRunSeconds = 600.0;
	TestTrue(TEXT("유효한 여진 튜닝"), FDigetherRunRules::IsValidRuleTuning(Tuning));
	int32 PreviousStage = 0;
	double PreviousIntervalSeconds = Tuning.AftershockMaxIntervalSeconds;
	for (int32 Elapsed = 0; Elapsed <= 600; ++Elapsed)
	{
		const FDigetherRunAftershockResult Result = FDigetherRunRules::CalculateAftershock(FixtureRunSeconds,
			FixtureRunSeconds - Elapsed, Tuning);
		TestTrue(TEXT("모든 남은 비율 계산 성공"), Result.Result == EDigetherRunResult::Success);
		TestTrue(TEXT("시간 감소 시 단계는 감소하지 않음"), Result.Stage >= PreviousStage);
		TestTrue(TEXT("단계는 0부터 튜닝 최종 단계 사이"), Result.Stage >= 0 && Result.Stage <= Tuning.AftershockStageCount);
		TestTrue(TEXT("시간 감소 시 간격은 길어지지 않음"), Result.IntervalSeconds <= PreviousIntervalSeconds);
		TestTrue(TEXT("간격은 튜닝 최소·최대 사이"), Result.IntervalSeconds >= Tuning.AftershockMinIntervalSeconds
			&& Result.IntervalSeconds <= Tuning.AftershockMaxIntervalSeconds);
		PreviousStage = Result.Stage;
		PreviousIntervalSeconds = Result.IntervalSeconds;
	}
	const FDigetherRunAftershockResult Initial = FDigetherRunRules::CalculateAftershock(FixtureRunSeconds,
		FixtureRunSeconds, Tuning);
	const FDigetherRunAftershockResult Expired = FDigetherRunRules::CalculateAftershock(FixtureRunSeconds, 0.0, Tuning);
	TestEqual(TEXT("초기 여진 단계 0"), Initial.Stage, 0);
	TestEqual(TEXT("초기 간격은 최대 간격"), Initial.IntervalSeconds, Tuning.AftershockMaxIntervalSeconds);
	TestEqual(TEXT("만료 단계는 마지막 단계"), Expired.Stage, Tuning.AftershockStageCount);
	TestEqual(TEXT("만료 간격은 최소 간격"), Expired.IntervalSeconds, Tuning.AftershockMinIntervalSeconds);
	TestEqual(TEXT("전체 제한 시간을 넘는 남은 시간은 초기 단계로 제한"),
		FDigetherRunRules::CalculateAftershock(FixtureRunSeconds, FixtureRunSeconds * 2.0, Tuning).Stage, 0);
	Tuning.AftershockStageCount = MAX_int32;
	TestEqual(TEXT("기획 근거 없는 단계 수 상한을 추가하지 않음"),
		FDigetherRunRules::CalculateAftershock(FixtureRunSeconds, 0.0, Tuning).Stage, MAX_int32);

	const double InvalidSeconds[] = { -1.0, DigetherTestValues::NaN<double>(), DigetherTestValues::Infinity<double>(), -DigetherTestValues::Infinity<double>() };
	for (const double Invalid : InvalidSeconds)
	{
		TestTrue(TEXT("잘못된 여진 전체 시간 거절"),
			FDigetherRunRules::CalculateAftershock(Invalid, 0.0, Tuning).Result == EDigetherRunResult::InvalidInput);
		TestTrue(TEXT("잘못된 여진 남은 시간 거절"),
			FDigetherRunRules::CalculateAftershock(FixtureRunSeconds, Invalid, Tuning).Result == EDigetherRunResult::InvalidInput);
		FDigetherRunRuleTuning InvalidTuning = Tuning;
		InvalidTuning.AftershockMinIntervalSeconds = Invalid;
		TestFalse(TEXT("음수·비유한 최소 간격 거절"), FDigetherRunRules::IsValidRuleTuning(InvalidTuning));
		InvalidTuning = Tuning;
		InvalidTuning.AftershockMaxIntervalSeconds = Invalid;
		TestFalse(TEXT("음수·비유한 최대 간격 거절"), FDigetherRunRules::IsValidRuleTuning(InvalidTuning));
	}
	TestTrue(TEXT("여진 전체 시간 0 거절"),
		FDigetherRunRules::CalculateAftershock(0.0, 0.0, Tuning).Result == EDigetherRunResult::InvalidInput);
	Tuning.AftershockStageCount = 0;
	TestFalse(TEXT("0 단계 튜닝 거절"), FDigetherRunRules::IsValidRuleTuning(Tuning));
	Tuning.AftershockStageCount = -1;
	TestFalse(TEXT("음수 단계 튜닝 거절"), FDigetherRunRules::IsValidRuleTuning(Tuning));
	Tuning.AftershockStageCount = 1;
	Tuning.AftershockMinIntervalSeconds = 0.0;
	TestFalse(TEXT("간격 0은 지속 호출이 되므로 거절"), FDigetherRunRules::IsValidRuleTuning(Tuning));
	Tuning.AftershockMinIntervalSeconds = Tuning.AftershockMaxIntervalSeconds + 1.0;
	TestFalse(TEXT("뒤집힌 최소·최대 간격 거절"), FDigetherRunRules::IsValidRuleTuning(Tuning));
	Tuning.AftershockMinIntervalSeconds = Tuning.AftershockMaxIntervalSeconds;
	TestTrue(TEXT("동일한 최소·최대 간격은 허용"), FDigetherRunRules::IsValidRuleTuning(Tuning));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherRunTerminationRulesTest,
	"DIGETHER.Game.Run.Rules.Termination", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherRunTerminationRulesTest::RunTest(const FString& Parameters)
{
	const EDigetherRunTerminationTrigger Triggers[] =
	{
		EDigetherRunTerminationTrigger::TimeExpired,
		EDigetherRunTerminationTrigger::FinalAscentCompleted
	};
	const EDigetherRunElevatorState ElevatorStates[] =
	{
		EDigetherRunElevatorState::StoppedInMine,
		EDigetherRunElevatorState::InTransit,
		EDigetherRunElevatorState::AtBase
	};
	const EDigetherLifeState Lives[] =
	{
		EDigetherLifeState::Active,
		EDigetherLifeState::Dead,
		EDigetherLifeState::Escaped
	};
	const EDigetherRunPlayerLocation Locations[] =
	{
		EDigetherRunPlayerLocation::Elevator,
		EDigetherRunPlayerLocation::MineOrDungeon,
		EDigetherRunPlayerLocation::Base
	};
	for (const EDigetherRunTerminationTrigger Trigger : Triggers)
	{
		for (const EDigetherRunElevatorState ElevatorState : ElevatorStates)
		{
			for (const bool bOverweight : { false, true })
			{
				FDigetherRunTerminationInput Input;
				Input.Trigger = Trigger;
				Input.ElevatorState = ElevatorState;
				Input.bElevatorOverweight = bOverweight;
				for (const EDigetherLifeState Life : Lives)
				{
					for (const EDigetherRunPlayerLocation Location : Locations)
					{
						FDigetherRunPlayerInput Player;
						Player.PlayerKey = FString::Printf(TEXT("FixturePlayer%d"), Input.Players.Num());
						Player.Life = Life;
						Player.Location = Location;
						Input.Players.Add(Player);
					}
				}
				const FDigetherRunTerminationResult Result = FDigetherRunRules::CalculateTermination(Input);
				TestTrue(TEXT("유효한 종료 계획 계산 성공"), Result.Result == EDigetherRunResult::Success);
				TestEqual(TEXT("전체 플레이어 결과 수 보존"), Result.Players.Num(), Input.Players.Num());
				const bool bEject = ElevatorState == EDigetherRunElevatorState::StoppedInMine && bOverweight;
				TestEqual(TEXT("사출은 광산 정지·과중량일 때만"), Result.bEjectCargo, bEject);
				const EDigetherRunElevatorAction ExpectedElevatorAction = bEject
					? EDigetherRunElevatorAction::EmptyAscentAfterEjection
					: (ElevatorState == EDigetherRunElevatorState::StoppedInMine
						? EDigetherRunElevatorAction::NormalReturn : EDigetherRunElevatorAction::Unchanged);
				TestTrue(TEXT("상태별 엘리베이터 후속 명령"), Result.ElevatorAction == ExpectedElevatorAction);
				if (Result.Players.Num() != Input.Players.Num())
				{
					continue;
				}
				for (int32 Index = 0; Index < Input.Players.Num(); ++Index)
				{
					const FDigetherRunPlayerInput& Player = Input.Players[Index];
					const FDigetherRunPlayerOutcome& Outcome = Result.Players[Index];
					TestEqual(TEXT("식별자·입력 순서 유지"), Outcome.PlayerKey, Player.PlayerKey);
					EDigetherRunPlayerOutcome ExpectedOutcome = EDigetherRunPlayerOutcome::Unchanged;
					EDigetherDeathCause ExpectedCause = EDigetherDeathCause::None;
					if (Player.Life == EDigetherLifeState::Active)
					{
						if (Player.Location == EDigetherRunPlayerLocation::MineOrDungeon)
						{
							ExpectedOutcome = EDigetherRunPlayerOutcome::MissingDeath;
							ExpectedCause = Trigger == EDigetherRunTerminationTrigger::TimeExpired
								? EDigetherDeathCause::CollapseTimeExpired
								: EDigetherDeathCause::CollapseRemainedAfterFinalAscent;
						}
						else if (Player.Location == EDigetherRunPlayerLocation::Elevator
							&& ElevatorState == EDigetherRunElevatorState::InTransit)
						{
							ExpectedOutcome = EDigetherRunPlayerOutcome::SurvivedInTransit;
						}
						else if (Player.Location == EDigetherRunPlayerLocation::Elevator && bEject)
						{
							ExpectedOutcome = EDigetherRunPlayerOutcome::MissingDeath;
							ExpectedCause = EDigetherDeathCause::CollapseEjected;
						}
						else
						{
							ExpectedOutcome = EDigetherRunPlayerOutcome::Returned;
						}
						TestTrue(TEXT("상태·위치·트리거별 생존 결과"), Outcome.Outcome == ExpectedOutcome);
					}
					else
					{
						TestTrue(TEXT("사망·탈출 플레이어는 위치와 무관하게 유지"), Outcome.Outcome == ExpectedOutcome);
					}
					TestTrue(TEXT("사망 원인은 트리거·사출로 구분"), Outcome.DeathCause == ExpectedCause);
				}
			}
		}
	}

	FDigetherRunTerminationInput Valid;
	FDigetherRunPlayerInput First;
	First.PlayerKey = TEXT("First");
	Valid.Players.Add(First);
	FDigetherRunPlayerInput Second = First;
	Second.PlayerKey = TEXT("Second");
	Valid.Players.Add(Second);
	const auto CheckInvalid = [this](const FDigetherRunTerminationInput& Input)
	{
		const FDigetherRunTerminationResult Result = FDigetherRunRules::CalculateTermination(Input);
		TestTrue(TEXT("잘못된 종료 입력 거절"), Result.Result == EDigetherRunResult::InvalidInput);
		TestTrue(TEXT("실패 시 엘리베이터 명령 없음"), Result.ElevatorAction == EDigetherRunElevatorAction::Unchanged);
		TestFalse(TEXT("실패 시 화물 사출 없음"), Result.bEjectCargo);
		TestEqual(TEXT("실패 시 앞 플레이어만 부분 확정하지 않음"), Result.Players.Num(), 0);
	};
	FDigetherRunTerminationInput Invalid = Valid;
	Invalid.Players[1].PlayerKey = First.PlayerKey;
	CheckInvalid(Invalid);
	for (const FString& InvalidKey : { FString(), FString(TEXT(" ")), FString(TEXT("\t\r\n")) })
	{
		Invalid = Valid;
		Invalid.Players[1].PlayerKey = InvalidKey;
		CheckInvalid(Invalid);
	}
	Invalid = Valid;
	Invalid.Trigger = static_cast<EDigetherRunTerminationTrigger>(255);
	CheckInvalid(Invalid);
	Invalid = Valid;
	Invalid.ElevatorState = static_cast<EDigetherRunElevatorState>(255);
	CheckInvalid(Invalid);
	Invalid = Valid;
	Invalid.Players[1].Life = static_cast<EDigetherLifeState>(255);
	CheckInvalid(Invalid);
	Invalid = Valid;
	Invalid.Players[1].Location = static_cast<EDigetherRunPlayerLocation>(255);
	CheckInvalid(Invalid);
	TestEqual(TEXT("순수 종료 계획이 입력 인원 수를 바꾸지 않음"), Valid.Players.Num(), 2);
	TestEqual(TEXT("순수 종료 계획이 입력 키를 바꾸지 않음"), Valid.Players[1].PlayerKey, Second.PlayerKey);
	TestTrue(TEXT("플레이어 없는 종료 계획 허용"),
		FDigetherRunRules::CalculateTermination(FDigetherRunTerminationInput()).Result == EDigetherRunResult::Success);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherRunQuotaRulesTest,
	"DIGETHER.Game.Run.Rules.Quota", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherRunQuotaRulesTest::RunTest(const FString& Parameters)
{
	// 테스트 픽스처, 기획 값 아님
	FDigetherQuotaState Quota;
	Quota.TargetCredits = 100;
	Quota.DepositedCredits = 0;
	for (int32 Day = 1; Day < DigetherRunRules::PaymentDeadlineDay; ++Day)
	{
		Quota.Day = Day;
		const FDigetherRunQuotaResult Result = FDigetherRunRules::EvaluateQuota(Quota);
		TestTrue(TEXT("1~3일차 유효 입력"), Result.Result == EDigetherRunResult::Success);
		TestFalse(TEXT("1~3일차 마감 아님"), Result.bPaymentDeadline);
		TestFalse(TEXT("1~3일차 미달 게임 오버 아님"), Result.bGameOver);
	}
	Quota.Day = DigetherRunRules::PaymentDeadlineDay;
	const FDigetherRunQuotaResult LeverCompleted = FDigetherRunRules::EvaluateQuota(Quota);
	TestTrue(TEXT("4일차 레버 완료 시 마감"), LeverCompleted.bPaymentDeadline);
	TestFalse(TEXT("4일차 탐사 중에는 아직 게임 오버 아님"), LeverCompleted.bGameOver);
	// 경제 일차 전환 없이 호출자가 전달할 귀환 후 스냅샷만 구성
	FDigetherQuotaState MissedReturnSnapshot = Quota;
	MissedReturnSnapshot.Day = 5;
	const FDigetherRunQuotaResult Missed = FDigetherRunRules::EvaluateQuota(MissedReturnSnapshot);
	TestTrue(TEXT("미달 귀환 스냅샷 판정 성공"), Missed.Result == EDigetherRunResult::Success);
	TestTrue(TEXT("4일차 귀환 후 목표 미달은 게임 오버"), Missed.bGameOver);
	TestFalse(TEXT("귀환 후 5일차는 레버 마감 표시 아님"), Missed.bPaymentDeadline);

	FDigetherQuotaState PaidResetSnapshot;
	PaidResetSnapshot.Day = 1;
	PaidResetSnapshot.TargetCredits = 200;
	PaidResetSnapshot.DepositedCredits = 0;
	PaidResetSnapshot.PeriodGeneration = 1;
	const FDigetherRunQuotaResult PaidReset = FDigetherRunRules::EvaluateQuota(PaidResetSnapshot);
	TestTrue(TEXT("목표 달성 초기화 스냅샷 판정 성공"), PaidReset.Result == EDigetherRunResult::Success);
	TestFalse(TEXT("목표 달성 day1 초기화 직후 마감 해제"), PaidReset.bPaymentDeadline);
	TestFalse(TEXT("목표 달성 day1 초기화 직후 게임 오버 없음"), PaidReset.bGameOver);
	FDigetherQuotaState NextPeriodReturnSnapshot = PaidResetSnapshot;
	NextPeriodReturnSnapshot.Day = 2;
	const FDigetherRunQuotaResult NextPeriod = FDigetherRunRules::EvaluateQuota(NextPeriodReturnSnapshot);
	TestTrue(TEXT("새 기간의 유효한 귀환 판정"), NextPeriod.Result == EDigetherRunResult::Success);
	TestFalse(TEXT("새 기간 미달은 이전 기간 게임 오버를 만들지 않음"), NextPeriod.bGameOver);
	TestFalse(TEXT("새 기간에는 이전 마감 표시 없음"), NextPeriod.bPaymentDeadline);
	Quota.Day = 5;
	Quota.DepositedCredits = Quota.TargetCredits;
	TestFalse(TEXT("달성한 목표는 늦은 일차여도 게임 오버 아님"), FDigetherRunRules::EvaluateQuota(Quota).bGameOver);

	const auto CheckInvalid = [this](const FDigetherQuotaState& State)
	{
		const FDigetherRunQuotaResult Result = FDigetherRunRules::EvaluateQuota(State);
		TestTrue(TEXT("잘못된 quota 입력 거절"), Result.Result == EDigetherRunResult::InvalidInput);
		TestFalse(TEXT("실패 시 마감 미확정"), Result.bPaymentDeadline);
		TestFalse(TEXT("실패 시 게임 오버 미확정"), Result.bGameOver);
	};
	FDigetherQuotaState Invalid = Quota;
	Invalid.Day = 0;
	CheckInvalid(Invalid);
	Invalid.Day = -1;
	CheckInvalid(Invalid);
	Invalid = Quota;
	Invalid.TargetCredits = -1;
	CheckInvalid(Invalid);
	Invalid = Quota;
	Invalid.DepositedCredits = -1;
	CheckInvalid(Invalid);
	Invalid = Quota;
	Invalid.DepositedCredits = Invalid.TargetCredits + 1;
	CheckInvalid(Invalid);
	Invalid = Quota;
	Invalid.PeriodGeneration = -1;
	CheckInvalid(Invalid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherRunPublicStateRulesTest,
	"DIGETHER.Game.Run.Rules.PublicState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherRunPublicStateRulesTest::RunTest(const FString& Parameters)
{
	FDigetherRunPublicState State;
	TestTrue(TEXT("기본 공개 상태 유효"), FDigetherRunRules::IsValidPublicState(State));
	for (const EDigetherRunPhase Phase : { EDigetherRunPhase::NotStarted, EDigetherRunPhase::InProgress,
		EDigetherRunPhase::Ended })
	{
		State.Phase = Phase;
		TestTrue(TEXT("정의된 단계 허용"), FDigetherRunRules::IsValidPublicState(State));
	}
	State.Phase = static_cast<EDigetherRunPhase>(255);
	TestFalse(TEXT("알 수 없는 공개 단계 거절"), FDigetherRunRules::IsValidPublicState(State));
	State.Phase = EDigetherRunPhase::InProgress;
	State.AftershockStage = -1;
	TestFalse(TEXT("음수 공개 여진 단계 거절"), FDigetherRunRules::IsValidPublicState(State));
	return true;
}

#endif
