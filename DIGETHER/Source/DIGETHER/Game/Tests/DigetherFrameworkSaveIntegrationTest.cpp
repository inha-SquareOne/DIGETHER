#if WITH_DEV_AUTOMATION_TESTS

// 아래 숫자·ID는 테스트 픽스처, 기획 값 아님

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
#include "Game/Elevator/DigetherElevatorLeverActor.h"
#include "Game/Elevator/DigetherElevatorLeverComponent.h"
#include "Game/Elevator/DigetherElevatorRules.h"
#include "Game/Dev/DigetherCheatManager.h"
#include "Game/Dev/DigetherDevCommands.h"
#include "Game/Framework/DigetherFrameworkDefaults.h"
#include "Game/Framework/DigetherFrameworkLog.h"
#include "Game/Framework/DigetherGameplayGameMode.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Framework/DigetherTestPawn.h"
#include "Game/Hub/DigetherHubWellActor.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Run/DigetherGameOverFlowComponent.h"
#include "Game/Run/DigetherGameOverPublicStateComponent.h"
#include "Game/Run/DigetherGameOverRules.h"
#include "Game/Online/DigetherJoinRules.h"
#include "Game/Online/DigetherSessionText.h"
#include "Game/Run/DigetherRunCoordinatorComponent.h"
#include "Game/Save/DigetherSaveGame.h"
#include "Game/Save/DigetherSaveRules.h"
#include "Game/Save/DigetherSaveStorage.h"
#include "Game/Save/DigetherSaveSubsystem.h"
#include "Game/Save/DigetherSaveTuning.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/GameState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineSubsystem.h"
#include "Templates/Function.h"
#include "TimerManager.h"
#include "UObject/Class.h"
#include "UObject/EnumProperty.h"
#include "UObject/UnrealType.h"

// 테스트 준비·관찰만 노출, 출발·정산·로그인·재빙의는 제품 함수 직접 호출
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
		// 월드 BeginPlay 없이 엔진 스폰 조건만 준비, 탐사 phase는 실제 ProcessDeparture가 변경
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
	// 게임 맵 시작(StartPlay·PostLogin)의 저장 분기와 같은 함수, 새 게임 시도 시 true
	static bool RunStartupSaveAction(ADigetherGameplayGameMode& Mode, ADigetherGameplayPlayerController* Host)
	{
		return Mode.RunStartupSaveAction(Host);
	}
	// 맵 배치와 같은 레버·신호 연결을 제품 함수로 생성
	static bool InitializeRunActors(ADigetherGameplayGameMode& Mode) { return Mode.InitializeRunActors(); }
	// 레버 게이지 완료 처리와 같은 진입점, 레버 없는 문 닫힘 실패 경로 검사용
	static void BeginDoorClose(ADigetherGameplayGameMode& Mode, const EDigetherElevatorLeverKind Kind)
	{
		Mode.BeginDoorClose(Kind);
	}
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
	static void AddRecord(ADigetherGameplayGameMode& Mode, const FDigetherSavePlayerRecord& Record)
	{
		Mode.SavedPlayerRecords.Add(Record);
	}
	static bool Capture(const ADigetherGameplayGameMode& Mode, FDigetherSaveSnapshot& Out)
	{
		return Mode.CaptureProgress(Out);
	}
	static bool Restore(ADigetherGameplayGameMode& Mode, const FDigetherSaveSnapshot& Snapshot,
		const bool bValidateOnly = false)
	{
		return Mode.RestoreProgress(Snapshot, bValidateOnly);
	}
	// 출발 소집·이동 없이 런 단계만 진행 중으로 설정, 거점 이탈(자리 반환)과 탐사 중 이탈(보존) 구분 기준만 필요한
	// 시험용, 실제 출발 인원·정산은 Depart로 검사
	static bool ForceRunInProgress(ADigetherGameplayGameMode& Mode)
	{
		const ADigetherGameplayGameState* State = Mode.GetGameState<ADigetherGameplayGameState>();
		if (!IsValid(State))
		{
			return false;
		}
		Mode.RunCoordinatorComponent->HandleDepartureLeverCompleted(State->GetQuotaComponent()->CaptureState().Quota);
		return Mode.RunCoordinatorComponent->StartRun(Mode.RunTuning, Mode.FixedRunRuleTuning,
				Mode.GetWorld()->GetTimeSeconds(), false) == EDigetherRunResult::Success
			&& Mode.RunCoordinatorComponent->CapturePublicState().Phase == EDigetherRunPhase::InProgress;
	}
	static bool IsJoinSpectator(const ADigetherGameplayGameMode& Mode, const APlayerController* Controller)
	{
		return Mode.IsJoinSpectator(Controller);
	}
	static bool FindSavedRecord(const ADigetherGameplayGameMode& Mode, const FUniqueNetIdRepl& Id,
		FDigetherSavePlayerRecord& OutRecord)
	{
		bool bSkipInitialGrant = false;
		return DigetherSaveRules::TryResolveReturningPlayer(Mode.SavedPlayerRecords, Id, OutRecord, bSkipInitialGrant)
			== EDigetherSaveResult::Success;
	}
	// 시험 컨트롤러는 실제 연결이 없어 타이머 대신 같은 판정 함수를 원하는 시각으로 호출
	static void PollLoginReplacementsAt(ADigetherGameplayGameMode& Mode, const double NowSeconds)
	{
		Mode.PollLoginReplacementsAt(NowSeconds);
	}
	static int32 PendingLoginReplacements(const ADigetherGameplayGameMode& Mode)
	{
		return Mode.LoginReplacements.Num();
	}
	// 자동 저장 시점(할당량 달성·탐사 정상 완료)과 같은 제품 함수
	static void RequestAutosave(ADigetherGameplayGameMode& Mode) { Mode.RequestAutosave(TEXT("IntegrationTest")); }
	static bool HasPendingAutosave(const ADigetherGameplayGameMode& Mode)
	{
		return Mode.PendingAutosave.IsSet() || Mode.bAutosaveRecapturePending;
	}
	static int32 PendingAutosaveDay(const ADigetherGameplayGameMode& Mode)
	{
		return Mode.PendingAutosave.IsSet() ? Mode.PendingAutosave->Quota.Quota.Day : INDEX_NONE;
	}
	static bool IsAutosaveRetryScheduled(const ADigetherGameplayGameMode& Mode)
	{
		return Mode.GetWorldTimerManager().IsTimerActive(Mode.AutosaveRetryTimer);
	}
	// 시험 월드는 타이머 미진행, 재시도 타이머가 부르는 함수 직접 호출
	static void RetryAutosave(ADigetherGameplayGameMode& Mode) { Mode.RetryPendingAutosave(); }
	static bool SaveOnHostExit(ADigetherGameplayGameMode& Mode) { return Mode.SaveOnHostExit(); }
	static void BeginWorldTearDown(ADigetherGameplayGameMode& Mode, UWorld* World) { Mode.HandleWorldBeginTearDown(World); }
	// 시험 월드는 시계 정지, 게임 오버 Tick의 같은 함수를 원하는 서버 시각으로 호출
	static void TickGameOverAt(ADigetherGameplayGameMode& Mode, const double NowSeconds) { Mode.TickGameOverAt(NowSeconds); }
	static bool SetCreditFastForwardAt(ADigetherGameplayGameMode& Mode, ADigetherGameplayPlayerController* Requester,
		const bool bFastForward, const double NowSeconds)
	{
		return Mode.SetCreditFastForwardAt(Requester, bFastForward, NowSeconds);
	}
	static bool CreateNextGameAfterCredits(ADigetherGameplayGameMode& Mode) { return Mode.CreateNextGameAfterCredits(); }
	static bool IsNewJoinLocked(const ADigetherGameplayGameMode& Mode) { return Mode.IsNewJoinLockedNow(); }
	static FGuid GetGameId(const ADigetherGameplayGameMode& Mode) { return Mode.GameId; }
	static const TArray<FDigetherGameResult>& GetArchivedResults(const ADigetherGameplayGameMode& Mode)
	{
		return Mode.ArchivedGameResults;
	}
};

namespace
{
	// 기본 모드는 초기화 전 액터 검사, Test 지정 시 실제 Login·SaveSubsystem용 독립 월드
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
				// 합성 ID를 실제 Steam 제공자에 전달 금지, 이 L2 fixture 실행 조건은 -NoSteam
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
			// 진행 슬롯 선택 시험이 슬롯 1 이상에도 기록하므로 모든 진행 슬롯 주·백업 추적
			TArray<FString> Candidates;
			for (int32 SlotIndex = 0; SlotIndex < DigetherSaveTuning::MaxProgressSlots; ++SlotIndex)
			{
				const FDigetherSaveSlotNames Names = DigetherSaveStorage::MakeSlotNames(Key, SlotIndex);
				Candidates.Add(Names.Primary);
				Candidates.Add(Names.Backup);
			}
			for (const FString& Slot : Candidates)
			{
				if (!Test->TestFalse(TEXT("고유 호스트 ID의 슬롯은 새 시험 전 없음"), UGameplayStatics::DoesSaveGameExist(Slot, 0)))
				{
					// 기존 슬롯은 추적·삭제 안 함, 이 호스트로 저장 시작 안 함
					return false;
				}
			}
			TemporarySlots.Append(Candidates);
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
			// 실제 로그인은 엔진이 겹치지 않는 PlayerId 부여, 직접 만든 시험 플레이어도 동일하게 맞춤
			// (게임 오버 흡입·관전 대상은 PlayerId로 구분)
			static int32 NextIntegrationPlayerId = 1000;
			if (Player->GetPlayerId() == 0)
			{
				Player->SetPlayerId(NextIntegrationPlayerId++);
			}
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

	// 엔진 연결 정리 순서(PawnLeavingGame → Logout의 AddInactivePlayer → CleanupPlayerState의 OnDeactivated) 재현
	// 비활성 보관 PS는 OnDeactivated가 유지, 거점 이탈처럼 보관 안 한 PS는 엔진 기본대로 파괴
	void DisconnectIntegrationPlayer(ADigetherGameplayGameMode& Mode, ADigetherGameplayPlayerController& Controller)
	{
		ADigetherGameplayPlayerState* Player = Controller.GetPlayerState<ADigetherGameplayPlayerState>();
		Controller.PawnLeavingGame();
		Mode.AddInactivePlayer(Player, &Controller);
		if (IsValid(Player))
		{
			Player->OnDeactivated();
		}
		Controller.SetPlayerState(nullptr);
		Controller.Destroy();
	}

	// PreLoginAsync 완료 콜백 관찰용, 월드 종료 정리에서 늦게 불려도 안전하게 공유 상태로 수신
	struct FDigetherAsyncLoginResult
	{
		bool bCompleted = false;
		FString Error;
	};

	AGameModeBase::FOnPreLoginCompleteDelegate MakeAsyncLoginObserver(const TSharedRef<FDigetherAsyncLoginResult>& Result)
	{
		return AGameModeBase::FOnPreLoginCompleteDelegate::CreateLambda([Result](const FString& Error)
		{
			Result->bCompleted = true;
			Result->Error = Error;
		});
	}

