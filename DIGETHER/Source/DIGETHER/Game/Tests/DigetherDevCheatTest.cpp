#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

// 개발 치트 진입점(GameMode·런 조율자) 검사, 금액·시간·위치는 테스트 픽스처이며 기획 값 아님
// 치트 처리기(DIGETHERDevTools)가 부르는 같은 개발 함수를 직접 호출

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/Economy/DigetherDepositStationActor.h"
#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Economy/DigetherQuotaStateComponent.h"
#include "Game/Economy/DigetherWalletComponent.h"
#include "Game/Elevator/DigetherElevatorActor.h"
#include "Game/Elevator/DigetherElevatorLeverActor.h"
#include "Game/Framework/DigetherGameplayGameMode.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Framework/DigetherTestPawn.h"
#include "Game/Run/DigetherRunCoordinatorComponent.h"
#include "Game/Run/DigetherRunCoordinatorState.h"
#include "Game/Run/DigetherRunRules.h"
#include "Game/Save/DigetherSaveRules.h"
#include "Game/Save/DigetherSaveStorage.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "OnlineSubsystemTypes.h"
#include "UObject/UnrealType.h"

// 준비·관찰만 노출, 판정 변경은 공개 개발 함수 직접 호출
class FDigetherDevCheatTestAccess
{
public:
	static bool Prepare(ADigetherGameplayGameMode& Mode, const FDigetherRunTuning& Tuning)
	{
		Mode.RunTuning = Tuning;
		Mode.GameId = FGuid::NewGuid();
		Mode.CurrentRunId = FGuid::NewGuid();
		Mode.bRunReady = true;
		Mode.bHasSaveStartupAttempted = true;
		const ADigetherGameplayGameState* State = Mode.GetGameState<ADigetherGameplayGameState>();
		return IsValid(State)
			&& State->GetQuotaComponent()->SetSignalTuning(Mode.EconomyTuning) == EDigetherEconomyResult::Success
			&& State->GetQuotaComponent()->SetQuotaState(FDigetherEconomyRules::MakeInitialQuotaState(Tuning), false)
				== EDigetherEconomyResult::Success
			&& Mode.InitializeRunActors();
	}
	static const FDigetherEconomyTuning& Economy(const ADigetherGameplayGameMode& Mode) { return Mode.EconomyTuning; }
	static int32 PourSessionCount(const ADigetherGameplayGameMode& Mode) { return Mode.PourSessions.CaptureSessions().Num(); }
	static bool IsTimeExpiredPending(const ADigetherGameplayGameMode& Mode) { return Mode.bTimeExpiredPending; }
	static void ClearTimeExpiredPending(ADigetherGameplayGameMode& Mode) { Mode.bTimeExpiredPending = false; }
	static void ClearPendingAutosave(ADigetherGameplayGameMode& Mode) { Mode.ClearPendingAutosave(); }
	static bool HasPendingAutosave(const ADigetherGameplayGameMode& Mode)
	{
		return Mode.PendingAutosave.IsSet() || Mode.bAutosaveRecapturePending;
	}
};

namespace
{
	const FName DevCheatStationId(TEXT("DevCheatStation"));
	const FVector DevCheatStationLocation(1000.0, 0.0, 0.0);
	// 투입구(정산기 앞면 +X 50cm, 높이 100cm) 앞에서 -X를 보는 자리
	const FVector DevCheatPourerOffset(150.0, 0.0, 100.0);
	const FRotator DevCheatFacingStation(0.0, 180.0, 0.0);
	constexpr float DevCheatPourTickSeconds = 0.5f;

	FDigetherRunTuning MakeDevCheatTuning()
	{
		FDigetherRunTuning Tuning;
		Tuning.SourceProfile = FPrimaryAssetId(TEXT("DigetherTuningProfile:DA_DevCheatFixture"));
		Tuning.ProfileVersion = 1;
		Tuning.InitialQuotaCredits = 100;
		Tuning.RunSeconds = 120.f;
		Tuning.InteractionDistanceCm = 200.f;
		Tuning.ElevatorCapacityKg = 500.f;
		Tuning.ElevatorTripLimit = 2;
		Tuning.AscentPenaltySeconds = 10.f;
		Tuning.ElevatorTravelSeconds = 2.f;
		Tuning.DepositIntervalSeconds = DevCheatPourTickSeconds;
		Tuning.EmergencyWeightLimitKg = 3.f;
		Tuning.DeathLossPercent = 50;
		Tuning.RecoveredDeathLossPercent = 50;
		return Tuning;
	}

