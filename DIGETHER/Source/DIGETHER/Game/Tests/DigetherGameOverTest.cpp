#if WITH_DEV_AUTOMATION_TESTS

// 기획 S33 게임 오버·결과 크레딧의 순수 규칙·진행 컴포넌트·저장 왕복 검사
// 시각·거리·금액·ID는 테스트 픽스처, 기획 값 아님(T29 금액만 작업리스트 1.1.0 S33 247행 예시)

#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Run/DigetherGameOverFlowComponent.h"
#include "Game/Run/DigetherGameOverRules.h"
#include "Game/Save/DigetherSaveRules.h"
#include "Game/Save/DigetherSaveTuning.h"
#include "Misc/AutomationTest.h"

namespace
{
	FDigetherSaveSnapshot MakeGameOverSaveFixture()
	{
		FDigetherSaveSnapshot Snapshot;
		Snapshot.HostAuthenticatedPlayerKey = TEXT("GAMEOVER_TEST:host");
		Snapshot.GameId = FGuid(1, 1, 1, 1);
		Snapshot.RunId = FGuid(2, 2, 2, 2);
		Snapshot.Quota.Quota.Day = 1;
		Snapshot.Quota.Quota.TotalDay = 1;
		Snapshot.Quota.Quota.TargetCredits = 100;
		Snapshot.RunTuning.SourceProfile = FPrimaryAssetId(TEXT("DigetherTuningProfile:DA_GameOverFixture"));
		Snapshot.RunTuning.ProfileVersion = 1;
		Snapshot.RunTuning.InitialQuotaCredits = 100;
		Snapshot.RunTuning.RunSeconds = 120.f;
		Snapshot.RunTuning.InteractionDistanceCm = 200.f;
		Snapshot.RunTuning.ElevatorCapacityKg = 500.f;
		Snapshot.RunTuning.ElevatorTripLimit = 2;
		Snapshot.RunTuning.AscentPenaltySeconds = 10.f;
		Snapshot.RunTuning.ElevatorTravelSeconds = 2.f;
		Snapshot.RunTuning.DepositIntervalSeconds = 0.5f;
		Snapshot.RunTuning.EmergencyWeightLimitKg = 3.f;
		Snapshot.RunTuning.DeathLossPercent = 100;
		Snapshot.RunTuning.RecoveredDeathLossPercent = 50;
		FDigetherSavePlayerRecord Host;
		Host.AuthenticatedPlayerKey = Snapshot.HostAuthenticatedPlayerKey;
		Host.GameplayPlayerId = FGuid(3, 3, 3, 3);
		Host.DisplayName = TEXT("A");
		Host.JoinOrder = 1;
		Snapshot.Players.Add(Host);
		FDigetherSavePlayerRecord Guest;
		Guest.AuthenticatedPlayerKey = TEXT("GAMEOVER_TEST:guest");
		Guest.GameplayPlayerId = FGuid(4, 4, 4, 4);
		Guest.DisplayName = TEXT("B");
		Guest.JoinOrder = 2;
		Snapshot.Players.Add(Guest);
		return Snapshot;
	}

	FDigetherPlayerSettlement MakePayout(const int32 General, const int32 Hidden)
	{
		FDigetherPlayerSettlement Settlement;
		Settlement.GeneralCredits = General;
		Settlement.HiddenCredits = Hidden;
		Settlement.PayoutCredits = General + Hidden;
		return Settlement;
	}

	FDigetherGameOverPresentPlayer MakePresent(const int32 PlayerId, const double DistanceCm)
	{
		FDigetherGameOverPresentPlayer Player;
		Player.PlayerId = PlayerId;
		Player.DistanceToWellCm = DistanceCm;
		return Player;
	}

