#if WITH_DEV_AUTOMATION_TESTS

// 아래 숫자와 ID는 테스트 픽스처, 기획 값 아님.

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Economy/DigetherQuotaStateComponent.h"
#include "Game/Elevator/DigetherElevatorActor.h"
#include "Game/Elevator/DigetherElevatorComponent.h"
#include "Game/Dev/DigetherCheatManager.h"
#include "Game/Dev/DigetherDevCommands.h"
#include "Game/Framework/DigetherGameplayGameMode.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Framework/DigetherTestPawn.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Run/DigetherRunCoordinatorComponent.h"
#include "Game/Save/DigetherSaveGame.h"
#include "Game/Save/DigetherSaveRules.h"
#include "Game/Save/DigetherSaveStorage.h"
#include "Game/Save/DigetherSaveTuning.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/GameState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineSubsystem.h"
#include "UObject/Class.h"

// 테스트 준비와 관찰만 노출한다. 출발·정산·로그인·재빙의는 제품의 실제 함수를 호출한다.
class FDigetherFrameworkSaveIntegrationAccess
{
public:
	static bool PrepareRun(ADigetherGameplayGameMode& Mode, ADigetherElevatorActor& Elevator,
		const FDigetherRunTuning& Tuning)
	{
		Mode.RunTuning = Tuning;
		Mode.GameId = FGuid::NewGuid();
		Mode.CurrentRunId = FGuid::NewGuid();
		Mode.ElevatorActor = &Elevator;
		Mode.bRunReady = true;
		Mode.bHasSaveStartupAttempted = true;
		// 월드 BeginPlay 없이 엔진 스폰 조건만 갖춘다. 탐사 phase는 실제 ProcessDeparture가 바꾼다.
		Mode.MatchState = MatchState::InProgress;
		ADigetherGameplayGameState* State = Mode.GetGameState<ADigetherGameplayGameState>();
		return IsValid(State)
			&& State->GetQuotaComponent()->SetQuotaState(FDigetherEconomyRules::MakeInitialQuotaState(Tuning), false)
				== EDigetherEconomyResult::Success
			&& Mode.RunCoordinatorComponent->SetPublicStateComponent(State->GetRunPublicStateComponent())
				== EDigetherRunResult::Success
			&& Elevator.GetElevatorComponent()->InitializeForRun(Tuning, FDigetherElevatorTuning())
				== EDigetherElevatorResult::Success;
	}
	static void Depart(ADigetherGameplayGameMode& Mode) { Mode.ProcessDeparture(); }
	static void Finish(ADigetherGameplayGameMode& Mode)
	{
		Mode.FinishRunAndReturn(EDigetherRunTerminationTrigger::TimeExpired);
	}
	static int32 DepartureCount(const ADigetherGameplayGameMode& Mode) { return Mode.DeparturePlayerCount; }
	static bool IsParticipant(const ADigetherGameplayGameMode& Mode, ADigetherGameplayPlayerState* Player)
	{
		TArray<ADigetherGameplayPlayerState*> Players;
		Mode.CollectRunParticipants(Players);
		return Players.Contains(Player);
	}
	static bool IsReconnectPending(const ADigetherGameplayGameMode& Mode, ADigetherGameplayPlayerState* Player)
	{
		return Mode.ReconnectedPlayers.Contains(Player);
	}
	static bool PullContext(const ADigetherGameplayGameMode& Mode, FDigetherElevatorPullContext& Out)
	{
		return Mode.TryMakePullContext(Out);
	}
	static void AddRecord(ADigetherGameplayGameMode& Mode, const FDigetherSavePlayerRecord& Record)
	{
		Mode.SavedPlayerRecords.Add(Record);
	}
	static bool Capture(const ADigetherGameplayGameMode& Mode, FDigetherSaveSnapshot& Out)
	{
		return Mode.CaptureProgress(Out);
	}
};

namespace
{
	// 기본 모드는 초기화 전 액터 검사, Test를 받으면 실제 Login·SaveSubsystem용 독립 월드다.
	class FDigetherSaveIntegrationWorld
	{
	public:
		explicit FDigetherSaveIntegrationWorld(FAutomationTestBase* InTest = nullptr) : Test(InTest)
		{
			if (Test != nullptr)
			{
				if (GEngine == nullptr)
				{
					Test->AddError(TEXT("실제 로그인·저장 통합 fixture에 GEngine이 필요합니다."));
					return;
				}
				// 합성 ID를 실제 Steam 제공자에 전달하지 않는다. 이 L2 fixture의 실행 조건은 -NoSteam이다.
				const IOnlineSubsystem* Online = IOnlineSubsystem::Get();
				if (Online == nullptr || Online->GetSubsystemName() != FName(TEXT("NULL")))
				{
					Test->AddError(TEXT("SaveIntegration 실제 로그인 fixture는 NULL OSS가 필요합니다. -NoSteam으로 실행하세요. Steam 제공자 검증은 M3 대상입니다."));
					return;
				}
				Instance = NewObject<UGameInstance>(GEngine);
				Instance->AddToRoot();
				Instance->InitializeStandalone();
				World = Instance->GetWorld();
				const FURL URL(nullptr, TEXT("/Engine/Maps/Entry?game=/Script/DIGETHER.DigetherGameplayGameMode"), TRAVEL_Absolute);
				if (World != nullptr && World->SetGameMode(URL))
				{
					World->InitializeActorsForPlay(URL);
					GameMode = Cast<ADigetherGameplayGameMode>(World->GetAuthGameMode());
					GameState = World->GetGameState<ADigetherGameplayGameState>();
				}
				return;
			}
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &Values);
			if (World != nullptr)
			{
				if (GEngine != nullptr)
				{
					GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
				}
				GameMode = World->SpawnActor<ADigetherGameplayGameMode>();
				GameState = World->SpawnActor<ADigetherGameplayGameState>();
				if (IsValid(GameMode) && IsValid(GameState))
				{
					GameMode->GameState = GameState;
					World->SetGameState(GameState);
				}
			}
		}

		~FDigetherSaveIntegrationWorld()
		{
			if (Instance != nullptr)
			{
				Instance->Shutdown();
			}
			if (World != nullptr)
			{
				World->DestroyWorld(false);
				if (GEngine != nullptr)
				{
					GEngine->DestroyWorldContext(World);
				}
			}
			if (Instance != nullptr)
			{
				Instance->RemoveFromRoot();
			}
			for (const FString& Slot : TemporarySlots)
			{
				if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
				{
					Test->TestTrue(TEXT("통합 시험 임시 슬롯 삭제"), UGameplayStatics::DeleteGameInSlot(Slot, 0));
				}
				Test->TestFalse(TEXT("통합 시험 임시 슬롯 정리 확인"), UGameplayStatics::DoesSaveGameExist(Slot, 0));
			}
		}

		FDigetherSaveIntegrationWorld(const FDigetherSaveIntegrationWorld&) = delete;
		FDigetherSaveIntegrationWorld& operator=(const FDigetherSaveIntegrationWorld&) = delete;

