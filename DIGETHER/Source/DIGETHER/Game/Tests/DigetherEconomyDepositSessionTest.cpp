#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Economy/DigetherDepositSessionRegistry.h"
#include "Game/Economy/DigetherEconomyTuning.h"
#include "Misc/AutomationTest.h"

#include <limits>

namespace
{
	bool AreDepositSessionsEqual(const FDigetherDepositSession& Left, const FDigetherDepositSession& Right)
	{
		return Left.PlayerKey == Right.PlayerKey && Left.SessionId == Right.SessionId &&
			Left.StationId == Right.StationId && Left.PeriodGeneration == Right.PeriodGeneration &&
			Left.ExpiresAtServerSeconds == Right.ExpiresAtServerSeconds;
	}

	bool AreDepositSessionListsEqual(const TArray<FDigetherDepositSession>& Left,
		const TArray<FDigetherDepositSession>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (!AreDepositSessionsEqual(Left[Index], Right[Index]))
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherEconomyDepositSessionTest,
	"DIGETHER.Game.Economy.DepositSession", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherEconomyDepositSessionTest::RunTest(const FString& Parameters)
{
	const FDigetherEconomyTuning Tuning;
	const auto CheckWallet = [this](const FString& Name, const FDigetherWalletState& Actual,
		const FDigetherWalletState& Expected)
	{
		TestTrue(Name, Actual.BalanceCredits == Expected.BalanceCredits &&
			Actual.LastRunPayoutCredits == Expected.LastRunPayoutCredits &&
			Actual.HiddenLifetimeCredits == Expected.HiddenLifetimeCredits &&
			Actual.TotalDeathLossCredits == Expected.TotalDeathLossCredits &&
			Actual.LastPreDeathBalanceCredits == Expected.LastPreDeathBalanceCredits);
	};
	const auto CheckQuota = [this](const FString& Name, const FDigetherQuotaState& Actual,
		const FDigetherQuotaState& Expected)
	{
		TestTrue(Name, Actual.Day == Expected.Day && Actual.TargetCredits == Expected.TargetCredits &&
			Actual.DepositedCredits == Expected.DepositedCredits && Actual.PeriodGeneration == Expected.PeriodGeneration);
	};
	const auto CheckSessions = [this](const FString& Name, const FDigetherDepositSessionRegistry& Registry,
		const TArray<FDigetherDepositSession>& Expected)
	{
		TestTrue(Name, AreDepositSessionListsEqual(Registry.CaptureSessions(), Expected));
	};
	const auto Open = [this](const FString& Name, FDigetherDepositSessionRegistry& Registry,
		const FString& PlayerKey, const FName StationId, const int32 Generation,
		const double NowSeconds, const double LifetimeSeconds, const int32 ExpectedGeneration = INDEX_NONE)
	{
		const FDigetherDepositSessionOpenResult Result = Registry.OpenSession(
			PlayerKey, StationId, Generation, NowSeconds, LifetimeSeconds);
		TestTrue(Name + TEXT(": 열기 성공"), Result.Result == EDigetherEconomyResult::Success);
		TestTrue(Name + TEXT(": 반환 세션 전체 값"), Result.Session.PlayerKey == PlayerKey &&
			Result.Session.SessionId.IsValid() && Result.Session.StationId == StationId &&
			Result.Session.PeriodGeneration == (ExpectedGeneration == INDEX_NONE ? Generation : ExpectedGeneration) &&
			Result.Session.ExpiresAtServerSeconds == NowSeconds + LifetimeSeconds);
		return Result.Session;
	};
	const auto CheckOpenRejected = [this, &CheckSessions](const FString& Name,
		FDigetherDepositSessionRegistry& Registry, const FString& PlayerKey, const FName StationId,
		const int32 Generation, const double NowSeconds, const double LifetimeSeconds,
		const EDigetherEconomyResult ExpectedResult)
	{
		const TArray<FDigetherDepositSession> Before = Registry.CaptureSessions();
		const FDigetherDepositSessionOpenResult Result = Registry.OpenSession(
			PlayerKey, StationId, Generation, NowSeconds, LifetimeSeconds);
		TestTrue(Name + TEXT(": 열기 거절 사유"), Result.Result == ExpectedResult);
		TestTrue(Name + TEXT(": 부분 세션 반환 없음"), AreDepositSessionsEqual(Result.Session, FDigetherDepositSession()));
		CheckSessions(Name + TEXT(": 기존 세션 전체 보존"), Registry, Before);
	};
	const auto CheckRejected = [this, &CheckWallet, &CheckQuota, &CheckSessions, &Tuning](const FString& Name,
		FDigetherDepositSessionRegistry& Registry, const FDigetherDepositSubmission& Submission,
		FDigetherWalletState& Wallet, FDigetherQuotaState& Quota, const EDigetherEconomyResult ExpectedResult,
		const FDigetherEconomyTuning* TuningOverride = nullptr)
	{
		const FDigetherWalletState BeforeWallet = Wallet;
		const FDigetherQuotaState BeforeQuota = Quota;
		const TArray<FDigetherDepositSession> BeforeSessions = Registry.CaptureSessions();
		const FDigetherDepositResult Result = Registry.SubmitDeposit(Submission, Wallet, Quota,
			TuningOverride != nullptr ? *TuningOverride : Tuning);
		TestTrue(Name + TEXT(": 납부 거절 사유"), Result.Result == ExpectedResult);
		TestEqual(Name + TEXT(": 부분 납부 없음"), Result.AcceptedCredits, 0);
		TestEqual(Name + TEXT(": 목표 반영액 없음"), Result.CreditedCredits, 0);
		TestFalse(Name + TEXT(": 목표 완료 없음"), Result.bTargetCompleted);
		CheckWallet(Name + TEXT(": 반환 지갑 전체 보존"), Result.WalletState, BeforeWallet);
		CheckQuota(Name + TEXT(": 반환 목표 전체 보존"), Result.QuotaState, BeforeQuota);
		CheckWallet(Name + TEXT(": 참조 지갑 전체 보존"), Wallet, BeforeWallet);
		CheckQuota(Name + TEXT(": 참조 목표 전체 보존"), Quota, BeforeQuota);
		CheckSessions(Name + TEXT(": 모든 세션 필드 보존"), Registry, BeforeSessions);
	};
	const auto CheckAccepted = [this, &CheckWallet, &CheckQuota](const FString& Name,
		const FDigetherDepositResult& Result, const int32 AcceptedCredits, const int32 CreditedCredits, const bool bCompleted,
		const FDigetherWalletState& Wallet, const FDigetherQuotaState& Quota,
		const FDigetherWalletState& ExpectedWallet, const FDigetherQuotaState& ExpectedQuota)
	{
		TestTrue(Name + TEXT(": 납부 성공"), Result.Result == EDigetherEconomyResult::Success);
		TestEqual(Name + TEXT(": 실제 차감액"), Result.AcceptedCredits, AcceptedCredits);
		TestEqual(Name + TEXT(": 목표 반영액"), Result.CreditedCredits, CreditedCredits);
		TestEqual(Name + TEXT(": 목표 완료 여부"), Result.bTargetCompleted, bCompleted);
		CheckWallet(Name + TEXT(": 반환 지갑 전체 값"), Result.WalletState, ExpectedWallet);
		CheckQuota(Name + TEXT(": 반환 목표 전체 값"), Result.QuotaState, ExpectedQuota);
		CheckWallet(Name + TEXT(": 참조 지갑 전체 값"), Wallet, ExpectedWallet);
		CheckQuota(Name + TEXT(": 참조 목표 전체 값"), Quota, ExpectedQuota);
	};
	const auto CheckClosed = [this, &CheckSessions](const FString& Name,
		const FDigetherDepositSessionCloseResult& Result, const EDigetherEconomyResult ExpectedResult,
		const EDigetherDepositSessionCloseReason Reason, const TArray<FDigetherDepositSession>& ExpectedClosed,
		const FDigetherDepositSessionRegistry& Registry, const TArray<FDigetherDepositSession>& ExpectedRemaining)
	{
		TestTrue(Name + TEXT(": 닫기 결과"), Result.Result == ExpectedResult);
		TestTrue(Name + TEXT(": 닫기 사유"), Result.Reason == Reason);
		TestTrue(Name + TEXT(": 닫힌 세션 전체 값"), AreDepositSessionListsEqual(Result.ClosedSessions, ExpectedClosed));
		CheckSessions(Name + TEXT(": 남은 세션 전체 값"), Registry, ExpectedRemaining);
	};

	// 금액·세대·시각·수명 경계 입력은 테스트 픽스처이며 기본 효율과 수명은 Tuning에서 읽는다.
	const FString PlayerA = TEXT("deposit-session-player-a");
	const FString PlayerB = TEXT("deposit-session-player-b");
	const FName StationA(TEXT("DepositSessionStationA"));
	const FName StationB(TEXT("DepositSessionStationB"));
	const FName StationC(TEXT("DepositSessionStationC"));
	constexpr int32 Generation = 7;
	constexpr double StartSeconds = 100.0;
	const double LifetimeSeconds = Tuning.DepositSessionLifetimeSeconds;
	const double NotANumber = std::numeric_limits<double>::quiet_NaN();
	const double Infinity = std::numeric_limits<double>::infinity();
	const double MaximumTime = std::numeric_limits<double>::max();
	FDigetherWalletState BaseWallet;
	BaseWallet.BalanceCredits = 200;
	BaseWallet.LastRunPayoutCredits = 13;
	BaseWallet.HiddenLifetimeCredits = 17;
	BaseWallet.TotalDeathLossCredits = 40;
	BaseWallet.LastPreDeathBalanceCredits = 42;
	FDigetherQuotaState BaseQuota;
	BaseQuota.Day = 4;
	BaseQuota.TargetCredits = 100;
	BaseQuota.DepositedCredits = 20;
	BaseQuota.PeriodGeneration = Generation;
	const auto MakeSubmission = [](const FDigetherDepositSession& Session)
	{
		FDigetherDepositSubmission Submission;
		Submission.PlayerKey = Session.PlayerKey;
		Submission.SessionId = Session.SessionId;
		Submission.StationId = Session.StationId;
		Submission.PeriodGeneration = Session.PeriodGeneration;
		Submission.AmountCredits = 10;
		Submission.NowServerSeconds = 101.0;
		return Submission;
	};

	FDigetherDepositSessionRegistry OpenRegistry;
	const FDigetherDepositSession FirstA = Open(TEXT("첫 플레이어"), OpenRegistry,
		PlayerA, StationA, Generation, StartSeconds, LifetimeSeconds);
	const FDigetherDepositSession FirstB = Open(TEXT("서로 다른 납부대"), OpenRegistry,
		PlayerB, StationB, Generation, StartSeconds, LifetimeSeconds);
	const FDigetherDepositSession RefreshedA = Open(TEXT("동일 납부대 재열기"), OpenRegistry,
		PlayerA, StationA, Generation, 102.0, 20.0);
	TestTrue(TEXT("재열기는 기존 ID 유지"), RefreshedA.SessionId == FirstA.SessionId);
	const FDigetherDepositSession MovedA = Open(TEXT("비어 있는 납부대로 이동"), OpenRegistry,
		PlayerA, StationC, Generation, 103.0, LifetimeSeconds);
	TestTrue(TEXT("납부대를 옮기면 새 ID 발급"), MovedA.SessionId != RefreshedA.SessionId);
	CheckSessions(TEXT("납부대 교체는 본인 세션만 교체"), OpenRegistry, { MovedA, FirstB });
	CheckClosed(TEXT("이전 화면의 늦은 취소는 새 세션을 닫지 않음"),
		OpenRegistry.CloseSession(PlayerA, FirstA.SessionId, EDigetherDepositSessionCloseReason::ExplicitCancel),
		EDigetherEconomyResult::SessionNotFound, EDigetherDepositSessionCloseReason::ExplicitCancel,
		{}, OpenRegistry, { MovedA, FirstB });
	CheckOpenRejected(TEXT("다른 플레이어가 이용 중인 납부대로 이동"), OpenRegistry,
		PlayerA, StationB, Generation, 104.0, LifetimeSeconds, EDigetherEconomyResult::StationOccupied);
	CheckOpenRejected(TEXT("다른 플레이어의 점유 납부대 열기"), OpenRegistry,
		TEXT("deposit-session-player-c"), StationB, Generation, 104.0, LifetimeSeconds,
		EDigetherEconomyResult::StationOccupied);
	TArray<FDigetherDepositSession> DetachedCopy = OpenRegistry.CaptureSessions();
	if (!DetachedCopy.IsEmpty())
	{
		DetachedCopy[0].PlayerKey = TEXT("changed-copy");
		DetachedCopy[0].SessionId.Invalidate();
		DetachedCopy[0].StationId = NAME_None;
		DetachedCopy[0].PeriodGeneration = 99;
		DetachedCopy[0].ExpiresAtServerSeconds = 0.0;
	}
	DetachedCopy.Reset();
	CheckSessions(TEXT("조회 복사본 변경·삭제는 레지스트리와 독립"), OpenRegistry, { MovedA, FirstB });

	struct FDigetherInvalidOpenCase
	{
		const TCHAR* Name;
		FString PlayerKey;
		FName StationId;
		int32 PeriodGeneration;
		double NowSeconds;
		double LifetimeSeconds;
	};
	const FDigetherInvalidOpenCase InvalidOpenCases[] =
	{
		{ TEXT("빈 플레이어"), TEXT(""), StationA, Generation, 104.0, LifetimeSeconds },
		{ TEXT("납부대 없음"), PlayerA, NAME_None, Generation, 104.0, LifetimeSeconds },
		{ TEXT("음수 세대"), PlayerA, StationA, -1, 104.0, LifetimeSeconds },
		{ TEXT("음수 현재 시각"), PlayerA, StationA, Generation, -1.0, LifetimeSeconds },
		{ TEXT("NaN 현재 시각"), PlayerA, StationA, Generation, NotANumber, LifetimeSeconds },
		{ TEXT("무한 현재 시각"), PlayerA, StationA, Generation, Infinity, LifetimeSeconds },
		{ TEXT("수명 0"), PlayerA, StationA, Generation, 104.0, 0.0 },
		{ TEXT("음수 수명"), PlayerA, StationA, Generation, 104.0, -1.0 },
		{ TEXT("NaN 수명"), PlayerA, StationA, Generation, 104.0, NotANumber },
		{ TEXT("무한 수명"), PlayerA, StationA, Generation, 104.0, Infinity },
		{ TEXT("만료 시각 합산 오버플로"), PlayerA, StationA, Generation, MaximumTime, MaximumTime },
		{ TEXT("수명이 시각 정밀도보다 작음"), PlayerA, StationA, Generation, MaximumTime, 1.0 }
	};
	for (const FDigetherInvalidOpenCase& Case : InvalidOpenCases)
	{
		CheckOpenRejected(Case.Name, OpenRegistry, Case.PlayerKey, Case.StationId,
			Case.PeriodGeneration, Case.NowSeconds, Case.LifetimeSeconds, EDigetherEconomyResult::InvalidInput);
	}
	// 프로토타입과 같은 재열기 계약이다. 세대 전환·만료 제거는 호출자가 별도로 수행한다.
	const FDigetherDepositSession RevivedA = Open(TEXT("제거 전 만료 세션을 다른 요청 세대로 재열기"),
		OpenRegistry, PlayerA, StationC, Generation + 1, 113.0, LifetimeSeconds, Generation);
	TestTrue(TEXT("만료 후 재열기도 ID와 기억한 세대 유지"),
		RevivedA.SessionId == MovedA.SessionId && RevivedA.PeriodGeneration == MovedA.PeriodGeneration);
	CheckSessions(TEXT("재열기는 만료만 갱신하고 다른 세션 보존"), OpenRegistry, { RevivedA, FirstB });
	FDigetherWalletState OpenWallet = BaseWallet;
	FDigetherQuotaState OpenQuota = BaseQuota;
	CheckRejected(TEXT("납부대 교체 전 실제 발급 ID 거절"), OpenRegistry, MakeSubmission(FirstA),
		OpenWallet, OpenQuota, EDigetherEconomyResult::SessionNotFound);

	FDigetherDepositSessionRegistry SubmitRegistry;
	const FDigetherDepositSession SubmitA = Open(TEXT("납부 검사 A"), SubmitRegistry,
		PlayerA, StationA, Generation, StartSeconds, LifetimeSeconds);
	const FDigetherDepositSession SubmitB = Open(TEXT("납부 검사 B"), SubmitRegistry,
		PlayerB, StationB, Generation, StartSeconds, LifetimeSeconds);
	FDigetherWalletState Wallet = BaseWallet;
	FDigetherQuotaState Quota = BaseQuota;
	const FDigetherDepositSubmission ValidSubmission = MakeSubmission(SubmitA);
	FDigetherDepositSubmission InvalidSubmission = ValidSubmission;
	for (const int32 InvalidAmount : { 0, -1, BaseWallet.BalanceCredits + 1 })
	{
		InvalidSubmission.AmountCredits = InvalidAmount;
		CheckRejected(FString::Printf(TEXT("금액 %d 거절"), InvalidAmount), SubmitRegistry,
			InvalidSubmission, Wallet, Quota, InvalidAmount <= 0
				? EDigetherEconomyResult::InvalidAmount : EDigetherEconomyResult::InsufficientFunds);
	}
	FDigetherQuotaState NearTarget = Quota;
	NearTarget.DepositedCredits = 90;
	CheckRejected(TEXT("잔여 목표가 적어도 요청 전체 잔고 초과 거절"), SubmitRegistry,
		InvalidSubmission, Wallet, NearTarget, EDigetherEconomyResult::InsufficientFunds);
	InvalidSubmission = ValidSubmission;
	InvalidSubmission.SessionId = FGuid::NewGuid();
	CheckRejected(TEXT("오래된 세션 ID"), SubmitRegistry, InvalidSubmission, Wallet, Quota,
		EDigetherEconomyResult::SessionNotFound);
	InvalidSubmission.SessionId.Invalidate();
	CheckRejected(TEXT("빈 세션 ID"), SubmitRegistry, InvalidSubmission, Wallet, Quota,
		EDigetherEconomyResult::SessionNotFound);
	InvalidSubmission = ValidSubmission;
	InvalidSubmission.PlayerKey = PlayerB;
	CheckRejected(TEXT("다른 소유자의 세션 ID"), SubmitRegistry, InvalidSubmission, Wallet, Quota,
		EDigetherEconomyResult::SessionOwnerMismatch);
	InvalidSubmission = ValidSubmission;
	InvalidSubmission.StationId = StationB;
	CheckRejected(TEXT("다른 납부대"), SubmitRegistry, InvalidSubmission, Wallet, Quota,
		EDigetherEconomyResult::StationMismatch);
	InvalidSubmission = ValidSubmission;
	InvalidSubmission.PeriodGeneration = Generation + 1;
	CheckRejected(TEXT("클라이언트 세대와 세션 세대 불일치"), SubmitRegistry, InvalidSubmission,
		Wallet, Quota, EDigetherEconomyResult::PeriodMismatch);
	FDigetherQuotaState NextGeneration = Quota;
	NextGeneration.PeriodGeneration = Generation + 1;
	CheckRejected(TEXT("세션 세대와 현재 목표 세대 불일치"), SubmitRegistry, ValidSubmission,
		Wallet, NextGeneration, EDigetherEconomyResult::PeriodMismatch);
	InvalidSubmission = ValidSubmission;
	InvalidSubmission.bDepositClosed = true;
	CheckRejected(TEXT("납부 마감"), SubmitRegistry, InvalidSubmission, Wallet, Quota,
		EDigetherEconomyResult::DepositClosed);
	for (const int32 InvalidDay : { -1, 0, 5, MAX_int32 })
	{
		FDigetherQuotaState InvalidDayQuota = Quota;
		InvalidDayQuota.Day = InvalidDay;
		CheckRejected(FString::Printf(TEXT("납부 일차 범위 오류 %d"), InvalidDay), SubmitRegistry,
			ValidSubmission, Wallet, InvalidDayQuota, EDigetherEconomyResult::InvalidInput);
	}
	InvalidSubmission = ValidSubmission;
	for (const double InvalidTime : { -1.0, NotANumber, Infinity })
	{
		InvalidSubmission.NowServerSeconds = InvalidTime;
		CheckRejected(TEXT("납부 현재 시각 오류"), SubmitRegistry, InvalidSubmission, Wallet, Quota,
			EDigetherEconomyResult::InvalidInput);
	}
	FDigetherEconomyTuning InvalidTuning = Tuning;
	InvalidTuning.TargetIncreaseCredits = -1;
	CheckRejected(TEXT("음수 목표 증가량 튜닝 전달"), SubmitRegistry, ValidSubmission, Wallet, Quota,
		EDigetherEconomyResult::InvalidInput, &InvalidTuning);
	FDigetherWalletState InvalidWallet = Wallet;
	InvalidWallet.HiddenLifetimeCredits = -1;
	CheckRejected(TEXT("잘못된 지갑 전달"), SubmitRegistry, ValidSubmission, InvalidWallet, Quota,
		EDigetherEconomyResult::InvalidInput);
	FDigetherQuotaState FullQuota = Quota;
	FullQuota.DepositedCredits = FullQuota.TargetCredits;
	CheckRejected(TEXT("이미 달성한 목표 전달"), SubmitRegistry, ValidSubmission, Wallet, FullQuota,
		EDigetherEconomyResult::QuotaAlreadySatisfied);

	FDigetherDepositSubmission JustBeforeExpiry = ValidSubmission;
	JustBeforeExpiry.NowServerSeconds = 109.999;
	FDigetherWalletState ExpectedWallet = Wallet;
	ExpectedWallet.BalanceCredits = 190;
	FDigetherQuotaState ExpectedQuota = Quota;
	ExpectedQuota.DepositedCredits = 30;
	const FDigetherDepositResult Partial = SubmitRegistry.SubmitDeposit(JustBeforeExpiry, Wallet, Quota, Tuning);
	CheckAccepted(TEXT("만료 직전 4일차 1:1 미완료 납부"), Partial, 10, 10, false,
		Wallet, Quota, ExpectedWallet, ExpectedQuota);
	CheckSessions(TEXT("성공은 제출한 세션만 소비"), SubmitRegistry, { SubmitB });
	CheckRejected(TEXT("성공한 세션 재사용"), SubmitRegistry, JustBeforeExpiry, Wallet, Quota,
		EDigetherEconomyResult::SessionNotFound);
	const FDigetherDepositSession PendingA = Open(TEXT("목표 달성 전 다른 플레이어 세션"), SubmitRegistry,
		PlayerA, StationC, Generation, 109.999, LifetimeSeconds);
	FDigetherWalletState WalletB = BaseWallet;
	Quota.DepositedCredits = 90;
	FDigetherDepositSubmission CompleteSubmission = MakeSubmission(SubmitB);
	CompleteSubmission.AmountCredits = 40;
	CompleteSubmission.NowServerSeconds = 109.999;
	ExpectedWallet = WalletB;
	ExpectedWallet.BalanceCredits = 190;
	ExpectedQuota = Quota;
	ExpectedQuota.Day = 1;
	ExpectedQuota.TargetCredits = 200;
	ExpectedQuota.DepositedCredits = 0;
	ExpectedQuota.PeriodGeneration = Generation + 1;
	const FDigetherDepositResult Complete = SubmitRegistry.SubmitDeposit(CompleteSubmission, WalletB, Quota, Tuning);
	CheckAccepted(TEXT("4일차 1:1 목표 잔여액만 차감하고 기간 전환"), Complete, 10, 10, true,
		WalletB, Quota, ExpectedWallet, ExpectedQuota);
	CheckSessions(TEXT("기간 완료가 다른 플레이어 세션을 자동 소비하지 않음"), SubmitRegistry, { PendingA });
	FDigetherDepositSubmission LateSubmission = MakeSubmission(PendingA);
	LateSubmission.NowServerSeconds = 110.0;
	CheckRejected(TEXT("다른 플레이어의 목표 달성 직후 늦은 요청"), SubmitRegistry,
		LateSubmission, Wallet, Quota, EDigetherEconomyResult::PeriodMismatch);
	CheckClosed(TEXT("호출자가 기간 전환 세션을 일괄 종료"),
		SubmitRegistry.CloseAllSessions(EDigetherDepositSessionCloseReason::PeriodChanged),
		EDigetherEconomyResult::Success, EDigetherDepositSessionCloseReason::PeriodChanged,
		{ PendingA }, SubmitRegistry, {});
	CheckRejected(TEXT("기간 전환 종료 뒤 늦은 요청"), SubmitRegistry, LateSubmission, Wallet, Quota,
		EDigetherEconomyResult::SessionNotFound);

	FDigetherDepositSessionRegistry DayOneRegistry;
	const FDigetherDepositSession DayOneSession = Open(TEXT("1일차 납부 세션"), DayOneRegistry,
		PlayerA, StationA, Generation, StartSeconds, LifetimeSeconds);
	FDigetherWalletState DayOneWallet = BaseWallet;
	FDigetherQuotaState DayOneQuota = BaseQuota;
	DayOneQuota.Day = 1;
	DayOneQuota.DepositedCredits = 50;
	FDigetherDepositSubmission DayOneSubmission = MakeSubmission(DayOneSession);
	DayOneSubmission.AmountCredits = 1;
	CheckRejected(TEXT("현재 목표 1일차 효율로 반영액 0인 요청은 재시도 상태 보존"), DayOneRegistry,
		DayOneSubmission, DayOneWallet, DayOneQuota, EDigetherEconomyResult::DepositCreditTooSmall);
	DayOneSubmission.AmountCredits = 100;
	ExpectedWallet = DayOneWallet;
	ExpectedWallet.BalanceCredits = 128;
	ExpectedQuota = DayOneQuota;
	ExpectedQuota.Day = 1;
	ExpectedQuota.TargetCredits = 200;
	ExpectedQuota.DepositedCredits = 0;
	ExpectedQuota.PeriodGeneration = Generation + 1;
	const FDigetherDepositResult DayOneRetry = DayOneRegistry.SubmitDeposit(
		DayOneSubmission, DayOneWallet, DayOneQuota, Tuning);
	CheckAccepted(TEXT("같은 세션 재시도는 목표의 1일차 70%로 72 차감·50 반영"), DayOneRetry, 72, 50, true,
		DayOneWallet, DayOneQuota, ExpectedWallet, ExpectedQuota);
	CheckSessions(TEXT("1일차 성공 후 재시도 세션 소비"), DayOneRegistry, {});

	FDigetherDepositSessionRegistry ExpiryRegistry;
	const FDigetherDepositSession ExpiringA = Open(TEXT("만료 경계 A"), ExpiryRegistry,
		PlayerA, StationA, Generation, StartSeconds, LifetimeSeconds);
	const FDigetherDepositSession LongerB = Open(TEXT("만료 경계 B"), ExpiryRegistry,
		PlayerB, StationB, Generation, StartSeconds, 20.0);
	Wallet = BaseWallet;
	Quota = BaseQuota;
	FDigetherDepositSubmission ExpiredSubmission = MakeSubmission(ExpiringA);
	ExpiredSubmission.NowServerSeconds = ExpiringA.ExpiresAtServerSeconds;
	CheckRejected(TEXT("정확한 만료 시각 제출"), ExpiryRegistry, ExpiredSubmission, Wallet, Quota,
		EDigetherEconomyResult::SessionExpired);
	for (const double InvalidTime : { -1.0, NotANumber, Infinity })
	{
		const TArray<FDigetherDepositSession> Before = ExpiryRegistry.CaptureSessions();
		CheckClosed(TEXT("잘못된 만료 검사 시각은 전체 보존"), ExpiryRegistry.ExpireSessions(InvalidTime),
			EDigetherEconomyResult::InvalidInput, EDigetherDepositSessionCloseReason::Expired,
			{}, ExpiryRegistry, Before);
	}
	CheckClosed(TEXT("만료 직전에는 아무 세션도 제거하지 않음"), ExpiryRegistry.ExpireSessions(109.999),
		EDigetherEconomyResult::Success, EDigetherDepositSessionCloseReason::Expired,
		{}, ExpiryRegistry, { ExpiringA, LongerB });
	CheckClosed(TEXT("정확한 만료 시각에 만료 세션만 제거"), ExpiryRegistry.ExpireSessions(110.0),
		EDigetherEconomyResult::Success, EDigetherDepositSessionCloseReason::Expired,
		{ ExpiringA }, ExpiryRegistry, { LongerB });
	CheckRejected(TEXT("만료 정리 뒤 제출"), ExpiryRegistry, ExpiredSubmission, Wallet, Quota,
		EDigetherEconomyResult::SessionNotFound);
	const FDigetherDepositSession NewA = Open(TEXT("만료 정리 뒤 같은 납부대 재열기"), ExpiryRegistry,
		PlayerA, StationA, Generation, 110.0, LifetimeSeconds);
	TestTrue(TEXT("실제 제거 뒤 재열기는 새 ID"), NewA.SessionId != ExpiringA.SessionId);
	CheckSessions(TEXT("새 세션과 만료되지 않은 다른 세션 전체 유지"), ExpiryRegistry, { LongerB, NewA });

	const EDigetherDepositSessionCloseReason CloseReasons[] =
	{
		EDigetherDepositSessionCloseReason::ExplicitCancel,
		EDigetherDepositSessionCloseReason::ScreenClosed,
		EDigetherDepositSessionCloseReason::InteractionLost,
		EDigetherDepositSessionCloseReason::Death,
		EDigetherDepositSessionCloseReason::Departure,
		EDigetherDepositSessionCloseReason::Disconnected,
		EDigetherDepositSessionCloseReason::Loaded,
		EDigetherDepositSessionCloseReason::Expired,
		EDigetherDepositSessionCloseReason::PeriodChanged
	};
	for (const EDigetherDepositSessionCloseReason Reason : CloseReasons)
	{
		const FString Name = FString::Printf(TEXT("닫힘 사유 %d"), static_cast<int32>(Reason));
		FDigetherDepositSessionRegistry CloseRegistry;
		const FDigetherDepositSession ClosingA = Open(Name, CloseRegistry,
			PlayerA, StationA, Generation, StartSeconds, LifetimeSeconds);
		CheckClosed(Name, CloseRegistry.CloseSession(PlayerA, ClosingA.SessionId, Reason),
			EDigetherEconomyResult::Success, Reason, { ClosingA }, CloseRegistry, {});
		Open(Name + TEXT(": 다른 플레이어의 해제 납부대 점유"), CloseRegistry,
			PlayerB, StationA, Generation, 101.0, LifetimeSeconds);
		Wallet = BaseWallet;
		Quota = BaseQuota;
		CheckRejected(Name + TEXT(": 종료한 세션으로 제출"), CloseRegistry, MakeSubmission(ClosingA),
			Wallet, Quota, EDigetherEconomyResult::SessionNotFound);
	}

	FDigetherDepositSessionRegistry CloseRegistry;
	const FDigetherDepositSession ClosingA = Open(TEXT("닫기 오류 검사 A"), CloseRegistry,
		PlayerA, StationA, Generation, StartSeconds, LifetimeSeconds);
	const FDigetherDepositSession ClosingB = Open(TEXT("닫기 오류 검사 B"), CloseRegistry,
		PlayerB, StationB, Generation, StartSeconds, LifetimeSeconds);
	const TArray<FDigetherDepositSession> BeforeClose = { ClosingA, ClosingB };
	const EDigetherDepositSessionCloseReason InvalidReason = static_cast<EDigetherDepositSessionCloseReason>(255);
	CheckClosed(TEXT("소유자 불일치 닫기"), CloseRegistry.CloseSession(
		PlayerB, ClosingA.SessionId, EDigetherDepositSessionCloseReason::ScreenClosed),
		EDigetherEconomyResult::SessionOwnerMismatch, EDigetherDepositSessionCloseReason::ScreenClosed,
		{}, CloseRegistry, BeforeClose);
	CheckClosed(TEXT("오래된 ID 닫기"), CloseRegistry.CloseSession(
		PlayerA, FGuid::NewGuid(), EDigetherDepositSessionCloseReason::ExplicitCancel),
		EDigetherEconomyResult::SessionNotFound, EDigetherDepositSessionCloseReason::ExplicitCancel,
		{}, CloseRegistry, BeforeClose);
	CheckClosed(TEXT("잘못된 사유로 ID 닫기"), CloseRegistry.CloseSession(PlayerA, ClosingA.SessionId, InvalidReason),
		EDigetherEconomyResult::InvalidInput, InvalidReason, {}, CloseRegistry, BeforeClose);
	CheckClosed(TEXT("없는 플레이어 닫기"), CloseRegistry.ClosePlayerSession(
		TEXT("missing-player"), EDigetherDepositSessionCloseReason::Disconnected),
		EDigetherEconomyResult::SessionNotFound, EDigetherDepositSessionCloseReason::Disconnected,
		{}, CloseRegistry, BeforeClose);
	CheckClosed(TEXT("잘못된 사유로 플레이어 닫기"), CloseRegistry.ClosePlayerSession(PlayerA, InvalidReason),
		EDigetherEconomyResult::InvalidInput, InvalidReason, {}, CloseRegistry, BeforeClose);
	CheckClosed(TEXT("잘못된 사유로 전체 닫기"), CloseRegistry.CloseAllSessions(InvalidReason),
		EDigetherEconomyResult::InvalidInput, InvalidReason, {}, CloseRegistry, BeforeClose);
	CheckClosed(TEXT("플레이어 닫기는 다른 플레이어 세션 보존"),
		CloseRegistry.ClosePlayerSession(PlayerA, EDigetherDepositSessionCloseReason::Death),
		EDigetherEconomyResult::Success, EDigetherDepositSessionCloseReason::Death,
		{ ClosingA }, CloseRegistry, { ClosingB });
	Wallet = BaseWallet;
	Quota = BaseQuota;
	CheckRejected(TEXT("플레이어 단위 닫기 뒤 제출"), CloseRegistry, MakeSubmission(ClosingA),
		Wallet, Quota, EDigetherEconomyResult::SessionNotFound);
	CheckClosed(TEXT("불러오기 시 남은 세션 전체 닫기"),
		CloseRegistry.CloseAllSessions(EDigetherDepositSessionCloseReason::Loaded),
		EDigetherEconomyResult::Success, EDigetherDepositSessionCloseReason::Loaded,
		{ ClosingB }, CloseRegistry, {});
	CheckRejected(TEXT("전체 닫기 뒤 제출"), CloseRegistry, MakeSubmission(ClosingB),
		Wallet, Quota, EDigetherEconomyResult::SessionNotFound);
	CheckClosed(TEXT("빈 레지스트리 전체 닫기는 성공"),
		CloseRegistry.CloseAllSessions(EDigetherDepositSessionCloseReason::Loaded),
		EDigetherEconomyResult::Success, EDigetherDepositSessionCloseReason::Loaded,
		{}, CloseRegistry, {});
	const FDigetherDepositSession AllA = Open(TEXT("일괄 닫기 A"), CloseRegistry,
		PlayerA, StationA, Generation, StartSeconds, LifetimeSeconds);
	const FDigetherDepositSession AllB = Open(TEXT("일괄 닫기 B"), CloseRegistry,
		PlayerB, StationB, Generation, StartSeconds, LifetimeSeconds);
	CheckClosed(TEXT("전체 닫기는 두 세션을 모두 반환하고 제거"),
		CloseRegistry.CloseAllSessions(EDigetherDepositSessionCloseReason::Departure),
		EDigetherEconomyResult::Success, EDigetherDepositSessionCloseReason::Departure,
		{ AllA, AllB }, CloseRegistry, {});
	return true;
}

#endif