	struct FDevCheatPlayer
	{
		ADigetherGameplayPlayerController* Controller = nullptr;
		ADigetherGameplayPlayerState* State = nullptr;
		APawn* Pawn = nullptr;
		bool IsReady() const { return IsValid(Controller) && IsValid(State) && IsValid(Pawn); }
	};

	// 붓기 통합 검사와 같은 임시 월드 구성(엘리베이터·레버 3개·정산기), 자동 저장 슬롯은 종료 시 삭제
	class FDevCheatWorld
	{
	public:
		explicit FDevCheatWorld(FAutomationTestBase& InTest) : Test(InTest)
		{
			if (!GEngine) return;
			Instance = NewObject<UGameInstance>(GEngine);
			Instance->AddToRoot();
			Instance->InitializeStandalone();
			World = Instance->GetWorld();
			const FURL URL(nullptr, TEXT("/Engine/Maps/Entry?game=/Script/DIGETHER.DigetherGameplayGameMode"), TRAVEL_Absolute);
			if (!World || !World->SetGameMode(URL)) return;
			World->InitializeActorsForPlay(URL);
			Mode = World->GetAuthGameMode<ADigetherGameplayGameMode>();
			GameState = World->GetGameState<ADigetherGameplayGameState>();
			if (!IsValid(Mode) || !IsValid(GameState)) return;
			Elevator = World->SpawnActor<ADigetherElevatorActor>();
			if (!IsValid(Elevator)
				|| !AddLever(EDigetherElevatorLeverKind::Departure, FVector(-100, 0, 0))
				|| !AddLever(EDigetherElevatorLeverKind::Ascent, FVector(100, 0, 0))
				|| !AddLever(EDigetherElevatorLeverKind::Ascent, FVector(400, 0, 0), EDigetherElevatorLeverPlacement::Outside)
				|| !FDigetherDevCheatTestAccess::Prepare(*Mode, MakeDevCheatTuning())) return;
			Station = World->SpawnActor<ADigetherDepositStationActor>(DevCheatStationLocation, FRotator::ZeroRotator);
			FNameProperty* StationIdProperty = FindFProperty<FNameProperty>(ADigetherDepositStationActor::StaticClass(), TEXT("StationId"));
			if (!IsValid(Station) || StationIdProperty == nullptr) return;
			*StationIdProperty->ContainerPtrToValuePtr<FName>(Station) = DevCheatStationId;
			bReady = true;
		}