	void UpdateIntegrationOverlaps(ADigetherElevatorActor& Elevator, APawn* Pawn)
	{
		// 엔진 UpdateOverlapsImpl은 BeginPlay 전 액터 겹침 미계산, 이 fixture는 월드 BeginPlay가 없어 겹침 대상만 시작
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

	// 맵 배치 인스턴스 전용 설정(EditInstanceOnly)을 배치 레버와 같은 값으로 채움
	ADigetherElevatorLeverActor* SpawnIntegrationLever(FDigetherSaveIntegrationWorld& Fixture,
		ADigetherElevatorActor& Elevator, const EDigetherElevatorLeverKind Kind,
		const EDigetherElevatorLeverPlacement Placement = EDigetherElevatorLeverPlacement::Inside)
	{
		ADigetherElevatorLeverActor* Lever = Fixture.GetWorld()->SpawnActor<ADigetherElevatorLeverActor>();
		FEnumProperty* KindProperty = FindFProperty<FEnumProperty>(ADigetherElevatorLeverActor::StaticClass(), TEXT("LeverKind"));
		FEnumProperty* PlacementProperty = FindFProperty<FEnumProperty>(ADigetherElevatorLeverActor::StaticClass(), TEXT("Placement"));
		FObjectPropertyBase* ElevatorProperty = FindFProperty<FObjectPropertyBase>(
			ADigetherElevatorLeverActor::StaticClass(), TEXT("ElevatorActor"));
		if (!IsValid(Lever) || KindProperty == nullptr || PlacementProperty == nullptr || ElevatorProperty == nullptr)
		{
			return nullptr;
		}
		*KindProperty->ContainerPtrToValuePtr<EDigetherElevatorLeverKind>(Lever) = Kind;
		*PlacementProperty->ContainerPtrToValuePtr<EDigetherElevatorLeverPlacement>(Lever) = Placement;
		ElevatorProperty->SetObjectPropertyValue_InContainer(Lever, &Elevator);
		return Lever;
	}

	// 맵 배치와 같은 엘리베이터 기준 상대 위치, 안쪽 레버는 탑승 영역 안, 밖 레버는 탑승 영역(반폭 250cm) 밖
	const FVector IntegrationDepartureLeverOffsetCm(-100.0, 0.0, 0.0);
	const FVector IntegrationInsideLeverOffsetCm(100.0, 0.0, 0.0);
	const FVector IntegrationOutsideLeverOffsetCm(400.0, 0.0, 0.0);

	struct FDigetherIntegrationLevers
	{
		ADigetherElevatorLeverActor* Departure = nullptr;
		ADigetherElevatorLeverActor* AscentInside = nullptr;
		ADigetherElevatorLeverActor* AscentOutside = nullptr;
		bool IsReady() const { return IsValid(Departure) && IsValid(AscentInside) && IsValid(AscentOutside); }
	};

	// 출발 1·상승 안 1·상승 밖 1을 맵 배치 위치로 생성(기획 S15 레버 구성)
	FDigetherIntegrationLevers SpawnIntegrationLeverSet(FDigetherSaveIntegrationWorld& Fixture, ADigetherElevatorActor& Elevator)
	{
		FDigetherIntegrationLevers Levers;
		const FVector ElevatorLocation = Elevator.GetActorLocation();
		Levers.Departure = SpawnIntegrationLever(Fixture, Elevator, EDigetherElevatorLeverKind::Departure);
		Levers.AscentInside = SpawnIntegrationLever(Fixture, Elevator, EDigetherElevatorLeverKind::Ascent);
		Levers.AscentOutside = SpawnIntegrationLever(Fixture, Elevator, EDigetherElevatorLeverKind::Ascent,
			EDigetherElevatorLeverPlacement::Outside);
		if (IsValid(Levers.Departure)) Levers.Departure->SetActorLocation(ElevatorLocation + IntegrationDepartureLeverOffsetCm);
		if (IsValid(Levers.AscentInside)) Levers.AscentInside->SetActorLocation(ElevatorLocation + IntegrationInsideLeverOffsetCm);
		if (IsValid(Levers.AscentOutside)) Levers.AscentOutside->SetActorLocation(ElevatorLocation + IntegrationOutsideLeverOffsetCm);
		return Levers;
	}

	// 실제 GameMode Tick을 짧은 간격으로 실행, 게이지 완료 Tick과 문 닫힘 완료 Tick 구별
	constexpr float IntegrationTickSeconds = 0.5f;
	// INDEX_NONE는 이름 없는 enum이라 TestNotEqual의 int32 비교와 형 불일치
	constexpr int32 IntegrationTickTimedOut = -1;

	int32 TickIntegrationUntil(ADigetherGameplayGameMode& Mode, const TFunctionRef<bool()> Done, const int32 MaxTicks)
	{
		for (int32 Count = 1; Count <= MaxTicks; ++Count)
		{
			Mode.Tick(IntegrationTickSeconds);
			if (Done())
			{
				return Count;
			}
		}
		return IntegrationTickTimedOut;
	}

	int32 ToIntegrationTicks(const float Seconds)
	{
		return FMath::CeilToInt32(Seconds / IntegrationTickSeconds);
	}

	struct FDigetherDoorIntegrationRun
	{
		ADigetherElevatorActor* Elevator = nullptr;
		ADigetherElevatorLeverActor* DepartureLever = nullptr;
		// 엘리베이터 안쪽 상승 레버
		ADigetherElevatorLeverActor* AscentLever = nullptr;
		// 엘리베이터 밖 상승 레버(기획 S15·T93)
		ADigetherElevatorLeverActor* OutsideAscentLever = nullptr;
		ADigetherGameplayPlayerController* Host = nullptr;
		APawn* HostPawn = nullptr;
	};

	bool PrepareDoorIntegrationRun(FDigetherSaveIntegrationWorld& Fixture, FDigetherDoorIntegrationRun& OutRun)
	{
		OutRun.Elevator = PrepareIntegrationRun(Fixture);
		if (!IsValid(OutRun.Elevator))
		{
			return false;
		}
		// 맵 배치처럼 레버와 조작자를 같은 탑승 공간에 배치, 원점 레버는 출발 후 20m 밖에 남음
		const FDigetherIntegrationLevers Levers = SpawnIntegrationLeverSet(Fixture, *OutRun.Elevator);
		OutRun.DepartureLever = Levers.Departure;
		OutRun.AscentLever = Levers.AscentInside;
		OutRun.OutsideAscentLever = Levers.AscentOutside;
		const FVector ElevatorLocation = OutRun.Elevator->GetActorLocation();
		OutRun.Host = MakeRuntimeIntegrationPlayer(Fixture, MakeRuntimeIntegrationNetId(), true);
		if (!Levers.IsReady() || !IsValid(OutRun.Host)
			|| !FDigetherFrameworkSaveIntegrationAccess::InitializeRunActors(*Fixture.GetGameMode()))
		{
			return false;
		}
		OutRun.HostPawn = GiveIntegrationPawn(Fixture, OutRun.Host, ElevatorLocation + FVector(0.0, 0.0, 100.0));
		return IsValid(OutRun.HostPawn);
	}

	EDigetherElevatorPhase GetIntegrationElevatorPhase(const ADigetherElevatorActor& Elevator)
	{
		return Elevator.GetElevatorComponent()->GetPublicState().Phase;
	}

	// 출발 레버를 실제 요청 경로로 당겨 거점 문 닫힘까지 진행, 거점 문 닫힘 시작 Tick 수 반환
	int32 PullDepartureUntilDoorClosing(ADigetherGameplayGameMode& Mode, const FDigetherDoorIntegrationRun& Run)
	{
		if (!Mode.BeginLeverPull(Run.Host, Run.DepartureLever))
		{
			return IntegrationTickTimedOut;
		}
		const float HoldSeconds = DigetherElevatorRules::CalculateDepartureDuration(
			Mode.GetGameState<ADigetherGameplayGameState>()->GetQuotaComponent()->CaptureState().Quota.Day,
			FDigetherElevatorTuning());
		return TickIntegrationUntil(Mode,
			[&Run]() { return GetIntegrationElevatorPhase(*Run.Elevator) == EDigetherElevatorPhase::DoorClosing; },
			ToIntegrationTicks(HoldSeconds) + 1);
	}

	// 출발 레버 → 거점 문 닫힘 → 출발 → 첫 하강 → 하부 대기까지 실제 경로로 진행
	bool AdvanceDoorIntegrationRunToBottom(ADigetherGameplayGameMode& Mode, const FDigetherDoorIntegrationRun& Run)
	{
		return PullDepartureUntilDoorClosing(Mode, Run) != IntegrationTickTimedOut
			&& TickIntegrationUntil(Mode, [&Mode]()
				{
					return Mode.GetRunCoordinatorComponent()->CapturePublicState().Phase == EDigetherRunPhase::InProgress;
				}, ToIntegrationTicks(FDigetherElevatorTuning().HubDoorCloseSeconds) + 1) != IntegrationTickTimedOut
			&& TickIntegrationUntil(Mode,
				[&Run]() { return GetIntegrationElevatorPhase(*Run.Elevator) == EDigetherElevatorPhase::BottomWaiting; },
				ToIntegrationTicks(MakeIntegrationTuning().ElevatorTravelSeconds) + 1) != IntegrationTickTimedOut;
	}

	// 갱도에서 밖 상승 레버 옆(탑승 영역 밖)으로 이동 후 탑승 겹침 재계산
	void MovePawnToOutsideLever(const FDigetherDoorIntegrationRun& Run, APawn& Pawn)
	{
		Pawn.SetActorLocation(Run.OutsideAscentLever->GetActorLocation() + FVector(0.0, 0.0, 100.0), false, nullptr,
			ETeleportType::TeleportPhysics);
		UpdateIntegrationOverlaps(*Run.Elevator, &Pawn);
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
	// 비활성 PS 보관은 탐사 출발 후 이탈에만 적용(거점 이탈은 자리 반환, S01)
	if (!TestNotNull(TEXT("실제 재빙의 시작 훅용 런 구성"), PrepareIntegrationRun(Fixture))
		|| !TestTrue(TEXT("탐사 진행 중 단계"), FDigetherFrameworkSaveIntegrationAccess::ForceRunInProgress(*GameMode)))
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
	// 같은 표시 이름·주소의 새 연결도 인증 ID가 없거나 다르면 복원 대상 아님
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
	"DIGETHER.Game.Framework.SaveIntegration.HubDepartureReleasesSeatAndRemovesBody",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkHubInactiveDepartureTest::RunTest(const FString& Parameters)
{
	// S01·T77: 거점 퇴장·연결 끊김은 캐릭터 제거·자리 반환, 장부는 저장 명단에 유지
	// 소지 장비를 그 자리에 실물로 남기는 부분은 인벤토리 시스템이 없어 범위 밖(TODO)
	for (const bool bRetainPawn : { true, false })
	{
		FDigetherSaveIntegrationWorld Fixture(this);
		if (!TestTrue(TEXT("거점 이탈 시험 월드"), Fixture.IsReady()))
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
		if (!TestNotNull(TEXT("호스트 출발 Pawn"), HostPawn) || (bRetainPawn && !TestNotNull(TEXT("거점 이탈자 Pawn"), OfflinePawn)))
		{
			return false;
		}
		ADigetherGameplayPlayerState* Offline = Leaving->GetPlayerState<ADigetherGameplayPlayerState>();
		FDigetherWalletState Wallet;
		Wallet.BalanceCredits = 80;
		Wallet.LastRunPayoutCredits = 19;
		Offline->RestoreState(Wallet);
		const FGuid OfflineGameplayId = Offline->GetGameplayPlayerId();
		DisconnectIntegrationPlayer(*Mode, *Leaving);
		TestFalse(TEXT("거점 이탈은 비활성 PS로 자리를 예약하지 않음"), Mode->InactivePlayerArray.Contains(Offline));
		TestFalse(TEXT("거점 이탈자의 PS는 엔진 정리로 파괴됨"), IsValid(Offline));
		TestEqual(TEXT("반환 뒤 활성 명단은 호스트 한 명"), Fixture.GetGameState()->PlayerArray.Num(), 1);
		if (bRetainPawn)
		{
			TestFalse(TEXT("거점 이탈자의 캐릭터 제거"), IsValid(OfflinePawn));
		}
		FDigetherSavePlayerRecord Record;
		if (TestTrue(TEXT("거점 이탈자의 장부를 저장 명단에 남김"),
			FDigetherFrameworkSaveIntegrationAccess::FindSavedRecord(*Mode, OfflineId, Record)))
		{
			TestTrue(TEXT("남긴 장부는 이탈 시점 최신 값"), IntegrationStatesEqual(Record.Wallet, Wallet));
			TestEqual(TEXT("남긴 기록의 서버 GUID 유지"), Record.GameplayPlayerId, OfflineGameplayId);
			TestFalse(TEXT("몸을 제거했으므로 거점 위치는 남기지 않음"), Record.bHasHubTransform);
		}
		FDigetherFrameworkSaveIntegrationAccess::Depart(*Mode);
		if (!TestTrue(TEXT("거점 이탈자를 뺀 실제 출발 성공"),
			Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase == EDigetherRunPhase::InProgress))
		{
			return false;
		}
		TestEqual(TEXT("실제 출발 인원은 연결자 한 명"), FDigetherFrameworkSaveIntegrationAccess::DepartureCount(*Mode), 1);
		FDigetherFrameworkSaveIntegrationAccess::Finish(*Mode);
		TestTrue(TEXT("실제 귀환으로 탐사 종료"), Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase != EDigetherRunPhase::InProgress);
		if (FDigetherFrameworkSaveIntegrationAccess::FindSavedRecord(*Mode, OfflineId, Record))
		{
			TestTrue(TEXT("거점 이탈자 장부는 런 정산·사망 손실에서 제외"), IntegrationStatesEqual(Record.Wallet, Wallet));
		}

		// T77: 반환한 자리를 다른 사람이 채우면 기존 참가자도 정원 초과 입장 불가
		for (int32 Index = 0; Index < 3; ++Index)
		{
			TestNotNull(TEXT("반환된 자리를 채운 참가자"), MakeRuntimeIntegrationPlayer(Fixture, MakeRuntimeIntegrationNetId()));
		}
		FString Error;
		Mode->PreLogin(FString(), FString(), OfflineId, Error);
		TestEqual(TEXT("자리가 찼으면 기존 참가자도 정원 초과"), Error,
			DigetherJoinRules::MakeRejectionMessage(EDigetherSessionText::RoomFull));
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
			// 자리 예약은 탐사 출발 후 끊긴 참가자에게만(거점 이탈은 자리 반환, S01)
			if (!TestNotNull(TEXT("네 번째 자리 예약자"), Leaving)
				|| !TestNotNull(TEXT("예약 시험 런 구성"), PrepareIntegrationRun(Fixture))
				|| !TestTrue(TEXT("탐사 진행 중 단계"), FDigetherFrameworkSaveIntegrationAccess::ForceRunInProgress(*Mode)))
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
	"DIGETHER.Game.Framework.SaveIntegration.StoredRecordSpectatesDuringRunWithoutRepeatedGrant",
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
	// S01: 이 진행의 기존 참가자(저장 명단)가 탐사 중 돌아오면 그날 탐사 제외, 관전 대기
	FString Error;
	Mode->PreLogin(FString(), FString(), StoredId, Error);
	TestTrue(TEXT("저장 명단 ID는 탐사 중 관전 합류로 PreLogin 수락"), Error.IsEmpty());
	APlayerController* Joining = Mode->Login(nullptr, ROLE_AutonomousProxy, FString(), FString(), StoredId, Error);
	if (!TestNotNull(TEXT("저장 명단 ID의 탐사 중 실제 Login 수락"), Joining))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("탐사 중 확정은 관전 합류자"), FDigetherFrameworkSaveIntegrationAccess::IsJoinSpectator(*Mode, Joining));
	Mode->HandleStartingNewPlayer_Implementation(Joining);
	ADigetherGameplayPlayerState* Player = Joining->GetPlayerState<ADigetherGameplayPlayerState>();
	if (!TestNotNull(TEXT("저장 명단 ID 상태"), Player))
	{
		return false;
	}
	TestNull(TEXT("관전 합류자는 탐사 중 몸을 만들지 않음"), Joining->GetPawn());
	TestFalse(TEXT("관전 합류자는 그날 탐사 참가자가 아님"), FDigetherFrameworkSaveIntegrationAccess::IsParticipant(*Mode, Player));

	FDigetherFrameworkSaveIntegrationAccess::Finish(*Mode);
	TestTrue(TEXT("실제 귀환으로 탐사 종료"), Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase != EDigetherRunPhase::InProgress);
	TestFalse(TEXT("귀환하면 관전 합류 해제"), FDigetherFrameworkSaveIntegrationAccess::IsJoinSpectator(*Mode, Joining));
	TestTrue(TEXT("귀환 때 일반 참가자로 몸 생성"), Joining->GetPawn() != nullptr);
	TestTrue(TEXT("저장 장부 전체를 복원하고 정산·시작 지급은 적용하지 않음"), IntegrationStatesEqual(Player->CaptureState(), Record.Wallet));
	TestEqual(TEXT("저장 GUID를 새 지급 없이 복원"), Player->GetGameplayPlayerId(), Record.GameplayPlayerId);
	TestTrue(TEXT("저장 명단 복원은 지급 완료로 표시"), Player->IsGameplayInitialized());
	FDigetherWalletState Latest = Player->CaptureState();
	Latest.BalanceCredits = 123;
	Player->RestoreState(Latest);
	Mode->HandleStartingNewPlayer_Implementation(Joining);
	TestTrue(TEXT("반복 시작 훅은 초기 지급·과거 저장 복원을 반복하지 않음"), IntegrationStatesEqual(Player->CaptureState(), Latest));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkReconnectWithoutPawnTest,
	"DIGETHER.Game.Framework.SaveIntegration.ReconnectWithoutPawnSpectatesThenSpawnsAtReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkReconnectWithoutPawnTest::RunTest(const FString& Parameters)
{
	FDigetherSaveIntegrationWorld Fixture(this);
	ADigetherElevatorActor* Elevator = Fixture.IsReady() ? PrepareIntegrationRun(Fixture) : nullptr;
	if (!TestTrue(TEXT("보존 Pawn 없는 재접속 월드"), Fixture.IsReady())
		|| !TestNotNull(TEXT("일반 스폰 준비"), Elevator))
	{
		return false;
	}
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	ADigetherGameplayPlayerController* Host = MakeRuntimeIntegrationPlayer(Fixture, MakeRuntimeIntegrationNetId(), true);
	const FUniqueNetIdRepl Id = MakeRuntimeIntegrationNetId();
	ADigetherGameplayPlayerController* Leaving = MakeRuntimeIntegrationPlayer(Fixture, Id);
	if (!TestNotNull(TEXT("호스트"), Host) || !TestNotNull(TEXT("끊길 참가자"), Leaving)
		|| !TestNotNull(TEXT("호스트 Pawn"), GiveIntegrationPawn(Fixture, Host, FVector(0.0, 0.0, 200.0))))
	{
		return false;
	}
	APawn* LostPawn = GiveIntegrationPawn(Fixture, Leaving, FVector(500.0, 0.0, 200.0));
	if (!TestNotNull(TEXT("출발용 참가자 Pawn"), LostPawn))
	{
		return false;
	}
	ADigetherGameplayPlayerState* Original = Leaving->GetPlayerState<ADigetherGameplayPlayerState>();
	FDigetherFrameworkSaveIntegrationAccess::Depart(*Mode);
	if (!TestTrue(TEXT("두 명의 실제 출발"), Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase == EDigetherRunPhase::InProgress))
	{
		return false;
	}
	// 탐사 중 몸이 사라진 뒤 끊긴 경우, 비활성 PS·자리는 보존되나 재빙의할 Pawn 없음
	Leaving->UnPossess();
	LostPawn->Destroy();
	DisconnectIntegrationPlayer(*Mode, *Leaving);
	FString Error;
	APlayerController* Joining = Mode->Login(nullptr, ROLE_AutonomousProxy, FString(), FString(), Id, Error);
	if (!TestNotNull(TEXT("Pawn 없는 기존 상태의 실제 재접속 Login"), Joining))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("시작 훅 전 재접속 표식"), FDigetherFrameworkSaveIntegrationAccess::IsReconnectPending(*Mode, Original));
	TestFalse(TEXT("탐사 출발 후 끊긴 참가자의 재접속은 관전 합류가 아님"), FDigetherFrameworkSaveIntegrationAccess::IsJoinSpectator(*Mode, Joining));
	Mode->HandleStartingNewPlayer_Implementation(Joining);
	TestNull(TEXT("탐사 중에는 보존 Pawn이 없어도 새 몸을 만들지 않음"), Joining->GetPawn());
	TestFalse(TEXT("보존 Pawn이 없어도 pending 표식 제거"), FDigetherFrameworkSaveIntegrationAccess::IsReconnectPending(*Mode, Original));
	FDigetherFrameworkSaveIntegrationAccess::Finish(*Mode);
	TestTrue(TEXT("귀환 때 일반 스폰"), Joining->GetPawn() != nullptr);
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
	"DIGETHER.Game.Framework.SaveIntegration.HubLeaverReturnsDuringRunAsSpectatorThenJoinsNextDeparture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkHubReconnectNextDepartureTest::RunTest(const FString& Parameters)
{
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("거점 이탈 후 탐사 중 복귀 월드"), Fixture.IsReady()))
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
	if (!TestNotNull(TEXT("다음 출발 호스트 Pawn"), HostPawn) || !TestNotNull(TEXT("거점 이탈자 Pawn"), Pawn))
	{
		return false;
	}
	ADigetherGameplayPlayerState* Original = Leaving->GetPlayerState<ADigetherGameplayPlayerState>();
	const FGuid GameplayId = Original->GetGameplayPlayerId();
	FDigetherWalletState Wallet;
	Wallet.BalanceCredits = 80;
	Wallet.LastRunPayoutCredits = 19;
	Original->RestoreState(Wallet);
	DisconnectIntegrationPlayer(*Mode, *Leaving);
	TestFalse(TEXT("거점 이탈로 캐릭터 제거"), IsValid(Pawn));
	FDigetherFrameworkSaveIntegrationAccess::Depart(*Mode);
	if (!TestTrue(TEXT("거점 이탈자를 제외한 실제 출발"), Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase == EDigetherRunPhase::InProgress))
	{
		return false;
	}
	FString Error;
	Mode->PreLogin(FString(), FString(), Id, Error);
	TestTrue(TEXT("거점 이탈자의 탐사 중 PreLogin은 관전 합류로 수락"), Error.IsEmpty());
	APlayerController* Joining = Mode->Login(nullptr, ROLE_AutonomousProxy, FString(), FString(), Id, Error);
	if (!TestNotNull(TEXT("탐사 중 거점 이탈자의 실제 Login 수락"), Joining))
	{
		AddError(Error);
		return false;
	}
	ADigetherGameplayPlayerState* Returned = Joining->GetPlayerState<ADigetherGameplayPlayerState>();
	TestTrue(TEXT("탐사 중 복귀는 관전 합류자"), FDigetherFrameworkSaveIntegrationAccess::IsJoinSpectator(*Mode, Joining));
	Mode->HandleStartingNewPlayer_Implementation(Joining);
	TestNull(TEXT("관전 중에는 몸이 없음"), Joining->GetPawn());
	TestFalse(TEXT("출발 인원에 소급하지 않음"), FDigetherFrameworkSaveIntegrationAccess::IsParticipant(*Mode, Returned));
	FDigetherFrameworkSaveIntegrationAccess::Finish(*Mode);
	TestTrue(TEXT("귀환 뒤 일반 참가자로 몸 생성"), Joining->GetPawn() != nullptr);
	TestEqual(TEXT("같은 서버 GUID로 복귀"), Returned->GetGameplayPlayerId(), GameplayId);
	TestTrue(TEXT("복귀에 초기 지급·이번 런 정산 없음"), IntegrationStatesEqual(Returned->CaptureState(), Wallet));
	FDigetherFrameworkSaveIntegrationAccess::Depart(*Mode);
	TestTrue(TEXT("다음 실제 출발에 복귀자를 포함"), Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase == EDigetherRunPhase::InProgress
		&& FDigetherFrameworkSaveIntegrationAccess::IsParticipant(*Mode, Returned));
	TestEqual(TEXT("다음 출발 인원에 복귀자 포함"), FDigetherFrameworkSaveIntegrationAccess::DepartureCount(*Mode), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkGaugeLateJoinSpectatorTest,
	"DIGETHER.Game.Framework.SaveIntegration.JoinAcceptedBeforeGaugeConfirmedAfterSpectates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkGaugeLateJoinSpectatorTest::RunTest(const FString& Parameters)
{
	// T76: 수락은 게이지 완료 전, 확정(Login)은 완료 후인 참가자는 관전 합류, 그날 출발 인원 제외
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("게이지 후 확정 시험 월드"), Fixture.IsReady()))
	{
		return false;
	}
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	FDigetherDoorIntegrationRun Run;
	if (!TestTrue(TEXT("레버·문 신호를 제품 초기화로 연결"), PrepareDoorIntegrationRun(Fixture, Run)))
	{
		return false;
	}
	const FUniqueNetIdRepl LateId = MakeRuntimeIntegrationNetId();
	FString Error;
	Mode->PreLogin(FString(), FString(), LateId, Error);
	if (!TestTrue(TEXT("게이지 완료 전 PreLogin 수락"), Error.IsEmpty()))
	{
		return false;
	}
	if (!TestNotEqual(TEXT("수락 뒤 게이지 완료로 거점 문 닫힘"), PullDepartureUntilDoorClosing(*Mode, Run), IntegrationTickTimedOut))
	{
		return false;
	}
	// 게이지 완료 뒤 수락된 요청은 거절(PreLogin 자체가 잠금 중, Login 재검사도 동일)
	const FUniqueNetIdRepl AfterGaugeId = MakeRuntimeIntegrationNetId();
	const FString InProgressMessage = DigetherJoinRules::MakeRejectionMessage(EDigetherSessionText::RunInProgress);
	Error.Reset();
	Mode->PreLogin(FString(), FString(), AfterGaugeId, Error);
	TestEqual(TEXT("게이지 완료 후 수락 요청은 ③ 거절"), Error, InProgressMessage);
	Error.Reset();
	TestNull(TEXT("게이지 완료 후 수락 요청의 Login도 거절"),
		Mode->Login(nullptr, ROLE_AutonomousProxy, FString(), FString(), AfterGaugeId, Error));
	TestEqual(TEXT("Login 거절도 ③"), Error, InProgressMessage);

	Error.Reset();
	APlayerController* Late = Mode->Login(nullptr, ROLE_AutonomousProxy, FString(), FString(), LateId, Error);
	if (!TestNotNull(TEXT("수락 전 게이지 → 확정 후 게이지의 실제 Login 수락"), Late))
	{
		AddError(Error);
		return false;
	}
	ADigetherGameplayPlayerState* LatePlayer = Late->GetPlayerState<ADigetherGameplayPlayerState>();
	TestTrue(TEXT("잠금 중 확정은 관전 합류"), FDigetherFrameworkSaveIntegrationAccess::IsJoinSpectator(*Mode, Late));
	Mode->HandleStartingNewPlayer_Implementation(Late);
	TestNull(TEXT("문 닫힘 중 관전 합류자는 몸 없음"), Late->GetPawn());
	TestFalse(TEXT("관전 중에는 시작 지급 없음"), LatePlayer->IsGameplayInitialized());

	const int32 DepartureTicks = TickIntegrationUntil(*Mode, [Mode]()
		{
			return Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase == EDigetherRunPhase::InProgress;
		}, ToIntegrationTicks(FDigetherElevatorTuning().HubDoorCloseSeconds) + 1);
	if (!TestNotEqual(TEXT("관전 합류자가 있어도 출발 확정"), DepartureTicks, IntegrationTickTimedOut))
	{
		return false;
	}
	TestEqual(TEXT("그날 출발 인원에서 관전 합류자 제외"), FDigetherFrameworkSaveIntegrationAccess::DepartureCount(*Mode), 1);
	TestFalse(TEXT("관전 합류자는 런 참가자가 아님"), FDigetherFrameworkSaveIntegrationAccess::IsParticipant(*Mode, LatePlayer));
	TestTrue(TEXT("탐사 중에도 관전 유지"), FDigetherFrameworkSaveIntegrationAccess::IsJoinSpectator(*Mode, Late) && Late->GetPawn() == nullptr);

	FDigetherFrameworkSaveIntegrationAccess::Finish(*Mode);
	TestFalse(TEXT("귀환하면 관전 해제"), FDigetherFrameworkSaveIntegrationAccess::IsJoinSpectator(*Mode, Late));
	TestTrue(TEXT("귀환 때 캐릭터 생성"), Late->GetPawn() != nullptr);
	TestTrue(TEXT("귀환 때 시작 지급 한 번"), LatePlayer->IsGameplayInitialized()
		&& LatePlayer->CaptureState().BalanceCredits == DigetherFrameworkDefaults::StartingBalanceCredits);
	TestEqual(TEXT("관전한 탐사의 결과는 받지 않음"), LatePlayer->GetLastRunResult().RunId, FGuid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkDoorReopenReleasesJoinSpectatorTest,
	"DIGETHER.Game.Framework.SaveIntegration.DoorReopenReleasesJoinSpectatorToHub",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkDoorReopenReleasesJoinSpectatorTest::RunTest(const FString& Parameters)
{
	// 출발 실패로 문 재개방 시 잠금 해제, 관전 합류자는 즉시 거점 참가자
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("문 재개방 관전 해제 시험 월드"), Fixture.IsReady()))
	{
		return false;
	}
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	ADigetherElevatorActor* Elevator = PrepareIntegrationRun(Fixture);
	if (!TestNotNull(TEXT("재개방 엘리베이터"), Elevator)
		|| !TestTrue(TEXT("출발·상승 안·밖 레버"), SpawnIntegrationLeverSet(Fixture, *Elevator).IsReady())
		|| !TestNotNull(TEXT("Pawn 없는 호스트"), MakeRuntimeIntegrationPlayer(Fixture, MakeRuntimeIntegrationNetId(), true))
		|| !TestTrue(TEXT("문 신호를 제품 초기화로 연결"), FDigetherFrameworkSaveIntegrationAccess::InitializeRunActors(*Mode)))
	{
		return false;
	}
	const FUniqueNetIdRepl LateId = MakeRuntimeIntegrationNetId();
	FString Error;
	Mode->PreLogin(FString(), FString(), LateId, Error);
	TestTrue(TEXT("문 닫힘 전 PreLogin 수락"), Error.IsEmpty());
	FDigetherFrameworkSaveIntegrationAccess::BeginDoorClose(*Mode, EDigetherElevatorLeverKind::Departure);
	APlayerController* Late = Mode->Login(nullptr, ROLE_AutonomousProxy, FString(), FString(), LateId, Error);
	if (!TestNotNull(TEXT("문 닫힘 중 확정"), Late))
	{
		AddError(Error);
		return false;
	}
	Mode->HandleStartingNewPlayer_Implementation(Late);
	TestTrue(TEXT("문 닫힘 중에는 관전"), FDigetherFrameworkSaveIntegrationAccess::IsJoinSpectator(*Mode, Late) && Late->GetPawn() == nullptr);
	// Pawn 없는 호스트로 출발 실패, 문 재개방(기존 출발 실패 시험과 같은 경로)
	AddExpectedError(TEXT("Departure rejected: active player Pawn unavailable."), EAutomationExpectedErrorFlags::Contains, 1);
	TestNotEqual(TEXT("닫힘 완료 뒤 출발 실패로 문 재개방"), TickIntegrationUntil(*Mode,
		[Elevator]() { return GetIntegrationElevatorPhase(*Elevator) == EDigetherElevatorPhase::HubWaiting; },
		ToIntegrationTicks(FDigetherElevatorTuning().HubDoorCloseSeconds) + 1), IntegrationTickTimedOut);
	Mode->Tick(IntegrationTickSeconds);
	TestFalse(TEXT("잠금이 풀리면 관전 해제"), FDigetherFrameworkSaveIntegrationAccess::IsJoinSpectator(*Mode, Late));
	TestTrue(TEXT("잠금이 풀리면 거점에 캐릭터 생성"), Late->GetPawn() != nullptr);
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
		|| !TestTrue(TEXT("실제 진행으로 슬롯 구성"), DigetherSaveRules::TryBuildSlot(Snapshot, nullptr, false, Slot) == EDigetherSaveResult::Success)
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
	const int32 FutureVersion = DigetherSaveWire::LatestVersion + 1;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkRestoreClearsRunResultTest,
	"DIGETHER.Game.Framework.SaveIntegration.RestoreClearsRunResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkRestoreClearsRunResultTest::RunTest(const FString& Parameters)
{
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("결과 복원 경계 시험 월드"), Fixture.IsReady())
		|| !TestNotNull(TEXT("결과 복원 경계 런 준비"), PrepareIntegrationRun(Fixture)))
	{
		return false;
	}
	ADigetherGameplayPlayerController* Host = MakeRuntimeIntegrationPlayer(Fixture, MakeRuntimeIntegrationNetId(), true);
	if (!TestNotNull(TEXT("결과 복원 경계 호스트"), Host)
		|| !TestNotNull(TEXT("결과 복원 경계 Pawn"), GiveIntegrationPawn(Fixture, Host, FVector(0.0, 0.0, 200.0))))
	{
		return false;
	}
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	ADigetherGameplayPlayerState* Player = Host->GetPlayerState<ADigetherGameplayPlayerState>();
	FDigetherWalletState Wallet;
	Wallet.BalanceCredits = 67;
	Player->RestoreState(Wallet);
	FDigetherSaveSnapshot Snapshot;
	if (!TestTrue(TEXT("복원할 실제 진행 캡처"), FDigetherFrameworkSaveIntegrationAccess::Capture(*Mode, Snapshot)))
	{
		return false;
	}
	FDigetherRunResult PreviousResult;
	PreviousResult.RunId = FGuid::NewGuid();
	PreviousResult.CompletedDay = 1;
	PreviousResult.FinalBalanceCredits = Wallet.BalanceCredits;
	if (!TestTrue(TEXT("복원 전에 화면에 남은 결과 발행"), Player->PublishRunResult(PreviousResult)))
	{
		return false;
	}
	TestTrue(TEXT("사전 복원 검사는 유효 진행 수락"),
		FDigetherFrameworkSaveIntegrationAccess::Restore(*Mode, Snapshot, true));
	TestTrue(TEXT("검사만 한 경우 현재 결과 보존"), IntegrationStatesEqual(Player->GetLastRunResult(), PreviousResult));
	FDigetherSaveSnapshot Invalid = Snapshot;
	Invalid.GameId.Invalidate();
	TestFalse(TEXT("손상 진행 복원 거절"), FDigetherFrameworkSaveIntegrationAccess::Restore(*Mode, Invalid));
	TestTrue(TEXT("거절한 복원은 현재 결과 보존"), IntegrationStatesEqual(Player->GetLastRunResult(), PreviousResult));
	TestTrue(TEXT("유효 진행 최종 복원 성공"), FDigetherFrameworkSaveIntegrationAccess::Restore(*Mode, Snapshot));
	TestFalse(TEXT("불러오기·새게임의 성공 복원은 이전 결과 제거"), Player->GetLastRunResult().RunId.IsValid());
	TestEqual(TEXT("결과 제거와 별개로 복원 지갑 적용"), Player->CaptureState().BalanceCredits, Wallet.BalanceCredits);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkDoorClosingDepartureAndAscentTest,
	"DIGETHER.Game.Framework.SaveIntegration.DepartureAndAscentWaitForDoorClose",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkDoorClosingDepartureAndAscentTest::RunTest(const FString& Parameters)
{
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("문 닫힘 출발·상승 시험 월드"), Fixture.IsReady()))
	{
		return false;
	}
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	FDigetherDoorIntegrationRun Run;
	if (!TestTrue(TEXT("레버·문 신호를 제품 초기화로 연결"), PrepareDoorIntegrationRun(Fixture, Run)))
	{
		return false;
	}
	const FDigetherElevatorTuning ElevatorTuning;
	const int32 DoorTicks = ToIntegrationTicks(ElevatorTuning.HubDoorCloseSeconds);
	const int32 MineDoorTicks = ToIntegrationTicks(ElevatorTuning.MineDoorCloseSeconds);