	FDigetherGameResult MakeFlowResult(const int32 PlayerCount)
	{
		FDigetherGameResult Result;
		Result.ResultId = FGuid(9, 9, 9, 9);
		Result.GameId = FGuid(8, 8, 8, 8);
		for (int32 Index = 0; Index < PlayerCount; ++Index)
		{
			FDigetherGameResultPlayer& Player = Result.Players.AddDefaulted_GetRef();
			Player.JoinOrder = Index + 1;
		}
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherGameOverTransitionTest,
	"DIGETHER.Game.Run.GameOver.TransitionTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherGameOverTransitionTest::RunTest(const FString& Parameters)
{
	using EPhase = EDigetherGameOverPhase;
	const EPhase Phases[] = { EPhase::None, EPhase::SettlementScreen, EPhase::FailureEvent, EPhase::Credits };
	for (const EPhase From : Phases)
	{
		for (const EPhase To : Phases)
		{
			const bool bExpected = (From == EPhase::None && To == EPhase::SettlementScreen)
				|| (From == EPhase::SettlementScreen && To == EPhase::FailureEvent)
				|| (From == EPhase::FailureEvent && To == EPhase::Credits)
				|| (From == EPhase::Credits && To == EPhase::None);
			TestEqual(*FString::Printf(TEXT("전이 %s → %s"), *UEnum::GetValueAsString(From), *UEnum::GetValueAsString(To)),
				DigetherGameOverRules::CanTransition(From, To), bExpected);
		}
	}
	TestTrue(TEXT("기본 연출 튜닝 유효"), DigetherGameOverRules::IsValidTuning(FDigetherGameOverTuning()));
	FDigetherGameOverTuning Invalid;
	Invalid.CreditFastForwardMultiplier = 0.5f;
	TestFalse(TEXT("1배 미만 빨리감기 거절"), DigetherGameOverRules::IsValidTuning(Invalid));
	Invalid = FDigetherGameOverTuning();
	Invalid.FloodRatio = 1.5f;
	TestFalse(TEXT("범람 비율 1 초과 거절"), DigetherGameOverRules::IsValidTuning(Invalid));
	Invalid = FDigetherGameOverTuning();
	Invalid.CreditScrollSpeed = 0.f;
	TestFalse(TEXT("스크롤 속도 0 거절"), DigetherGameOverRules::IsValidTuning(Invalid));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherGameOverFlowTest,
	"DIGETHER.Game.Run.GameOver.FlowSuctionWaitAndCreditsOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherGameOverFlowTest::RunTest(const FString& Parameters)
{
	// T28·T34·T38: 정산 화면 → 실패 사건 → (가까운 사람부터 흡입, 먼저 흡입된 사람 대기, 떠난 사람 제외) → 크레딧 → 새 게임 한 번
	UDigetherGameOverFlowComponent* Flow = NewObject<UDigetherGameOverFlowComponent>();
	FDigetherGameOverTuning Tuning;
	Tuning.SettlementScreenSeconds = 5.f;
	Tuning.FailMotionSeconds = 10.f;
	Tuning.FloodRatio = 0.5f;
	Tuning.SuctionSpeedCmPerSecond = 1000.f;
	const FDigetherGameResult Result = MakeFlowResult(2);
	TestFalse(TEXT("결과 ID 없는 시작 거절"), Flow->Begin(0.0, FDigetherGameResult(), Tuning));
	if (!TestTrue(TEXT("게임 오버 시작"), Flow->Begin(100.0, Result, Tuning)))
	{
		return false;
	}
	TestEqual(TEXT("정산 화면 단계"), Flow->GetPhase(), EDigetherGameOverPhase::SettlementScreen);
	TestFalse(TEXT("중복 시작 거절(정산·통계·일차 반복 없음)"), Flow->Begin(101.0, Result, Tuning));
	TestFalse(TEXT("정산 화면 중 결과 비공개"), Flow->GetPublicState().PublishedResult.IsValid());
	const TArray<FDigetherGameOverPresentPlayer> Both = { MakePresent(1, 500.0), MakePresent(2, 2500.0) };
	FDigetherGameOverStep Step = Flow->Advance(104.9, Both);
	TestFalse(TEXT("정산 화면 시간 전에는 실패 사건 아님"), Step.bEnteredFailureEvent);
	Step = Flow->Advance(105.0, Both);
	TestTrue(TEXT("정산 화면 시간 뒤 실패 사건"), Step.bEnteredFailureEvent);
	TestEqual(TEXT("실패 사건 단계"), Flow->GetPhase(), EDigetherGameOverPhase::FailureEvent);
	const double FailureStart = Flow->GetPublicState().PhaseStartServerSeconds;
	TestEqual(TEXT("실패 사건 시작 시각"), FailureStart, 105.0);
	// 흡입 시작 = 105 + 5, A(500cm)는 110.5, B(2500cm)는 112.5에 흡입
	Step = Flow->Advance(109.9, Both);
	TestTrue(TEXT("범람·침수 중 흡입 없음"), Step.NewlyAbsorbedPlayerIds.IsEmpty());
	TestEqual(TEXT("범람 중 대기 인원 2"), Flow->GetPublicState().WaitingPlayerCount, 2);
	Step = Flow->Advance(110.6, Both);
	TestTrue(TEXT("가까운 A가 먼저 흡입"), Step.NewlyAbsorbedPlayerIds == TArray<int32>({ 1 }));
	TestEqual(TEXT("A는 B를 기다림(대기 1)"), Flow->GetPublicState().WaitingPlayerCount, 1);
	TestFalse(TEXT("한 명이 남으면 크레딧 시작 안 함"), Step.bEnteredCredits);
	Step = Flow->Advance(111.0, Both);
	TestTrue(TEXT("같은 사람을 다시 흡입하지 않음"), Step.NewlyAbsorbedPlayerIds.IsEmpty());
	Step = Flow->Advance(112.6, Both);
	TestTrue(TEXT("B 흡입"), Step.NewlyAbsorbedPlayerIds == TArray<int32>({ 2 }));
	TestTrue(TEXT("전원 흡입 뒤 함께 크레딧"), Step.bEnteredCredits);
	TestEqual(TEXT("크레딧 단계"), Flow->GetPhase(), EDigetherGameOverPhase::Credits);
	TestTrue(TEXT("크레딧에 들어갈 때 결과 공개"), Flow->GetPublicState().PublishedResult.ResultId == Result.ResultId);
	const FDigetherCreditPlayback Playback = Flow->GetPublicState().Playback;
	TestEqual(TEXT("크레딧 기준 시각"), Playback.AnchorServerSeconds, 112.6);
	TestEqual(TEXT("크레딧 전체 길이는 결정적"), Playback.TotalSeconds,
		DigetherGameOverRules::CalculateCreditTotalSeconds(2, Tuning));
	TestFalse(TEXT("크레딧 중 되돌아가는 전이 없음"), Flow->CompleteNewGame(113.0));
	const double End = Playback.AnchorServerSeconds + Playback.TotalSeconds;
	Step = Flow->Advance(End - 0.01, TArray<FDigetherGameOverPresentPlayer>());
	TestFalse(TEXT("크레딧 끝 전에는 새 게임 요청 없음"), Step.bCreditsFinished);
	Step = Flow->Advance(End, TArray<FDigetherGameOverPresentPlayer>());
	TestTrue(TEXT("크레딧 끝에 새 게임 요청"), Step.bCreditsFinished);
	Step = Flow->Advance(End + 1.0, TArray<FDigetherGameOverPresentPlayer>());
	TestFalse(TEXT("종료 신호가 다시 와도 새 게임 요청 한 번(T38)"), Step.bCreditsFinished);
	TestTrue(TEXT("새 게임 대기"), Flow->IsAwaitingNewGame());
	TestTrue(TEXT("새 게임 완료로 None"), Flow->CompleteNewGame(End + 1.0));
	TestFalse(TEXT("새 게임 완료 중복 거절"), Flow->CompleteNewGame(End + 2.0));
	TestEqual(TEXT("게임 오버 끝"), Flow->GetPhase(), EDigetherGameOverPhase::None);
	TestFalse(TEXT("새 게임 뒤 결과 공개 해제"), Flow->GetPublicState().PublishedResult.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherGameOverLeaveAndDeadlineTest,
	"DIGETHER.Game.Run.GameOver.LeaverNotWaitedAndDeadlineForcesSuction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherGameOverLeaveAndDeadlineTest::RunTest(const FString& Parameters)
{
	FDigetherGameOverTuning Tuning;
	Tuning.SettlementScreenSeconds = 0.f;
	Tuning.FailMotionSeconds = 10.f;
	Tuning.FloodRatio = 0.5f;
	Tuning.SuctionSpeedCmPerSecond = 1000.f;
	// T34: B가 흡입 대기 중 떠나면 대기 인원에서 제외, 진행 계속
	{
		UDigetherGameOverFlowComponent* Flow = NewObject<UDigetherGameOverFlowComponent>();
		Flow->Begin(0.0, MakeFlowResult(2), Tuning);
		Flow->Advance(0.0, { MakePresent(1, 0.0), MakePresent(2, 4000.0) });
		FDigetherGameOverStep Step = Flow->Advance(5.1, { MakePresent(1, 0.0), MakePresent(2, 4000.0) });
		TestTrue(TEXT("A 흡입, B 대기"), Step.NewlyAbsorbedPlayerIds == TArray<int32>({ 1 }) && !Step.bEnteredCredits);
		Step = Flow->Advance(5.2, { MakePresent(1, 0.0) });
		TestTrue(TEXT("B가 떠나면 기다리지 않고 크레딧"), Step.bEnteredCredits);
		TestEqual(TEXT("떠난 사람은 대기 인원 아님"), Flow->GetPublicState().WaitingPlayerCount, 0);
	}
	// 연출 오류·먼 거리로 미흡입이어도 마감에 남은 사람 강제 흡입 후 크레딧(S33 244행)
	{
		UDigetherGameOverFlowComponent* Flow = NewObject<UDigetherGameOverFlowComponent>();
		Flow->Begin(0.0, MakeFlowResult(1), Tuning);
		Flow->Advance(0.0, { MakePresent(1, 1.0e9) });
		FDigetherGameOverStep Step = Flow->Advance(9.9, { MakePresent(1, 1.0e9) });
		TestTrue(TEXT("마감 전에는 먼 사람 대기"), Step.NewlyAbsorbedPlayerIds.IsEmpty());
		Step = Flow->Advance(10.0, { MakePresent(1, 1.0e9) });
		TestTrue(TEXT("마감에 강제 흡입"), Step.NewlyAbsorbedPlayerIds == TArray<int32>({ 1 }));
		TestTrue(TEXT("마감에 크레딧 시작"), Step.bEnteredCredits);
	}
	// 1인: 자기 흡입 종료 순간 크레딧, 전원 퇴장이면 즉시 크레딧
	{
		UDigetherGameOverFlowComponent* Flow = NewObject<UDigetherGameOverFlowComponent>();
		Flow->Begin(0.0, MakeFlowResult(1), Tuning);
		Flow->Advance(0.0, TArray<FDigetherGameOverPresentPlayer>());
		const FDigetherGameOverStep Step = Flow->Advance(0.1, TArray<FDigetherGameOverPresentPlayer>());
		TestTrue(TEXT("남은 참가자가 없으면 기다리지 않고 크레딧"), Step.bEnteredCredits);
	}
	// 흡입 시각 계산: 마감 이내
	TestEqual(TEXT("거리 0은 흡입 시작 시각"), DigetherGameOverRules::CalculateAbsorbSeconds(0.0, 0.0, Tuning), 5.0);
	TestEqual(TEXT("먼 거리도 마감 이하"), DigetherGameOverRules::CalculateAbsorbSeconds(0.0, 1.0e9, Tuning), 10.0);
	TestEqual(TEXT("범람 진행률 시작"), DigetherGameOverRules::CalculateFloodAlpha(0.0, 0.0, Tuning), 0.f);
	TestEqual(TEXT("범람 진행률 절반"), DigetherGameOverRules::CalculateFloodAlpha(0.0, 2.5, Tuning), 0.5f);
	TestEqual(TEXT("흡입 시작에 범람 완료"), DigetherGameOverRules::CalculateFloodAlpha(0.0, 6.0, Tuning), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherGameOverCreditSyncTest,
	"DIGETHER.Game.Run.GameOver.CreditPlaybackSameForAllAndFastForward",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherGameOverCreditSyncTest::RunTest(const FString& Parameters)
{
	// T33: 모든 화면이 같은 기준점·서버 시각으로 같은 진행 계산, 빨리감기는 기준점 재설정
	FDigetherGameOverTuning Tuning;
	Tuning.CreditFastForwardMultiplier = 4.f;
	const FDigetherCreditPlayback Start = DigetherGameOverRules::MakeCreditPlayback(50.0, 4, Tuning);
	TestEqual(TEXT("4인 크레딧 줄 수"), DigetherGameOverRules::CountCreditLines(4), 2 + 3 * (1 + 4));
	TestTrue(TEXT("인원이 많으면 크레딧이 길다"), DigetherGameOverRules::CalculateCreditTotalSeconds(4, Tuning)
		> DigetherGameOverRules::CalculateCreditTotalSeconds(1, Tuning));
	const FDigetherCreditPlayback HostView = Start;
	const FDigetherCreditPlayback GuestView = Start;
	TestEqual(TEXT("같은 서버 시각이면 호스트·참가자 진행 같음"),
		DigetherGameOverRules::CalculateCreditProgressSeconds(HostView, 60.0),
		DigetherGameOverRules::CalculateCreditProgressSeconds(GuestView, 60.0));
	TestEqual(TEXT("1배속 진행"), DigetherGameOverRules::CalculateCreditProgressSeconds(Start, 60.0), 10.f);
	TestEqual(TEXT("시계 오차로 기준 전 시각이어도 뒤로 가지 않음"),
		DigetherGameOverRules::CalculateCreditProgressSeconds(Start, 40.0), 0.f);
	FDigetherCreditPlayback Fast;
	TestTrue(TEXT("빨리감기 켬"), DigetherGameOverRules::TrySetFastForward(Start, true, 60.0, Tuning, Fast));
	TestEqual(TEXT("빨리감기 기준 진행"), Fast.AnchorProgressSeconds, 10.f);
	TestEqual(TEXT("빨리감기 배율"), Fast.PlaybackRate, 4.f);
	TestEqual(TEXT("빨리감기 뒤 진행"), DigetherGameOverRules::CalculateCreditProgressSeconds(Fast, 62.0), 18.f);
	FDigetherCreditPlayback Same = Fast;
	TestFalse(TEXT("같은 상태 요청은 기준점 유지"), DigetherGameOverRules::TrySetFastForward(Fast, true, 63.0, Tuning, Same));
	TestEqual(TEXT("같은 상태 요청 뒤 출력 보존"), Same.AnchorServerSeconds, Fast.AnchorServerSeconds);
	FDigetherCreditPlayback Normal;
	TestTrue(TEXT("빨리감기 끔"), DigetherGameOverRules::TrySetFastForward(Fast, false, 62.0, Tuning, Normal));
	TestEqual(TEXT("끈 뒤 1배속"), DigetherGameOverRules::CalculateCreditProgressSeconds(Normal, 63.0), 19.f);
	TestTrue(TEXT("끝 이후 진행은 전체 길이"), DigetherGameOverRules::IsCreditFinished(Start, 50.0 + Start.TotalSeconds + 1.0));
	TestEqual(TEXT("진행은 전체 길이를 넘지 않음"),
		DigetherGameOverRules::CalculateCreditProgressSeconds(Start, 1.0e6), Start.TotalSeconds);
	// 진행 컴포넌트: 크레딧 단계에서만 빨리감기 수락
	UDigetherGameOverFlowComponent* Flow = NewObject<UDigetherGameOverFlowComponent>();
	FDigetherGameOverTuning FlowTuning;
	FlowTuning.SettlementScreenSeconds = 0.f;
	Flow->Begin(0.0, MakeFlowResult(1), FlowTuning);
	TestFalse(TEXT("크레딧 전 빨리감기 거절"), Flow->SetCreditFastForward(true, 0.0));
	Flow->Advance(0.0, TArray<FDigetherGameOverPresentPlayer>());
	Flow->Advance(0.1, TArray<FDigetherGameOverPresentPlayer>());
	TestTrue(TEXT("크레딧 중 빨리감기"), Flow->SetCreditFastForward(true, 1.0));
	TestTrue(TEXT("공개 재생 기준점에 빨리감기 반영"), Flow->GetPublicState().Playback.bFastForward);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherGameOverResultStatsTest,
	"DIGETHER.Game.Run.GameOver.ResultStatsT29",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherGameOverResultStatsTest::RunTest(const FString& Parameters)
{
	// T29·S33 247행: A 일반 600 + 히든 100, B 일반 300 + 히든 50 → 전체 1,050, 획득 A 700·B 350, 히든 A 100·B 50
	// 런 플로우 v1.1.0 9.4·10.4: A가 구매 300 뒤 사망·시체 유효 회수면 잔고 400의 50% = 200 차감(복구금 없음)
	// 통계 사망 손실은 실제 차감액 200, 이전 시험의 '차감 400 - 복구 200' 산식을 실제 차감 산식으로 변경
	FDigetherWalletState A = FDigetherEconomyRules::ApplyRunSettlement(FDigetherWalletState(), MakePayout(600, 100)).State;
	FDigetherWalletState B = FDigetherEconomyRules::ApplyRunSettlement(FDigetherWalletState(), MakePayout(300, 50)).State;
	TestEqual(TEXT("A 누적 획득"), A.LifetimeEarnedCredits, 700);
	A.BalanceCredits -= 300;
	FDigetherRunTuning LossTuning;
	LossTuning.DeathLossPercent = 100;
	LossTuning.RecoveredDeathLossPercent = 50;
	A = FDigetherEconomyRules::ApplyDeathLoss(A, FDigetherEconomyRules::GetExplorationDeathLossPercent(LossTuning, true)).State;
	TestEqual(TEXT("A 회수 시체 사망 차감 200"), A.TotalDeathLossCredits, 200);
	const FDigetherPlayerSettlement DeadSettlement;
	A = FDigetherEconomyRules::ApplyRunSettlement(A, DeadSettlement).State;
	TestEqual(TEXT("손실 뒤 잔고 200"), A.BalanceCredits, 200);
	TestEqual(TEXT("사망 정산은 획득 불변"), A.LifetimeEarnedCredits, 700);
	TestEqual(TEXT("복구 누적 없음"), A.LifetimeCorpseRefundCredits, 0);

	TArray<FDigetherGameResultInput> Inputs;
	FDigetherGameResultInput& InputB = Inputs.AddDefaulted_GetRef();
	InputB.DisplayName = TEXT("B");
	InputB.JoinOrder = 2;
	InputB.Wallet = B;
	FDigetherGameResultInput& InputA = Inputs.AddDefaulted_GetRef();
	InputA.DisplayName = TEXT("A");
	InputA.JoinOrder = 1;
	InputA.Wallet = A;
	FDigetherGameResultInput& Unknown = Inputs.AddDefaulted_GetRef();
	Unknown.DisplayName = TEXT("C");
	FDigetherGameResult Result;
	if (!TestTrue(TEXT("결과 기록 생성"), DigetherGameOverRules::TryMakeGameResult(FGuid(1, 0, 0, 0), FGuid(2, 0, 0, 0), 5, 0,
		Inputs, Result)))
	{
		return false;
	}
	TestEqual(TEXT("전체 획득 1,050(히든 중복 합산 1,200 아님)"), Result.TotalEarnedCredits, 1050);
	if (TestEqual(TEXT("결과 인원(0원 참가자 생략 안 함)"), Result.Players.Num(), 3))
	{
		TestEqual(TEXT("참가 순서 1번 A"), Result.Players[0].DisplayName, FString(TEXT("A")));
		TestEqual(TEXT("참가 순서 2번 B"), Result.Players[1].DisplayName, FString(TEXT("B")));
		TestEqual(TEXT("순서를 모르는 기록은 뒤"), Result.Players[2].DisplayName, FString(TEXT("C")));
		TestEqual(TEXT("A 획득 700"), Result.Players[0].EarnedCredits, 700);
		TestEqual(TEXT("B 획득 350"), Result.Players[1].EarnedCredits, 350);
		TestEqual(TEXT("A 히든 100"), Result.Players[0].HiddenCredits, 100);
		TestEqual(TEXT("B 히든 50"), Result.Players[1].HiddenCredits, 50);
		TestEqual(TEXT("A 사망 손실 순액 200"), Result.Players[0].DeathLossCredits, 200);
		TestEqual(TEXT("B 사망 손실 0"), Result.Players[1].DeathLossCredits, 0);
		TestEqual(TEXT("C 0원 표시"), Result.Players[2].EarnedCredits, 0);
	}
	TestTrue(TEXT("결과 값 형태 유효"), DigetherGameOverRules::IsValidGameResult(Result, DigetherSaveTuning::MaxPlayerRecords));

	// 정산 재전송: 같은 탐사 정산은 한 번만(AlreadySettled), 실제 차감 손실 200 유지
	FDigetherSaveSnapshot Snapshot = MakeGameOverSaveFixture();
	Snapshot.Players[0].Wallet = A;
	FDigetherSaveSnapshot Settled;
	const TArray<FDigetherPlayerSettlement> DeadOnly = { DeadSettlement, FDigetherPlayerSettlement() };
	if (TestTrue(TEXT("사망자 정산 한 번"), DigetherSaveRules::TryApplySettlement(Snapshot, Snapshot.RunId, DeadOnly, Settled)
		== EDigetherSaveResult::Success))
	{
		FDigetherSaveSnapshot Twice = Settled;
		TestTrue(TEXT("같은 정산 재전송 거절"), DigetherSaveRules::TryApplySettlement(Settled, Settled.RunId, DeadOnly, Twice)
			== EDigetherSaveResult::AlreadySettled);
		TestEqual(TEXT("재전송 뒤 잔고 200 유지"), Twice.Players[0].Wallet.BalanceCredits, 200);
		TestEqual(TEXT("재전송 뒤 사망 손실 200 유지"),
			DigetherGameOverRules::CalculateNetDeathLoss(Twice.Players[0].Wallet), 200);
	}
	// 구 저장(복구금 시절, 차감 400·복구 200)도 통계는 실제로 빠진 200
	FDigetherWalletState LegacyWallet;
	LegacyWallet.TotalDeathLossCredits = 400;
	LegacyWallet.LifetimeCorpseRefundCredits = 200;
	TestEqual(TEXT("구 저장 사망 손실 순액 200"), DigetherGameOverRules::CalculateNetDeathLoss(LegacyWallet), 200);

	// 이전 게임 결과 보관: 같은 결과는 한 번, 상한 초과 시 오래된 것부터 삭제
	TArray<FDigetherGameResult> Archive;
	for (int32 Index = 0; Index < DigetherSaveTuning::MaxArchivedGameResults + 2; ++Index)
	{
		FDigetherGameResult Entry = Result;
		Entry.ResultId = FGuid(100 + Index, 0, 0, 0);
		DigetherGameOverRules::ArchiveGameResult(Archive, Entry, DigetherSaveTuning::MaxArchivedGameResults);
		DigetherGameOverRules::ArchiveGameResult(Archive, Entry, DigetherSaveTuning::MaxArchivedGameResults);
	}
	TestEqual(TEXT("보관 상한"), Archive.Num(), DigetherSaveTuning::MaxArchivedGameResults);
	TestTrue(TEXT("가장 오래된 결과부터 버림"), Archive[0].ResultId == FGuid(102, 0, 0, 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherGameOverSaveRoundTripTest,
	"DIGETHER.Game.Save.GameOver.ResultStatsRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherGameOverSaveRoundTripTest::RunTest(const FString& Parameters)
{
	// 게임 오버 확정 저장(정산·통계·bGameOver·결과 기록)과 이전 결과 보관 목록의 진행 확장 v2 왕복
	FDigetherSaveSlotData Slot;
	Slot.Progress = MakeGameOverSaveFixture();
	Slot.Progress.Quota.Quota.Day = 5;
	Slot.Progress.Quota.Quota.TotalDay = 9;
	Slot.Progress.RunPublicState.bGameOver = true;
	Slot.Progress.RunPublicState.bPaymentDeadline = true;
	Slot.Progress.Players[0].Wallet.LifetimeEarnedCredits = 700;
	Slot.Progress.Players[0].Wallet.HiddenLifetimeCredits = 100;
	Slot.Progress.Players[0].Wallet.TotalDeathLossCredits = 400;
	Slot.Progress.Players[0].Wallet.LifetimeCorpseRefundCredits = 200;
	Slot.Progress.Players[1].Wallet.LifetimeEarnedCredits = 350;
	Slot.Progress.Players[1].Wallet.HiddenLifetimeCredits = 50;
	TArray<FDigetherGameResultInput> Inputs;
	for (const FDigetherSavePlayerRecord& Record : Slot.Progress.Players)
	{
		FDigetherGameResultInput& Input = Inputs.AddDefaulted_GetRef();
		Input.DisplayName = Record.DisplayName;
		Input.JoinOrder = Record.JoinOrder;
		Input.Wallet = Record.Wallet;
	}
	if (!TestTrue(TEXT("확정 결과 생성"), DigetherGameOverRules::TryMakeGameResult(FGuid(7, 7, 7, 7), Slot.Progress.GameId,
		9, 1, Inputs, Slot.Progress.GameResult)))
	{
		return false;
	}
	Slot.Progress.bHasGameResult = true;
	FDigetherGameResult Previous = Slot.Progress.GameResult;
	Previous.ResultId = FGuid(6, 6, 6, 6);
	Previous.GameId = FGuid(5, 5, 5, 5);
	Slot.Progress.ArchivedGameResults.Add(Previous);
	Slot.SavedAtUtcTicks = FDateTime(2026, 11, 5, 9, 0, 0).GetTicks();
	TArray<uint8> Bytes;
	if (!TestTrue(TEXT("게임 오버 확정 슬롯 직렬화"), DigetherSaveRules::SerializeSlot(Slot, Bytes) == EDigetherSaveResult::Success))
	{
		return false;
	}
	FDigetherSaveSlotData Restored;
	if (!TestTrue(TEXT("게임 오버 확정 슬롯 읽기"), DigetherSaveRules::DeserializeSlot(Bytes, Restored) == EDigetherSaveResult::Success))
	{
		return false;
	}
	const FDigetherSaveSnapshot& Progress = Restored.Progress;
	TestTrue(TEXT("게임 오버 확정 유지"), Progress.RunPublicState.bGameOver);
	TestTrue(TEXT("결과 기록 유지"), Progress.bHasGameResult && Progress.GameResult.ResultId == FGuid(7, 7, 7, 7));
	TestEqual(TEXT("결과 전체 획득 유지"), Progress.GameResult.TotalEarnedCredits, 1050);
	TestEqual(TEXT("결과 사망 손실 유지"), Progress.GameResult.Players[0].DeathLossCredits, 200);
	TestEqual(TEXT("결과 합산 기간 유지"), Progress.GameResult.FinalTotalDay, 9);
	TestEqual(TEXT("보관 결과 유지"), Progress.ArchivedGameResults.Num(), 1);
	TestEqual(TEXT("플레이어 누적 획득 유지"), Progress.Players[0].Wallet.LifetimeEarnedCredits, 700);
	TestEqual(TEXT("플레이어 복구 누적 유지"), Progress.Players[0].Wallet.LifetimeCorpseRefundCredits, 200);
	TestEqual(TEXT("표시 이름 유지"), Progress.Players[1].DisplayName, FString(TEXT("B")));
	TestEqual(TEXT("참가 순서 유지"), Progress.Players[1].JoinOrder, 2);
	TArray<uint8> Reencoded;
	TestTrue(TEXT("재직렬화가 바이트 단위로 동일"), DigetherSaveRules::SerializeSlot(Restored, Reencoded)
		== EDigetherSaveResult::Success && Reencoded == Bytes);
	// 결과 플레이어 수가 기본 본문과 불일치하면 거절(손상 파일)
	TArray<uint8> Truncated = Bytes;
	Truncated.SetNum(Truncated.Num() - 1);
	TestTrue(TEXT("잘린 결과 확장 거절"), DigetherSaveRules::DeserializeSlot(Truncated, Restored) == EDigetherSaveResult::CorruptData);
	// 제어 문자가 섞인 이름은 저장 가능한 형태로 정리
	TestEqual(TEXT("표시 이름 제어 문자 제거"), DigetherSaveRules::MakeStoredDisplayName(TEXT("A\tB\nC")), FString(TEXT("ABC")));
	return true;
}

#endif