		bool IsReady() const { return World != nullptr && IsValid(GameMode) && IsValid(GameState); }
		UWorld* GetWorld() const { return World; }
		ADigetherGameplayGameMode* GetGameMode() const { return GameMode; }
		ADigetherGameplayGameState* GetGameState() const { return GameState; }
		bool TrackHostSlots(const FUniqueNetIdRepl& Id)
		{
			FString Key;
			if (!Test->TestTrue(TEXT("시험 호스트 키 생성"),
				DigetherSaveRules::MakeAuthenticatedPlayerKey(Id, Key) == EDigetherSaveResult::Success))
			{
				return false;
			}
			const FDigetherSaveSlotNames Names = DigetherSaveStorage::MakeSlotNames(Key);
			for (const FString& Slot : { Names.Primary, Names.Backup })
			{
				if (!Test->TestFalse(TEXT("고유 호스트 ID의 슬롯은 새 시험 전 없음"), UGameplayStatics::DoesSaveGameExist(Slot, 0)))
				{
					// 기존 슬롯은 추적·삭제하지 않으며 이 호스트로 저장을 시작하지 않는다.
					return false;
				}
			}
			TemporarySlots.Add(Names.Primary);
			TemporarySlots.Add(Names.Backup);
			return true;
		}

	private:
		UWorld* World = nullptr;
		ADigetherGameplayGameMode* GameMode = nullptr;
		ADigetherGameplayGameState* GameState = nullptr;
		UGameInstance* Instance = nullptr;
		FAutomationTestBase* Test = nullptr;
		TArray<FString> TemporarySlots;
	};

	FUniqueNetIdRepl MakeIntegrationNetId(const TCHAR* Value)
	{
		return FUniqueNetIdRepl(FUniqueNetIdRef(FUniqueNetIdString::Create(Value, FName(TEXT("DIGETHER_TEST")))));
	}

	ADigetherGameplayPlayerState* AddIntegrationPlayer(UWorld* World,
		ADigetherGameplayGameState* GameState, ADigetherGameplayPlayerController* Controller)
	{
		ADigetherGameplayPlayerState* Player = Controller->GetPlayerState<ADigetherGameplayPlayerState>();
		if (!IsValid(Player))
		{
			Player = World->SpawnActor<ADigetherGameplayPlayerState>();
		}
		if (IsValid(Player))
		{
			Player->SetOwner(Controller);
			Controller->SetPlayerState(Player);
			GameState->AddPlayerState(Player);
		}
		return Player;
	}

	FUniqueNetIdRepl MakeRuntimeIntegrationNetId()
	{
		return FUniqueNetIdRepl(FUniqueNetIdRef(FUniqueNetIdString::Create(FGuid::NewGuid().ToString(), FName(TEXT("NULL")))));
	}

	FDigetherRunTuning MakeIntegrationTuning()
	{
		FDigetherRunTuning Tuning;
		Tuning.SourceProfile = FPrimaryAssetId(TEXT("DigetherTuningProfile:DA_IntegrationFixture"));
		Tuning.ProfileVersion = 1;
		Tuning.InitialQuotaCredits = 100;
		Tuning.RunSeconds = 120.f;
		Tuning.InteractionDistanceCm = 200.f;
		Tuning.ElevatorCapacityKg = 500.f;
		Tuning.ElevatorTripLimit = 2;
		Tuning.AscentPenaltySeconds = 10.f;
		Tuning.ElevatorTravelSeconds = 2.f;
		Tuning.DepositIntervalSeconds = 0.5f;
		Tuning.EmergencyWeightLimitKg = 3.f;
		Tuning.DeathLossPercent = 50;
		Tuning.CorpseRefundPercent = 50;
		return Tuning;
	}

	ADigetherElevatorActor* PrepareIntegrationRun(FDigetherSaveIntegrationWorld& Fixture)
	{
		ADigetherElevatorActor* Elevator = Fixture.GetWorld()->SpawnActor<ADigetherElevatorActor>();
		APlayerStart* Start = Fixture.GetWorld()->SpawnActor<APlayerStart>();
		if (!IsValid(Elevator) || !IsValid(Start))
		{
			return nullptr;
		}
		Elevator->SetActorLocation(FVector(2000.0, 0.0, 0.0));
		Start->SetActorLocation(FVector(0.0, 0.0, 200.0));
		return FDigetherFrameworkSaveIntegrationAccess::PrepareRun(*Fixture.GetGameMode(), *Elevator,
			MakeIntegrationTuning()) ? Elevator : nullptr;
	}

	ADigetherGameplayPlayerController* MakeRuntimeIntegrationPlayer(FDigetherSaveIntegrationWorld& Fixture,
		const FUniqueNetIdRepl& Id, const bool bHost = false, const bool bInitialized = true)
	{
		ADigetherGameplayPlayerController* Controller = Fixture.GetWorld()->SpawnActor<ADigetherGameplayPlayerController>();
		if (!IsValid(Controller))
		{
			return nullptr;
		}
		if (bHost)
		{
			if (!Fixture.TrackHostSlots(Id))
			{
				Controller->Destroy();
				return nullptr;
			}
			Controller->SetAsLocalPlayerController();
		}
		ADigetherGameplayPlayerState* Player = AddIntegrationPlayer(Fixture.GetWorld(), Fixture.GetGameState(), Controller);
		if (!IsValid(Player))
		{
			return nullptr;
		}
		Player->SetUniqueId(Id);
		if (bInitialized)
		{
			Player->InitializeGameplayIdentity();
			Player->MarkGameplayInitialized();
		}
		return Controller;
	}

	APawn* GiveIntegrationPawn(FDigetherSaveIntegrationWorld& Fixture,
		ADigetherGameplayPlayerController* Controller, const FVector& Location)
	{
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APawn* Pawn = Fixture.GetWorld()->SpawnActor<ADigetherTestPawn>(Location, FRotator::ZeroRotator, Spawn);
		if (IsValid(Pawn))
		{
			Controller->Possess(Pawn);
		}
		return Pawn;
	}

	void DisconnectIntegrationPlayer(ADigetherGameplayGameMode& Mode, ADigetherGameplayPlayerController& Controller)
	{
		ADigetherGameplayPlayerState* Player = Controller.GetPlayerState<ADigetherGameplayPlayerState>();
		Controller.PawnLeavingGame();
		Mode.AddInactivePlayer(Player, &Controller);
		Controller.SetPlayerState(nullptr);
		Controller.Destroy();
	}

	void UpdateIntegrationOverlaps(ADigetherElevatorActor& Elevator, APawn* Pawn)
	{
		// 엔진 UpdateOverlapsImpl은 BeginPlay 전 액터의 겹침을 계산하지 않는다. 이 fixture는 월드 BeginPlay를 하지 않으므로 겹침 대상만 시작한다.
		if (!Elevator.HasActorBegunPlay())
		{
			Elevator.DispatchBeginPlay();
		}
		if (IsValid(Pawn) && !Pawn->HasActorBegunPlay())
		{
			Pawn->DispatchBeginPlay();
		}
		if (IsValid(Pawn))
		{
			if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Pawn->GetRootComponent()))
			{
				Root->UpdateOverlaps();
			}
		}
		Elevator.GetBoardingVolume()->UpdateOverlaps();
	}

	template <typename TState>
	bool IntegrationStatesEqual(const TState& Left, const TState& Right)
	{
		return TState::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkSaveStartupGuardsTest,
	"DIGETHER.Game.Framework.SaveIntegration.StartupGuards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkSaveStartupGuardsTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("비활성 플레이어를 제공하는 AGameMode 상속"),
		ADigetherGameplayGameMode::StaticClass()->IsChildOf(AGameMode::StaticClass()));
	TestTrue(TEXT("매치 상태와 호환되는 AGameState 상속"),
		ADigetherGameplayGameState::StaticClass()->IsChildOf(AGameState::StaticClass()));
	FDigetherSaveIntegrationWorld Fixture;
	if (!TestTrue(TEXT("초기화 전 서버 연결층 구성"), Fixture.IsReady()))
	{
		return false;
	}
	ADigetherGameplayGameMode* GameMode = Fixture.GetGameMode();
	ADigetherGameplayPlayerController* Controller = Fixture.GetWorld()->SpawnActor<ADigetherGameplayPlayerController>();
	if (!TestNotNull(TEXT("미초기화 참가 Controller"), Controller))
	{
		return false;
	}
	ADigetherGameplayPlayerState* Player = AddIntegrationPlayer(Fixture.GetWorld(), Fixture.GetGameState(), Controller);
	if (!TestNotNull(TEXT("미초기화 참가 상태"), Player))
	{
		return false;
	}
	TestEqual(TEXT("GameMode의 기본 GameState 클래스 일치"),
		GameMode->GameStateClass.Get(), ADigetherGameplayGameState::StaticClass());
	TestFalse(TEXT("튜닝·복원 전 런 준비 아님"), GameMode->IsRunReady());
	TestNull(TEXT("미초기화 런 튜닝 미노출"), GameMode->GetRunTuning());
	TestFalse(TEXT("저장 복원 전 엔진 매치 시작 차단"), GameMode->ReadyToStartMatch_Implementation());
	TestFalse(TEXT("저장 복원 전 엔진 재시작 요청 차단"), GameMode->PlayerCanRestart_Implementation(Controller));
	GameMode->HandleStartingNewPlayer_Implementation(Controller);
	TestNull(TEXT("초기화 전 로그인은 Pawn을 생성하지 않음"), Controller->GetPawn());
	TestFalse(TEXT("초기화 전 로그인은 지급 완료로 표시하지 않음"), Player->IsGameplayInitialized());
	TestFalse(TEXT("초기화 전 로그인은 서버 GUID를 발급하지 않음"), Player->GetGameplayPlayerId().IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkInactiveRegistryTest,
	"DIGETHER.Game.Framework.SaveIntegration.InactiveIdentityRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkInactiveRegistryTest::RunTest(const FString& Parameters)
{
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("재접속 레지스트리 시험 구성"), Fixture.IsReady()))
	{
		return false;
	}
	UWorld* World = Fixture.GetWorld();
	ADigetherGameplayGameMode* GameMode = Fixture.GetGameMode();
	ADigetherGameplayGameState* GameState = Fixture.GetGameState();
	if (!TestNotNull(TEXT("실제 재빙의 시작 훅용 런 구성"), PrepareIntegrationRun(Fixture)))
	{
		return false;
	}
	ADigetherGameplayPlayerController* Leaving = World->SpawnActor<ADigetherGameplayPlayerController>();
	ADigetherGameplayPlayerController* Joining = World->SpawnActor<ADigetherGameplayPlayerController>();
	APawn* Pawn = World->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("끊길 Controller"), Leaving) || !TestNotNull(TEXT("재접속 Controller"), Joining)
		|| !TestNotNull(TEXT("그 자리에 남길 Pawn"), Pawn))
	{
		return false;
	}
	ADigetherGameplayPlayerState* Original = AddIntegrationPlayer(World, GameState, Leaving);
	ADigetherGameplayPlayerState* Fresh = AddIntegrationPlayer(World, GameState, Joining);
	if (!TestNotNull(TEXT("원본 참가 상태"), Original) || !TestNotNull(TEXT("임시 로그인 상태"), Fresh))
	{
		return false;
	}
	const FUniqueNetIdRepl OriginalNetId = MakeIntegrationNetId(TEXT("fixture-original"));
	Original->SetUniqueId(OriginalNetId);
	Original->InitializeGameplayIdentity();
	Original->MarkGameplayInitialized();
	const FGuid GameplayId = Original->GetGameplayPlayerId();
	// 같은 표시 이름·주소를 가진 새 연결도 인증 ID가 없거나 다르면 복원 대상이 아니다.
	Original->SetPlayerName(TEXT("same-fixture-name"));
	Fresh->SetPlayerName(TEXT("same-fixture-name"));
	Original->SavedNetworkAddress = TEXT("fixture-same-address");
	Fresh->SavedNetworkAddress = Original->SavedNetworkAddress;
	Leaving->Possess(Pawn);
	Leaving->PawnLeavingGame();
	GameMode->AddInactivePlayer(Original, Leaving);
	GameMode->AddInactivePlayer(Original, Leaving);
	TestEqual(TEXT("반복 이탈 등록도 원본 하나만 유지"), GameMode->InactivePlayerArray.Num(), 1);
	TestTrue(TEXT("복제본 대신 같은 PlayerState를 보관"), GameMode->InactivePlayerArray.Contains(Original));
	TestTrue(TEXT("원본 비활성 표식"), Original->IsInactive());
	TestEqual(TEXT("세션 동안 시간 만료 없음"), Original->GetLifeSpan(), 0.f);
	TestNull(TEXT("끊긴 Controller 소유권 제거"), Original->GetOwner());
	TestFalse(TEXT("접속자 목록에서 원본 제거"), GameState->PlayerArray.Contains(Original));
	Leaving->SetPlayerState(nullptr);
	Original->OnDeactivated();
	if (!TestTrue(TEXT("CleanupPlayerState 후에도 원본 생존"), IsValid(Original)))
	{
		return false;
	}
	FDigetherWalletState LatestWallet;
	LatestWallet.BalanceCredits = 9;
	LatestWallet.LastRunPayoutCredits = 23;
	LatestWallet.HiddenLifetimeCredits = 31;
	LatestWallet.TotalDeathLossCredits = 97;
	LatestWallet.LastPreDeathBalanceCredits = 106;
	TestEqual(TEXT("끊긴 뒤 장부 갱신"), Original->RestoreState(LatestWallet), EDigetherEconomyResult::Success);
	TestEqual(TEXT("끊긴 뒤 사망 반영"), Original->GetLifeComponent()->ApplyDeath(EDigetherDeathCause::Killed),
		EDigetherLifeResult::Success);
	const FDigetherLifeComponentState LatestLife = Original->GetLifeComponent()->CaptureState();
	TestFalse(TEXT("같은 이름·주소라도 무효 ID는 재접속 거절"), GameMode->FindInactivePlayer(Joining));
	Fresh->SetUniqueId(MakeIntegrationNetId(TEXT("fixture-different")));
	TestFalse(TEXT("같은 이름·주소라도 다른 ID는 재접속 거절"), GameMode->FindInactivePlayer(Joining));
	TestEqual(TEXT("거절 뒤 임시 로그인 상태 유지"), Joining->GetPlayerState<ADigetherGameplayPlayerState>(), Fresh);
	TestEqual(TEXT("거절 뒤 비활성 원본 유지"), GameMode->InactivePlayerArray.Num(), 1);
	Fresh->SetUniqueId(OriginalNetId);
	if (!TestTrue(TEXT("동일 유효 ID의 재접속 수락"), GameMode->FindInactivePlayer(Joining)))
	{
		return false;
	}
	TestEqual(TEXT("재접속은 같은 원본 PlayerState 연결"), Joining->GetPlayerState<ADigetherGameplayPlayerState>(), Original);
	TestEqual(TEXT("원본 소유권은 새 Controller"), Original->GetOwner(), static_cast<AActor*>(Joining));
	TestFalse(TEXT("재접속 뒤 inactive 해제"), Original->IsInactive());
	TestTrue(TEXT("재접속 뒤 상태 복제 재개"), Original->GetIsReplicated());
	TestTrue(TEXT("재접속자 목록에 원본 추가"), GameState->PlayerArray.Contains(Original));
	TestTrue(TEXT("비활성 레지스트리에서 원본 제거"), GameMode->InactivePlayerArray.IsEmpty());
	TestEqual(TEXT("서버 GUID 유지"), Original->GetGameplayPlayerId(), GameplayId);
	TestTrue(TEXT("초기 지급 완료 표식 유지"), Original->IsGameplayInitialized());
	const FDigetherWalletState RestoredWallet = Original->CaptureState();
	const FDigetherLifeComponentState RestoredLife = Original->GetLifeComponent()->CaptureState();
	TestTrue(TEXT("최신 장부 전체를 초기 지급 없이 유지"),
		FDigetherWalletState::StaticStruct()->CompareScriptStruct(&LatestWallet, &RestoredWallet, 0));
	TestTrue(TEXT("끊긴 동안의 사망 상태 전체 유지"),
		FDigetherLifeComponentState::StaticStruct()->CompareScriptStruct(&LatestLife, &RestoredLife, 0));
	TestEqual(TEXT("기존 Pawn도 같은 원본 상태를 계속 참조"), Pawn->GetPlayerState(), static_cast<APlayerState*>(Original));
	TestEqual(TEXT("재빙의할 Pawn이 같은 개체"), Original->GetGameplayPawn(), Pawn);
	TestTrue(TEXT("재빙의 전 pending 표식"), FDigetherFrameworkSaveIntegrationAccess::IsReconnectPending(*GameMode, Original));
	GameMode->HandleStartingNewPlayer_Implementation(Joining);
	TestTrue(TEXT("실제 시작 훅이 새 Pawn 생성 없이 기존 Pawn 재빙의"), Joining->GetPawn() == Pawn);
	TestFalse(TEXT("재빙의 성공 후 pending 표식 소비"), FDigetherFrameworkSaveIntegrationAccess::IsReconnectPending(*GameMode, Original));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkHubInactiveDepartureTest,
	"DIGETHER.Game.Framework.SaveIntegration.HubInactiveExcludedFromDepartureAndSettlement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkHubInactiveDepartureTest::RunTest(const FString& Parameters)
{
	for (const bool bRetainPawn : { true, false })
	{
		FDigetherSaveIntegrationWorld Fixture(this);
		if (!TestTrue(TEXT("거점 비활성 출발 시험 월드"), Fixture.IsReady()))
		{
			return false;
		}
		ADigetherElevatorActor* Elevator = PrepareIntegrationRun(Fixture);
		ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
		ADigetherGameplayPlayerController* Host = MakeRuntimeIntegrationPlayer(Fixture, MakeRuntimeIntegrationNetId(), true);
		const FUniqueNetIdRepl OfflineId = MakeRuntimeIntegrationNetId();
		ADigetherGameplayPlayerController* Leaving = MakeRuntimeIntegrationPlayer(Fixture, OfflineId);
		if (!TestNotNull(TEXT("출발용 엘리베이터"), Elevator) || !TestNotNull(TEXT("호스트"), Host)
			|| !TestNotNull(TEXT("거점 이탈자"), Leaving))
		{
			return false;
		}
		APawn* HostPawn = GiveIntegrationPawn(Fixture, Host, FVector(0.0, 0.0, 200.0));
		APawn* OfflinePawn = bRetainPawn ? GiveIntegrationPawn(Fixture, Leaving, FVector(500.0, 0.0, 200.0)) : nullptr;
		if (!TestNotNull(TEXT("호스트 출발 Pawn"), HostPawn) || (bRetainPawn && !TestNotNull(TEXT("거점 보존 Pawn"), OfflinePawn)))
		{
			return false;
		}
		ADigetherGameplayPlayerState* Offline = Leaving->GetPlayerState<ADigetherGameplayPlayerState>();
		FDigetherWalletState Wallet;
		Wallet.BalanceCredits = 80;
		Wallet.LastRunPayoutCredits = 19;
		Offline->RestoreState(Wallet);
		const FDigetherLifeComponentState Life = Offline->GetLifeComponent()->CaptureState();
		const FVector OfflineLocation = IsValid(OfflinePawn) ? OfflinePawn->GetActorLocation() : FVector::ZeroVector;
		DisconnectIntegrationPlayer(*Mode, *Leaving);
		if (IsValid(OfflinePawn))
		{
			OfflinePawn->SetActorLocation(Elevator->GetBoardingVolume()->GetComponentLocation());
			UpdateIntegrationOverlaps(*Elevator, OfflinePawn);
			TArray<APawn*> Overlapping;
			Elevator->GetOverlappingPawns(Overlapping);
			TestTrue(TEXT("거점 inactive Pawn의 실제 탑승 overlap 구성"), Overlapping.Contains(OfflinePawn));
			FDigetherElevatorPullContext Context;
			if (TestTrue(TEXT("거점 대기 탑승 컨텍스트 구성"), FDigetherFrameworkSaveIntegrationAccess::PullContext(*Mode, Context)))
			{
				TestTrue(TEXT("거점 inactive는 출발 전 탑승 명단에서도 제외"), Context.LoadEntries.IsEmpty());
			}
			OfflinePawn->SetActorLocation(OfflineLocation);
			UpdateIntegrationOverlaps(*Elevator, OfflinePawn);
		}
		FDigetherFrameworkSaveIntegrationAccess::Depart(*Mode);
		if (!TestTrue(bRetainPawn ? TEXT("거점 inactive Pawn이 있어도 실제 출발 성공") : TEXT("거점 inactive Pawn이 없어도 실제 출발 성공"),
			Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase == EDigetherRunPhase::InProgress))
		{
			return false;
		}
		TestEqual(TEXT("실제 출발 인원은 연결자 한 명"), FDigetherFrameworkSaveIntegrationAccess::DepartureCount(*Mode), 1);
		TestFalse(TEXT("거점 inactive는 런 참가자로 캡처하지 않음"), FDigetherFrameworkSaveIntegrationAccess::IsParticipant(*Mode, Offline));
		if (IsValid(OfflinePawn))
		{
			TestTrue(TEXT("출발은 보존 Pawn을 거점에 유지"), OfflinePawn->GetActorLocation().Equals(OfflineLocation));
			// 보존 Pawn을 실제 겹침에 넣어도 출발에 참여하지 않은 ID는 탑승 명단에서 제외한다.
			OfflinePawn->SetActorLocation(Elevator->GetBoardingVolume()->GetComponentLocation());
			UpdateIntegrationOverlaps(*Elevator, OfflinePawn);
			UpdateIntegrationOverlaps(*Elevator, HostPawn);
			TArray<APawn*> Overlapping;
			Elevator->GetOverlappingPawns(Overlapping);
			TestTrue(TEXT("보존 Pawn은 실제 엘리베이터 overlap에 있음"), Overlapping.Contains(OfflinePawn));
			FDigetherElevatorPullContext Context;
			if (TestTrue(TEXT("탐사 탑승 컨텍스트 구성"), FDigetherFrameworkSaveIntegrationAccess::PullContext(*Mode, Context)))
			{
				TestEqual(TEXT("탑승 명단도 출발 참가자 한 명"), Context.LoadEntries.Num(), 1);
			}
		}
		FDigetherFrameworkSaveIntegrationAccess::Finish(*Mode);
		TestTrue(TEXT("실제 귀환으로 탐사 종료"), Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase != EDigetherRunPhase::InProgress);
		TestTrue(TEXT("거점 inactive 장부는 런 정산과 사망 손실에서 제외"), IntegrationStatesEqual(Offline->CaptureState(), Wallet));
		TestTrue(TEXT("거점 inactive 생존 상태는 런 종료 영향 없음"), IntegrationStatesEqual(Offline->GetLifeComponent()->CaptureState(), Life));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkDisconnectedRunSettlementTest,
	"DIGETHER.Game.Framework.SaveIntegration.DisconnectedRunParticipantSettlementAndLoss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkDisconnectedRunSettlementTest::RunTest(const FString& Parameters)
{
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("끊긴 런 참가자 정산 월드"), Fixture.IsReady()))
	{
		return false;
	}
	ADigetherElevatorActor* Elevator = PrepareIntegrationRun(Fixture);
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	ADigetherGameplayPlayerController* Host = MakeRuntimeIntegrationPlayer(Fixture, MakeRuntimeIntegrationNetId(), true);
	ADigetherGameplayPlayerController* Leaving = MakeRuntimeIntegrationPlayer(Fixture, MakeRuntimeIntegrationNetId());
	if (!TestNotNull(TEXT("정산 엘리베이터"), Elevator) || !TestNotNull(TEXT("정산 호스트"), Host)
		|| !TestNotNull(TEXT("끊길 탐사 참가자"), Leaving))
	{
		return false;
	}
	APawn* HostPawn = GiveIntegrationPawn(Fixture, Host, FVector(0.0, 0.0, 200.0));
	APawn* Pawn = GiveIntegrationPawn(Fixture, Leaving, FVector(500.0, 0.0, 200.0));
	if (!TestNotNull(TEXT("정산 호스트 Pawn"), HostPawn) || !TestNotNull(TEXT("정산 참가자 Pawn"), Pawn))
	{
		return false;
	}
	ADigetherGameplayPlayerState* Player = Leaving->GetPlayerState<ADigetherGameplayPlayerState>();
	FDigetherWalletState Wallet;
	Wallet.BalanceCredits = 80;
	Wallet.LastRunPayoutCredits = 19;
	Player->RestoreState(Wallet);
	FDigetherFrameworkSaveIntegrationAccess::Depart(*Mode);
	if (!TestTrue(TEXT("두 명의 실제 출발 성공"), Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase == EDigetherRunPhase::InProgress))
	{
		return false;
	}
	DisconnectIntegrationPlayer(*Mode, *Leaving);
	TestTrue(TEXT("탐사 중 inactive도 기존 RunParticipants에 포함"), FDigetherFrameworkSaveIntegrationAccess::IsParticipant(*Mode, Player));
	Pawn->SetActorLocation(FVector(500.0, 0.0, 200.0));
	TestEqual(TEXT("실제 운행 컴포넌트의 광산 도착"),
		Elevator->GetElevatorComponent()->AdvanceTravel(MakeIntegrationTuning().ElevatorTravelSeconds), EDigetherElevatorResult::Success);
	TestTrue(TEXT("실종 사망 판정 전 광산 정지 상태"),
		Elevator->GetElevatorComponent()->GetPublicState().Phase == EDigetherElevatorPhase::BottomWaiting);
	UpdateIntegrationOverlaps(*Elevator, Pawn);
	UpdateIntegrationOverlaps(*Elevator, HostPawn);
	FDigetherFrameworkSaveIntegrationAccess::Finish(*Mode);
	const FDigetherWalletState Settled = Player->CaptureState();
	TestTrue(TEXT("실제 런 귀환 완료"), Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase != EDigetherRunPhase::InProgress);
	TestEqual(TEXT("끊긴 참가자의 실종 사망 손실 적용"), Settled.BalanceCredits, 40);
	TestEqual(TEXT("끊긴 참가자의 누적 손실 적용"), Settled.TotalDeathLossCredits, 40);
	TestEqual(TEXT("끊긴 참가자의 사망 직전 잔고 기록"), Settled.LastPreDeathBalanceCredits, 80);
	TestEqual(TEXT("끊긴 참가자도 런 정산하여 이전 지급 장부 갱신"), Settled.LastRunPayoutCredits, 0);
	TestTrue(TEXT("귀환 후 동일 비활성 상태 부활"), Player->IsInactive()
		&& Player->GetLifeComponent()->CaptureState().PublicState.Life == EDigetherLifeState::Active);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkLoginReservationsTest,
	"DIGETHER.Game.Framework.SaveIntegration.LoginReservesFourSlotsAndRechecksAdmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkLoginReservationsTest::RunTest(const FString& Parameters)
{
	for (const bool bInactiveReservation : { true, false })
	{
		FDigetherSaveIntegrationWorld Fixture(this);
		if (!TestTrue(TEXT("실제 로그인 정원 시험 월드"), Fixture.IsReady()))
		{
			return false;
		}
		ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
		Fixture.GetWorld()->SpawnActor<APlayerStart>();
		for (int32 Index = 0; Index < 3; ++Index)
		{
			if (!TestNotNull(TEXT("기존 활성 연결"), MakeRuntimeIntegrationPlayer(Fixture, MakeRuntimeIntegrationNetId())))
			{
				return false;
			}
		}
		const FUniqueNetIdRepl FirstId = MakeRuntimeIntegrationNetId();
		const FUniqueNetIdRepl SecondId = MakeRuntimeIntegrationNetId();
		ADigetherGameplayPlayerState* Original = nullptr;
		if (bInactiveReservation)
		{
			ADigetherGameplayPlayerController* Leaving = MakeRuntimeIntegrationPlayer(Fixture, FirstId);
			if (!TestNotNull(TEXT("네 번째 자리 예약자"), Leaving))
			{
				return false;
			}
			Original = Leaving->GetPlayerState<ADigetherGameplayPlayerState>();
			DisconnectIntegrationPlayer(*Mode, *Leaving);
			FString FullError;
			Mode->PreLogin(FString(), FString(), SecondId, FullError);
			TestFalse(TEXT("활성 3+비활성 1은 신규 PreLogin 거절"), FullError.IsEmpty());
			FullError.Reset();
			TestNull(TEXT("활성 3+비활성 1은 실제 신규 Login 거절"),
				Mode->Login(nullptr, ROLE_AutonomousProxy, FString(), FString(), SecondId, FullError));
			TestFalse(TEXT("예약 정원 거절 이유 반환"), FullError.IsEmpty());
		}
		FString FirstError;
		FString SecondError;
		Mode->PreLogin(FString(), FString(), FirstId, FirstError);
		TestTrue(bInactiveReservation ? TEXT("정원 full이어도 예약 ID PreLogin 허용") : TEXT("첫 번째 동시 PreLogin 승인"), FirstError.IsEmpty());
		if (!bInactiveReservation)
		{
			Mode->PreLogin(FString(), FString(), SecondId, SecondError);
			TestTrue(TEXT("같은 빈자리에 두 번째 PreLogin도 먼저 승인"), SecondError.IsEmpty());
		}
		APlayerController* Accepted = Mode->Login(nullptr, ROLE_AutonomousProxy, FString(), FString(), FirstId, FirstError);
		if (!TestNotNull(bInactiveReservation ? TEXT("예약 ID의 실제 Login 수락") : TEXT("먼저 처리한 실제 Login 수락"), Accepted))
		{
			AddError(FirstError);
			return false;
		}
		if (bInactiveReservation)
		{
			TestEqual(TEXT("Login 반환 시 원본 상태가 이미 연결됨"), Accepted->GetPlayerState<ADigetherGameplayPlayerState>(), Original);
			TestTrue(TEXT("PostLogin 전에도 예약을 소비하여 중복 인원 없음"), Mode->InactivePlayerArray.IsEmpty());
		}
		TestEqual(TEXT("PostLogin 전 활성 명단은 네 명"), Fixture.GetGameState()->PlayerArray.Num(), 4);
		SecondError.Reset();
		TestNull(TEXT("연속 Login 재검사로 다섯 번째 유효 ID 거절"),
			Mode->Login(nullptr, ROLE_AutonomousProxy, FString(), FString(), SecondId, SecondError));
		TestFalse(TEXT("연속 Login 정원 거절 이유"), SecondError.IsEmpty());
		FString LocalError;
		TestNull(TEXT("무효 ID의 로컬 Login도 다섯 번째 자리 거절"),
			Mode->Login(nullptr, ROLE_SimulatedProxy, FString(), FString(), FUniqueNetIdRepl(), LocalError));
		TestFalse(TEXT("무효 ID 로컬 정원 거절 이유"), LocalError.IsEmpty());
		TestEqual(TEXT("거절 뒤 실제 총 명단은 네 명 유지"),
			Fixture.GetGameState()->PlayerArray.Num() + Mode->InactivePlayerArray.Num(), 4);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkStoredRecordAdmissionTest,
	"DIGETHER.Game.Framework.SaveIntegration.StoredRecordOnlyJoinsAtHubWithoutRepeatedGrant",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkStoredRecordAdmissionTest::RunTest(const FString& Parameters)
{
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("저장 명단 전용 ID 시험 월드"), Fixture.IsReady()))
	{
		return false;
	}
	ADigetherElevatorActor* Elevator = PrepareIntegrationRun(Fixture);
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	ADigetherGameplayPlayerController* Host = MakeRuntimeIntegrationPlayer(Fixture, MakeRuntimeIntegrationNetId(), true);
	if (!TestNotNull(TEXT("저장 명단 시험 엘리베이터"), Elevator) || !TestNotNull(TEXT("저장 명단 시험 호스트"), Host)
		|| !TestNotNull(TEXT("저장 명단 호스트 Pawn"), GiveIntegrationPawn(Fixture, Host, FVector(0.0, 0.0, 200.0))))
	{
		return false;
	}
	const FUniqueNetIdRepl StoredId = MakeRuntimeIntegrationNetId();
	FDigetherSavePlayerRecord Record;
	DigetherSaveRules::MakeAuthenticatedPlayerKey(StoredId, Record.AuthenticatedPlayerKey);
	Record.GameplayPlayerId = FGuid::NewGuid();
	Record.Wallet.BalanceCredits = 89;
	Record.Wallet.LastRunPayoutCredits = 17;
	FDigetherFrameworkSaveIntegrationAccess::AddRecord(*Mode, Record);
	FDigetherFrameworkSaveIntegrationAccess::Depart(*Mode);
	if (!TestTrue(TEXT("저장 명단 전용 ID를 남겨 실제 출발"), Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase == EDigetherRunPhase::InProgress))
	{
		return false;
	}
	FString Error;
	Mode->PreLogin(FString(), FString(), StoredId, Error);
	TestFalse(TEXT("저장 명단만 있는 ID는 탐사 중 PreLogin 거절"), Error.IsEmpty());
	Error.Reset();
	TestNull(TEXT("저장 명단만 있는 ID는 탐사 중 실제 Login 거절"),
		Mode->Login(nullptr, ROLE_AutonomousProxy, FString(), FString(), StoredId, Error));
	FDigetherFrameworkSaveIntegrationAccess::Finish(*Mode);
	Error.Reset();
	APlayerController* Joining = Mode->Login(nullptr, ROLE_AutonomousProxy, FString(), FString(), StoredId, Error);
	if (!TestNotNull(TEXT("거점 귀환 후 저장 명단 ID 실제 Login 허용"), Joining))
	{
		AddError(Error);
		return false;
	}
	Mode->HandleStartingNewPlayer_Implementation(Joining);
	ADigetherGameplayPlayerState* Player = Joining->GetPlayerState<ADigetherGameplayPlayerState>();
	if (!TestNotNull(TEXT("저장 명단 ID 복원 상태"), Player))
	{
		return false;
	}
	TestTrue(TEXT("실제 시작 훅이 저장 장부 전체 복원"), IntegrationStatesEqual(Player->CaptureState(), Record.Wallet));
	TestEqual(TEXT("저장 GUID를 새 지급 없이 복원"), Player->GetGameplayPlayerId(), Record.GameplayPlayerId);
	TestTrue(TEXT("저장 명단 복원은 지급 완료로 표시"), Player->IsGameplayInitialized());
	TestTrue(TEXT("거점 복원 후 일반 Pawn 스폰"), Joining->GetPawn() != nullptr);
	FDigetherWalletState Latest = Player->CaptureState();
	Latest.BalanceCredits = 123;
	Player->RestoreState(Latest);
	Mode->HandleStartingNewPlayer_Implementation(Joining);
	TestTrue(TEXT("반복 시작 훅은 초기 지급·과거 저장 복원을 반복하지 않음"), IntegrationStatesEqual(Player->CaptureState(), Latest));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkReconnectWithoutPawnTest,
	"DIGETHER.Game.Framework.SaveIntegration.ReconnectWithoutPawnUsesNormalSpawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkReconnectWithoutPawnTest::RunTest(const FString& Parameters)
{
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("보존 Pawn 없는 재접속 월드"), Fixture.IsReady())
		|| !TestNotNull(TEXT("일반 스폰 준비"), PrepareIntegrationRun(Fixture)))
	{
		return false;
	}
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	const FUniqueNetIdRepl Id = MakeRuntimeIntegrationNetId();
	ADigetherGameplayPlayerController* Leaving = MakeRuntimeIntegrationPlayer(Fixture, Id);
	if (!TestNotNull(TEXT("Pawn 없는 기존 연결"), Leaving))
	{
		return false;
	}
	ADigetherGameplayPlayerState* Original = Leaving->GetPlayerState<ADigetherGameplayPlayerState>();
	DisconnectIntegrationPlayer(*Mode, *Leaving);
	FString Error;
	APlayerController* Joining = Mode->Login(nullptr, ROLE_AutonomousProxy, FString(), FString(), Id, Error);
	if (!TestNotNull(TEXT("Pawn 없는 기존 상태의 실제 재접속 Login"), Joining))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("시작 훅 전 재접속 표식"), FDigetherFrameworkSaveIntegrationAccess::IsReconnectPending(*Mode, Original));
	Mode->HandleStartingNewPlayer_Implementation(Joining);
	TestTrue(TEXT("실제 시작 훅은 보존 Pawn이 없으면 일반 스폰"), Joining->GetPawn() != nullptr);
	TestFalse(TEXT("보존 Pawn이 없어도 pending 표식 제거"), FDigetherFrameworkSaveIntegrationAccess::IsReconnectPending(*Mode, Original));
	TestTrue(TEXT("이후 엔진 재시작 요청을 표식이 영구 차단하지 않음"), Mode->PlayerCanRestart_Implementation(Joining));
	APawn* FirstPawn = Joining->GetPawn();
	if (IsValid(FirstPawn))
	{
		Joining->UnPossess();
		FirstPawn->Destroy();
		Mode->HandleStartingNewPlayer_Implementation(Joining);
		TestTrue(TEXT("재시도 시작 훅도 일반 Pawn 스폰 가능"), IsValid(Joining->GetPawn()) && Joining->GetPawn() != FirstPawn);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkHubReconnectNextDepartureTest,
	"DIGETHER.Game.Framework.SaveIntegration.HubReconnectJoinsNextDeparture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkHubReconnectNextDepartureTest::RunTest(const FString& Parameters)
{
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("거점 이탈 후 탐사 재접속 월드"), Fixture.IsReady()))
	{
		return false;
	}
	ADigetherElevatorActor* Elevator = PrepareIntegrationRun(Fixture);
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	ADigetherGameplayPlayerController* Host = MakeRuntimeIntegrationPlayer(Fixture, MakeRuntimeIntegrationNetId(), true);
	const FUniqueNetIdRepl Id = MakeRuntimeIntegrationNetId();
	ADigetherGameplayPlayerController* Leaving = MakeRuntimeIntegrationPlayer(Fixture, Id);
	if (!TestNotNull(TEXT("다음 출발 시험 엘리베이터"), Elevator) || !TestNotNull(TEXT("다음 출발 호스트"), Host)
		|| !TestNotNull(TEXT("다음 출발 거점 이탈자"), Leaving))
	{
		return false;
	}
	APawn* HostPawn = GiveIntegrationPawn(Fixture, Host, FVector(0.0, 0.0, 200.0));
	APawn* Pawn = GiveIntegrationPawn(Fixture, Leaving, FVector(500.0, 0.0, 200.0));
	if (!TestNotNull(TEXT("다음 출발 호스트 Pawn"), HostPawn) || !TestNotNull(TEXT("거점에 보존할 Pawn"), Pawn))
	{
		return false;
	}
	ADigetherGameplayPlayerState* Original = Leaving->GetPlayerState<ADigetherGameplayPlayerState>();
	FDigetherWalletState Wallet;
	Wallet.BalanceCredits = 80;
	Wallet.LastRunPayoutCredits = 19;
	Original->RestoreState(Wallet);
	DisconnectIntegrationPlayer(*Mode, *Leaving);
	FDigetherFrameworkSaveIntegrationAccess::Depart(*Mode);
	if (!TestTrue(TEXT("거점 이탈자를 제외한 실제 출발"), Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase == EDigetherRunPhase::InProgress))
	{
		return false;
	}
	FString Error;
	APlayerController* Joining = Mode->Login(nullptr, ROLE_AutonomousProxy, FString(), FString(), Id, Error);
	if (!TestNotNull(TEXT("탐사 중에도 거점 이탈 예약 ID 실제 Login 허용"), Joining))
	{
		AddError(Error);
		return false;
	}
	Mode->HandleStartingNewPlayer_Implementation(Joining);
	TestTrue(TEXT("실제 시작 훅이 같은 거점 Pawn 재빙의"), Joining->GetPawn() == Pawn);
	TestFalse(TEXT("탐사 중 재빙의 성공 후 pending 표식 소비"), FDigetherFrameworkSaveIntegrationAccess::IsReconnectPending(*Mode, Original));
	TestTrue(TEXT("재접속에 초기 지급·장부 변경 없음"), IntegrationStatesEqual(Original->CaptureState(), Wallet));
	TestFalse(TEXT("재접속해도 진행 중 런 참가자로 소급하지 않음"), FDigetherFrameworkSaveIntegrationAccess::IsParticipant(*Mode, Original));
	FDigetherFrameworkSaveIntegrationAccess::Finish(*Mode);
	TestTrue(TEXT("이번 런 귀환의 정산도 거점 이탈자에게 적용하지 않음"), IntegrationStatesEqual(Original->CaptureState(), Wallet));
	FDigetherFrameworkSaveIntegrationAccess::Depart(*Mode);
	TestTrue(TEXT("다음 실제 출발에 재접속자를 포함"), Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase == EDigetherRunPhase::InProgress
		&& FDigetherFrameworkSaveIntegrationAccess::IsParticipant(*Mode, Original));
	TestEqual(TEXT("다음 출발 인원에 재접속자 포함"), FDigetherFrameworkSaveIntegrationAccess::DepartureCount(*Mode), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkFutureSaveNewGameTest,
	"DIGETHER.Game.Framework.SaveIntegration.FutureSaveRejectsNewGameAndNotifiesHost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkFutureSaveNewGameTest::RunTest(const FString& Parameters)
{
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("미래 저장 새 게임 거절 시험 월드"), Fixture.IsReady())
		|| !TestNotNull(TEXT("미래 저장 시험 런 준비"), PrepareIntegrationRun(Fixture)))
	{
		return false;
	}
	const FUniqueNetIdRepl HostId = MakeRuntimeIntegrationNetId();
	ADigetherGameplayPlayerController* Host = MakeRuntimeIntegrationPlayer(Fixture, HostId, true);
	if (!TestNotNull(TEXT("미래 저장 시험 호스트"), Host)
		|| !TestNotNull(TEXT("미래 저장 시험 호스트 Pawn"), GiveIntegrationPawn(Fixture, Host, FVector(0.0, 0.0, 200.0))))
	{
		return false;
	}
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	FDigetherWalletState ExistingWallet;
	ExistingWallet.BalanceCredits = 67;
	Host->GetPlayerState<ADigetherGameplayPlayerState>()->RestoreState(ExistingWallet);
	FDigetherSaveSnapshot Snapshot;
	FDigetherSaveSlotData Slot;
	TArray<uint8> Payload;
	TArray<uint8> CurrentBytes;
	UDigetherSaveGame* SaveGame = NewObject<UDigetherSaveGame>();
	if (!TestTrue(TEXT("실제 GameMode 진행 캡처"), FDigetherFrameworkSaveIntegrationAccess::Capture(*Mode, Snapshot))
		|| !TestTrue(TEXT("실제 진행으로 기준점 슬롯 구성"), DigetherSaveRules::TryBuildSlot(Snapshot, nullptr, false, true, Slot) == EDigetherSaveResult::Success)
		|| !TestTrue(TEXT("미래 버전 주입용 payload"), DigetherSaveRules::SerializeSlot(Slot, Payload) == EDigetherSaveResult::Success)
		|| !TestTrue(TEXT("엔진 SaveGame 설정"), SaveGame->SetSlotData(Slot) == EDigetherSaveResult::Success)
		|| !TestTrue(TEXT("엔진 헤더 포함 현재 저장 바이트"), UGameplayStatics::SaveGameToMemory(SaveGame, CurrentBytes)))
	{
		return false;
	}
	int32 PayloadOffset = INDEX_NONE;
	int32 Matches = 0;
	for (int32 Index = 0; Index <= CurrentBytes.Num() - Payload.Num(); ++Index)
	{
		if (FMemory::Memcmp(CurrentBytes.GetData() + Index, Payload.GetData(), Payload.Num()) == 0)
		{
			PayloadOffset = Index;
			++Matches;
		}
	}
	if (!TestEqual(TEXT("미래 버전 주입 위치가 하나"), Matches, 1))
	{
		return false;
	}
	TArray<uint8> FutureBytes = CurrentBytes;
	const int32 FutureVersion = DigetherSaveTuning::SchemaVersion + 1;
	FMemory::Memcpy(FutureBytes.GetData() + PayloadOffset, &FutureVersion, sizeof(FutureVersion));
	const FDigetherSaveSlotNames Names = DigetherSaveStorage::MakeSlotNames(Snapshot.HostAuthenticatedPlayerKey);
	if (!TestTrue(TEXT("고유 시험 주 슬롯에 미래 버전 저장"), UGameplayStatics::SaveDataToSlot(FutureBytes, Names.Primary, 0))
		|| !TestTrue(TEXT("고유 시험 백업 슬롯에 현재 버전 저장"), UGameplayStatics::SaveDataToSlot(CurrentBytes, Names.Backup, 0)))
	{
		return false;
	}
	const FDigetherWalletState Before = Host->GetPlayerState<ADigetherGameplayPlayerState>()->CaptureState();
	AddExpectedMessagePlain(TEXT("진행 요청 결과: Succeeded=0 Message=미래 버전 저장이 있어 새 게임을 거절했습니다. 기존 저장은 보존됩니다."),
		ELogVerbosity::Display);
	TestFalse(TEXT("실제 GameMode StartNewGame도 미래 저장 거절"), Mode->StartNewGame(Host));
	TestTrue(TEXT("새 게임 거절은 현재 장부도 보존"), IntegrationStatesEqual(Host->GetPlayerState<ADigetherGameplayPlayerState>()->CaptureState(), Before));
	for (const bool bPrimary : { true, false })
	{
		TArray<uint8> Actual;
		if (TestTrue(TEXT("새 게임 거절 뒤 실제 슬롯 바이트 읽기"), UGameplayStatics::LoadDataFromSlot(Actual, bPrimary ? Names.Primary : Names.Backup, 0)))
		{
			TestTrue(TEXT("새 게임 거절 뒤 주·백업 바이트 보존"), Actual == (bPrimary ? FutureBytes : CurrentBytes));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkSaveCommandContractTest,
	"DIGETHER.Game.Framework.SaveIntegration.CommandContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkSaveCommandContractTest::RunTest(const FString& Parameters)
{
	const TConstArrayView<DigetherDev::FCommandInfo> Commands = DigetherDev::GetPlannedCommands();
	TestEqual(TEXT("저장 명령을 포함한 예정 명령 수"), Commands.Num(), 7);
	const TArray<FName> SaveCommands = { TEXT("DigetherSave"), TEXT("DigetherLoad"), TEXT("DigetherNewGame") };
	for (const FName Name : SaveCommands)
	{
		int32 Count = 0;
		for (const DigetherDev::FCommandInfo& Command : Commands)
		{
			Count += Command.Name == Name ? 1 : 0;
		}
		TestEqual(FString::Printf(TEXT("%s 예약은 한 번"), *Name.ToString()), Count, 1);
		const UFunction* Function = UDigetherCheatManager::StaticClass()->FindFunctionByName(Name);
		if (TestNotNull(FString::Printf(TEXT("%s 함수 등록"), *Name.ToString()), Function))
		{
			TestTrue(FString::Printf(TEXT("%s 콘솔 Exec 선언"), *Name.ToString()), Function->HasAllFunctionFlags(FUNC_Exec));
		}
	}
	const TArray<FName> ServerRequests = { TEXT("Server_ManualSave"), TEXT("Server_LoadGame"), TEXT("Server_NewGame") };
	for (const FName Name : ServerRequests)
	{
		const UFunction* Function = ADigetherGameplayPlayerController::StaticClass()->FindFunctionByName(Name);
		if (TestNotNull(FString::Printf(TEXT("%s 요청 등록"), *Name.ToString()), Function))
		{
			TestTrue(FString::Printf(TEXT("%s reliable 서버 RPC"), *Name.ToString()),
				Function->HasAllFunctionFlags(FUNC_Net | FUNC_NetServer | FUNC_NetReliable));
		}
	}
	const UFunction* Result = ADigetherGameplayPlayerController::StaticClass()->FindFunctionByName(TEXT("Client_SaveResult"));
	if (TestNotNull(TEXT("소유 클라이언트 저장 결과 등록"), Result))
	{
		TestTrue(TEXT("결과는 reliable 클라이언트 RPC"), Result->HasAllFunctionFlags(FUNC_Net | FUNC_NetClient | FUNC_NetReliable));
	}
	return true;
}

#endif
