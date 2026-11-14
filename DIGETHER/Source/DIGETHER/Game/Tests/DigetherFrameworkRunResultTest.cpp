#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Game/Framework/DigetherFrameworkRules.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Life/DigetherLifeRules.h"
#include "Game/UI/Results/DigetherRunResultWidget.h"
#include "Misc/AutomationTest.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

namespace
{
	// 정산 표시 경계 테스트 픽스처, 기획 수치 아님
	FDigetherRunTuning MakeRunResultTuning()
	{
		FDigetherRunTuning Tuning;
		Tuning.ProfileVersion = 17;
		Tuning.InitialQuotaCredits = 175;
		Tuning.RunSeconds = 540.f;
		Tuning.InteractionDistanceCm = 245.f;
		Tuning.ElevatorCapacityKg = 35.f;
		Tuning.ElevatorTripLimit = 2;
		Tuning.AscentPenaltySeconds = 5.f;
		Tuning.ElevatorTravelSeconds = 3.f;
		Tuning.DepositIntervalSeconds = 0.2f;
		Tuning.EmergencyWeightLimitKg = 4.f;
		Tuning.DeathLossPercent = 80;
		Tuning.RecoveredDeathLossPercent = 30;
		return Tuning;
	}

	FDigetherRunResult MakeRunResultFixture()
	{
		FDigetherRunResult Result;
		Result.RunId = FGuid(1, 2, 3, 4);
		Result.CompletedDay = 2;
		Result.GeneralCredits = 11;
		Result.HiddenCredits = 13;
		Result.PayoutCredits = 24;
		Result.FinalBalanceCredits = 100;
		return Result;
	}

	bool AreRunResultsEqual(const FDigetherRunResult& Left, const FDigetherRunResult& Right)
	{
		return FDigetherRunResult::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
	}