		~FDevCheatWorld()
		{
			if (IsValid(Mode))
			{
				FDigetherDevCheatTestAccess::ClearPendingAutosave(*Mode);
			}
			if (Instance) Instance->Shutdown();
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine) GEngine->DestroyWorldContext(World);
			}
			if (Instance) Instance->RemoveFromRoot();
			for (const FString& Slot : TrackedSlots)
			{
				if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
				{
					Test.TestTrue(TEXT("치트 시험 임시 슬롯 삭제"), UGameplayStatics::DeleteGameInSlot(Slot, 0));
				}
			}
		}

		FDevCheatWorld(const FDevCheatWorld&) = delete;
		FDevCheatWorld& operator=(const FDevCheatWorld&) = delete;

		bool IsReady() const { return bReady; }
		UWorld* GetWorld() const { return World; }
		ADigetherGameplayGameMode* GetMode() const { return Mode; }
		ADigetherDepositStationActor* GetStation() const { return Station; }
		UDigetherQuotaStateComponent* GetQuota() const { return GameState->GetQuotaComponent(); }
		FDigetherQuotaState QuotaState() const { return GetQuota()->CaptureState().Quota; }
		UDigetherRunCoordinatorComponent* GetRun() const { return Mode->GetRunCoordinatorComponent(); }

		void SetQuota(const int32 Day, const int32 Target, const int32 Deposited)
		{
			FDigetherQuotaState Quota = QuotaState();
			Quota.Day = Day;
			Quota.TargetCredits = Target;
			Quota.DepositedCredits = Deposited;
			Test.TestTrue(TEXT("시험 목표 설정"), GetQuota()->SetQuotaState(Quota) == EDigetherEconomyResult::Success);
		}

		FDevCheatPlayer AddPlayer(const FVector& Location, const FRotator& Rotation, const int32 Credits)
		{
			FDevCheatPlayer Player;
			Player.Controller = World->SpawnActor<ADigetherGameplayPlayerController>();
			if (!IsValid(Player.Controller)) return Player;
			if (!bHostAssigned)
			{
				Player.Controller->SetAsLocalPlayerController();
				bHostAssigned = true;
			}
			Player.State = Player.Controller->GetPlayerState<ADigetherGameplayPlayerState>();
			if (!IsValid(Player.State)) Player.State = World->SpawnActor<ADigetherGameplayPlayerState>();
			if (!IsValid(Player.State)) return Player;
			Player.State->SetOwner(Player.Controller);
			Player.Controller->SetPlayerState(Player.State);
			GameState->AddPlayerState(Player.State);
			const FUniqueNetIdRepl Id(FUniqueNetIdRef(FUniqueNetIdString::Create(FGuid::NewGuid().ToString(), FName(TEXT("NULL")))));
			Player.State->SetUniqueId(Id);
			TrackSlots(Id);
			Player.State->InitializeGameplayIdentity();
			Player.State->MarkGameplayInitialized();
			FDigetherWalletState Wallet;
			Wallet.BalanceCredits = Credits;
			Player.State->RestoreState(Wallet, false);
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player.Pawn = World->SpawnActor<ADigetherTestPawn>(Location, Rotation, Spawn);
			if (IsValid(Player.Pawn))
			{
				Player.Controller->Possess(Player.Pawn);
				Player.Controller->SetViewTarget(Player.Pawn);
			}
			return Player;
		}

		bool WasAutosaveRequested() const
		{
			for (const FString& Slot : TrackedSlots)
			{
				if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
				{
					return true;
				}
			}
			return FDigetherDevCheatTestAccess::HasPendingAutosave(*Mode);
		}

	private:
		bool AddLever(const EDigetherElevatorLeverKind Kind, const FVector& Location,
			const EDigetherElevatorLeverPlacement Placement = EDigetherElevatorLeverPlacement::Inside)
		{
			ADigetherElevatorLeverActor* Lever = World->SpawnActor<ADigetherElevatorLeverActor>();
			if (!IsValid(Lever)) return false;
			FEnumProperty* KindProperty = FindFProperty<FEnumProperty>(Lever->GetClass(), TEXT("LeverKind"));
			FEnumProperty* PlacementProperty = FindFProperty<FEnumProperty>(Lever->GetClass(), TEXT("Placement"));
			FObjectPropertyBase* ElevatorProperty = FindFProperty<FObjectPropertyBase>(Lever->GetClass(), TEXT("ElevatorActor"));
			if (!KindProperty || !PlacementProperty || !ElevatorProperty) return false;
			*KindProperty->ContainerPtrToValuePtr<EDigetherElevatorLeverKind>(Lever) = Kind;
			*PlacementProperty->ContainerPtrToValuePtr<EDigetherElevatorLeverPlacement>(Lever) = Placement;
			ElevatorProperty->SetObjectPropertyValue_InContainer(Lever, Elevator);
			Lever->SetActorLocation(Location);
			return true;
		}

		void TrackSlots(const FUniqueNetIdRepl& Id)
		{
			FString Key;
			if (DigetherSaveRules::MakeAuthenticatedPlayerKey(Id, Key) == EDigetherSaveResult::Success)
			{
				const FDigetherSaveSlotNames Names = DigetherSaveStorage::MakeSlotNames(Key);
				TrackedSlots.Add(Names.Primary);
				TrackedSlots.Add(Names.Backup);
			}
		}

		FAutomationTestBase& Test;
		UGameInstance* Instance = nullptr;
		UWorld* World = nullptr;
		ADigetherGameplayGameMode* Mode = nullptr;
		ADigetherGameplayGameState* GameState = nullptr;
		ADigetherElevatorActor* Elevator = nullptr;
		ADigetherDepositStationActor* Station = nullptr;
		TArray<FString> TrackedSlots;
		bool bHostAssigned = false;
		bool bReady = false;
	};

	// 시계를 바로 시작한 런, 출발 경로 대신 조율자 공개 함수로 탐사 중 상태만 만듦
	bool StartDevCheatRun(FDevCheatWorld& Fixture)
	{
		return Fixture.GetRun()->StartRun(MakeDevCheatTuning(), FDigetherRunRuleTuning(),
			Fixture.GetWorld()->GetTimeSeconds(), true) == EDigetherRunResult::Success;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherDevCheatSetDayTest,
	"DIGETHER.Game.Dev.Cheat.SetDayThenEvaluate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherDevCheatSetDayTest::RunTest(const FString& Parameters)
{
	// A-01: 거점 대기에서 일차 설정 뒤 실제 판정 규칙이 그 일차를 읽는지
	FDevCheatWorld Fixture(*this);
	if (!TestTrue(TEXT("치트 시험 월드 준비"), Fixture.IsReady())) return false;
	Fixture.SetQuota(1, 100, 30);
	const FDigetherQuotaState Before = Fixture.QuotaState();
	FString Message;
	TestFalse(TEXT("기간 일차 5 거절"), Fixture.GetMode()->DevSetDay(5, 5, Message));
	TestFalse(TEXT("기간 일차 0 거절"), Fixture.GetMode()->DevSetDay(1, 0, Message));
	TestFalse(TEXT("전체 일차 0 거절"), Fixture.GetMode()->DevSetDay(0, 1, Message));
	TestEqual(TEXT("거절은 상태 불변"), Fixture.QuotaState().Day, Before.Day);

	if (!TestTrue(TEXT("전체 8일차·기간 4일차 설정"), Fixture.GetMode()->DevSetDay(8, 4, Message))) return false;
	const FDigetherQuotaState After = Fixture.QuotaState();
	TestEqual(TEXT("기간 일차"), After.Day, 4);
	TestEqual(TEXT("전체 일차"), After.TotalDay, 8);
	TestEqual(TEXT("목표 유지"), After.TargetCredits, Before.TargetCredits);
	TestEqual(TEXT("인정액 유지"), After.DepositedCredits, Before.DepositedCredits);
	TestEqual(TEXT("기간 세대 유지"), After.PeriodGeneration, Before.PeriodGeneration);

	const FDigetherRunQuotaResult Departure = FDigetherRunRules::EvaluateQuota(After);
	TestTrue(TEXT("4일차 출발은 납부 마감"), Departure.bPaymentDeadline);
	TestFalse(TEXT("4일차 출발 자체는 게임 오버 아님"), Departure.bGameOver);
	int32 EfficiencyPercent = 0;
	TestEqual(TEXT("4일차 납부 효율 조회"), FDigetherEconomyRules::GetDepositEfficiencyPercent(After.Day,
		FDigetherDevCheatTestAccess::Economy(*Fixture.GetMode()), EfficiencyPercent), EDigetherEconomyResult::Success);
	TestEqual(TEXT("4일차 효율은 경제 튜닝 4일차 값"), EfficiencyPercent,
		FDigetherDevCheatTestAccess::Economy(*Fixture.GetMode()).DayFourEfficiencyPercent);
	// 5일차 실패는 귀환 때 일차 증가 뒤 판정(RunRules), 같은 규칙으로 확인
	const FDigetherDayAdvanceResult Returned = FDigetherEconomyRules::AdvanceDay(After);
	TestEqual(TEXT("귀환 일차 증가"), Returned.Result, EDigetherEconomyResult::Success);
	TestTrue(TEXT("미달 4일차 귀환은 게임 오버"), FDigetherRunRules::EvaluateQuota(Returned.State).bGameOver);

	if (!TestTrue(TEXT("탐사 시작"), StartDevCheatRun(Fixture))) return false;
	TestFalse(TEXT("탐사 중 일차 설정 거절"), Fixture.GetMode()->DevSetDay(2, 2, Message));
	TestEqual(TEXT("탐사 중 거절은 상태 불변"), Fixture.QuotaState().Day, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherDevCheatCompleteQuotaTest,
	"DIGETHER.Game.Dev.Cheat.CompleteQuotaMatchesPour",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherDevCheatCompleteQuotaTest::RunTest(const FString& Parameters)
{
	// A-02: 즉시 달성이 붓기 달성과 같은 새 기간 상태·후속 처리(연출·붓기 종료·자동 저장)를 내는지
	FDevCheatWorld Fixture(*this);
	if (!TestTrue(TEXT("치트 시험 월드 준비"), Fixture.IsReady())) return false;
	Fixture.SetQuota(3, 100, 95);
	const FDigetherQuotaState Before = Fixture.QuotaState();
	FDigetherWalletState PourWallet;
	PourWallet.BalanceCredits = 100;
	// 붓기 한 주기가 같은 상태에서 달성할 때의 결과(붓기 경로의 순수 계산)
	const FDigetherPourTickResult PourResult = FDigetherEconomyRules::CalculatePourTick(PourWallet, Before,
		FDigetherDevCheatTestAccess::Economy(*Fixture.GetMode()), false);
	if (!TestTrue(TEXT("기준 붓기 주기가 달성"), PourResult.Result == EDigetherEconomyResult::Success && PourResult.bTargetCompleted))
	{
		return false;
	}

	FDevCheatPlayer Pourer = Fixture.AddPlayer(DevCheatStationLocation + DevCheatPourerOffset, DevCheatFacingStation, 100);
	if (!TestTrue(TEXT("붓는 사람 준비"), Pourer.IsReady())) return false;
	TestEqual(TEXT("붓기 시작"), Fixture.GetMode()->BeginPour(Pourer.Controller, Fixture.GetStation()).Result,
		EDigetherEconomyResult::Success);
	TestEqual(TEXT("붓기 세션 1개"), FDigetherDevCheatTestAccess::PourSessionCount(*Fixture.GetMode()), 1);
	int32 CompletedCount = 0;
	Fixture.GetQuota()->OnQuotaCompleted.AddLambda([&CompletedCount]() { ++CompletedCount; });
	TestFalse(TEXT("달성 전 자동 저장 없음"), Fixture.WasAutosaveRequested());

	FString Message;
	if (!TestTrue(TEXT("즉시 달성"), Fixture.GetMode()->DevCompleteQuota(Message))) return false;
	const FDigetherQuotaState After = Fixture.QuotaState();
	TestEqual(TEXT("다음 목표가 붓기 달성과 같음"), After.TargetCredits, PourResult.QuotaState.TargetCredits);
	TestEqual(TEXT("인정액이 붓기 달성과 같음"), After.DepositedCredits, PourResult.QuotaState.DepositedCredits);
	TestEqual(TEXT("기간 일차가 붓기 달성과 같음"), After.Day, PourResult.QuotaState.Day);
	TestEqual(TEXT("기간 세대가 붓기 달성과 같음"), After.PeriodGeneration, PourResult.QuotaState.PeriodGeneration);
	TestEqual(TEXT("전체 일차가 붓기 달성과 같음"), After.TotalDay, PourResult.QuotaState.TotalDay);
	TestEqual(TEXT("달성 연출 한 번"), CompletedCount, 1);
	TestEqual(TEXT("달성 순간 모든 붓기 끊김"), FDigetherDevCheatTestAccess::PourSessionCount(*Fixture.GetMode()), 0);
	TestTrue(TEXT("달성 시 자동 저장 요청"), Fixture.WasAutosaveRequested());
	TestEqual(TEXT("붓는 사람 잔고 불변(치트는 차감 없음)"), Pourer.State->CaptureState().BalanceCredits, 100);

	// 목표 설정: 인정액 이하 거절, 그보다 크면 반영
	TestFalse(TEXT("인정액 이하 목표 거절"), Fixture.GetMode()->DevSetQuotaTarget(After.DepositedCredits, Message));
	TestTrue(TEXT("목표 설정"), Fixture.GetMode()->DevSetQuotaTarget(500, Message));
	TestEqual(TEXT("목표 반영"), Fixture.QuotaState().TargetCredits, 500);

	// 탐사 중에는 납부 접수가 닫혀 달성 거절(붓기와 같은 조건)
	if (!TestTrue(TEXT("탐사 시작"), StartDevCheatRun(Fixture))) return false;
	TestFalse(TEXT("탐사 중 즉시 달성 거절"), Fixture.GetMode()->DevCompleteQuota(Message));
	TestEqual(TEXT("거절 시 연출 없음"), CompletedCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherDevCheatRunClockRulesTest,
	"DIGETHER.Game.Dev.Cheat.RunClockSetPauseAscent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherDevCheatRunClockRulesTest::RunTest(const FString& Parameters)
{
	// B-01: 남은 시간 설정·정지와 정지 중 상승 확정(시작 시각만 미는 방식의 거절 회귀 방지)
	FDigetherRunTuning Tuning;
	Tuning.RunSeconds = 100.f;
	Tuning.AscentPenaltySeconds = 10.f;
	const FDigetherRunRuleTuning RuleTuning;
	FDigetherRunCoordinatorState State;
	TestEqual(TEXT("런 시작(시계 정지)"), FDigetherRunCoordinatorRules::StartRun(State, Tuning, RuleTuning, 10.0, false).Result,
		EDigetherRunResult::Success);
	TestEqual(TEXT("시계 시작 전 설정 거절"), FDigetherRunCoordinatorRules::DevSetRemainingSeconds(State, 50.0, 12.0).Result,
		EDigetherRunResult::InvalidInput);
	TestEqual(TEXT("시계 시작"), FDigetherRunCoordinatorRules::StartClock(State, 20.0).Result, EDigetherRunResult::Success);

	TestEqual(TEXT("남은 시간 60초 설정"), FDigetherRunCoordinatorRules::DevSetRemainingSeconds(State, 60.0, 25.0).Result,
		EDigetherRunResult::Success);
	TestTrue(TEXT("설정 값 반영"), FMath::IsNearlyEqual(State.RemainingSeconds, 60.0, 1e-6));
	TestEqual(TEXT("설정 시각으로 갱신"), State.LastUpdatedAtSeconds, 25.0);
	FDigetherRunCoordinatorRules::UpdateTime(State, 35.0);
	TestTrue(TEXT("설정 뒤 시간 흐름"), FMath::IsNearlyEqual(State.RemainingSeconds, 50.0, 1e-6));
	TestEqual(TEXT("최대치 초과 거절"), FDigetherRunCoordinatorRules::DevSetRemainingSeconds(State, 100.5, 36.0).Result,
		EDigetherRunResult::InvalidInput);
	TestEqual(TEXT("과거 시각 거절"), FDigetherRunCoordinatorRules::DevSetRemainingSeconds(State, 40.0, 30.0).Result,
		EDigetherRunResult::InvalidInput);

	// 정지: 시간이 흘러도 남은 시간 유지
	State.bDevClockPaused = true;
	TestEqual(TEXT("정지 중 갱신 성공"), FDigetherRunCoordinatorRules::UpdateTime(State, 100.0).Result, EDigetherRunResult::Success);
	TestTrue(TEXT("정지 중 남은 시간 유지"), FMath::IsNearlyEqual(State.RemainingSeconds, 50.0, 1e-6));
	TestFalse(TEXT("정지 중 시간 종료 없음"), FDigetherRunCoordinatorRules::UpdateTime(State, 500.0).bTimeExpiredNow);
	TestTrue(TEXT("긴 정지 뒤에도 남은 시간 유지"), FMath::IsNearlyEqual(State.RemainingSeconds, 50.0, 1e-6));
	TestTrue(TEXT("시작 시각이 마지막 갱신 시각을 넘지 않음"), State.StartedAtSeconds <= State.LastUpdatedAtSeconds);

	// 정지 중 상승 확정: 마지막 갱신 기준 재계산이 거절되지 않고 페널티만 반영
	TestEqual(TEXT("정지 중 상승 확정"), FDigetherRunCoordinatorRules::RecordConfirmedAscent(State).Result,
		EDigetherRunResult::Success);
	TestEqual(TEXT("상승 횟수"), State.ConfirmedAscentCount, 1);
	TestTrue(TEXT("정지 중 상승 페널티 반영"), FMath::IsNearlyEqual(State.RemainingSeconds, 40.0, 1e-6));
	TestTrue(TEXT("최대치는 페널티만큼 감소"),
		FMath::IsNearlyEqual(FDigetherRunCoordinatorRules::DevGetMaxRemainingSeconds(State), 90.0, 1e-6));

	// 재개: 그때부터 다시 흐름
	State.bDevClockPaused = false;
	FDigetherRunCoordinatorRules::UpdateTime(State, 510.0);
	TestTrue(TEXT("재개 뒤 시간 흐름"), FMath::IsNearlyEqual(State.RemainingSeconds, 30.0, 1e-6));

	// 0 설정은 실제 만료 통지, 정지 중이어도 만료 유지
	State.bDevClockPaused = true;
	const FDigetherRunAdvanceResult Expired = FDigetherRunCoordinatorRules::DevSetRemainingSeconds(State, 0.0, 520.0);
	TestEqual(TEXT("0 설정 성공"), Expired.Result, EDigetherRunResult::Success);
	TestTrue(TEXT("0 설정은 시간 종료 통지"), Expired.bTimeExpiredNow);
	TestEqual(TEXT("남은 시간 0"), State.RemainingSeconds, 0.0);
	TestFalse(TEXT("정지 갱신에서 재통지 없음"), FDigetherRunCoordinatorRules::UpdateTime(State, 530.0).bTimeExpiredNow);
	TestEqual(TEXT("만료 뒤 설정 거절"), FDigetherRunCoordinatorRules::DevSetRemainingSeconds(State, 10.0, 540.0).Result,
		EDigetherRunResult::InvalidInput);

	// 다음 런은 정지가 이어지지 않음
	FDigetherRunTerminationInput Input;
	Input.Trigger = EDigetherRunTerminationTrigger::TimeExpired;
	Input.ElevatorState = EDigetherRunElevatorState::StoppedInMine;
	FDigetherRunCoordinatorRules::CompleteRun(State, Input);
	TestEqual(TEXT("다음 런 시작"), FDigetherRunCoordinatorRules::StartRun(State, Tuning, RuleTuning, 600.0).Result,
		EDigetherRunResult::Success);
	TestFalse(TEXT("다음 런 정지 해제"), State.bDevClockPaused);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherDevCheatRunTimeGameModeTest,
	"DIGETHER.Game.Dev.Cheat.RunTimeTimeoutPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherDevCheatRunTimeGameModeTest::RunTest(const FString& Parameters)
{
	// B-01·A-03 Timeout: GameMode 진입점이 조율자 규칙을 거쳐 실제 시간 종료 대기 표시까지 가는지
	FDevCheatWorld Fixture(*this);
	if (!TestTrue(TEXT("치트 시험 월드 준비"), Fixture.IsReady())) return false;
	FString Message;
	TestFalse(TEXT("탐사 전 남은 시간 설정 거절"), Fixture.GetMode()->DevSetRunRemainingSeconds(30.0, Message));
	TestFalse(TEXT("탐사 전 Timeout 거절"), Fixture.GetMode()->DevEndRunTimeout(Message));
	TestFalse(TEXT("탐사 전 Survive 거절"), Fixture.GetMode()->DevEndRunSurvive(nullptr, Message));
	if (!TestTrue(TEXT("탐사 시작"), StartDevCheatRun(Fixture))) return false;

	TestTrue(TEXT("남은 시간 30초"), Fixture.GetMode()->DevSetRunRemainingSeconds(30.0, Message));
	TestTrue(TEXT("조율자 남은 시간"), FMath::IsNearlyEqual(Fixture.GetRun()->GetRemainingSeconds(), 30.0, 1e-4));
	TestFalse(TEXT("최대치 초과 거절"), Fixture.GetMode()->DevSetRunRemainingSeconds(10000.0, Message));
	TestTrue(TEXT("10초 진행"), Fixture.GetMode()->DevAdvanceRunTime(10.0, Message));
	TestTrue(TEXT("진행 반영"), FMath::IsNearlyEqual(Fixture.GetRun()->GetRemainingSeconds(), 20.0, 1e-4));
	TestTrue(TEXT("정지"), Fixture.GetMode()->DevSetRunTimePaused(true, Message));
	TestTrue(TEXT("정지 표시"), Fixture.GetRun()->IsDevClockPaused());
	// 엘리베이터가 바닥 대기가 아니므로(거점 대기 상태 그대로) Survive 거절
	TestFalse(TEXT("바닥 대기 아님 Survive 거절"), Fixture.GetMode()->DevEndRunSurvive(nullptr, Message));

	TestFalse(TEXT("시간 종료 대기 전"), FDigetherDevCheatTestAccess::IsTimeExpiredPending(*Fixture.GetMode()));
	TestTrue(TEXT("Timeout"), Fixture.GetMode()->DevEndRunTimeout(Message));
	TestEqual(TEXT("남은 시간 0"), Fixture.GetRun()->GetRemainingSeconds(), 0.0);
	TestTrue(TEXT("실제 시간 종료 통지로 대기 표시"), FDigetherDevCheatTestAccess::IsTimeExpiredPending(*Fixture.GetMode()));
	TestFalse(TEXT("종료 처리 중 재설정 거절"), Fixture.GetMode()->DevSetRunRemainingSeconds(10.0, Message));
	// 귀환 처리는 참가자 없는 시험 월드에서 진행하지 않음
	FDigetherDevCheatTestAccess::ClearTimeExpiredPending(*Fixture.GetMode());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherDevCheatSetBalanceTest,
	"DIGETHER.Game.Dev.Cheat.SetBalance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherDevCheatSetBalanceTest::RunTest(const FString& Parameters)
{
	// D-01: 잔고 절대값 설정, 다른 지갑 통계 유지
	FDevCheatWorld Fixture(*this);
	if (!TestTrue(TEXT("치트 시험 월드 준비"), Fixture.IsReady())) return false;
	FDevCheatPlayer Player = Fixture.AddPlayer(FVector(-500.0, 0.0, 100.0), FRotator::ZeroRotator, 100);
	if (!TestTrue(TEXT("플레이어 준비"), Player.IsReady())) return false;
	FDigetherWalletState Seed = Player.State->CaptureState();
	Seed.LastRunPayoutCredits = 40;
	Seed.LifetimeEarnedCredits = 70;
	TestEqual(TEXT("통계 준비"), Player.State->RestoreState(Seed, false), EDigetherEconomyResult::Success);

	FString Message;
	TestTrue(TEXT("잔고 777 설정"), Fixture.GetMode()->DevSetBalance(Player.State, 777, Message));
	const FDigetherWalletState After = Player.State->CaptureState();
	TestEqual(TEXT("잔고 절대값"), After.BalanceCredits, 777);
	TestEqual(TEXT("최근 지급 통계 유지"), After.LastRunPayoutCredits, 40);
	TestEqual(TEXT("누적 획득 통계 유지"), After.LifetimeEarnedCredits, 70);
	TestTrue(TEXT("잔고 0 설정"), Fixture.GetMode()->DevSetBalance(Player.State, 0, Message));
	TestEqual(TEXT("잔고 0"), Player.State->CaptureState().BalanceCredits, 0);
	TestFalse(TEXT("음수 거절"), Fixture.GetMode()->DevSetBalance(Player.State, -1, Message));
	TestFalse(TEXT("대상 없음 거절"), Fixture.GetMode()->DevSetBalance(nullptr, 10, Message));
	TestEqual(TEXT("거절은 상태 불변"), Player.State->CaptureState().BalanceCredits, 0);
	return true;
}

#endif
