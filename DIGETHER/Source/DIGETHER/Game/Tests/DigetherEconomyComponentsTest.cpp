#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Economy/DigetherWalletComponent.h"
#include "Game/Economy/DigetherQuotaStateComponent.h"
#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Economy/DigetherEconomyTuning.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

namespace
{
	// 디스크 맵·기존 에디터 월드를 쓰지 않는다. 조기 반환도 월드를 정리한다.
	class FDigetherEconomyTestWorld
	{
	public:
		FDigetherEconomyTestWorld()
		{
			UWorld::InitializationValues Values;
			Values.InitializeScenes(false).AllowAudioPlayback(false).CreatePhysicsScene(false)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false)
				.CreateFXSystem(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &Values);
		}

		~FDigetherEconomyTestWorld()
		{
			if (World != nullptr)
			{
				World->DestroyWorld(false);
			}
		}

		FDigetherEconomyTestWorld(const FDigetherEconomyTestWorld&) = delete;
		FDigetherEconomyTestWorld& operator=(const FDigetherEconomyTestWorld&) = delete;

		UWorld* World = nullptr;
	};

	bool AreWalletStatesEqual(const FDigetherWalletState& Left, const FDigetherWalletState& Right)
	{
		return FDigetherWalletState::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
	}

	bool ArePublicStatesEqual(const FDigetherQuotaPublicState& Left, const FDigetherQuotaPublicState& Right)
	{
		return FDigetherQuotaPublicState::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
	}

	bool HasReplicationCondition(const UActorComponent* Component, const FName PropertyName,
		const ELifetimeCondition Condition)
	{
		UClass* ComponentClass = Component->GetClass();
		ComponentClass->SetUpRuntimeReplicationData();
		const FProperty* Property = FindFProperty<FProperty>(ComponentClass, PropertyName);
		if (Property == nullptr || !Property->HasAllPropertyFlags(CPF_Net | CPF_RepNotify))
		{
			return false;
		}

		TArray<FLifetimeProperty> Properties;
		Component->GetLifetimeReplicatedProps(Properties);
		for (const FLifetimeProperty& ReplicatedProperty : Properties)
		{
			if (ReplicatedProperty.RepIndex == Property->RepIndex)
			{
				return ReplicatedProperty.Condition == Condition;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherEconomyComponentsTest,
	"DIGETHER.Game.Economy.Components", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherEconomyComponentsTest::RunTest(const FString& Parameters)
{
	FDigetherWalletState WalletState;
	WalletState.BalanceCredits = 321;
	WalletState.LastRunPayoutCredits = 87;
	WalletState.HiddenLifetimeCredits = 54;
	WalletState.TotalDeathLossCredits = 13;
	WalletState.LastPreDeathBalanceCredits = 101;

	FDigetherQuotaPublicState PublicState;
	PublicState.Quota.Day = 3;
	PublicState.Quota.TargetCredits = 500;
	PublicState.Quota.DepositedCredits = 120;
	PublicState.Quota.PeriodGeneration = 7;
	FDigetherHiddenRevealEntry Reveal;
	Reveal.PlayerKey = TEXT("test-player-a");
	Reveal.HiddenLifetimeCredits = WalletState.HiddenLifetimeCredits;
	PublicState.HiddenReveals.Add(Reveal);
	Reveal.PlayerKey = TEXT("test-player-b");
	Reveal.HiddenLifetimeCredits = 0;
	PublicState.HiddenReveals.Add(Reveal);

	UDigetherWalletComponent* OrphanWallet = NewObject<UDigetherWalletComponent>();
	UDigetherQuotaStateComponent* OrphanQuota = NewObject<UDigetherQuotaStateComponent>();
	const FDigetherWalletState EmptyWallet = OrphanWallet->CaptureState();
	const FDigetherQuotaPublicState EmptyPublic = OrphanQuota->CaptureState();
	TestTrue(TEXT("지갑 기본 복제 활성"), OrphanWallet->GetIsReplicated());
	TestTrue(TEXT("공개 상태 기본 복제 활성"), OrphanQuota->GetIsReplicated());
	TestTrue(TEXT("지갑 개인 값은 소유자 전용 복제"), HasReplicationCondition(OrphanWallet, TEXT("PrivateState"), COND_OwnerOnly));
	TestTrue(TEXT("할당량 복제 값은 조건 없는 복제"), HasReplicationCondition(OrphanQuota, TEXT("ReplicatedState"), COND_None));
	const FProperty* ServerQuotaProperty = FindFProperty<FProperty>(OrphanQuota->GetClass(), TEXT("PublicState"));
	TestTrue(TEXT("누적 인정액을 담은 서버 원본은 비복제"),
		ServerQuotaProperty != nullptr && !ServerQuotaProperty->HasAnyPropertyFlags(CPF_Net));
	TestNull(TEXT("복제 구조체에 누적 인정액 없음"),
		FindFProperty<FProperty>(FDigetherQuotaReplicatedState::StaticStruct(), TEXT("DepositedCredits")));
	TestEqual(TEXT("목표 전 신호기 없음"), OrphanQuota->GetPaymentSignal(), EDigetherPaymentSignal::None);
	TestEqual(TEXT("공개 전 히든 목록은 비어 있음"), EmptyPublic.HiddenReveals.Num(), 0);
	TestEqual(TEXT("소유자 없는 지갑 복원 거절"), OrphanWallet->RestoreState(WalletState), EDigetherEconomyResult::NotAuthority);
	FDigetherWalletUpdateResult Update;
	Update.Result = EDigetherEconomyResult::Success;
	Update.State = WalletState;
	TestEqual(TEXT("소유자 없는 지갑 결과 적용 거절"), OrphanWallet->ApplyUpdate(Update), EDigetherEconomyResult::NotAuthority);
	TestEqual(TEXT("소유자 없는 지갑 통지 거절"), OrphanWallet->NotifyStateChanged(), EDigetherEconomyResult::NotAuthority);
	TestEqual(TEXT("소유자 없는 공개 상태 복원 거절"), OrphanQuota->RestoreState(PublicState), EDigetherEconomyResult::NotAuthority);
	TestEqual(TEXT("소유자 없는 목표 setter 거절"), OrphanQuota->SetQuotaState(PublicState.Quota), EDigetherEconomyResult::NotAuthority);
	TestEqual(TEXT("소유자 없는 히든 setter 거절"), OrphanQuota->SetHiddenReveals(PublicState.HiddenReveals), EDigetherEconomyResult::NotAuthority);
	TestEqual(TEXT("소유자 없는 공개 통지 거절"), OrphanQuota->NotifyStateChanged(), EDigetherEconomyResult::NotAuthority);
	TestTrue(TEXT("거절된 지갑은 불변"), AreWalletStatesEqual(EmptyWallet, OrphanWallet->CaptureState()));
	TestTrue(TEXT("거절된 공개 상태는 불변"), ArePublicStatesEqual(EmptyPublic, OrphanQuota->CaptureState()));

	FDigetherEconomyTestWorld Fixture;
	if (!TestNotNull(TEXT("임시 게임 월드"), Fixture.World))
	{
		return false;
	}
	AActor* Owner = Fixture.World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("권위 소유 액터"), Owner))
	{
		return false;
	}
	TestTrue(TEXT("임시 소유 액터는 서버 권위"), Owner->HasAuthority());
	UDigetherWalletComponent* Wallet = NewObject<UDigetherWalletComponent>(Owner);
	UDigetherWalletComponent* RestoredWallet = NewObject<UDigetherWalletComponent>(Owner);
	UDigetherQuotaStateComponent* Quota = NewObject<UDigetherQuotaStateComponent>(Owner);
	UDigetherQuotaStateComponent* RestoredQuota = NewObject<UDigetherQuotaStateComponent>(Owner);

	int32 WalletNotificationCount = 0;
	int32 QuotaNotificationCount = 0;
	FDigetherWalletState NotifiedWallet;
	FDigetherQuotaPublicState NotifiedPublic;
	const FDelegateHandle WalletHandle = Wallet->OnWalletChanged.AddLambda(
		[&WalletNotificationCount, &NotifiedWallet](const FDigetherWalletState& State)
		{
			++WalletNotificationCount;
			NotifiedWallet = State;
		});
	const FDelegateHandle QuotaHandle = Quota->OnQuotaStateChanged.AddLambda(
		[&QuotaNotificationCount, &NotifiedPublic](const FDigetherQuotaPublicState& State)
		{
			++QuotaNotificationCount;
			NotifiedPublic = State;
		});

	TestEqual(TEXT("지갑 복원"), Wallet->RestoreState(WalletState, false), EDigetherEconomyResult::Success);
	TestEqual(TEXT("공개 상태 복원"), Quota->RestoreState(PublicState, false), EDigetherEconomyResult::Success);
	TestEqual(TEXT("지갑 통지 지연"), WalletNotificationCount, 0);
	TestEqual(TEXT("공개 통지 지연"), QuotaNotificationCount, 0);
	TestEqual(TEXT("새 지갑으로 인계"), RestoredWallet->RestoreState(Wallet->CaptureState()), EDigetherEconomyResult::Success);
	TestEqual(TEXT("새 공개 상태로 인계"), RestoredQuota->RestoreState(Quota->CaptureState()), EDigetherEconomyResult::Success);
	TestTrue(TEXT("지갑 전체 필드 왕복 일치"), AreWalletStatesEqual(WalletState, RestoredWallet->CaptureState()));
	TestTrue(TEXT("공개 전체 필드와 목록 왕복 일치"), ArePublicStatesEqual(PublicState, RestoredQuota->CaptureState()));
	TestEqual(TEXT("지갑 확정 통지"), Wallet->NotifyStateChanged(), EDigetherEconomyResult::Success);
	TestEqual(TEXT("공개 확정 통지"), Quota->NotifyStateChanged(), EDigetherEconomyResult::Success);
	TestEqual(TEXT("지갑 통지 횟수"), WalletNotificationCount, 1);
	TestEqual(TEXT("공개 통지 횟수"), QuotaNotificationCount, 1);
	TestTrue(TEXT("통지된 지갑 값 일치"), AreWalletStatesEqual(WalletState, NotifiedWallet));
	TestTrue(TEXT("통지된 공개 값 일치"), ArePublicStatesEqual(PublicState, NotifiedPublic));
	// 픽스처 120/500 = 24%는 빨강 구간(PAY_SIGNAL_THRESHOLDS 시험 기준 0.5·0.8).
	TestEqual(TEXT("서버 복원 값의 신호기 단계"), Quota->GetPaymentSignal(), EDigetherPaymentSignal::Red);
	TestEqual(TEXT("서버 원본은 누적 인정액 유지"), Quota->CaptureState().Quota.DepositedCredits, 120);

	// 콜백의 표현 통지만 검사한다. 실제 네트워크 전달 검증은 별도 M2 범위다.
	// 서버(권위)에서 복제 알림을 불러도 누적 인정액을 가진 원본을 덮어쓰지 않는다.
	UFunction* WalletOnRep = Wallet->FindFunction(TEXT("OnRep_PrivateState"));
	UFunction* QuotaOnRep = Quota->FindFunction(TEXT("OnRep_ReplicatedState"));
	if (TestNotNull(TEXT("지갑 복제 알림 함수 등록"), WalletOnRep))
	{
		Wallet->ProcessEvent(WalletOnRep, nullptr);
	}
	if (TestNotNull(TEXT("공개 복제 알림 함수 등록"), QuotaOnRep))
	{
		Quota->ProcessEvent(QuotaOnRep, nullptr);
	}
	TestEqual(TEXT("지갑 복제 알림은 1회 표현 통지"), WalletNotificationCount, 2);
	TestEqual(TEXT("공개 복제 알림은 1회 표현 통지"), QuotaNotificationCount, 2);
	TestTrue(TEXT("지갑 복제 알림은 현재 값 전달"), AreWalletStatesEqual(WalletState, NotifiedWallet));
	TestTrue(TEXT("공개 복제 알림은 현재 값 전달"), ArePublicStatesEqual(PublicState, NotifiedPublic));

	FDigetherWalletState InvalidWallet = WalletState;
	InvalidWallet.BalanceCredits = -1;
	FDigetherQuotaPublicState InvalidPublic = PublicState;
	InvalidPublic.HiddenReveals.Add(Reveal);
	TestEqual(TEXT("잘못된 지갑 복원 거절"), Wallet->RestoreState(InvalidWallet), EDigetherEconomyResult::InvalidInput);
	TestEqual(TEXT("중복 식별자 공개 거절"), Quota->RestoreState(InvalidPublic), EDigetherEconomyResult::InvalidInput);
	TestTrue(TEXT("형태 오류 지갑 불변"), AreWalletStatesEqual(WalletState, Wallet->CaptureState()));
	TestTrue(TEXT("형태 오류 공개 상태 불변"), ArePublicStatesEqual(PublicState, Quota->CaptureState()));
	TestEqual(TEXT("형태 오류 지갑 통지 없음"), WalletNotificationCount, 2);
	TestEqual(TEXT("형태 오류 공개 통지 없음"), QuotaNotificationCount, 2);

	const FDigetherWalletUpdateResult Death = FDigetherEconomyRules::ApplyDeathLoss(WalletState, 30);
	TestEqual(TEXT("순수 함수 확정 결과 적용"), Wallet->ApplyUpdate(Death), EDigetherEconomyResult::Success);
	TestTrue(TEXT("접속·보관 값에 같은 결과"), AreWalletStatesEqual(Death.State, Wallet->CaptureState()));
	TestEqual(TEXT("확정 결과 1회 통지"), WalletNotificationCount, 3);
	FDigetherWalletUpdateResult Rejected;
	Rejected.Result = EDigetherEconomyResult::InsufficientFunds;
	Rejected.State = WalletState;
	TestEqual(TEXT("실패 결과 적용 거절"), Wallet->ApplyUpdate(Rejected), EDigetherEconomyResult::InsufficientFunds);
	TestTrue(TEXT("실패 결과 지갑 불변"), AreWalletStatesEqual(Death.State, Wallet->CaptureState()));
	TestEqual(TEXT("실패 결과 통지 없음"), WalletNotificationCount, 3);

	FDigetherQuotaState NextQuota = PublicState.Quota;
	NextQuota.Day = 4;
	TestEqual(TEXT("확정 목표 setter"), Quota->SetQuotaState(NextQuota), EDigetherEconomyResult::Success);
	PublicState.Quota = NextQuota;
	TestTrue(TEXT("목표 setter는 히든 목록 보존"), ArePublicStatesEqual(PublicState, Quota->CaptureState()));
	TestEqual(TEXT("공개 목록 비우기"), Quota->SetHiddenReveals(TArray<FDigetherHiddenRevealEntry>()), EDigetherEconomyResult::Success);
	PublicState.HiddenReveals.Reset();
	TestTrue(TEXT("목록 setter는 목표 상태 보존"), ArePublicStatesEqual(PublicState, Quota->CaptureState()));
	TestEqual(TEXT("공개 setter별 확정 통지"), QuotaNotificationCount, 4);

	Wallet->OnWalletChanged.Remove(WalletHandle);
	Quota->OnQuotaStateChanged.Remove(QuotaHandle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherEconomyPaymentSignalTest,
	"DIGETHER.Game.Economy.PaymentSignal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherEconomyPaymentSignalTest::RunTest(const FString& Parameters)
{
	const FDigetherEconomyTuning Tuning;
	TestTrue(TEXT("기본 경제 튜닝 유효"), FDigetherEconomyRules::IsValidTuning(Tuning));
	TestEqual(TEXT("PAY_SIGNAL_THRESHOLDS 주황 시험 기준"), Tuning.PaymentSignalOrangeRatio, 0.5);
	TestEqual(TEXT("PAY_SIGNAL_THRESHOLDS 초록 시험 기준"), Tuning.PaymentSignalGreenRatio, 0.8);

	// 목표 1000은 테스트 픽스처이며 최종 기획 목표액이 아니다.
	FDigetherQuotaState Quota;
	Quota.Day = 2;
	Quota.TargetCredits = 1000;
	const auto SignalAt = [&Quota, &Tuning](const int32 Deposited)
	{
		FDigetherQuotaState Candidate = Quota;
		Candidate.DepositedCredits = Deposited;
		return FDigetherEconomyRules::CalculatePaymentSignal(Candidate, Tuning);
	};
	TestEqual(TEXT("납부 0은 빨강"), SignalAt(0), EDigetherPaymentSignal::Red);
	TestEqual(TEXT("50% 미만은 빨강"), SignalAt(499), EDigetherPaymentSignal::Red);
	TestEqual(TEXT("50% 이상은 주황"), SignalAt(500), EDigetherPaymentSignal::Orange);
	TestEqual(TEXT("80% 미만은 주황"), SignalAt(799), EDigetherPaymentSignal::Orange);
	TestEqual(TEXT("80% 이상은 초록"), SignalAt(800), EDigetherPaymentSignal::Green);
	TestEqual(TEXT("목표 직전도 초록"), SignalAt(999), EDigetherPaymentSignal::Green);
	FDigetherQuotaState NoTarget;
	TestEqual(TEXT("초기화 전 목표는 신호기 없음"), FDigetherEconomyRules::CalculatePaymentSignal(NoTarget, Tuning),
		EDigetherPaymentSignal::None);
	FDigetherEconomyTuning Inverted = Tuning;
	Inverted.PaymentSignalOrangeRatio = 0.9;
	TestFalse(TEXT("주황 경계가 초록 이상이면 튜닝 거절"), FDigetherEconomyRules::IsValidTuning(Inverted));
	TestEqual(TEXT("잘못된 경계는 신호기 없음"), FDigetherEconomyRules::CalculatePaymentSignal(Quota, Inverted),
		EDigetherPaymentSignal::None);

	// 복제 값은 누적 인정액 없이 신호기만 담고, 클라이언트 조회 값은 누적 인정액 0이다.
	FDigetherQuotaPublicState Server;
	Server.Quota = Quota;
	Server.Quota.DepositedCredits = 650;
	Server.Quota.PeriodGeneration = 3;
	Server.Quota.TotalDay = 6;
	FDigetherHiddenRevealEntry Reveal;
	Reveal.PlayerKey = TEXT("test-player");
	Reveal.HiddenLifetimeCredits = 9;
	Server.HiddenReveals.Add(Reveal);
	const FDigetherQuotaReplicatedState Replicated = FDigetherEconomyRules::MakeReplicatedQuotaState(Server, Tuning);
	TestEqual(TEXT("복제 일차"), Replicated.Day, 2);
	TestEqual(TEXT("복제 목표"), Replicated.TargetCredits, 1000);
	TestEqual(TEXT("복제 기간 세대"), Replicated.PeriodGeneration, 3);
	TestEqual(TEXT("복제 전체 일차(공개 값)"), Replicated.TotalDay, 6);
	TestEqual(TEXT("복제 신호기 65%는 주황"), Replicated.PaymentSignal, EDigetherPaymentSignal::Orange);
	TestEqual(TEXT("복제 공개 목록"), Replicated.HiddenReveals.Num(), 1);
	const FDigetherQuotaPublicState Client = FDigetherEconomyRules::MakeClientQuotaPublicState(Replicated);
	TestEqual(TEXT("클라이언트 조회 값은 누적 인정액 0"), Client.Quota.DepositedCredits, 0);
	TestEqual(TEXT("클라이언트 조회 값 목표 유지"), Client.Quota.TargetCredits, 1000);
	TestEqual(TEXT("클라이언트 조회 값 기간 세대 유지"), Client.Quota.PeriodGeneration, 3);
	TestEqual(TEXT("클라이언트 조회 값 전체 일차 유지"), Client.Quota.TotalDay, 6);
	TestEqual(TEXT("클라이언트 조회 값 기간 일차 유지"), Client.Quota.Day, 2);
	TestTrue(TEXT("클라이언트 조회 값은 형태 검사 통과"), FDigetherEconomyRules::IsValidPublicState(Client));
	return true;
}

#endif