	class FDigetherRunResultTestWorld
	{
	public:
		FDigetherRunResultTestWorld()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &Values);
		}

		~FDigetherRunResultTestWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
			}
		}

		FDigetherRunResultTestWorld(const FDigetherRunResultTestWorld&) = delete;
		FDigetherRunResultTestWorld& operator=(const FDigetherRunResultTestWorld&) = delete;
		UWorld* Get() const { return World; }

	private:
		UWorld* World = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkRunResultPlanTest,
	"DIGETHER.Game.Framework.RunResult.ReturnPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkRunResultPlanTest::RunTest(const FString& Parameters)
{
	TArray<FDigetherFrameworkPlayerSnapshot> Players;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		FDigetherFrameworkPlayerSnapshot& Player = Players.AddDefaulted_GetRef();
		Player.PlayerKey = FString::Printf(TEXT("fixture-result-%d"), Index);
		Player.Wallet.BalanceCredits = 100;
	}
	Players[1].Wallet.BalanceCredits = 200;
	Players[1].Wallet.TotalDeathLossCredits = MAX_int32;
	Players[2].Life = FDigetherLifeRules::ApplyDeath(Players[2].Life, EDigetherDeathCause::Killed).State;
	// D25 개정: 탐사 중 사망도 지갑 미차감 상태로 정산에 들어와 한 번만 차감(이전 시험은 사망 시점 차감 뒤 잔고 50 가정)
	Players[2].Wallet.BalanceCredits = 250;
	Players[2].Wallet.TotalDeathLossCredits = MAX_int32;
	Players[3].Life = FDigetherLifeRules::ApplyEscape(Players[3].Life, EDigetherEscapeKind::Emergency).State;
	Players[3].Wallet.LastPreDeathBalanceCredits = 999;

	FDigetherRunTerminationInput Input;
	Input.Trigger = EDigetherRunTerminationTrigger::FinalAscentCompleted;
	Input.ElevatorState = EDigetherRunElevatorState::AtBase;
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		FDigetherRunPlayerInput& Player = Input.Players.AddDefaulted_GetRef();
		Player.PlayerKey = Players[Index].PlayerKey;
		Player.Life = Players[Index].Life.PublicState.Life;
		Player.Location = Index == 1 ? EDigetherRunPlayerLocation::MineOrDungeon : EDigetherRunPlayerLocation::Base;
	}
	FDigetherQuotaState Quota;
	Quota.Day = 1;
	Quota.TargetCredits = 175;
	Quota.PeriodGeneration = 3;
	FDigetherFrameworkReturnPlan Plan;
	if (!TestTrue(TEXT("결과를 포함한 귀환 계획"), DigetherFrameworkRules::TryPlanReturn(
		Input, Players, Quota, MakeRunResultTuning(), Players.Num(), 0, Plan))
		|| !TestEqual(TEXT("모든 결과 순서 보존"), Plan.Players.Num(), Players.Num()))
	{
		return false;
	}
	TestEqual(TEXT("생존 귀환"), Plan.Players[0].ReturnOutcome, EDigetherRunReturnOutcome::Returned);
	TestEqual(TEXT("활동 잔류자는 실종"), Plan.Players[1].ReturnOutcome, EDigetherRunReturnOutcome::Missing);
	TestEqual(TEXT("기존 사망자는 Unchanged 종료여도 사망"), Plan.Players[2].ReturnOutcome, EDigetherRunReturnOutcome::Died);
	TestEqual(TEXT("기존 비상 탈출자는 귀환"), Plan.Players[3].ReturnOutcome, EDigetherRunReturnOutcome::Returned);
	TestEqual(TEXT("실종 손실은 포화된 누적 차이가 아님"), Plan.Players[1].DeathLossCredits, 160);
	TestEqual(TEXT("기존 사망 손실은 정산 때 사망 전 잔고 기준"), Plan.Players[2].DeathLossCredits, 200);
	TestEqual(TEXT("기존 사망자 최종 지갑"), Plan.Players[2].SettledWallet.BalanceCredits, 50);
	TestEqual(TEXT("시체 회수 미연결은 미회수 손실률"), Plan.Players[2].DeathLossPercent, 80);
	TestFalse(TEXT("시체 회수 미연결"), Plan.Players[2].bCorpseRecovered);
	TestEqual(TEXT("사망자 이전 잔고"), Plan.Players[2].PreviousBalanceCredits, 250);
	TestEqual(TEXT("이전 사망 전 잔고가 남아도 탈출자의 이번 손실 0"), Plan.Players[3].DeathLossCredits, 0);
	TestEqual(TEXT("누적 손실 포화가 결과 손실을 지우지 않음"),
		Plan.Players[1].WalletAtEnd.TotalDeathLossCredits, MAX_int32);
	TestEqual(TEXT("실종자의 최종 지갑"), Plan.Players[1].SettledWallet.BalanceCredits, 40);
	for (const FDigetherFrameworkPlayerReturn& Returned : Plan.Players)
	{
		TestEqual(TEXT("현재 화물 미연결의 실제 일반 몫 0"), Returned.Settlement.GeneralCredits, 0);
		TestEqual(TEXT("현재 히든 미연결의 실제 수익 0"), Returned.Settlement.HiddenCredits, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkRunResultProjectionTest,
	"DIGETHER.Game.Framework.RunResult.ProjectCommittedValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkRunResultProjectionTest::RunTest(const FString& Parameters)
{
	FDigetherFrameworkReturnPlan Plan;
	FDigetherFrameworkPlayerReturn& Returned = Plan.Players.AddDefaulted_GetRef();
	Returned.Settlement.GeneralCredits = 11;
	Returned.Settlement.HiddenCredits = 13;
	Returned.Settlement.PayoutCredits = 24;
	Returned.SettledWallet.BalanceCredits = 100;
	const FDigetherRunResult Expected = MakeRunResultFixture();
	const FDigetherRunResult Result = DigetherFrameworkRules::MakeRunResult(Expected.RunId, Expected.CompletedDay, Plan, 0);
	TestTrue(TEXT("런 ID·일차·정산 상세를 손실 없이 투영"), AreRunResultsEqual(Result, Expected));

	// 런 플로우 9.5: 위쪽 공통 값(파밍액·사망자·실종자 이름·제외액)과 아래쪽 본인 값, 복구금 항목 없음
	Plan.GeneralCargoCredits = 901;
	Plan.ExcludedGeneralCredits = 676;
	Plan.DeadPlayerNames = { TEXT("A") };
	Plan.MissingPlayerNames = { TEXT("B"), TEXT("C") };
	Returned.ReturnOutcome = EDigetherRunReturnOutcome::Died;
	Returned.Settlement = FDigetherPlayerSettlement();
	Returned.PreviousBalanceCredits = 101;
	Returned.DeathLossCredits = 51;
	Returned.DeathLossPercent = 50;
	Returned.bCorpseRecovered = true;
	Returned.SettledWallet.BalanceCredits = 50;
	const FDigetherRunResult DeadResult = DigetherFrameworkRules::MakeRunResult(Expected.RunId, Expected.CompletedDay, Plan, 0);
	TestTrue(TEXT("사망 결과 유효"), DeadResult.IsValid());
	TestEqual(TEXT("파밍액 공개"), DeadResult.FarmingCredits, 901);
	TestEqual(TEXT("제외된 금액 공개"), DeadResult.ExcludedCredits, 676);
	TestTrue(TEXT("사망자 이름 별도 목록"), DeadResult.DeadPlayerNames == TArray<FString>({ TEXT("A") }));
	TestTrue(TEXT("실종자 이름 별도 목록"), DeadResult.MissingPlayerNames == TArray<FString>({ TEXT("B"), TEXT("C") }));
	TestEqual(TEXT("본인 이전 잔고"), DeadResult.PreviousBalanceCredits, 101);
	TestEqual(TEXT("사망 손실 실제 차감액"), DeadResult.DeathLossCredits, 51);
	TestEqual(TEXT("적용 손실률"), DeadResult.DeathLossPercent, 50);
	TestTrue(TEXT("시체 회수 여부"), DeadResult.bCorpseRecovered);
	TestEqual(TEXT("사망자 탐사 지급액 0"), DeadResult.PayoutCredits, 0);
	TestEqual(TEXT("손실 뒤 실제 최종 지갑"), DeadResult.FinalBalanceCredits, 50);
	TestFalse(TEXT("범위 밖 순번은 무효"), DigetherFrameworkRules::MakeRunResult(Expected.RunId, Expected.CompletedDay, Plan, 1).IsValid());

	FDigetherRunResult Invalid = DeadResult;
	Invalid.ExcludedCredits = 902;
	TestFalse(TEXT("제외액이 파밍액보다 크면 무효"), Invalid.IsValid());
	Invalid = DeadResult;
	Invalid.DeathLossPercent = 101;
	TestFalse(TEXT("손실률 100% 초과 무효"), Invalid.IsValid());
	Invalid = DeadResult;
	Invalid.Outcome = EDigetherRunReturnOutcome::Returned;
	TestFalse(TEXT("귀환자 시체 회수 표시는 무효"), Invalid.IsValid());

	// 결과 화면 문구: 사망 손실에 회수 여부·비율, 이름 목록, 복구 항목 없음
	TestEqual(TEXT("회수 시체 손실 문구"), UDigetherRunResultWidget::MakeDeathLossText(DeadResult).ToString(),
		FString(TEXT("사망 손실  -51 (시체 회수, 50%)")));
	FDigetherRunResult Unrecovered = DeadResult;
	Unrecovered.bCorpseRecovered = false;
	Unrecovered.DeathLossPercent = 100;
	Unrecovered.DeathLossCredits = 101;
	TestEqual(TEXT("미회수 시체 손실 문구"), UDigetherRunResultWidget::MakeDeathLossText(Unrecovered).ToString(),
		FString(TEXT("사망 손실  -101 (시체 미회수, 100%)")));
	TestEqual(TEXT("귀환자 손실 문구"), UDigetherRunResultWidget::MakeDeathLossText(Result).ToString(),
		FString(TEXT("사망 손실  0")));
	TestEqual(TEXT("이름 목록 문구"), UDigetherRunResultWidget::JoinPlayerNames(DeadResult.MissingPlayerNames).ToString(),
		FString(TEXT("B, C")));
	TestEqual(TEXT("빈 이름 목록 문구"), UDigetherRunResultWidget::JoinPlayerNames(TArray<FString>()).ToString(),
		FString(TEXT("없음")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkRunResultStateTest,
	"DIGETHER.Game.Framework.RunResult.OwnerStateHandoff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkRunResultStateTest::RunTest(const FString& Parameters)
{
	FDigetherRunResultTestWorld TestWorld;
	UWorld* World = TestWorld.Get();
	if (!TestNotNull(TEXT("격리 결과 시험 월드"), World))
	{
		return false;
	}
	ADigetherGameplayPlayerState* Source = World->SpawnActor<ADigetherGameplayPlayerState>();
	ADigetherGameplayPlayerState* Copy = World->SpawnActor<ADigetherGameplayPlayerState>();
	ADigetherGameplayPlayerState* Override = World->SpawnActor<ADigetherGameplayPlayerState>();
	if (!TestNotNull(TEXT("결과 원본"), Source) || !TestNotNull(TEXT("인계 대상"), Copy)
		|| !TestNotNull(TEXT("복원 대상"), Override))
	{
		return false;
	}
	Source->InitializeGameplayIdentity();
	Source->MarkGameplayInitialized();
	FDigetherWalletState Wallet;
	Wallet.BalanceCredits = 100;
	Source->RestoreState(Wallet);
	const FDigetherRunResult Expected = MakeRunResultFixture();
	int32 Notifications = 0;
	const FDelegateHandle Handle = Source->OnRunResultChanged.AddLambda([&Notifications](const FDigetherRunResult&)
	{
		++Notifications;
	});
	TestTrue(TEXT("커밋된 지갑과 일치한 결과 발행"), Source->PublishRunResult(Expected));
	TestEqual(TEXT("호스트도 같은 알림 경로"), Notifications, 1);
	FDigetherRunResult Invalid = Expected;
	Invalid.FinalBalanceCredits = 101;
	TestFalse(TEXT("아직 적용되지 않은 잔고 발행 거절"), Source->PublishRunResult(Invalid));
	Invalid = Expected;
	Invalid.HiddenCredits = -1;
	TestFalse(TEXT("음수 정산 발행 거절"), Source->PublishRunResult(Invalid));
	Invalid = Expected;
	Invalid.RunId.Invalidate();
	TestFalse(TEXT("식별자 없는 결과 거절"), Source->PublishRunResult(Invalid));
	TestTrue(TEXT("거절은 마지막 결과 보존"), AreRunResultsEqual(Source->GetLastRunResult(), Expected));
	TestEqual(TEXT("거절은 UI 재통지하지 않음"), Notifications, 1);
	Source->CopyProperties(Copy);
	Override->OverrideWith(Source);
	TestTrue(TEXT("PlayerState 복사에 결과 포함"), AreRunResultsEqual(Copy->GetLastRunResult(), Expected));
	TestTrue(TEXT("재접속 상태 복원에 결과 포함"), AreRunResultsEqual(Override->GetLastRunResult(), Expected));
	Source->SetIsInactive(true);
	ADigetherGameplayPlayerState* FreshLogin = World->SpawnActor<ADigetherGameplayPlayerState>();
	if (TestNotNull(TEXT("재접속 기본 상태"), FreshLogin))
	{
		Source->OverrideWith(FreshLogin);
		TestTrue(TEXT("새 로그인 기본값이 끊긴 동안 결과를 덮지 않음"), AreRunResultsEqual(Source->GetLastRunResult(), Expected));
	}
	Source->ClearRunResult();
	TestFalse(TEXT("다음 출발의 clear는 결과 무효화"), Source->GetLastRunResult().RunId.IsValid());
	TestEqual(TEXT("clear 알림 1회"), Notifications, 2);
	Source->ClearRunResult();
	TestEqual(TEXT("중복 clear는 알림 없음"), Notifications, 2);
	Source->OnRunResultChanged.Remove(Handle);

	UClass* StateClass = Source->GetClass();
	StateClass->SetUpRuntimeReplicationData();
	const FProperty* Property = FindFProperty<FProperty>(StateClass, TEXT("LastRunResult"));
	if (TestNotNull(TEXT("복제 결과 프로퍼티"), Property))
	{
		TArray<FLifetimeProperty> Properties;
		Source->GetLifetimeReplicatedProps(Properties);
		const FLifetimeProperty* Replication = Properties.FindByPredicate([Property](const FLifetimeProperty& Entry)
		{
			return Entry.RepIndex == Property->RepIndex;
		});
		if (TestNotNull(TEXT("결과 복제 등록"), Replication))
		{
			TestEqual(TEXT("개인 정산은 소유 연결에만 전달"), Replication->Condition, COND_OwnerOnly);
		}
	}
	return true;
}

#endif