	// 기획 A3·C3: 출발 게이지 완료 시 거점 문 닫힘 시작, 그때부터 새 참가 차단
	if (!TestNotEqual(TEXT("출발 게이지 완료 뒤 거점 문 닫힘"), PullDepartureUntilDoorClosing(*Mode, Run), IntegrationTickTimedOut))
	{
		return false;
	}
	TestTrue(TEXT("문 닫힘 중에는 아직 출발 전"),
		Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase != EDigetherRunPhase::InProgress);
	TestTrue(TEXT("문 닫힘 중 레버를 놓아도 닫힘 유지"), Mode->ReleaseLeverPull(Run.Host, Run.DepartureLever) == false
		&& GetIntegrationElevatorPhase(*Run.Elevator) == EDigetherElevatorPhase::DoorClosing);
	FString JoinError;
	Mode->PreLogin(FString(), FString(), MakeRuntimeIntegrationNetId(), JoinError);
	TestFalse(TEXT("거점 문 닫힘부터 새 참가 PreLogin 거절"), JoinError.IsEmpty());
	// 게이지 완료 Tick에 문이 이미 한 간격 닫혀 남은 간격 뒤 출발
	const int32 DepartureTicks = TickIntegrationUntil(*Mode, [Mode]()
		{
			return Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase == EDigetherRunPhase::InProgress;
		}, DoorTicks + 1);
	TestEqual(TEXT("문 닫힘 시간이 다 지난 Tick에 출발 확정"), DepartureTicks, DoorTicks - 1);
	TestEqual(TEXT("출발 확정 뒤 첫 하강"), GetIntegrationElevatorPhase(*Run.Elevator), EDigetherElevatorPhase::InitialDescent);
	TestEqual(TEXT("출발 인원은 호스트 한 명"), FDigetherFrameworkSaveIntegrationAccess::DepartureCount(*Mode), 1);
	TestEqual(TEXT("출발 뒤 출발 게이지 0"), Run.DepartureLever->GetLeverComponent()->GetPublicState().Progress, 0.f);

	// 바닥 도착 뒤 상승 레버도 문 닫힘 경유
	TestNotEqual(TEXT("첫 하강 뒤 하부 대기"), TickIntegrationUntil(*Mode,
		[&Run]() { return GetIntegrationElevatorPhase(*Run.Elevator) == EDigetherElevatorPhase::BottomWaiting; },
		ToIntegrationTicks(MakeIntegrationTuning().ElevatorTravelSeconds) + 1), IntegrationTickTimedOut);
	UpdateIntegrationOverlaps(*Run.Elevator, Run.HostPawn);
	ADigetherGameplayPlayerState* HostState = Run.Host->GetPlayerState<ADigetherGameplayPlayerState>();
	if (!TestTrue(TEXT("상승 레버 실제 요청"), Mode->BeginLeverPull(Run.Host, Run.AscentLever)))
	{
		return false;
	}
	TestNotEqual(TEXT("상승 게이지 완료 뒤 갱도 문 닫힘"), TickIntegrationUntil(*Mode,
		[&Run]() { return GetIntegrationElevatorPhase(*Run.Elevator) == EDigetherElevatorPhase::DoorClosing; },
		ToIntegrationTicks(ElevatorTuning.AscentHoldSeconds) + 1), IntegrationTickTimedOut);
	TestEqual(TEXT("문 닫힘 중 탑승자는 아직 탈출 전"),
		HostState->GetLifeComponent()->CaptureState().PublicState.Life, EDigetherLifeState::Active);
	TestEqual(TEXT("문 닫힘 중 상승 횟수 기록 없음"), Mode->GetRunCoordinatorComponent()->GetConfirmedAscentCount(), 0);
	const int32 AscentTicks = TickIntegrationUntil(*Mode, [HostState]()
		{
			return HostState->GetLifeComponent()->CaptureState().PublicState.Life == EDigetherLifeState::Escaped;
		}, MineDoorTicks + 1);
	TestEqual(TEXT("문 닫힘 시간이 다 지난 Tick에 상승 확정"), AscentTicks, MineDoorTicks - 1);
	TestEqual(TEXT("닫힌 순간 탑승자로 첫 편 기록"), Mode->GetRunCoordinatorComponent()->GetConfirmedAscentCount(), 1);
	TestEqual(TEXT("2회 설정 첫 편은 일반 상승"), GetIntegrationElevatorPhase(*Run.Elevator), EDigetherElevatorPhase::Ascending);
	TestEqual(TEXT("상승 뒤 상승 게이지 0"), Run.AscentLever->GetLeverComponent()->GetPublicState().Progress, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkAscentLeverLayoutTest,
	"DIGETHER.Game.Framework.SaveIntegration.AscentLeverLayoutCheck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkAscentLeverLayoutTest::RunTest(const FString& Parameters)
{
	// 기획 S15: 출발 1·상승 안 1·상승 밖 1, 위치 불일치는 경고만, 개발 빌드는 밖 레버 없는 맵을 경고 후 허용
	AddExpectedError(TEXT("Map configuration rejected: expected one Departure, one inside Ascent and one outside Ascent lever"),
		EAutomationExpectedErrorFlags::Contains, 2);
	AddExpectedMessagePlain(TEXT("does not match the boarding volume"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1);
	{
		FDigetherSaveIntegrationWorld Fixture(this);
		if (!TestTrue(TEXT("레버 구성 검사 시험 월드"), Fixture.IsReady()))
		{
			return false;
		}
		ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
		ADigetherElevatorActor* Elevator = PrepareIntegrationRun(Fixture);
		if (!TestNotNull(TEXT("레버 구성 엘리베이터"), Elevator))
		{
			return false;
		}
		const FVector ElevatorLocation = Elevator->GetActorLocation();
		ADigetherElevatorLeverActor* Departure = SpawnIntegrationLever(Fixture, *Elevator, EDigetherElevatorLeverKind::Departure);
		ADigetherElevatorLeverActor* Outside = SpawnIntegrationLever(Fixture, *Elevator, EDigetherElevatorLeverKind::Ascent,
			EDigetherElevatorLeverPlacement::Outside);
		if (!TestNotNull(TEXT("출발 레버"), Departure) || !TestNotNull(TEXT("밖 상승 레버"), Outside))
		{
			return false;
		}
		Departure->SetActorLocation(ElevatorLocation + IntegrationDepartureLeverOffsetCm);
		Outside->SetActorLocation(ElevatorLocation + IntegrationOutsideLeverOffsetCm);
		TestFalse(TEXT("안 상승 레버 없으면 시작 거절"), FDigetherFrameworkSaveIntegrationAccess::InitializeRunActors(*Mode));
		ADigetherElevatorLeverActor* Inside = SpawnIntegrationLever(Fixture, *Elevator, EDigetherElevatorLeverKind::Ascent);
		ADigetherElevatorLeverActor* ExtraInside = SpawnIntegrationLever(Fixture, *Elevator, EDigetherElevatorLeverKind::Ascent);
		if (!TestNotNull(TEXT("안 상승 레버"), Inside) || !TestNotNull(TEXT("중복 안 상승 레버"), ExtraInside))
		{
			return false;
		}
		Inside->SetActorLocation(ElevatorLocation + IntegrationInsideLeverOffsetCm);
		ExtraInside->SetActorLocation(ElevatorLocation + IntegrationInsideLeverOffsetCm + FVector(0.0, 50.0, 0.0));
		TestFalse(TEXT("안 상승 레버 2개는 시작 거절"), FDigetherFrameworkSaveIntegrationAccess::InitializeRunActors(*Mode));
		ExtraInside->Destroy();
		// 밖 레버가 탑승 영역 안이면 경고만 남기고 시작(아트 배치 조정 여지)
		Outside->SetActorLocation(ElevatorLocation + FVector(200.0, 0.0, 0.0));
		TestTrue(TEXT("위치 불일치는 경고 후 시작"), FDigetherFrameworkSaveIntegrationAccess::InitializeRunActors(*Mode));
	}
	{
		FDigetherSaveIntegrationWorld Fixture(this);
		if (!TestTrue(TEXT("밖 레버 없는 맵 시험 월드"), Fixture.IsReady()))
		{
			return false;
		}
		ADigetherElevatorActor* Elevator = PrepareIntegrationRun(Fixture);
		if (!TestNotNull(TEXT("밖 레버 없는 맵 엘리베이터"), Elevator))
		{
			return false;
		}
		ADigetherElevatorLeverActor* Departure = SpawnIntegrationLever(Fixture, *Elevator, EDigetherElevatorLeverKind::Departure);
		ADigetherElevatorLeverActor* Inside = SpawnIntegrationLever(Fixture, *Elevator, EDigetherElevatorLeverKind::Ascent);
		if (!TestNotNull(TEXT("출발 레버"), Departure) || !TestNotNull(TEXT("안 상승 레버"), Inside))
		{
			return false;
		}
		Departure->SetActorLocation(Elevator->GetActorLocation() + IntegrationDepartureLeverOffsetCm);
		Inside->SetActorLocation(Elevator->GetActorLocation() + IntegrationInsideLeverOffsetCm);
		// 자동 테스트는 개발 빌드 전용, Shipping 거절 분기는 이 시험으로 확인 불가
		AddExpectedMessagePlain(TEXT("outside Ascent lever missing"), ELogVerbosity::Warning,
			EAutomationExpectedMessageFlags::Contains, 1);
		TestTrue(TEXT("개발 빌드는 밖 레버 없는 맵을 경고 후 시작"),
			FDigetherFrameworkSaveIntegrationAccess::InitializeRunActors(*Fixture.GetGameMode()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkOutsideAscentLeverTest,
	"DIGETHER.Game.Framework.SaveIntegration.OutsideAscentLeverSendsElevatorWithoutPuller",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkOutsideAscentLeverTest::RunTest(const FString& Parameters)
{
	// 기획 S15·T93: 밖 레버를 당긴 사람은 갱도에 남고 엘리베이터는 탑승자 없이 한 편 상승
	// 화물 적재 경로가 없어 화물 목록 고정은 규칙 단위 시험(Elevator.CargoOnlyAscent)에서 확인
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("밖 레버 상승 시험 월드"), Fixture.IsReady()))
	{
		return false;
	}
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	FDigetherDoorIntegrationRun Run;
	if (!TestTrue(TEXT("레버·문 신호를 제품 초기화로 연결"), PrepareDoorIntegrationRun(Fixture, Run))
		|| !TestTrue(TEXT("출발해 하부 대기"), AdvanceDoorIntegrationRunToBottom(*Mode, Run)))
	{
		return false;
	}
	ADigetherGameplayPlayerState* HostState = Run.Host->GetPlayerState<ADigetherGameplayPlayerState>();
	// 기획 02 개요 5행: 1차 상승 확정이어도 다른 사람이 탐사 중이면 일차 유지
	const FDigetherQuotaState QuotaBeforeAscent = Fixture.GetGameState()->GetQuotaComponent()->CaptureState().Quota;
	MovePawnToOutsideLever(Run, *Run.HostPawn);
	TArray<APawn*> Boarded;
	Run.Elevator->GetOverlappingPawns(Boarded);
	TestFalse(TEXT("밖 레버 조작자는 탑승 영역 밖"), Boarded.Contains(Run.HostPawn));
	const double RemainingBefore = Mode->GetRunCoordinatorComponent()->GetRemainingSeconds();
	if (!TestTrue(TEXT("밖 상승 레버 실제 요청"), Mode->BeginLeverPull(Run.Host, Run.OutsideAscentLever)))
	{
		return false;
	}
	TestNotEqual(TEXT("밖 레버 게이지 완료 뒤 갱도 문 닫힘"), TickIntegrationUntil(*Mode,
		[&Run]() { return GetIntegrationElevatorPhase(*Run.Elevator) == EDigetherElevatorPhase::DoorClosing; },
		ToIntegrationTicks(FDigetherElevatorTuning().AscentHoldSeconds) + 1), IntegrationTickTimedOut);
	TestNotEqual(TEXT("문 닫힘 완료 뒤 상승 확정"), TickIntegrationUntil(*Mode,
		[Mode]() { return Mode->GetRunCoordinatorComponent()->GetConfirmedAscentCount() == 1; },
		ToIntegrationTicks(FDigetherElevatorTuning().MineDoorCloseSeconds) + 1), IntegrationTickTimedOut);
	TestEqual(TEXT("탑승자 없는 편도 일반 상승"), GetIntegrationElevatorPhase(*Run.Elevator), EDigetherElevatorPhase::Ascending);
	FDigetherElevatorAscentConfirmed Ascent;
	if (TestTrue(TEXT("확정 명세 보관"), Run.Elevator->GetElevatorComponent()->TryGetLastAscent(Ascent)))
	{
		TestEqual(TEXT("확정 명세 탑승자 0명"), Ascent.PassengerIds.Num(), 0);
	}
	TestEqual(TEXT("밖 레버 조작자는 갱도에 남음"),
		HostState->GetLifeComponent()->CaptureState().PublicState.Life, EDigetherLifeState::Active);
	TestTrue(TEXT("밖 레버 조작자는 계속 런 참가자"), FDigetherFrameworkSaveIntegrationAccess::IsParticipant(*Mode, HostState));
	const FDigetherQuotaState QuotaAfterAscent = Fixture.GetGameState()->GetQuotaComponent()->CaptureState().Quota;
	TestEqual(TEXT("1차 상승 확정 뒤에도 기간 일차 그대로"), QuotaAfterAscent.Day, QuotaBeforeAscent.Day);
	TestEqual(TEXT("1차 상승 확정 뒤에도 전체 일차 그대로"), QuotaAfterAscent.TotalDay, QuotaBeforeAscent.TotalDay);
	TestTrue(TEXT("상승 확정으로 남은 시간 차감"),
		Mode->GetRunCoordinatorComponent()->GetRemainingSeconds() < RemainingBefore);
	TestEqual(TEXT("상승 뒤 밖 레버 게이지 0"), Run.OutsideAscentLever->GetLeverComponent()->GetPublicState().Progress, 0.f);
	TestEqual(TEXT("상승 뒤 안 레버 게이지 0"), Run.AscentLever->GetLeverComponent()->GetPublicState().Progress, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkAscentLeverPairTest,
	"DIGETHER.Game.Framework.SaveIntegration.AscentLeversAcceptOnlyFirstPull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkAscentLeverPairTest::RunTest(const FString& Parameters)
{
	// 기획 S15·T93: 안팎 레버를 두 사람이 동시에 당겨도 먼저 시작한 한 건만 수락, 한 번만 출발
	// 안쪽 레버로 확정한 편이 배열 뒤 밖 레버 때문에 실패하지 않는지 확인(회귀)
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("안팎 레버 동시 입력 시험 월드"), Fixture.IsReady()))
	{
		return false;
	}
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	FDigetherDoorIntegrationRun Run;
	if (!TestTrue(TEXT("레버·문 신호를 제품 초기화로 연결"), PrepareDoorIntegrationRun(Fixture, Run)))
	{
		return false;
	}
	ADigetherGameplayPlayerController* Guest = MakeRuntimeIntegrationPlayer(Fixture, MakeRuntimeIntegrationNetId());
	APawn* GuestPawn = IsValid(Guest)
		? GiveIntegrationPawn(Fixture, Guest, Run.Elevator->GetActorLocation() + FVector(0.0, 100.0, 100.0)) : nullptr;
	if (!TestNotNull(TEXT("두 번째 참가자 Pawn"), GuestPawn))
	{
		return false;
	}
	UpdateIntegrationOverlaps(*Run.Elevator, Run.HostPawn);
	UpdateIntegrationOverlaps(*Run.Elevator, GuestPawn);
	if (!TestTrue(TEXT("두 명 출발해 하부 대기"), AdvanceDoorIntegrationRunToBottom(*Mode, Run)))
	{
		return false;
	}
	ADigetherGameplayPlayerState* HostState = Run.Host->GetPlayerState<ADigetherGameplayPlayerState>();
	ADigetherGameplayPlayerState* GuestState = Guest->GetPlayerState<ADigetherGameplayPlayerState>();
	UpdateIntegrationOverlaps(*Run.Elevator, Run.HostPawn);
	MovePawnToOutsideLever(Run, *GuestPawn);

	if (!TestTrue(TEXT("안 레버 먼저 시작"), Mode->BeginLeverPull(Run.Host, Run.AscentLever)))
	{
		return false;
	}
	TestFalse(TEXT("안 레버 진행 중 밖 레버 시작 거절"), Mode->BeginLeverPull(Guest, Run.OutsideAscentLever));
	TestFalse(TEXT("거절된 밖 레버는 점유자 없음"), Run.OutsideAscentLever->GetLeverComponent()->GetActivePullerId().IsValid());
	Mode->Tick(IntegrationTickSeconds);
	TestTrue(TEXT("안 레버 게이지 진행"), Run.AscentLever->GetLeverComponent()->GetPublicState().Progress > 0.f);
	TestEqual(TEXT("밖 레버 게이지 0 유지"), Run.OutsideAscentLever->GetLeverComponent()->GetPublicState().Progress, 0.f);
	TestFalse(TEXT("안 레버 진행 중 밖 레버 재시도 거절"), Mode->BeginLeverPull(Guest, Run.OutsideAscentLever));
	TestNotEqual(TEXT("안 레버 게이지 완료 뒤 갱도 문 닫힘"), TickIntegrationUntil(*Mode,
		[&Run]() { return GetIntegrationElevatorPhase(*Run.Elevator) == EDigetherElevatorPhase::DoorClosing; },
		ToIntegrationTicks(FDigetherElevatorTuning().AscentHoldSeconds) + 1), IntegrationTickTimedOut);
	TestFalse(TEXT("문 닫힘 중 밖 레버 입력 무시"), Mode->BeginLeverPull(Guest, Run.OutsideAscentLever));
	TestEqual(TEXT("문 닫힘 중 밖 레버 게이지 0"), Run.OutsideAscentLever->GetLeverComponent()->GetPublicState().Progress, 0.f);
	TestNotEqual(TEXT("안 레버로 확정한 편 상승"), TickIntegrationUntil(*Mode, [HostState]()
		{
			return HostState->GetLifeComponent()->CaptureState().PublicState.Life == EDigetherLifeState::Escaped;
		}, ToIntegrationTicks(FDigetherElevatorTuning().MineDoorCloseSeconds) + 1), IntegrationTickTimedOut);
	TestEqual(TEXT("한 편만 출발"), Mode->GetRunCoordinatorComponent()->GetConfirmedAscentCount(), 1);
	TestEqual(TEXT("밖에 남은 참가자는 갱도에 남음"),
		GuestState->GetLifeComponent()->CaptureState().PublicState.Life, EDigetherLifeState::Active);
	TestEqual(TEXT("상승 뒤 안 레버 게이지 0"), Run.AscentLever->GetLeverComponent()->GetPublicState().Progress, 0.f);
	TestEqual(TEXT("상승 뒤 밖 레버 게이지 0"), Run.OutsideAscentLever->GetLeverComponent()->GetPublicState().Progress, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkAscentDoorReopenResetsLeversTest,
	"DIGETHER.Game.Framework.SaveIntegration.AscentDoorReopenResetsBothLevers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkAscentDoorReopenResetsLeversTest::RunTest(const FString& Parameters)
{
	// 기획 A3·S15: 갱도 문 닫힘 중 무게 초과 재개방 시 안·밖 레버 게이지 모두 0, 다시 당기기 가능
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("문 재개방 레버 초기화 시험 월드"), Fixture.IsReady()))
	{
		return false;
	}
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	FDigetherDoorIntegrationRun Run;
	if (!TestTrue(TEXT("레버·문 신호를 제품 초기화로 연결"), PrepareDoorIntegrationRun(Fixture, Run))
		|| !TestTrue(TEXT("출발해 하부 대기"), AdvanceDoorIntegrationRunToBottom(*Mode, Run)))
	{
		return false;
	}
	MovePawnToOutsideLever(Run, *Run.HostPawn);
	if (!TestTrue(TEXT("밖 상승 레버 실제 요청"), Mode->BeginLeverPull(Run.Host, Run.OutsideAscentLever))
		|| !TestNotEqual(TEXT("밖 레버 게이지 완료 뒤 갱도 문 닫힘"), TickIntegrationUntil(*Mode,
			[&Run]() { return GetIntegrationElevatorPhase(*Run.Elevator) == EDigetherElevatorPhase::DoorClosing; },
			ToIntegrationTicks(FDigetherElevatorTuning().AscentHoldSeconds) + 1), IntegrationTickTimedOut))
	{
		return false;
	}
	TestEqual(TEXT("문 닫힘 중 확정 레버 게이지 유지"), Run.OutsideAscentLever->GetLeverComponent()->GetPublicState().Progress, 1.f);
	TestFalse(TEXT("문 닫힘 중 확정 레버 재입력 무시"), Mode->BeginLeverPull(Run.Host, Run.OutsideAscentLever));
	// 화물 적재 경로가 없어 엘리베이터 공개 API로 초과 화물 목록 주입, 다음 Tick이 재개방 처리
	FDigetherElevatorLoadEntry HeavyCargo;
	HeavyCargo.EntryId = FGuid::NewGuid();
	HeavyCargo.Kind = EDigetherElevatorLoadKind::Cargo;
	HeavyCargo.WeightKg = MakeIntegrationTuning().ElevatorCapacityKg + 1.f;
	Run.Elevator->GetElevatorComponent()->UpdateLoad({ HeavyCargo });
	TestEqual(TEXT("무게 초과로 문 재개방"), GetIntegrationElevatorPhase(*Run.Elevator), EDigetherElevatorPhase::BottomWaiting);
	Mode->Tick(IntegrationTickSeconds);
	TestEqual(TEXT("재개방 뒤 밖 레버 게이지 0"), Run.OutsideAscentLever->GetLeverComponent()->GetPublicState().Progress, 0.f);
	TestEqual(TEXT("재개방 뒤 안 레버 게이지 0"), Run.AscentLever->GetLeverComponent()->GetPublicState().Progress, 0.f);
	TestEqual(TEXT("재개방 시도는 상승 횟수 미기록"), Mode->GetRunCoordinatorComponent()->GetConfirmedAscentCount(), 0);
	TestTrue(TEXT("재개방 뒤 밖 레버 다시 시작"), Mode->BeginLeverPull(Run.Host, Run.OutsideAscentLever));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkDoorClosingTimeoutTest,
	"DIGETHER.Game.Framework.SaveIntegration.TimeoutDuringDoorCloseReturnsFromMine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkDoorClosingTimeoutTest::RunTest(const FString& Parameters)
{
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("문 닫힘 중 시간 종료 시험 월드"), Fixture.IsReady()))
	{
		return false;
	}
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	FDigetherDoorIntegrationRun Run;
	if (!TestTrue(TEXT("레버·문 신호를 제품 초기화로 연결"), PrepareDoorIntegrationRun(Fixture, Run)))
	{
		return false;
	}
	ADigetherGameplayPlayerState* HostState = Run.Host->GetPlayerState<ADigetherGameplayPlayerState>();
	FDigetherWalletState Wallet;
	Wallet.BalanceCredits = 80;
	HostState->RestoreState(Wallet);
	if (!TestNotEqual(TEXT("거점 문 닫힘"), PullDepartureUntilDoorClosing(*Mode, Run), IntegrationTickTimedOut)
		|| !TestNotEqual(TEXT("출발 확정"), TickIntegrationUntil(*Mode, [Mode]()
			{
				return Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase == EDigetherRunPhase::InProgress;
			}, ToIntegrationTicks(FDigetherElevatorTuning().HubDoorCloseSeconds) + 1), IntegrationTickTimedOut)
		|| !TestNotEqual(TEXT("하부 대기"), TickIntegrationUntil(*Mode,
			[&Run]() { return GetIntegrationElevatorPhase(*Run.Elevator) == EDigetherElevatorPhase::BottomWaiting; },
			ToIntegrationTicks(MakeIntegrationTuning().ElevatorTravelSeconds) + 1), IntegrationTickTimedOut))
	{
		return false;
	}
	UpdateIntegrationOverlaps(*Run.Elevator, Run.HostPawn);
	if (!TestTrue(TEXT("상승 레버 실제 요청"), Mode->BeginLeverPull(Run.Host, Run.AscentLever))
		|| !TestNotEqual(TEXT("갱도 문 닫힘"), TickIntegrationUntil(*Mode,
			[&Run]() { return GetIntegrationElevatorPhase(*Run.Elevator) == EDigetherElevatorPhase::DoorClosing; },
			ToIntegrationTicks(FDigetherElevatorTuning().AscentHoldSeconds) + 1), IntegrationTickTimedOut))
	{
		return false;
	}
	// 기획 A3: 문 닫히는 중 시간 종료는 갱도 대기 취급, 무게 정상이면 탑승자 자동 귀환
	FDigetherFrameworkSaveIntegrationAccess::Finish(*Mode);
	TestTrue(TEXT("문 닫힘 중 시간 종료로 탐사 종료"),
		Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase != EDigetherRunPhase::InProgress);
	TestEqual(TEXT("문 안 탑승자는 자동 귀환"), HostState->GetLastRunResult().Outcome, EDigetherRunReturnOutcome::Returned);
	TestEqual(TEXT("자동 귀환자는 사망 손실 없음"), HostState->CaptureState().BalanceCredits, 80);
	TestEqual(TEXT("귀환 뒤 거점 대기"), GetIntegrationElevatorPhase(*Run.Elevator), EDigetherElevatorPhase::HubWaiting);
	TestEqual(TEXT("귀환 뒤 상승 게이지 0"), Run.AscentLever->GetLeverComponent()->GetPublicState().Progress, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkDoorClosingDepartureFailureTest,
	"DIGETHER.Game.Framework.SaveIntegration.DepartureFailureReopensDoorAndUnlocksJoin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkDoorClosingDepartureFailureTest::RunTest(const FString& Parameters)
{
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("출발 실패 재개방 시험 월드"), Fixture.IsReady()))
	{
		return false;
	}
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	ADigetherElevatorActor* Elevator = PrepareIntegrationRun(Fixture);
	// Pawn 없는 연결자는 출발 공간 검사에서 거절
	if (!TestNotNull(TEXT("출발 실패 엘리베이터"), Elevator)
		|| !TestTrue(TEXT("출발·상승 안·밖 레버"), SpawnIntegrationLeverSet(Fixture, *Elevator).IsReady())
		|| !TestNotNull(TEXT("Pawn 없는 호스트"), MakeRuntimeIntegrationPlayer(Fixture, MakeRuntimeIntegrationNetId(), true))
		|| !TestTrue(TEXT("문 신호를 제품 초기화로 연결"), FDigetherFrameworkSaveIntegrationAccess::InitializeRunActors(*Mode)))
	{
		return false;
	}
	FDigetherFrameworkSaveIntegrationAccess::BeginDoorClose(*Mode, EDigetherElevatorLeverKind::Departure);
	TestEqual(TEXT("거점 문 닫힘 시작"), GetIntegrationElevatorPhase(*Elevator), EDigetherElevatorPhase::DoorClosing);
	FString LockedError;
	Mode->PreLogin(FString(), FString(), MakeRuntimeIntegrationNetId(), LockedError);
	TestFalse(TEXT("문 닫힘 중 새 참가 거절"), LockedError.IsEmpty());
	AddExpectedError(TEXT("Departure rejected: active player Pawn unavailable."),
		EAutomationExpectedErrorFlags::Contains, 1);
	TestNotEqual(TEXT("닫힘 완료 뒤 출발 실패로 문 재개방"), TickIntegrationUntil(*Mode,
		[Elevator]() { return GetIntegrationElevatorPhase(*Elevator) == EDigetherElevatorPhase::HubWaiting; },
		ToIntegrationTicks(FDigetherElevatorTuning().HubDoorCloseSeconds) + 1), IntegrationTickTimedOut);
	TestTrue(TEXT("출발 실패는 탐사를 시작하지 않음"),
		Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase != EDigetherRunPhase::InProgress);
	FString UnlockedError;
	Mode->PreLogin(FString(), FString(), MakeRuntimeIntegrationNetId(), UnlockedError);
	TestTrue(TEXT("문이 다시 열리면 새 참가 허용"), UnlockedError.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkDuplicateLoginRetryTest,
	"DIGETHER.Game.Framework.SaveIntegration.DuplicateLoginReplacesPreviousConnectionDuringRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkDuplicateLoginRetryTest::RunTest(const FString& Parameters)
{
	// 기획 C2: 같은 계정 새 접속은 첫 시도에 수락, 시험 컨트롤러는 UNetConnection이 없어 실제 연결 종료 없음
	// 연결 종료 뒤 엔진 정리 순서(PawnLeavingGame → Logout → AddInactivePlayer → CleanupPlayerState)는
	// DisconnectIntegrationPlayer로 재현, 대기 타이머 대신 같은 판정 함수를 시각 지정 호출, 실제 연결 종료·Welcome은 M2
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("중복 접속 교체 시험 월드"), Fixture.IsReady()))
	{
		return false;
	}
	ADigetherElevatorActor* Elevator = PrepareIntegrationRun(Fixture);
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	const FUniqueNetIdRepl HostId = MakeRuntimeIntegrationNetId();
	ADigetherGameplayPlayerController* Host = MakeRuntimeIntegrationPlayer(Fixture, HostId, true);
	const FUniqueNetIdRepl GuestId = MakeRuntimeIntegrationNetId();
	ADigetherGameplayPlayerController* Guest = MakeRuntimeIntegrationPlayer(Fixture, GuestId);
	const FUniqueNetIdRepl StuckId = MakeRuntimeIntegrationNetId();
	ADigetherGameplayPlayerController* Stuck = MakeRuntimeIntegrationPlayer(Fixture, StuckId);
	if (!TestNotNull(TEXT("중복 접속 시험 엘리베이터"), Elevator) || !TestNotNull(TEXT("중복 접속 시험 호스트"), Host)
		|| !TestNotNull(TEXT("중복 접속 시험 참가자"), Guest) || !TestNotNull(TEXT("정리되지 않을 참가자"), Stuck))
	{
		return false;
	}
	APawn* HostPawn = GiveIntegrationPawn(Fixture, Host, FVector(0.0, 0.0, 200.0));
	APawn* GuestPawn = GiveIntegrationPawn(Fixture, Guest, FVector(500.0, 0.0, 200.0));
	APawn* StuckPawn = GiveIntegrationPawn(Fixture, Stuck, FVector(-500.0, 0.0, 200.0));
	if (!TestNotNull(TEXT("중복 접속 호스트 Pawn"), HostPawn) || !TestNotNull(TEXT("중복 접속 참가자 Pawn"), GuestPawn)
		|| !TestNotNull(TEXT("정리되지 않을 참가자 Pawn"), StuckPawn))
	{
		return false;
	}
	ADigetherGameplayPlayerState* Original = Guest->GetPlayerState<ADigetherGameplayPlayerState>();
	FDigetherFrameworkSaveIntegrationAccess::Depart(*Mode);
	if (!TestTrue(TEXT("중복 접속 시험 실제 출발"),
		Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase == EDigetherRunPhase::InProgress))
	{
		return false;
	}
	const FString DuplicateMessage = DigetherJoinRules::MakeRejectionMessage(EDigetherSessionText::AccountAlreadyConnected);

	// 첫 시도: 승인 콜백 보류, 이전 연결 정리 대기, 이 시점엔 이전 Pawn 파괴·빙의 해제 없음
	const TSharedRef<FDigetherAsyncLoginResult> GuestResult = MakeShared<FDigetherAsyncLoginResult>();
	Mode->PreLoginAsync(FString(), FString(), GuestId, MakeAsyncLoginObserver(GuestResult));
	TestFalse(TEXT("이전 연결이 남아 있으면 승인 보류"), GuestResult->bCompleted);
	TestEqual(TEXT("교체 대기 하나"), FDigetherFrameworkSaveIntegrationAccess::PendingLoginReplacements(*Mode), 1);
	TestTrue(TEXT("대기 중 이전 Pawn 유지"), IsValid(GuestPawn) && Guest->GetPawn() == GuestPawn);

	// 대기 중 동기 Login 재검사는 이전 연결 유지, ②로 거절
	FString Error;
	TestNull(TEXT("대기 중 같은 ID의 Login 재검사는 거절"),
		Mode->Login(nullptr, ROLE_AutonomousProxy, FString(), FString(), GuestId, Error));
	TestEqual(TEXT("Login 재검사 사유는 ②"), Error, DuplicateMessage);

	// 호스트 자신과 같은 ID는 끊지 않고 바로 ②로 거절
	const TSharedRef<FDigetherAsyncLoginResult> HostResult = MakeShared<FDigetherAsyncLoginResult>();
	Mode->PreLoginAsync(FString(), FString(), HostId, MakeAsyncLoginObserver(HostResult));
	TestTrue(TEXT("호스트 ID 중복은 바로 ② 거절"), HostResult->bCompleted && HostResult->Error == DuplicateMessage);
	TestTrue(TEXT("호스트 자신의 연결·Pawn은 유지"), IsValid(Host) && Host->GetPawn() == HostPawn);

	// 탐사 중 완전 신규 참가는 비동기 경로에서도 즉시 ③으로 거절
	const TSharedRef<FDigetherAsyncLoginResult> NewResult = MakeShared<FDigetherAsyncLoginResult>();
	Mode->PreLoginAsync(FString(), FString(), MakeRuntimeIntegrationNetId(), MakeAsyncLoginObserver(NewResult));
	TestTrue(TEXT("탐사 중 신규 참가는 ③ 거절"), NewResult->bCompleted
		&& NewResult->Error == DigetherJoinRules::MakeRejectionMessage(EDigetherSessionText::RunInProgress));

	// 같은 계정의 더 늦은 접속 시 앞선 대기는 ②로 종료, 늦은 접속이 교체 승계
	const TSharedRef<FDigetherAsyncLoginResult> LaterResult = MakeShared<FDigetherAsyncLoginResult>();
	Mode->PreLoginAsync(FString(), FString(), GuestId, MakeAsyncLoginObserver(LaterResult));
	TestTrue(TEXT("앞선 대기는 ②로 완료"), GuestResult->bCompleted && GuestResult->Error == DuplicateMessage);
	TestFalse(TEXT("늦은 접속은 아직 대기"), LaterResult->bCompleted);

	// 이전 연결 종료·엔진 정리 완료, 탐사 중이라 비활성 PS·남긴 Pawn으로 보관
	DisconnectIntegrationPlayer(*Mode, *Guest);
	TestTrue(TEXT("이전 연결 정리 후 남긴 Pawn 유지"), IsValid(GuestPawn));
	FDigetherFrameworkSaveIntegrationAccess::PollLoginReplacementsAt(*Mode, FPlatformTime::Seconds());
	TestTrue(TEXT("정리되면 같은 시도를 승인"), LaterResult->bCompleted && LaterResult->Error.IsEmpty());
	TestEqual(TEXT("승인 뒤 교체 대기 없음"), FDigetherFrameworkSaveIntegrationAccess::PendingLoginReplacements(*Mode), 0);
	Error.Reset();
	APlayerController* Joining = Mode->Login(nullptr, ROLE_AutonomousProxy, FString(), FString(), GuestId, Error);
	if (!TestNotNull(TEXT("승인된 접속의 실제 Login 수락"), Joining))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("탐사 중 교체는 이전 PlayerState를 그대로 이어받음"), Joining->GetPlayerState<ADigetherGameplayPlayerState>(), Original);
	TestTrue(TEXT("탐사 중 교체는 런 참가 자격 유지"), FDigetherFrameworkSaveIntegrationAccess::IsParticipant(*Mode, Original));
	Mode->HandleStartingNewPlayer_Implementation(Joining);
	TestTrue(TEXT("탐사 중 교체는 남긴 Pawn에 다시 빙의"), Joining->GetPawn() == GuestPawn);

	// 제한 시간 내 이전 연결 미정리 시 ②로 거절, 남은 연결은 유지
	const TSharedRef<FDigetherAsyncLoginResult> StuckResult = MakeShared<FDigetherAsyncLoginResult>();
	Mode->PreLoginAsync(FString(), FString(), StuckId, MakeAsyncLoginObserver(StuckResult));
	TestFalse(TEXT("정리 전에는 대기"), StuckResult->bCompleted);
	FDigetherFrameworkSaveIntegrationAccess::PollLoginReplacementsAt(*Mode,
		FPlatformTime::Seconds() + DigetherJoinRules::LoginReplacementTimeoutSeconds + 1.0);
	TestTrue(TEXT("제한 시간 초과는 ② 거절"), StuckResult->bCompleted && StuckResult->Error == DuplicateMessage);
	TestTrue(TEXT("시간 초과 뒤 남은 연결·Pawn 유지"), IsValid(Stuck) && Stuck->GetPawn() == StuckPawn);
	TestEqual(TEXT("시간 초과 뒤 교체 대기 없음"), FDigetherFrameworkSaveIntegrationAccess::PendingLoginReplacements(*Mode), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkHubDuplicateLoginTest,
	"DIGETHER.Game.Framework.SaveIntegration.DuplicateLoginInHubReleasesSeatThenJoins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkHubDuplicateLoginTest::RunTest(const FString& Parameters)
{
	// 거점의 같은 계정 교체: 이전 연결은 거점 이탈 규칙대로 몸·자리 반환, 새 접속은 저장 장부로 같은 시도에 입장
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("거점 중복 접속 시험 월드"), Fixture.IsReady()))
	{
		return false;
	}
	ADigetherElevatorActor* Elevator = PrepareIntegrationRun(Fixture);
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	ADigetherGameplayPlayerController* Host = MakeRuntimeIntegrationPlayer(Fixture, MakeRuntimeIntegrationNetId(), true);
	const FUniqueNetIdRepl GuestId = MakeRuntimeIntegrationNetId();
	ADigetherGameplayPlayerController* Guest = MakeRuntimeIntegrationPlayer(Fixture, GuestId);
	if (!TestNotNull(TEXT("거점 시험 엘리베이터"), Elevator) || !TestNotNull(TEXT("거점 시험 호스트"), Host)
		|| !TestNotNull(TEXT("거점 시험 참가자"), Guest))
	{
		return false;
	}
	APawn* GuestPawn = GiveIntegrationPawn(Fixture, Guest, FVector(500.0, 0.0, 200.0));
	if (!TestNotNull(TEXT("거점 참가자 Pawn"), GuestPawn))
	{
		return false;
	}
	ADigetherGameplayPlayerState* Original = Guest->GetPlayerState<ADigetherGameplayPlayerState>();
	FDigetherWalletState Wallet;
	Wallet.BalanceCredits = 64;
	Original->RestoreState(Wallet);
	const FGuid GameplayId = Original->GetGameplayPlayerId();

	const TSharedRef<FDigetherAsyncLoginResult> Result = MakeShared<FDigetherAsyncLoginResult>();
	Mode->PreLoginAsync(FString(), FString(), GuestId, MakeAsyncLoginObserver(Result));
	TestFalse(TEXT("이전 연결이 남아 있으면 승인 보류"), Result->bCompleted);
	DisconnectIntegrationPlayer(*Mode, *Guest);
	TestFalse(TEXT("거점 교체의 이전 몸은 제거"), IsValid(GuestPawn));
	FDigetherFrameworkSaveIntegrationAccess::PollLoginReplacementsAt(*Mode, FPlatformTime::Seconds());
	TestTrue(TEXT("정리되면 같은 시도를 승인"), Result->bCompleted && Result->Error.IsEmpty());
	FString Error;
	APlayerController* Joining = Mode->Login(nullptr, ROLE_AutonomousProxy, FString(), FString(), GuestId, Error);
	if (!TestNotNull(TEXT("거점 교체 접속의 실제 Login 수락"), Joining))
	{
		AddError(Error);
		return false;
	}
	TestFalse(TEXT("거점 교체는 관전이 아닌 일반 참가"), FDigetherFrameworkSaveIntegrationAccess::IsJoinSpectator(*Mode, Joining));
	Mode->HandleStartingNewPlayer_Implementation(Joining);
	ADigetherGameplayPlayerState* Player = Joining->GetPlayerState<ADigetherGameplayPlayerState>();
	if (!TestNotNull(TEXT("교체 접속 상태"), Player))
	{
		return false;
	}
	TestTrue(TEXT("거점 교체는 새 몸 생성"), Joining->GetPawn() != nullptr);
	TestEqual(TEXT("같은 서버 GUID로 복원"), Player->GetGameplayPlayerId(), GameplayId);
	TestTrue(TEXT("장부는 초기 지급 없이 이전 값 유지"), IntegrationStatesEqual(Player->CaptureState(), Wallet));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkChainSummonTest,
	"DIGETHER.Game.Framework.SaveIntegration.HubDoorCloseChainSummonsOutsidePlayers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkChainSummonTest::RunTest(const FString& Parameters)
{
	// 소집 RPC는 서버 이동과 별개인 소유 클라이언트 연출 신호, 실제 원격 전달·BP 암전은 M2
	const UFunction* Summoned = ADigetherGameplayPlayerController::StaticClass()->FindFunctionByName(TEXT("Client_ChainSummoned"));
	if (TestNotNull(TEXT("사슬 소집 연출 RPC 등록"), Summoned))
	{
		TestTrue(TEXT("사슬 소집 연출은 reliable 클라이언트 RPC"),
			Summoned->HasAllFunctionFlags(FUNC_Net | FUNC_NetClient | FUNC_NetReliable));
	}
	TestNotNull(TEXT("BP 사슬 소집 연출 이벤트 등록"),
		ADigetherGameplayPlayerController::StaticClass()->FindFunctionByName(TEXT("ReceiveChainSummoned")));

	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("사슬 소집 시험 월드"), Fixture.IsReady()))
	{
		return false;
	}
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	FDigetherDoorIntegrationRun Run;
	if (!TestTrue(TEXT("레버·문 신호를 제품 초기화로 연결"), PrepareDoorIntegrationRun(Fixture, Run)))
	{
		return false;
	}
	ADigetherGameplayPlayerController* Guest = MakeRuntimeIntegrationPlayer(Fixture, MakeRuntimeIntegrationNetId());
	if (!TestNotNull(TEXT("엘리베이터 밖 참가자"), Guest))
	{
		return false;
	}
	const FVector ElevatorLocation = Run.Elevator->GetActorLocation();
	APawn* GuestPawn = GiveIntegrationPawn(Fixture, Guest, ElevatorLocation + FVector(900.0, 0.0, 100.0));
	if (!TestNotNull(TEXT("엘리베이터 밖 참가자 Pawn"), GuestPawn))
	{
		return false;
	}
	UpdateIntegrationOverlaps(*Run.Elevator, Run.HostPawn);
	UpdateIntegrationOverlaps(*Run.Elevator, GuestPawn);
	TArray<APawn*> Boarded;
	Run.Elevator->GetOverlappingPawns(Boarded);
	TestTrue(TEXT("호스트는 직접 탑승"), Boarded.Contains(Run.HostPawn));
	TestFalse(TEXT("참가자는 엘리베이터 밖"), Boarded.Contains(GuestPawn));
	const FVector HostLocation = Run.HostPawn->GetActorLocation();
	const FVector GuestOutside = GuestPawn->GetActorLocation();

	if (!TestNotEqual(TEXT("출발 게이지 완료 뒤 거점 문 닫힘"), PullDepartureUntilDoorClosing(*Mode, Run), IntegrationTickTimedOut))
	{
		return false;
	}
	TestTrue(TEXT("문이 닫히는 동안에는 소집하지 않음"), GuestPawn->GetActorLocation().Equals(GuestOutside));
	TestNotEqual(TEXT("문 닫힘 완료 뒤 출발"), TickIntegrationUntil(*Mode, [Mode]()
		{
			return Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase == EDigetherRunPhase::InProgress;
		}, ToIntegrationTicks(FDigetherElevatorTuning().HubDoorCloseSeconds) + 1), IntegrationTickTimedOut);

	// 기획: 출발 인원 = 문 닫힘 완료 때 직접 탑승 + 사슬 소집 인원
	TestEqual(TEXT("출발 인원은 직접 탑승 + 소집 2명"), FDigetherFrameworkSaveIntegrationAccess::DepartureCount(*Mode), 2);
	TestTrue(TEXT("소집된 참가자는 런 참가자"), FDigetherFrameworkSaveIntegrationAccess::IsParticipant(*Mode,
		Guest->GetPlayerState<ADigetherGameplayPlayerState>()));
	const UBoxComponent* Volume = Run.Elevator->GetBoardingVolume();
	const FVector Extent = Volume->GetUnscaledBoxExtent();
	const FVector GuestLocal = Volume->GetComponentTransform().InverseTransformPosition(GuestPawn->GetActorLocation());
	TestTrue(TEXT("소집된 참가자는 엘리베이터 바닥 안쪽"),
		FMath::Abs(GuestLocal.X) < Extent.X && FMath::Abs(GuestLocal.Y) < Extent.Y && FMath::Abs(GuestLocal.Z) < Extent.Z);
	TestTrue(TEXT("직접 탑승자는 엘리베이터 안 위치 유지"), Run.HostPawn->GetActorLocation().Equals(HostLocation, 0.01));
	TestTrue(TEXT("소집 위치는 직접 탑승자와 겹치지 않음"),
		FVector::Dist2D(GuestPawn->GetActorLocation(), Run.HostPawn->GetActorLocation())
			>= DigetherFrameworkDefaults::PlayerSpacingCm - 0.01);
	// 기획: 제한 시간은 갱도 도착부터 경과, 출발 직후엔 전체 시간 유지
	TestEqual(TEXT("하강 중 남은 시간은 전체 제한 시간"), Mode->GetRunCoordinatorComponent()->GetRemainingSeconds(),
		static_cast<double>(MakeIntegrationTuning().RunSeconds));
	TestFalse(TEXT("하강 중 시계 정지"), Mode->GetRunCoordinatorComponent()->IsClockStarted());
	TestNotEqual(TEXT("첫 하강 뒤 하부 대기"), TickIntegrationUntil(*Mode,
		[&Run]() { return GetIntegrationElevatorPhase(*Run.Elevator) == EDigetherElevatorPhase::BottomWaiting; },
		ToIntegrationTicks(MakeIntegrationTuning().ElevatorTravelSeconds) + 1), IntegrationTickTimedOut);
	TestTrue(TEXT("갱도 도착 때 시계 시작"), Mode->GetRunCoordinatorComponent()->IsClockStarted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkDoorCloseBeforeTimeExpiryTest,
	"DIGETHER.Game.Framework.SaveIntegration.MineDoorCloseResolvedBeforeSameTickTimeExpiry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkDoorCloseBeforeTimeExpiryTest::RunTest(const FString& Parameters)
{
	// 같은 처리 시점(같은 Tick)에 갱도 문 닫힘 완료와 시간 종료가 겹치면 닫힘 완료(편 확정) 먼저 판정
	// 그 Tick에 닫힘 미완료면 닫히는 중인 편을 시간 종료 귀환으로 처리
	for (const bool bDoorCompletesSameTick : { true, false })
	{
		FDigetherSaveIntegrationWorld Fixture(this);
		if (!TestTrue(TEXT("문 닫힘·시간 종료 순서 시험 월드"), Fixture.IsReady()))
		{
			return false;
		}
		ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
		FDigetherDoorIntegrationRun Run;
		if (!TestTrue(TEXT("레버·문 신호를 제품 초기화로 연결"), PrepareDoorIntegrationRun(Fixture, Run))
			|| !TestNotEqual(TEXT("거점 문 닫힘"), PullDepartureUntilDoorClosing(*Mode, Run), IntegrationTickTimedOut)
			|| !TestNotEqual(TEXT("출발 확정"), TickIntegrationUntil(*Mode, [Mode]()
				{
					return Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase == EDigetherRunPhase::InProgress;
				}, ToIntegrationTicks(FDigetherElevatorTuning().HubDoorCloseSeconds) + 1), IntegrationTickTimedOut)
			|| !TestNotEqual(TEXT("하부 대기"), TickIntegrationUntil(*Mode,
				[&Run]() { return GetIntegrationElevatorPhase(*Run.Elevator) == EDigetherElevatorPhase::BottomWaiting; },
				ToIntegrationTicks(MakeIntegrationTuning().ElevatorTravelSeconds) + 1), IntegrationTickTimedOut))
		{
			return false;
		}
		UpdateIntegrationOverlaps(*Run.Elevator, Run.HostPawn);
		if (!TestTrue(TEXT("상승 레버 실제 요청"), Mode->BeginLeverPull(Run.Host, Run.AscentLever))
			|| !TestNotEqual(TEXT("갱도 문 닫힘"), TickIntegrationUntil(*Mode,
				[&Run]() { return GetIntegrationElevatorPhase(*Run.Elevator) == EDigetherElevatorPhase::DoorClosing; },
				ToIntegrationTicks(FDigetherElevatorTuning().AscentHoldSeconds) + 1), IntegrationTickTimedOut))
		{
			return false;
		}
		// 게이지 완료 Tick에 문이 한 간격 닫혀 닫힘 완료까지 (간격 수 - 1) Tick 남음
		const int32 RemainingDoorTicks = ToIntegrationTicks(FDigetherElevatorTuning().MineDoorCloseSeconds) - 1;
		const int32 TicksBeforeExpiry = bDoorCompletesSameTick ? RemainingDoorTicks - 1 : RemainingDoorTicks - 2;
		TickIntegrationUntil(*Mode, []() { return false; }, TicksBeforeExpiry);
		if (!TestEqual(TEXT("시간 종료 직전 갱도 문은 닫히는 중"), GetIntegrationElevatorPhase(*Run.Elevator),
			EDigetherElevatorPhase::DoorClosing))
		{
			return false;
		}
		// 서버 시각을 크게 넘겨 다음 Tick 시작 시 시간 종료 신호 대기
		Mode->GetRunCoordinatorComponent()->UpdateTime(Fixture.GetWorld()->GetTimeSeconds() + 100000.0);
		Mode->Tick(IntegrationTickSeconds);
		TestTrue(TEXT("시간 종료로 탐사 종료"),
			Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase != EDigetherRunPhase::InProgress);
		TestEqual(bDoorCompletesSameTick ? TEXT("같은 Tick 닫힘 완료는 편을 먼저 확정")
				: TEXT("닫힘 전 시간 종료는 편 확정 없이 귀환"),
			Mode->GetRunCoordinatorComponent()->GetConfirmedAscentCount(), bDoorCompletesSameTick ? 1 : 0);
		TestEqual(TEXT("탑승자는 어느 쪽이든 귀환"),
			Run.Host->GetPlayerState<ADigetherGameplayPlayerState>()->GetLastRunResult().Outcome, EDigetherRunReturnOutcome::Returned);
	}
	return true;
}

namespace
{
	// 기획 1.1.0 저장 정책 시험용 디스크 보조 함수, 슬롯 이름은 TrackHostSlots가 추적·정리
	FDigetherSaveSlotNames MakeIntegrationSlotNames(const FUniqueNetIdRepl& HostId)
	{
		FString Key;
		DigetherSaveRules::MakeAuthenticatedPlayerKey(HostId, Key);
		return DigetherSaveStorage::MakeSlotNames(Key);
	}

	// 엔진 저장 헤더가 아닌 바이트, 두 슬롯 모두 이러면 일반 쓰기는 CorruptData로 거절(손상 파일 보호)
	const TArray<uint8>& GetIntegrationCorruptBytes()
	{
		static const TArray<uint8> Bytes = { 0x44, 0x49, 0x47, 0x21, 0xff, 0xff, 0xff, 0xff };
		return Bytes;
	}

	bool CorruptIntegrationSlots(const FDigetherSaveSlotNames& Names)
	{
		return UGameplayStatics::SaveDataToSlot(GetIntegrationCorruptBytes(), Names.Primary, 0)
			&& UGameplayStatics::SaveDataToSlot(GetIntegrationCorruptBytes(), Names.Backup, 0);
	}

	void RepairIntegrationSlots(const FDigetherSaveSlotNames& Names)
	{
		for (const FString& Slot : { Names.Primary, Names.Backup })
		{
			if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
			{
				UGameplayStatics::DeleteGameInSlot(Slot, 0);
			}
		}
	}

	bool IsIntegrationSlotCorrupt(const FString& Slot)
	{
		TArray<uint8> Actual;
		return UGameplayStatics::LoadDataFromSlot(Actual, Slot, 0) && Actual == GetIntegrationCorruptBytes();
	}

	EDigetherSaveResult ReadIntegrationProgress(const ADigetherGameplayGameMode& Mode, const FUniqueNetIdRepl& HostId,
		FDigetherSaveSnapshot& OutSnapshot)
	{
		UDigetherSaveSubsystem* Save = Mode.GetGameInstance()->GetSubsystem<UDigetherSaveSubsystem>();
		const EDigetherSaveResult Bound = Save->BindHostIdentity(HostId);
		return Bound == EDigetherSaveResult::Success ? Save->Read(OutSnapshot) : Bound;
	}

	int32 GetIntegrationSavedBalance(const FDigetherSaveSnapshot& Snapshot, const FUniqueNetIdRepl& Id)
	{
		FDigetherSavePlayerRecord Record;
		bool bSkip = false;
		return DigetherSaveRules::TryResolveReturningPlayer(Snapshot.Players, Id, Record, bSkip) == EDigetherSaveResult::Success
			? Record.Wallet.BalanceCredits : -1;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkAutosaveFailureRetryTest,
	"DIGETHER.Game.Framework.SaveIntegration.AutosaveFailureRetriesWithoutBlockingDeparture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkAutosaveFailureRetryTest::RunTest(const FString& Parameters)
{
	// 기획 S30 예외·T91: 플레이 중 자동 저장 실패는 알림·계속 재시도, 출발은 막지 않음
	// 오래된 실패 건은 더 최신 저장 시점으로 교체, 실패를 정상 저장으로 표시하지 않음
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("저장 실패 재시도 시험 월드"), Fixture.IsReady()))
	{
		return false;
	}
	ADigetherElevatorActor* Elevator = PrepareIntegrationRun(Fixture);
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	const FUniqueNetIdRepl HostId = MakeRuntimeIntegrationNetId();
	ADigetherGameplayPlayerController* Host = MakeRuntimeIntegrationPlayer(Fixture, HostId, true);
	if (!TestNotNull(TEXT("재시도 시험 엘리베이터"), Elevator) || !TestNotNull(TEXT("재시도 시험 호스트"), Host)
		|| !TestNotNull(TEXT("재시도 시험 호스트 Pawn"), GiveIntegrationPawn(Fixture, Host, FVector(0.0, 0.0, 200.0))))
	{
		return false;
	}
	const FDigetherSaveSlotNames Names = MakeIntegrationSlotNames(HostId);
	if (!TestTrue(TEXT("양쪽 슬롯을 손상시켜 자동 저장 실패 유도"), CorruptIntegrationSlots(Names)))
	{
		return false;
	}
	AddExpectedMessagePlain(TEXT("자동 저장(RunCompleted) 실패"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 2);
	AddExpectedMessagePlain(TEXT("자동 저장 재시도 실패"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1);
	AddExpectedMessagePlain(TEXT("Message=자동 저장에 실패했습니다. 진행은 계속되며 자동으로 다시 시도합니다."),
		ELogVerbosity::Display, EAutomationExpectedMessageFlags::Contains, 2);
	AddExpectedMessagePlain(TEXT("Message=자동 저장을 다시 시도해 저장했습니다."), ELogVerbosity::Display, EAutomationExpectedMessageFlags::Contains, 1);

	FDigetherFrameworkSaveIntegrationAccess::Depart(*Mode);
	TestTrue(TEXT("출발은 디스크 저장을 요구하지 않음"),
		Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase == EDigetherRunPhase::InProgress);
	TestTrue(TEXT("출발은 디스크 슬롯을 쓰지 않음"), IsIntegrationSlotCorrupt(Names.Primary) && IsIntegrationSlotCorrupt(Names.Backup));
	FDigetherFrameworkSaveIntegrationAccess::Finish(*Mode);
	if (!TestTrue(TEXT("첫 탐사 정상 귀환"),
		Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase != EDigetherRunPhase::InProgress))
	{
		return false;
	}
	TestTrue(TEXT("귀환 자동 저장 실패를 재시도 대기로 보관"), FDigetherFrameworkSaveIntegrationAccess::HasPendingAutosave(*Mode));
	TestEqual(TEXT("대기 저장은 귀환 판정 반영 뒤 2일차"), FDigetherFrameworkSaveIntegrationAccess::PendingAutosaveDay(*Mode), 2);
	TestTrue(TEXT("재시도 타이머 예약"), FDigetherFrameworkSaveIntegrationAccess::IsAutosaveRetryScheduled(*Mode));
	TestTrue(TEXT("실패한 저장은 손상 슬롯을 덮지 않음"), IsIntegrationSlotCorrupt(Names.Primary) && IsIntegrationSlotCorrupt(Names.Backup));

	FDigetherFrameworkSaveIntegrationAccess::RetryAutosave(*Mode);
	TestTrue(TEXT("원인이 남아 있으면 재시도도 실패하고 계속 대기"), FDigetherFrameworkSaveIntegrationAccess::HasPendingAutosave(*Mode)
		&& FDigetherFrameworkSaveIntegrationAccess::IsAutosaveRetryScheduled(*Mode));

	FDigetherFrameworkSaveIntegrationAccess::Depart(*Mode);
	if (!TestTrue(TEXT("저장 재시도 중에도 다음 출발 허용(T91)"),
		Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase == EDigetherRunPhase::InProgress))
	{
		return false;
	}
	FDigetherFrameworkSaveIntegrationAccess::Finish(*Mode);
	TestEqual(TEXT("다음 저장 시점이 오면 대기 저장을 최신 3일차로 교체"),
		FDigetherFrameworkSaveIntegrationAccess::PendingAutosaveDay(*Mode), 3);

	RepairIntegrationSlots(Names);
	FDigetherFrameworkSaveIntegrationAccess::Depart(*Mode);
	if (!TestTrue(TEXT("세 번째 출발"), Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase == EDigetherRunPhase::InProgress))
	{
		return false;
	}
	FDigetherFrameworkSaveIntegrationAccess::RetryAutosave(*Mode);
	TestFalse(TEXT("재시도 성공 뒤 대기 해제"), FDigetherFrameworkSaveIntegrationAccess::HasPendingAutosave(*Mode));
	TestFalse(TEXT("재시도 성공 뒤 타이머 해제"), FDigetherFrameworkSaveIntegrationAccess::IsAutosaveRetryScheduled(*Mode));
	FDigetherSaveSnapshot Saved;
	if (TestTrue(TEXT("재시도로 쓴 진행 읽기"), ReadIntegrationProgress(*Mode, HostId, Saved) == EDigetherSaveResult::Success))
	{
		TestEqual(TEXT("탐사 중 재시도는 현재 탐사가 아닌 마지막 정상 완료(3일차)를 씀"), Saved.Quota.Quota.Day, 3);
		TestEqual(TEXT("두 번의 탐사 종료로 전체 일차도 3일차로 저장"), Saved.Quota.Quota.TotalDay, 3);
		TestEqual(TEXT("저장된 진행은 탐사 전 단계"), Saved.RunPublicState.Phase, EDigetherRunPhase::NotStarted);
		TestEqual(TEXT("두 번의 정산 완료 기록 보존"), Saved.CompletedSettlementRunIds.Num(), 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkCommonLoadingFailureTest,
	"DIGETHER.Game.Framework.SaveIntegration.CommonLoadingFailureKeepsPreparationDeadlineAndDay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkCommonLoadingFailureTest::RunTest(const FString& Parameters)
{
	// 기획 S02·S24·S30·T59: 공통 로딩 실패는 디스크 쓰기 없이 같은 일차 거점으로 복귀
	// 구매·납부 등 확정된 준비와 게이지 완료 때 기록한 4일차 마감 유지, 일차 미소비
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("공통 로딩 실패 시험 월드"), Fixture.IsReady()))
	{
		return false;
	}
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	FDigetherDoorIntegrationRun Run;
	if (!TestTrue(TEXT("레버·문 신호를 제품 초기화로 연결"), PrepareDoorIntegrationRun(Fixture, Run)))
	{
		return false;
	}
	const FUniqueNetIdRepl HostId = Run.Host->PlayerState->GetUniqueId();
	ADigetherGameplayPlayerState* HostState = Run.Host->GetPlayerState<ADigetherGameplayPlayerState>();
	UDigetherQuotaStateComponent* QuotaComponent = Fixture.GetGameState()->GetQuotaComponent();
	FDigetherQuotaState Quota = FDigetherEconomyRules::MakeInitialQuotaState(MakeIntegrationTuning());
	Quota.Day = 4;
	FDigetherWalletState Wallet;
	Wallet.BalanceCredits = 100;
	if (!TestEqual(TEXT("4일차 거점 준비"), QuotaComponent->SetQuotaState(Quota, false), EDigetherEconomyResult::Success)
		|| !TestEqual(TEXT("디스크 저장 전 잔고"), HostState->RestoreState(Wallet), EDigetherEconomyResult::Success))
	{
		return false;
	}
	// 4일차 자동 저장(T59 전제)을 먼저 디스크에 기록, 이후 준비는 디스크에 없음
	FDigetherFrameworkSaveIntegrationAccess::RequestAutosave(*Mode);
	FDigetherSaveSnapshot Disk;
	if (!TestTrue(TEXT("4일차 자동 저장 기록"), ReadIntegrationProgress(*Mode, HostId, Disk) == EDigetherSaveResult::Success))
	{
		return false;
	}
	Wallet.BalanceCredits = 67;
	Quota.DepositedCredits = 30;
	if (!TestEqual(TEXT("저장 이후 구매로 줄어든 잔고"), HostState->RestoreState(Wallet), EDigetherEconomyResult::Success)
		|| !TestEqual(TEXT("저장 이후 확정 납부"), QuotaComponent->SetQuotaState(Quota, false), EDigetherEconomyResult::Success))
	{
		return false;
	}
	if (!TestNotEqual(TEXT("4일차 출발 게이지 완료"), PullDepartureUntilDoorClosing(*Mode, Run), IntegrationTickTimedOut))
	{
		return false;
	}
	TestTrue(TEXT("게이지 완료 순간 4일차 마감 기록(PQ-22 현재 기준)"),
		Mode->GetRunCoordinatorComponent()->CapturePublicState().bPaymentDeadline);
	const int32 DoorTicks = ToIntegrationTicks(FDigetherElevatorTuning().HubDoorCloseSeconds);
	if (!TestNotEqual(TEXT("문 닫힘 완료 뒤 출발 확정"), TickIntegrationUntil(*Mode, [Mode]()
		{
			return Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase == EDigetherRunPhase::InProgress;
		}, DoorTicks + 1), IntegrationTickTimedOut))
	{
		return false;
	}
	TestFalse(TEXT("갱도 도착 전(로딩 단계)"), Mode->GetRunCoordinatorComponent()->IsClockStarted());

	TestTrue(TEXT("공통 로딩 실패 복귀"), Mode->RecoverFromCommonLoadingFailure());
	const FDigetherRunPublicState Returned = Mode->GetRunCoordinatorComponent()->CapturePublicState();
	TestEqual(TEXT("탐사를 시작하지 않은 거점 상태"), Returned.Phase, EDigetherRunPhase::NotStarted);
	TestTrue(TEXT("기록한 4일차 마감 유지"), Returned.bPaymentDeadline);
	TestEqual(TEXT("엘리베이터는 거점 대기"), GetIntegrationElevatorPhase(*Run.Elevator), EDigetherElevatorPhase::HubWaiting);
	const FDigetherQuotaState After = QuotaComponent->CaptureState().Quota;
	TestEqual(TEXT("일차를 소비하지 않음"), After.Day, 4);
	TestEqual(TEXT("확정 납부 유지"), After.DepositedCredits, 30);
	TestEqual(TEXT("구매 뒤 잔고 유지(디스크로 되돌리지 않음)"), HostState->CaptureState().BalanceCredits, 67);
	FDigetherSaveSnapshot DiskAfter;
	if (TestTrue(TEXT("복귀 뒤 디스크 진행 읽기"), ReadIntegrationProgress(*Mode, HostId, DiskAfter) == EDigetherSaveResult::Success))
	{
		TestEqual(TEXT("복귀는 디스크를 쓰지 않음"), GetIntegrationSavedBalance(DiskAfter, HostId), 100);
		TestEqual(TEXT("복귀는 디스크 납부를 바꾸지 않음"), DiskAfter.Quota.Quota.DepositedCredits, 0);
	}

	// S02: 공통 로딩 실패 복귀 뒤 레버 취소에도 기록한 마감 유지
	if (TestTrue(TEXT("복귀 뒤 출발 레버 다시 잡기"), Mode->BeginLeverPull(Run.Host, Run.DepartureLever)))
	{
		Mode->ReleaseLeverPull(Run.Host, Run.DepartureLever);
	}
	TestTrue(TEXT("복귀 뒤 레버 취소에도 마감 유지"), Mode->GetRunCoordinatorComponent()->CapturePublicState().bPaymentDeadline);

	// 갱도 도착 후 제한 시간 경과 뒤엔 로딩 단계가 아니라 되돌리지 않음
	if (!TestNotEqual(TEXT("다시 출발 게이지 완료"), PullDepartureUntilDoorClosing(*Mode, Run), IntegrationTickTimedOut)
		|| !TestNotEqual(TEXT("다시 출발 확정"), TickIntegrationUntil(*Mode, [Mode]()
			{
				return Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase == EDigetherRunPhase::InProgress;
			}, DoorTicks + 1), IntegrationTickTimedOut)
		|| !TestNotEqual(TEXT("갱도 도착"), TickIntegrationUntil(*Mode, [Mode]()
			{
				return Mode->GetRunCoordinatorComponent()->IsClockStarted();
			}, ToIntegrationTicks(MakeIntegrationTuning().ElevatorTravelSeconds) + 2), IntegrationTickTimedOut))
	{
		return false;
	}
	AddExpectedError(TEXT("공통 로딩 실패 복귀 거절"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("갱도 도착 뒤 복귀 거절"), Mode->RecoverFromCommonLoadingFailure());
	TestEqual(TEXT("거절 뒤 탐사 유지"), Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase,
		EDigetherRunPhase::InProgress);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkHostExitSaveTest,
	"DIGETHER.Game.Framework.SaveIntegration.HostExitSavesHubButNotExploration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkHostExitSaveTest::RunTest(const FString& Parameters)
{
	// 기획 S30 ③·T23·T91: 정상 거점 종료는 직전 구매·납부까지 저장, 탐사 중 종료는 미저장
	// 종료 저장 실패는 알리고 저장 없이 종료, 재시도로 종료를 붙잡지 않음
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("호스트 종료 저장 시험 월드"), Fixture.IsReady()))
	{
		return false;
	}
	ADigetherElevatorActor* Elevator = PrepareIntegrationRun(Fixture);
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	const FUniqueNetIdRepl HostId = MakeRuntimeIntegrationNetId();
	ADigetherGameplayPlayerController* Host = MakeRuntimeIntegrationPlayer(Fixture, HostId, true);
	if (!TestNotNull(TEXT("종료 시험 엘리베이터"), Elevator) || !TestNotNull(TEXT("종료 시험 호스트"), Host)
		|| !TestNotNull(TEXT("종료 시험 호스트 Pawn"), GiveIntegrationPawn(Fixture, Host, FVector(0.0, 0.0, 200.0))))
	{
		return false;
	}
	ADigetherGameplayPlayerState* HostState = Host->GetPlayerState<ADigetherGameplayPlayerState>();
	UDigetherQuotaStateComponent* QuotaComponent = Fixture.GetGameState()->GetQuotaComponent();
	FDigetherWalletState Wallet;
	Wallet.BalanceCredits = 67;
	FDigetherQuotaState Quota = QuotaComponent->CaptureState().Quota;
	Quota.DepositedCredits = 20;
	if (!TestEqual(TEXT("종료 전 구매 잔고"), HostState->RestoreState(Wallet), EDigetherEconomyResult::Success)
		|| !TestEqual(TEXT("종료 전 납부"), QuotaComponent->SetQuotaState(Quota, false), EDigetherEconomyResult::Success))
	{
		return false;
	}

	FDigetherFrameworkSaveIntegrationAccess::BeginWorldTearDown(*Mode, nullptr);
	FDigetherSaveSnapshot Saved;
	TestTrue(TEXT("다른 월드의 정리 시작에는 저장하지 않음"), ReadIntegrationProgress(*Mode, HostId, Saved)
		== EDigetherSaveResult::NotFound);
	FDigetherFrameworkSaveIntegrationAccess::BeginWorldTearDown(*Mode, Fixture.GetWorld());
	if (TestTrue(TEXT("정상 거점 종료 저장"), ReadIntegrationProgress(*Mode, HostId, Saved) == EDigetherSaveResult::Success))
	{
		TestEqual(TEXT("직전 구매 잔고 저장"), GetIntegrationSavedBalance(Saved, HostId), 67);
		TestEqual(TEXT("직전 납부 저장"), Saved.Quota.Quota.DepositedCredits, 20);
	}

	FDigetherFrameworkSaveIntegrationAccess::Depart(*Mode);
	if (!TestTrue(TEXT("탐사 출발"), Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase == EDigetherRunPhase::InProgress))
	{
		return false;
	}
	Wallet.BalanceCredits = 5;
	HostState->RestoreState(Wallet);
	TestFalse(TEXT("탐사 중 종료는 저장하지 않음"), FDigetherFrameworkSaveIntegrationAccess::SaveOnHostExit(*Mode));
	if (TestTrue(TEXT("탐사 중 종료 뒤 디스크 읽기"), ReadIntegrationProgress(*Mode, HostId, Saved) == EDigetherSaveResult::Success))
	{
		TestEqual(TEXT("다음 실행은 마지막 정상 저장 잔고"), GetIntegrationSavedBalance(Saved, HostId), 67);
		TestEqual(TEXT("탐사 상태는 저장하지 않음"), Saved.RunPublicState.Phase, EDigetherRunPhase::NotStarted);
	}

	FDigetherFrameworkSaveIntegrationAccess::Finish(*Mode);
	TestTrue(TEXT("종료 시험 귀환"), Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase != EDigetherRunPhase::InProgress);
	const FDigetherSaveSlotNames Names = MakeIntegrationSlotNames(HostId);
	if (!TestTrue(TEXT("귀환 뒤 슬롯 손상으로 종료 저장 실패 유도"), CorruptIntegrationSlots(Names)))
	{
		return false;
	}
	AddExpectedError(TEXT("호스트 종료 저장 실패"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedMessagePlain(TEXT("Message=종료 저장에 실패했습니다. 저장하지 않고 종료하며, 다음 실행은 마지막 유효 저장에서 시작합니다."),
		ELogVerbosity::Display, EAutomationExpectedMessageFlags::Contains, 1);
	TestFalse(TEXT("종료 저장 실패를 성공으로 보고하지 않음"), FDigetherFrameworkSaveIntegrationAccess::SaveOnHostExit(*Mode));
	TestFalse(TEXT("종료 저장 실패는 재시도 대기를 남기지 않음"), FDigetherFrameworkSaveIntegrationAccess::HasPendingAutosave(*Mode));
	TestFalse(TEXT("종료 저장 실패는 재시도 타이머를 남기지 않음"), FDigetherFrameworkSaveIntegrationAccess::IsAutosaveRetryScheduled(*Mode));
	TestTrue(TEXT("실패한 종료 저장은 슬롯을 덮지 않음"), IsIntegrationSlotCorrupt(Names.Primary) && IsIntegrationSlotCorrupt(Names.Backup));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkStartupSlotSelectionTest,
	"DIGETHER.Game.Framework.SaveIntegration.StartupUsesSelectedSlotForNewGameOrContinue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkStartupSlotSelectionTest::RunTest(const FString& Parameters)
{
	// 기획 S01 2행·S30 205·208행: 방 생성 때 고른 슬롯 하나만 새 게임·이어하기 대상
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("슬롯 선택 시험 월드"), Fixture.IsReady())
		|| !TestNotNull(TEXT("슬롯 선택 시험 런 준비"), PrepareIntegrationRun(Fixture)))
	{
		return false;
	}
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	const FUniqueNetIdRepl HostId = MakeRuntimeIntegrationNetId();
	ADigetherGameplayPlayerController* Host = MakeRuntimeIntegrationPlayer(Fixture, HostId, true);
	if (!TestNotNull(TEXT("슬롯 선택 시험 호스트"), Host)
		|| !TestNotNull(TEXT("슬롯 선택 시험 호스트 Pawn"), GiveIntegrationPawn(Fixture, Host, FVector(0.0, 0.0, 200.0))))
	{
		return false;
	}
	FString HostKey;
	DigetherSaveRules::MakeAuthenticatedPlayerKey(HostId, HostKey);
	const auto SlotExists = [&HostKey](const int32 SlotIndex)
	{
		const FDigetherSaveSlotNames Names = DigetherSaveStorage::MakeSlotNames(HostKey, SlotIndex);
		return UGameplayStatics::DoesSaveGameExist(Names.Primary, 0) || UGameplayStatics::DoesSaveGameExist(Names.Backup, 0);
	};
	UDigetherSaveSubsystem* Save = Mode->GetGameInstance()->GetSubsystem<UDigetherSaveSubsystem>();
	TestEqual(TEXT("선택 전 기본은 슬롯 0"), Save->GetSlotSelection().SlotIndex, 0);
	TestEqual(TEXT("선택 전 기본은 이어하기"), Save->GetSlotSelection().StartMode, EDigetherSaveStartMode::Continue);

	FDigetherSaveSlotSelection NewGameSlot;
	NewGameSlot.SlotIndex = 1;
	NewGameSlot.StartMode = EDigetherSaveStartMode::NewGame;
	if (!TestTrue(TEXT("방 밖(독립 실행)에서 슬롯 1 새 게임 선택"), Save->SetSlotSelection(NewGameSlot)))
	{
		return false;
	}
	TestTrue(TEXT("새 게임 선택이면 시작 분기는 새 게임"),
		FDigetherFrameworkSaveIntegrationAccess::RunStartupSaveAction(*Mode, Host));
	TestTrue(TEXT("새 게임은 고른 슬롯 1에 저장"), SlotExists(1));
	TestFalse(TEXT("슬롯 0은 건드리지 않음"), SlotExists(0));
	TestFalse(TEXT("슬롯 2는 건드리지 않음"), SlotExists(2));
	TestEqual(TEXT("새 게임 저장 뒤 같은 슬롯 유지"), Save->GetSlotSelection().SlotIndex, 1);
	TestEqual(TEXT("새 게임 저장 뒤에는 이어하기로 바뀌어 다시 초기화하지 않음"),
		Save->GetSlotSelection().StartMode, EDigetherSaveStartMode::Continue);
	FDigetherSaveSnapshot Saved;
	if (TestTrue(TEXT("슬롯 1 새 게임 읽기"), Save->Read(Saved) == EDigetherSaveResult::Success))
	{
		TestEqual(TEXT("새 게임은 기간 1일차"), Saved.Quota.Quota.Day, 1);
		TestEqual(TEXT("새 게임은 전체 1일차"), Saved.Quota.Quota.TotalDay, 1);
	}

	// 같은 슬롯 이어하기: 새 게임 재생성 없이 불러오기
	TestFalse(TEXT("이어하기 선택이면 시작 분기는 불러오기"),
		FDigetherFrameworkSaveIntegrationAccess::RunStartupSaveAction(*Mode, Host));
	TestFalse(TEXT("이어하기 뒤에도 슬롯 0은 비어 있음"), SlotExists(0));

	// 빈 슬롯 이어하기는 쓰기 없음, 첫 자동 저장 시점 전엔 파일 미생성
	FDigetherSaveSlotSelection EmptyContinue;
	EmptyContinue.SlotIndex = 2;
	TestTrue(TEXT("빈 슬롯 2 이어하기 선택"), Save->SetSlotSelection(EmptyContinue));
	TestFalse(TEXT("빈 슬롯 이어하기는 불러오기 분기"),
		FDigetherFrameworkSaveIntegrationAccess::RunStartupSaveAction(*Mode, Host));
	TestFalse(TEXT("빈 슬롯 이어하기는 파일을 만들지 않음"), SlotExists(2));
	TestTrue(TEXT("다른 슬롯의 새 게임 저장은 보존"), SlotExists(1));

	FDigetherSaveSlotSelection OutOfRange;
	OutOfRange.SlotIndex = DigetherSaveTuning::MaxProgressSlots;
	AddExpectedMessagePlain(TEXT("진행 슬롯 선택 거절"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	TestFalse(TEXT("범위 밖 슬롯 선택 거절"), Save->SetSlotSelection(OutOfRange));
	TestEqual(TEXT("거절 시 이전 선택 유지"), Save->GetSlotSelection().SlotIndex, 2);
	return true;
}

namespace
{
	// 기획 S33 게임 오버 통합 시험 공통 준비, 위치·시각은 시험 픽스처이며 기획 값 아님(금액만 S33 247행·T29 예시)
	struct FDigetherGameOverIntegrationRun
	{
		ADigetherElevatorActor* Elevator = nullptr;
		FUniqueNetIdRepl HostId;
		ADigetherGameplayPlayerController* Host = nullptr;
		ADigetherGameplayPlayerController* Guest = nullptr;
		APawn* HostPawn = nullptr;
		APawn* GuestPawn = nullptr;
		FGuid EndedGameId;
	};

	void SetIntegrationResultStats(ADigetherGameplayPlayerController* Controller, const TCHAR* Name, const int32 JoinOrder,
		const int32 Earned, const int32 Hidden, const int32 DeathLoss, const int32 Refund)
	{
		ADigetherGameplayPlayerState* Player = Controller->GetPlayerState<ADigetherGameplayPlayerState>();
		Player->SetPlayerName(Name);
		Player->SetJoinOrder(JoinOrder);
		FDigetherWalletState Wallet;
		Wallet.LifetimeEarnedCredits = Earned;
		Wallet.HiddenLifetimeCredits = Hidden;
		Wallet.TotalDeathLossCredits = DeathLoss;
		Wallet.LifetimeCorpseRefundCredits = Refund;
		Player->RestoreState(Wallet);
	}

	// 4일차 미달 출발 → 탐사 종료 → 5일차 실패 귀환(게임 오버 확정)까지 제품 경로(출발·귀환 정산)로 진행
	bool ReachIntegrationGameOver(FDigetherSaveIntegrationWorld& Fixture, FDigetherGameOverIntegrationRun& Out)
	{
		Out.Elevator = PrepareIntegrationRun(Fixture);
		ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
		if (!IsValid(Out.Elevator))
		{
			return false;
		}
		// 흡입 중심(우물)을 엘리베이터 자리에 배치, 맵 배치 아닌 시험 픽스처
		const ADigetherHubWellActor* Well = Fixture.GetWorld()->SpawnActor<ADigetherHubWellActor>(
			Out.Elevator->GetActorLocation(), FRotator::ZeroRotator);
		Out.HostId = MakeRuntimeIntegrationNetId();
		Out.Host = MakeRuntimeIntegrationPlayer(Fixture, Out.HostId, true);
		Out.Guest = MakeRuntimeIntegrationPlayer(Fixture, MakeRuntimeIntegrationNetId());
		if (!IsValid(Well) || !IsValid(Out.Host) || !IsValid(Out.Guest))
		{
			return false;
		}
		Out.HostPawn = GiveIntegrationPawn(Fixture, Out.Host, FVector(0.0, 0.0, 200.0));
		Out.GuestPawn = GiveIntegrationPawn(Fixture, Out.Guest, FVector(500.0, 0.0, 200.0));
		if (!IsValid(Out.HostPawn) || !IsValid(Out.GuestPawn))
		{
			return false;
		}
		// 이번 게임 누적 통계(지급마다 누적), 잔고 0이라 마지막 탐사 사망 손실 0
		SetIntegrationResultStats(Out.Host, TEXT("Host"), 1, 700, 100, 400, 200);
		SetIntegrationResultStats(Out.Guest, TEXT("Guest"), 2, 350, 50, 0, 0);
		UDigetherQuotaStateComponent* Quota = Fixture.GetGameState()->GetQuotaComponent();
		FDigetherQuotaState Day4 = Quota->CaptureState().Quota;
		Day4.Day = 4;
		Day4.TotalDay = 4;
		if (Quota->SetQuotaState(Day4, false) != EDigetherEconomyResult::Success)
		{
			return false;
		}
		Mode->GetRunCoordinatorComponent()->HandleDepartureLeverCompleted(Day4);
		FDigetherFrameworkSaveIntegrationAccess::Depart(*Mode);
		if (Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase != EDigetherRunPhase::InProgress)
		{
			return false;
		}
		Out.EndedGameId = FDigetherFrameworkSaveIntegrationAccess::GetGameId(*Mode);
		FDigetherFrameworkSaveIntegrationAccess::Finish(*Mode);
		return Mode->GetRunCoordinatorComponent()->CapturePublicState().bGameOver;
	}

	// 흡입 시작 시각, 실패 사건 단계에서만 유효
	double GetIntegrationSuctionStart(const ADigetherGameplayGameMode& Mode)
	{
		const FDigetherGameOverPublicState& State = Mode.GetGameOverFlowComponent()->GetPublicState();
		return DigetherGameOverRules::GetSuctionStartSeconds(State.PhaseStartServerSeconds, State.Tuning);
	}

	// 현재 재생 기준점 기준 크레딧 종료 서버 시각
	double GetIntegrationCreditEnd(const ADigetherGameplayGameMode& Mode)
	{
		const FDigetherCreditPlayback& Playback = Mode.GetGameOverFlowComponent()->GetPublicState().Playback;
		return Playback.AnchorServerSeconds
			+ (Playback.TotalSeconds - Playback.AnchorProgressSeconds) / FMath::Max(Playback.PlaybackRate, 1.f);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkGameOverFlowTest,
	"DIGETHER.Game.Framework.SaveIntegration.GameOverSettlementFailureCreditsNewGame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkGameOverFlowTest::RunTest(const FString& Parameters)
{
	// T28·T29·T33·T38: 5일차 실패 귀환 → 검은 개인 정산 → 실패 사건(먼저 흡입된 사람 대기) → 함께 크레딧(호스트만 빨리감기)
	// → 현재 슬롯만 새 게임 한 번, 실패 사건·크레딧 중 출발·참가·불러오기·새 게임 요청·개발 지급 거절
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("게임 오버 통합 시험 월드"), Fixture.IsReady()))
	{
		return false;
	}
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	FDigetherGameOverIntegrationRun Run;
	if (!TestTrue(TEXT("4일차 미달 탐사 뒤 5일차 실패 귀환"), ReachIntegrationGameOver(Fixture, Run)))
	{
		return false;
	}
	UDigetherGameOverFlowComponent* Flow = Mode->GetGameOverFlowComponent();
	UDigetherGameOverPublicStateComponent* Public = Fixture.GetGameState()->GetGameOverPublicStateComponent();
	ADigetherGameplayPlayerState* HostState = Run.Host->GetPlayerState<ADigetherGameplayPlayerState>();
	ADigetherGameplayPlayerState* GuestState = Run.Guest->GetPlayerState<ADigetherGameplayPlayerState>();
	UDigetherSaveSubsystem* Save = Mode->GetGameInstance()->GetSubsystem<UDigetherSaveSubsystem>();
	TestEqual(TEXT("귀환 직후 검은 정산 화면 단계"), Flow->GetPhase(), EDigetherGameOverPhase::SettlementScreen);
	TestEqual(TEXT("공개 상태도 정산 화면"), Public->GetPhase(), EDigetherGameOverPhase::SettlementScreen);
	TestTrue(TEXT("5일차 실패 귀환도 개인 정산 결과를 보냄"), HostState->GetLastRunResult().IsValid());
	TestTrue(TEXT("귀환 순간 히든 전원 공개 없음"),
		Fixture.GetGameState()->GetQuotaComponent()->CaptureState().HiddenReveals.IsEmpty());
	TestFalse(TEXT("크레딧 전 결과 비공개"), Public->GetPublishedResult().IsValid());
	TestTrue(TEXT("귀환 즉시 같은 게임 ID 유지(슬롯 초기화는 크레딧 뒤)"),
		FDigetherFrameworkSaveIntegrationAccess::GetGameId(*Mode) == Run.EndedGameId);

	FDigetherSaveSnapshot Confirmed;
	if (!TestTrue(TEXT("게임 오버 확정 저장 읽기"), ReadIntegrationProgress(*Mode, Run.HostId, Confirmed) == EDigetherSaveResult::Success))
	{
		return false;
	}
	TestTrue(TEXT("확정 저장에 게임 오버 표시"), Confirmed.RunPublicState.bGameOver);
	TestTrue(TEXT("확정 저장은 끝난 게임 ID"), Confirmed.GameId == Run.EndedGameId);
	TestTrue(TEXT("확정 저장에 결과 기록"), Confirmed.bHasGameResult);
	TestEqual(TEXT("확정 저장 결과의 전체 획득"), Confirmed.GameResult.TotalEarnedCredits, 1050);
	// T38 준비: 다른 진행 슬롯 1에 별도 진행 B, 이 게임 오버는 슬롯 0만 변경
	FDigetherSaveSlotSelection SlotZero;
	FDigetherSaveSlotSelection SlotOne;
	SlotOne.SlotIndex = 1;
	const FGuid OtherGameId = FGuid::NewGuid();
	{
		FDigetherSaveSnapshot Other = Confirmed;
		Other.GameId = OtherGameId;
		Other.Quota.Quota.Day = 2;
		Other.Quota.Quota.TotalDay = 2;
		Other.RunPublicState = FDigetherRunPublicState();
		Other.bHasGameResult = false;
		Other.GameResult = FDigetherGameResult();
		TestTrue(TEXT("슬롯 1 선택"), Save->SetSlotSelection(SlotOne));
		TestEqual(TEXT("슬롯 1에 별도 진행 B 저장"), Save->Write(Other, true), EDigetherSaveResult::Success);
		TestTrue(TEXT("슬롯 0 다시 선택"), Save->SetSlotSelection(SlotZero));
	}

	// 잠금: 출발·신규 참가·불러오기·새 게임 요청·개발 지급·할당량 조작 거절
	TestTrue(TEXT("게임 오버 중 신규 참가 잠금"), FDigetherFrameworkSaveIntegrationAccess::IsNewJoinLocked(*Mode));
	TestFalse(TEXT("게임 오버 중 새 게임 요청 거절"), Mode->StartNewGame(Run.Host));
	TestFalse(TEXT("게임 오버 중 불러오기 거절"), Mode->LoadProgress(Run.Host));
	TestFalse(TEXT("게임 오버 중 개발 지급 거절"), Mode->GrantCredits(Run.Host, 10));
	TestFalse(TEXT("게임 오버 중 할당량 조작 거절"), Mode->SetQuotaProgressForDev(Run.Host, 1));
	FDigetherFrameworkSaveIntegrationAccess::Depart(*Mode);
	TestTrue(TEXT("게임 오버 중 출발 거절"), Mode->GetRunCoordinatorComponent()->CapturePublicState().Phase
		!= EDigetherRunPhase::InProgress);

	const FDigetherGameOverTuning Tuning = Flow->GetPublicState().Tuning;
	const double SettlementStart = Flow->GetPublicState().PhaseStartServerSeconds;
	FDigetherFrameworkSaveIntegrationAccess::TickGameOverAt(*Mode, SettlementStart + Tuning.SettlementScreenSeconds - 0.01);
	TestEqual(TEXT("정산 화면 시간 전에는 정산 화면 유지"), Flow->GetPhase(), EDigetherGameOverPhase::SettlementScreen);
	FDigetherFrameworkSaveIntegrationAccess::TickGameOverAt(*Mode, SettlementStart + Tuning.SettlementScreenSeconds);
	TestEqual(TEXT("정산 화면 뒤 실패 사건"), Public->GetPhase(), EDigetherGameOverPhase::FailureEvent);
	TestFalse(TEXT("실패 사건 진입 때 검은 정산 화면 닫힘"), HostState->GetLastRunResult().IsValid());
	TestTrue(TEXT("실패 사건 중 몸 충돌 유지(조작 유지)"), Run.HostPawn->GetActorEnableCollision());

	// 호스트는 우물 가까이, 참가자는 멀리, 가까운 사람이 먼저 흡입돼 대기(T34)
	Run.GuestPawn->SetActorLocation(Run.Elevator->GetActorLocation() + FVector(-5000.0, 0.0, 200.0));
	const double Suction = GetIntegrationSuctionStart(*Mode);
	FDigetherFrameworkSaveIntegrationAccess::TickGameOverAt(*Mode, Suction - 0.01);
	TestFalse(TEXT("흡입 시작 전에는 흡입 없음"), Public->IsPlayerAbsorbed(HostState));
	FDigetherFrameworkSaveIntegrationAccess::TickGameOverAt(*Mode, Suction + 1.5);
	TestTrue(TEXT("가까운 호스트 먼저 흡입"), Public->IsPlayerAbsorbed(HostState));
	TestFalse(TEXT("먼 참가자는 아직"), Public->IsPlayerAbsorbed(GuestState));
	TestEqual(TEXT("호스트는 참가자를 기다림"), Public->GetWaitingPlayerCount(), 1);
	TestEqual(TEXT("대기 중 크레딧 시작 안 함"), Public->GetPhase(), EDigetherGameOverPhase::FailureEvent);
	TestFalse(TEXT("흡입된 몸은 충돌을 끔"), Run.HostPawn->GetActorEnableCollision());
	FDigetherFrameworkSaveIntegrationAccess::TickGameOverAt(*Mode, Suction + 2.1);
	TestTrue(TEXT("참가자 흡입"), Public->IsPlayerAbsorbed(GuestState));
	if (!TestEqual(TEXT("전원 흡입 뒤 함께 크레딧"), Public->GetPhase(), EDigetherGameOverPhase::Credits))
	{
		return false;
	}
	const FDigetherGameResult Shown = Public->GetPublishedResult();
	TestEqual(TEXT("T29 전체 획득 1,050"), Shown.TotalEarnedCredits, 1050);
	if (TestEqual(TEXT("결과 인원"), Shown.Players.Num(), 2))
	{
		TestEqual(TEXT("참가 순서 1번 호스트"), Shown.Players[0].DisplayName, FString(TEXT("Host")));
		TestEqual(TEXT("호스트 획득 700"), Shown.Players[0].EarnedCredits, 700);
		TestEqual(TEXT("참가자 획득 350"), Shown.Players[1].EarnedCredits, 350);
		TestEqual(TEXT("호스트 히든 100"), Shown.Players[0].HiddenCredits, 100);
		TestEqual(TEXT("참가자 히든 50"), Shown.Players[1].HiddenCredits, 50);
		TestEqual(TEXT("호스트 사망 손실 순액 200"), Shown.Players[0].DeathLossCredits, 200);
	}

	// T33: 참가자 빨리감기는 무시, 호스트 빨리감기만 모두에게 같은 배속 적용
	const double CreditsStart = Flow->GetPublicState().PhaseStartServerSeconds;
	TestFalse(TEXT("참가자 빨리감기 거절"),
		FDigetherFrameworkSaveIntegrationAccess::SetCreditFastForwardAt(*Mode, Run.Guest, true, CreditsStart + 1.0));
	TestFalse(TEXT("참가자 요청 뒤 빨리감기 아님"), Public->IsCreditFastForward());
	TestTrue(TEXT("호스트 빨리감기"),
		FDigetherFrameworkSaveIntegrationAccess::SetCreditFastForwardAt(*Mode, Run.Host, true, CreditsStart + 1.0));
	TestEqual(TEXT("공개 재생 배속은 빨리감기 배율"), Public->GetPublicState().Playback.PlaybackRate,
		Tuning.CreditFastForwardMultiplier);
	const double CreditEnd = GetIntegrationCreditEnd(*Mode);
	FDigetherFrameworkSaveIntegrationAccess::TickGameOverAt(*Mode, CreditEnd - 0.01);
	TestEqual(TEXT("크레딧 끝 전에는 새 게임 아님"), Public->GetPhase(), EDigetherGameOverPhase::Credits);
	TestTrue(TEXT("크레딧 중 게임 ID 유지"), FDigetherFrameworkSaveIntegrationAccess::GetGameId(*Mode) == Run.EndedGameId);
	FDigetherFrameworkSaveIntegrationAccess::TickGameOverAt(*Mode, CreditEnd + 0.01);

	const FGuid NewGameId = FDigetherFrameworkSaveIntegrationAccess::GetGameId(*Mode);
	TestEqual(TEXT("크레딧 뒤 게임 오버 끝"), Flow->GetPhase(), EDigetherGameOverPhase::None);
	TestEqual(TEXT("공개 상태도 끝"), Public->GetPhase(), EDigetherGameOverPhase::None);
	TestTrue(TEXT("새 게임 ID"), NewGameId.IsValid() && NewGameId != Run.EndedGameId);
	TestFalse(TEXT("새 게임은 게임 오버 아님"), Mode->GetRunCoordinatorComponent()->CapturePublicState().bGameOver);
	TestFalse(TEXT("새 게임은 신규 참가 열림"), FDigetherFrameworkSaveIntegrationAccess::IsNewJoinLocked(*Mode));
	const FDigetherQuotaState NewQuota = Fixture.GetGameState()->GetQuotaComponent()->CaptureState().Quota;
	TestEqual(TEXT("새 게임 기간 1일차"), NewQuota.Day, 1);
	TestEqual(TEXT("새 게임 전체 1일차"), NewQuota.TotalDay, 1);
	TestEqual(TEXT("이전 통계를 넘기지 않음(호스트)"), HostState->CaptureState().LifetimeEarnedCredits, 0);
	TestEqual(TEXT("이전 통계를 넘기지 않음(참가자)"), GuestState->CaptureState().LifetimeEarnedCredits, 0);
	TestEqual(TEXT("새 게임 히든 0"), GuestState->CaptureState().HiddenLifetimeCredits, 0);
	TestTrue(TEXT("새 게임 뒤 몸 충돌 복구"), Run.HostPawn->GetActorEnableCollision());
	const TArray<FDigetherGameResult>& Archived = FDigetherFrameworkSaveIntegrationAccess::GetArchivedResults(*Mode);
	if (TestEqual(TEXT("이전 게임 결과 하나 보관"), Archived.Num(), 1))
	{
		TestTrue(TEXT("보관 결과는 끝난 게임 ID"), Archived[0].GameId == Run.EndedGameId);
		TestEqual(TEXT("보관 결과 전체 획득"), Archived[0].TotalEarnedCredits, 1050);
	}
	FDigetherSaveSnapshot Saved;
	if (TestTrue(TEXT("새 게임 저장 읽기"), ReadIntegrationProgress(*Mode, Run.HostId, Saved) == EDigetherSaveResult::Success))
	{
		TestTrue(TEXT("현재 슬롯은 새 게임 ID 하나"), Saved.GameId == NewGameId);
		TestFalse(TEXT("새 게임 저장은 게임 오버 아님"), Saved.RunPublicState.bGameOver);
		TestEqual(TEXT("새 게임 저장에 이전 결과 보관"), Saved.ArchivedGameResults.Num(), 1);
		TestEqual(TEXT("새 게임 저장 1일차"), Saved.Quota.Quota.Day, 1);
	}
	// 종료 신호 재전송·다음 Tick에도 새 게임 재생성 없음(T38)
	FDigetherFrameworkSaveIntegrationAccess::TickGameOverAt(*Mode, CreditEnd + 10.0);
	TestFalse(TEXT("새 게임 생성 재요청 거절"), FDigetherFrameworkSaveIntegrationAccess::CreateNextGameAfterCredits(*Mode));
	TestTrue(TEXT("새 게임 ID는 하나만"), FDigetherFrameworkSaveIntegrationAccess::GetGameId(*Mode) == NewGameId);
	TestTrue(TEXT("슬롯 1 다시 선택"), Save->SetSlotSelection(SlotOne));
	FDigetherSaveSnapshot Other;
	TestTrue(TEXT("다른 슬롯의 진행 B 유지"), Save->Read(Other) == EDigetherSaveResult::Success && Other.GameId == OtherGameId);
	TestTrue(TEXT("슬롯 0 복귀"), Save->SetSlotSelection(SlotZero));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkGameOverLeaverTest,
	"DIGETHER.Game.Framework.SaveIntegration.GameOverLeaverNotWaitedAndKeptInResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkGameOverLeaverTest::RunTest(const FString& Parameters)
{
	// T34·S33 231행: 흡입 대기 중 떠난 사람은 기다리지 않음, 그 게임에서 번·잃은 돈이 있으면 결과에 유지,
	// 기록 없는 떠난 사람은 결과 제외
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("퇴장 시험 월드"), Fixture.IsReady()))
	{
		return false;
	}
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	const FUniqueNetIdRepl LeftId = MakeIntegrationNetId(TEXT("left-earned"));
	FDigetherSavePlayerRecord Left;
	DigetherSaveRules::MakeAuthenticatedPlayerKey(LeftId, Left.AuthenticatedPlayerKey);
	Left.GameplayPlayerId = FGuid::NewGuid();
	Left.DisplayName = TEXT("Left");
	Left.JoinOrder = 3;
	Left.Wallet.LifetimeEarnedCredits = 30;
	FDigetherSavePlayerRecord Idle;
	DigetherSaveRules::MakeAuthenticatedPlayerKey(MakeIntegrationNetId(TEXT("left-idle")), Idle.AuthenticatedPlayerKey);
	Idle.GameplayPlayerId = FGuid::NewGuid();
	Idle.DisplayName = TEXT("Idle");
	Idle.JoinOrder = 4;
	FDigetherFrameworkSaveIntegrationAccess::AddRecord(*Mode, Left);
	FDigetherFrameworkSaveIntegrationAccess::AddRecord(*Mode, Idle);
	FDigetherGameOverIntegrationRun Run;
	if (!TestTrue(TEXT("5일차 실패 귀환"), ReachIntegrationGameOver(Fixture, Run)))
	{
		return false;
	}
	UDigetherGameOverFlowComponent* Flow = Mode->GetGameOverFlowComponent();
	UDigetherGameOverPublicStateComponent* Public = Fixture.GetGameState()->GetGameOverPublicStateComponent();
	const FDigetherGameOverTuning Tuning = Flow->GetPublicState().Tuning;
	FDigetherFrameworkSaveIntegrationAccess::TickGameOverAt(*Mode,
		Flow->GetPublicState().PhaseStartServerSeconds + Tuning.SettlementScreenSeconds);
	Run.GuestPawn->SetActorLocation(Run.Elevator->GetActorLocation() + FVector(-5000.0, 0.0, 200.0));
	const double Suction = GetIntegrationSuctionStart(*Mode);
	FDigetherFrameworkSaveIntegrationAccess::TickGameOverAt(*Mode, Suction + 1.5);
	TestEqual(TEXT("참가자를 기다리는 중"), Public->GetWaitingPlayerCount(), 1);
	// 흡입 대기 중 참가자 퇴장(거점 이탈은 몸 제거·자리 반환)
	DisconnectIntegrationPlayer(*Mode, *Run.Guest);
	FDigetherFrameworkSaveIntegrationAccess::TickGameOverAt(*Mode, Suction + 1.6);
	if (!TestEqual(TEXT("떠난 사람을 기다리지 않고 크레딧 시작"), Public->GetPhase(), EDigetherGameOverPhase::Credits))
	{
		return false;
	}
	const FDigetherGameResult Shown = Public->GetPublishedResult();
	TArray<FString> Names;
	for (const FDigetherGameResultPlayer& Player : Shown.Players)
	{
		Names.Add(Player.DisplayName);
	}
	TestTrue(TEXT("결과는 참가 순서(호스트·참가자·먼저 떠난 사람), 기록 없는 떠난 사람 제외"),
		Names == TArray<FString>({ TEXT("Host"), TEXT("Guest"), TEXT("Left") }));
	TestEqual(TEXT("떠난 사람 획득 포함 전체"), Shown.TotalEarnedCredits, 1080);
	// 크레딧 중 남은 사람만으로 끝나도 새 게임으로 전환
	FDigetherFrameworkSaveIntegrationAccess::TickGameOverAt(*Mode, GetIntegrationCreditEnd(*Mode) + 0.01);
	TestEqual(TEXT("크레딧 뒤 새 게임"), Flow->GetPhase(), EDigetherGameOverPhase::None);
	TestTrue(TEXT("새 게임 ID"), FDigetherFrameworkSaveIntegrationAccess::GetGameId(*Mode) != Run.EndedGameId);
	FDigetherSavePlayerRecord Dropped;
	TestFalse(TEXT("떠난 사람 기록은 새 게임에 넘기지 않음"),
		FDigetherFrameworkSaveIntegrationAccess::FindSavedRecord(*Mode, LeftId, Dropped));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkSavedGameOverStartsNewGameTest,
	"DIGETHER.Game.Framework.SaveIntegration.SavedGameOverStartsSameSlotNewGame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkSavedGameOverStartsNewGameTest::RunTest(const FString& Parameters)
{
	// S33 240행: 게임 오버 확정 저장 뒤 종료면 다음 실행에서 같은 슬롯 새 게임 시작, 결과 보관(1인)
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("시작 게임 오버 시험 월드"), Fixture.IsReady())
		|| !TestNotNull(TEXT("시작 게임 오버 런 준비"), PrepareIntegrationRun(Fixture)))
	{
		return false;
	}
	ADigetherGameplayGameMode* Mode = Fixture.GetGameMode();
	const FUniqueNetIdRepl HostId = MakeRuntimeIntegrationNetId();
	ADigetherGameplayPlayerController* Host = MakeRuntimeIntegrationPlayer(Fixture, HostId, true);
	if (!TestNotNull(TEXT("시작 게임 오버 호스트"), Host)
		|| !TestNotNull(TEXT("시작 게임 오버 호스트 Pawn"), GiveIntegrationPawn(Fixture, Host, FVector(0.0, 0.0, 200.0))))
	{
		return false;
	}
	FDigetherSaveSnapshot GameOver;
	if (!TestTrue(TEXT("기준 진행 캡처"), FDigetherFrameworkSaveIntegrationAccess::Capture(*Mode, GameOver)))
	{
		return false;
	}
	GameOver.Quota.Quota.Day = 5;
	GameOver.Quota.Quota.TotalDay = 5;
	GameOver.RunPublicState.bGameOver = true;
	GameOver.RunPublicState.bPaymentDeadline = true;
	GameOver.bHasGameResult = true;
	GameOver.GameResult.ResultId = FGuid::NewGuid();
	GameOver.GameResult.GameId = GameOver.GameId;
	GameOver.GameResult.FinalTotalDay = 5;
	FDigetherGameResultPlayer& ResultPlayer = GameOver.GameResult.Players.AddDefaulted_GetRef();
	ResultPlayer.DisplayName = TEXT("Host");
	ResultPlayer.JoinOrder = 1;
	ResultPlayer.EarnedCredits = 10;
	GameOver.GameResult.TotalEarnedCredits = 10;
	UDigetherSaveSubsystem* Save = Mode->GetGameInstance()->GetSubsystem<UDigetherSaveSubsystem>();
	if (!TestEqual(TEXT("게임 오버 확정 저장 준비"), Save->BindHostIdentity(HostId), EDigetherSaveResult::Success)
		|| !TestEqual(TEXT("게임 오버 확정 저장 쓰기"), Save->Write(GameOver, true), EDigetherSaveResult::Success))
	{
		return false;
	}
	TestTrue(TEXT("다음 실행 불러오기는 같은 슬롯 새 게임"), Mode->LoadProgress(Host, true));
	const FGuid NewGameId = FDigetherFrameworkSaveIntegrationAccess::GetGameId(*Mode);
	TestTrue(TEXT("새 게임 ID"), NewGameId.IsValid() && NewGameId != GameOver.GameId);
	TestFalse(TEXT("실패 사건·크레딧을 다시 재생하지 않음"), Mode->GetGameOverFlowComponent()->IsActive());
	TestFalse(TEXT("새 게임은 게임 오버 아님"), Mode->GetRunCoordinatorComponent()->CapturePublicState().bGameOver);
	TestEqual(TEXT("새 게임 1일차"), Fixture.GetGameState()->GetQuotaComponent()->CaptureState().Quota.Day, 1);
	FDigetherSaveSnapshot Saved;
	if (TestTrue(TEXT("새 게임 저장 읽기"), ReadIntegrationProgress(*Mode, HostId, Saved) == EDigetherSaveResult::Success))
	{
		TestTrue(TEXT("같은 슬롯이 새 게임으로 바뀜"), Saved.GameId == NewGameId);
		TestFalse(TEXT("새 게임 저장은 게임 오버 아님"), Saved.RunPublicState.bGameOver);
		TestTrue(TEXT("이전 게임 결과 보관"), Saved.ArchivedGameResults.Num() == 1
			&& Saved.ArchivedGameResults[0].ResultId == GameOver.GameResult.ResultId);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkDevSaveRequestSingleResultTest,
	"DIGETHER.Game.Framework.SaveIntegration.DevSaveRequestSendsSingleResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkDevSaveRequestSingleResultTest::RunTest(const FString& Parameters)
{
	// 개발 저장·불러오기·새 게임 요청 1건당 결과 RPC 1건
	FDigetherSaveIntegrationWorld Fixture(this);
	if (!TestTrue(TEXT("개발 저장 요청 시험 월드"), Fixture.IsReady())
		|| !TestNotNull(TEXT("개발 저장 요청 런 준비"), PrepareIntegrationRun(Fixture)))
	{
		return false;
	}
	ADigetherGameplayPlayerController* Host = MakeRuntimeIntegrationPlayer(Fixture, MakeRuntimeIntegrationNetId(), true);
	if (!TestNotNull(TEXT("개발 저장 요청 호스트"), Host)
		|| !TestNotNull(TEXT("개발 저장 요청 호스트 Pawn"), GiveIntegrationPawn(Fixture, Host, FVector(0.0, 0.0, 200.0))))
	{
		return false;
	}
	// 기대 메시지는 한 줄을 첫 일치 항목에만 세므로 전체 건수는 별도 출력 장치로 집계
	struct FSaveResultLogCounter final : FOutputDevice
	{
		int32 Count = 0;
		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type, const FName& Category) override
		{
			Count += Category == LogDigetherFramework.GetCategoryName()
				&& FCString::Strstr(Message, TEXT("진행 요청 결과:")) != nullptr ? 1 : 0;
		}
	};
	FSaveResultLogCounter Counter;
	AddExpectedMessagePlain(TEXT("Succeeded=0 Message=저장한 진행을 불러오지 못했습니다."),
		ELogVerbosity::Display, EAutomationExpectedMessageFlags::Contains, 1);
	AddExpectedMessagePlain(TEXT("Succeeded=1 Message=새 게임을 초기화했습니다."),
		ELogVerbosity::Display, EAutomationExpectedMessageFlags::Contains, 1);
	AddExpectedMessagePlain(TEXT("Succeeded=1 Message=진행을 저장했습니다."),
		ELogVerbosity::Display, EAutomationExpectedMessageFlags::Contains, 1);
	AddExpectedMessagePlain(TEXT("Succeeded=1 Message=저장된 진행을 불러왔습니다."),
		ELogVerbosity::Display, EAutomationExpectedMessageFlags::Contains, 1);
	// 빈 슬롯 불러오기(경계) 뒤 새 게임·저장·불러오기(정상)
	GLog->AddOutputDevice(&Counter);
	Host->RequestLoadGame();
	Host->RequestNewGame();
	Host->RequestManualSave();
	Host->RequestLoadGame();
	GLog->Flush();
	GLog->RemoveOutputDevice(&Counter);
	TestEqual(TEXT("요청 4건에 결과 4건"), Counter.Count, 4);
	return true;
}

#endif
