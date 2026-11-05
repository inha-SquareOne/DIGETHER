#include "Game/Framework/DigetherGameplayPlayerController.h"

#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Game/Dev/DigetherCheatManager.h"
#include "Game/Economy/DigetherDepositStationActor.h"
#include "Game/Economy/DigetherFloorCoinActor.h"
#include "Game/Elevator/DigetherElevatorLeverActor.h"
#include "Game/Framework/DigetherFrameworkDefaults.h"
#include "Game/Framework/DigetherFrameworkLog.h"
#include "Game/Framework/DigetherGameplayGameMode.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Hub/DigetherHubFacilityActor.h"
#include "Game/UI/Deposit/DigetherDepositPanelWidget.h"
#include "Game/UI/Hud/DigetherRunHudWidget.h"
#include "Game/UI/Results/DigetherGameOverCreditsWidget.h"
#include "Game/UI/Results/DigetherRunResultWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "HAL/PlatformTime.h"
#include "UObject/Class.h"

#if !UE_BUILD_SHIPPING
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "Game/Economy/DigetherQuotaStateComponent.h"
#include "Game/Economy/DigetherWalletComponent.h"
#include "Game/Elevator/DigetherElevatorActor.h"
#include "Game/Elevator/DigetherElevatorComponent.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Run/DigetherRunPublicStateComponent.h"
#include "Misc/Parse.h"

namespace
{
	// 임시 개발 시험용 식별자이며 실제 월드 납부대 배치와는 별개다.
	const FName DepositTestStationId(TEXT("GameTestDeposit"));
	// 개발 명령 금액 입력이다. 앞뒤 공백을 뺀 ASCII 숫자만 받고 int32를 넘으면 거절한다.
	constexpr int32 MaxDevCreditDigits = 10;

	bool TryParseDevCredits(const FString& Input, int32& OutCredits)
	{
		OutCredits = 0;
		const FString Trimmed = Input.TrimStartAndEnd();
		if (Trimmed.IsEmpty() || Trimmed.Len() > MaxDevCreditDigits)
		{
			return false;
		}
		int64 Value = 0;
		for (const TCHAR Character : Trimmed)
		{
			if (Character < TEXT('0') || Character > TEXT('9'))
			{
				return false;
			}
			Value = Value * 10 + (Character - TEXT('0'));
		}
		if (Value > MAX_int32)
		{
			return false;
		}
		OutCredits = static_cast<int32>(Value);
		return true;
	}
}
#endif

ADigetherGameplayPlayerController::ADigetherGameplayPlayerController()
{
	CheatClass = UDigetherCheatManager::StaticClass();
}

void ADigetherGameplayPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController() && HudWidgetClass)
	{
		HudWidget = CreateWidget<UDigetherRunHudWidget>(this, HudWidgetClass);
		if (IsValid(HudWidget))
		{
			HudWidget->AddToViewport();
		}
	}
	if (IsLocalController() && GameOverCreditsWidgetClass)
	{
		// 크레딧 단계가 아니면 위젯이 스스로 접혀 있다. 검은 정산 화면(110)보다 위에 둔다.
		GameOverCreditsWidget = CreateWidget<UDigetherGameOverCreditsWidget>(this, GameOverCreditsWidgetClass);
		if (IsValid(GameOverCreditsWidget) && !GameOverCreditsWidget->AddToPlayerScreen(120))
		{
			GameOverCreditsWidget = nullptr;
		}
	}
	RefreshRunResultBinding();
	if (IsLocalController() && InteractMappingContext)
	{
		if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Input->AddMappingContext(InteractMappingContext, 1);
			}
		}
	}
	if (IsLocalController() && PourMappingContext)
	{
		if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Input->AddMappingContext(PourMappingContext, 1);
			}
		}
	}
#if !UE_BUILD_SHIPPING
	ScheduleAutoCommands();
#endif
}

void ADigetherGameplayPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HandleInteractReleased();
	HandlePourReleased();
	CloseHubInteraction();
	if (IsLocalController() && (InteractMappingContext || PourMappingContext))
	{
		if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (InteractMappingContext)
				{
					Input->RemoveMappingContext(InteractMappingContext);
				}
				if (PourMappingContext)
				{
					Input->RemoveMappingContext(PourMappingContext);
				}
			}
		}
	}
	if (RunResultPlayerState.IsValid())
	{
		RunResultPlayerState->OnRunResultChanged.RemoveAll(this);
	}
	RunResultPlayerState.Reset();
	CloseRunResult();
	if (IsValid(HudWidget))
	{
		HudWidget->RemoveFromParent();
	}
	HudWidget = nullptr;
	if (IsValid(GameOverCreditsWidget))
	{
		GameOverCreditsWidget->RemoveFromParent();
	}
	GameOverCreditsWidget = nullptr;
	if (bLifeInputLocked)
	{
		SetIgnoreMoveInput(false);
		bLifeInputLocked = false;
	}
	if (bGameOverInputLocked)
	{
		SetIgnoreMoveInput(false);
		bGameOverInputLocked = false;
	}
	LastDepositRequestSeconds.Empty();
	Super::EndPlay(EndPlayReason);
}

void ADigetherGameplayPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InteractAction)
	{
		if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
		{
			Input->BindAction(InteractAction, ETriggerEvent::Started, this, &ThisClass::HandleInteractStarted);
			Input->BindAction(InteractAction, ETriggerEvent::Completed, this, &ThisClass::HandleInteractReleased);
			Input->BindAction(InteractAction, ETriggerEvent::Canceled, this, &ThisClass::HandleInteractReleased);
		}
	}
	// 기획 S24: 우클릭을 누르고 있는 동안 붓고, 떼면 멈춘다.
	if (PourAction)
	{
		if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
		{
			Input->BindAction(PourAction, ETriggerEvent::Started, this, &ThisClass::HandlePourStarted);
			Input->BindAction(PourAction, ETriggerEvent::Completed, this, &ThisClass::HandlePourReleased);
			Input->BindAction(PourAction, ETriggerEvent::Canceled, this, &ThisClass::HandlePourReleased);
		}
	}
}

void ADigetherGameplayPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	RefreshRunResultBinding();
	OnGameplayPlayerStateChanged.Broadcast();
}

void ADigetherGameplayPlayerController::OnPossess(APawn* InPawn)
{
	HandleInteractReleased();
	HandlePourReleased();
	CloseHubInteraction();
	Super::OnPossess(InPawn);
	RefreshRunResultBinding();
}

void ADigetherGameplayPlayerController::RefreshRunResultBinding()
{
	if (!IsLocalController() || !HasActorBegunPlay())
	{
		return;
	}
	ADigetherGameplayPlayerState* State = GetPlayerState<ADigetherGameplayPlayerState>();
	if (RunResultPlayerState.Get() != State)
	{
		if (RunResultPlayerState.IsValid())
		{
			RunResultPlayerState->OnRunResultChanged.RemoveAll(this);
		}
		RunResultPlayerState = State;
		if (IsValid(State))
		{
			State->OnRunResultChanged.AddUObject(this, &ThisClass::HandleRunResultChanged);
		}
	}
	// OnRep가 UI 준비보다 먼저 왔거나 같은 서버의 inactive 상태를 재빙의해도 현재 스냅샷을 적용한다.
	HandleRunResultChanged(IsValid(State) ? State->GetLastRunResult() : FDigetherRunResult());
}

void ADigetherGameplayPlayerController::HandleRunResultChanged(const FDigetherRunResult& Result)
{
	if (!IsLocalController())
	{
		return;
	}
	if (!Result.IsValid())
	{
		CloseRunResult();
		LastPresentedRunResultId.Invalidate();
		return;
	}
	if (LastPresentedRunResultId != Result.RunId)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogDigetherFramework, Display,
			TEXT("Run result ready: Run=%s Outcome=%s Payout=%d FinalBalance=%d"),
			*Result.RunId.ToString(), *UEnum::GetValueAsString(Result.Outcome),
			Result.PayoutCredits, Result.FinalBalanceCredits);
#endif
		ShowLastRunResult();
	}
}

bool ADigetherGameplayPlayerController::ShowLastRunResult()
{
	const ADigetherGameplayPlayerState* State = GetPlayerState<ADigetherGameplayPlayerState>();
	if (!IsLocalController() || !HasActorBegunPlay() || !IsValid(State) || !RunResultWidgetClass)
	{
		return false;
	}
	const FDigetherRunResult Result = State->GetLastRunResult();
	if (!Result.IsValid())
	{
		return false;
	}
	HandleInteractReleased();
	HandlePourReleased();
	CloseHubInteraction();
	if (!IsValid(RunResultWidget))
	{
		RunResultWidget = CreateWidget<UDigetherRunResultWidget>(this, RunResultWidgetClass);
		if (!IsValid(RunResultWidget) || !RunResultWidget->AddToPlayerScreen(110))
		{
			RunResultWidget = nullptr;
			return false;
		}
	}
	RunResultWidget->SetResult(Result);
	if (!bRunResultInputModeApplied)
	{
		bRunResultPreviousCursorVisible = bShowMouseCursor;
	}
	SetShowMouseCursor(true);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(RunResultWidget->TakeWidget());
	SetInputMode(InputMode);
	bRunResultInputModeApplied = true;
	RunResultWidget->FocusCloseButton();
	LastPresentedRunResultId = Result.RunId;
	return true;
}

void ADigetherGameplayPlayerController::CloseRunResult()
{
	if (IsValid(RunResultWidget))
	{
		RunResultWidget->RemoveFromParent();
	}
	RunResultWidget = nullptr;
	if (bRunResultInputModeApplied && IsLocalController())
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(bRunResultPreviousCursorVisible);
	}
	bRunResultInputModeApplied = false;
}

void ADigetherGameplayPlayerController::PawnLeavingGame()
{
	ADigetherGameplayPlayerState* State = GetPlayerState<ADigetherGameplayPlayerState>();
	APawn* LeavingPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!HasAuthority() || World == nullptr || World->bIsTearingDown
		|| !IsValid(State) || !State->GetUniqueId().IsValid() || !IsValid(LeavingPawn))
	{
		Super::PawnLeavingGame();
		return;
	}
	if (ADigetherGameplayGameMode* GameMode = ResolveGameplayGameMode())
	{
		// 레버 점유는 Pawn과 Controller 관계가 유효할 때 먼저 해제한다.
		GameMode->NotifyPlayerDisconnecting(this);
	}
	State->RetainPawn(LeavingPawn);
	UnPossess();
	// UnPossess는 Pawn의 PlayerState도 지운다. 사망·탈출 판정은 같은 최신 상태를 계속 사용한다.
	if (IsValid(LeavingPawn))
	{
		LeavingPawn->SetPlayerState(State);
	}
}

void ADigetherGameplayPlayerController::RequestManualSave()
{
#if !UE_BUILD_SHIPPING
	if (IsLocalController())
	{
		Server_ManualSave();
	}
#endif
}

void ADigetherGameplayPlayerController::RequestLoadGame()
{
#if !UE_BUILD_SHIPPING
	if (IsLocalController())
	{
		Server_LoadGame();
	}
#endif
}

void ADigetherGameplayPlayerController::RequestNewGame()
{
#if !UE_BUILD_SHIPPING
	if (IsLocalController())
	{
		Server_NewGame();
	}
#endif
}

void ADigetherGameplayPlayerController::RequestOpenDeposit(const FName StationId)
{
	// 폐기된 금액 입력 방식이다. BP 참조 호환을 위해 남기며 아무 동작도 하지 않는다.
}

void ADigetherGameplayPlayerController::RequestSubmitDeposit(const FGuid SessionId,
	const FName StationId, const int32 PeriodGeneration, const int32 Amount)
{
	// 폐기된 금액 입력 방식이다. BP 참조 호환을 위해 남기며 아무 동작도 하지 않는다.
}

void ADigetherGameplayPlayerController::RequestCancelDeposit(const FGuid SessionId)
{
	// 폐기된 금액 입력 방식이다. BP 참조 호환을 위해 남기며 아무 동작도 하지 않는다.
}

void ADigetherGameplayPlayerController::StartDepositInteraction(ADigetherDepositStationActor* Station)
{
	// 폐기된 금액 입력 방식이다. 정산기 납부는 StartPour(우클릭 유지)로 한다.
}

void ADigetherGameplayPlayerController::SubmitDepositInput(const FString& AmountInput)
{
	// 사용 중단된 금액 입력 위젯 호환용이다. 아무 동작도 하지 않는다.
}

void ADigetherGameplayPlayerController::CloseDepositInteraction()
{
	// 사용 중단된 금액 입력 위젯 호환용이다. 진행 중인 붓기 입력은 끝낸다.
	HandlePourReleased();
}

void ADigetherGameplayPlayerController::StartPour(ADigetherDepositStationActor* Station)
{
	if (!IsLocalController() || !IsValid(Station) || Station->GetWorld() != GetWorld()
		|| Station->GetStationId().IsNone() || bHubInputModeApplied || bRunResultInputModeApplied || bLifeInputLocked)
	{
		return;
	}
	HandleInteractReleased();
	HeldPourStation = Station;
	Server_BeginPour(Station);
}

void ADigetherGameplayPlayerController::StopPour()
{
	HandlePourReleased();
}

void ADigetherGameplayPlayerController::HandlePourStarted()
{
	StartPour(Cast<ADigetherDepositStationActor>(FindInteractionTarget()));
}

void ADigetherGameplayPlayerController::HandlePourReleased()
{
	if (HeldPourStation.IsValid() && IsLocalController())
	{
		Server_EndPour();
	}
	HeldPourStation.Reset();
#if !UE_BUILD_SHIPPING
	if (GetWorld() != nullptr)
	{
		GetWorldTimerManager().ClearTimer(DevPourTimer);
	}
#endif
}

void ADigetherGameplayPlayerController::Server_ManualSave_Implementation()
{
#if !UE_BUILD_SHIPPING
	// 결과는 GameMode가 한 번 보냄, 여기선 GameMode 전 거절만 보냄
	ADigetherGameplayGameMode* GameMode = IsDevServerRpcAllowed() ? ResolveGameplayGameMode() : nullptr;
	if (!IsValid(GameMode))
	{
		Client_SaveResult(false, NSLOCTEXT("DigetherSave", "ManualSaveRejected", "진행 저장을 완료하지 못했습니다."));
		return;
	}
	GameMode->SaveProgress(this);
#endif
}

void ADigetherGameplayPlayerController::Server_LoadGame_Implementation()
{
#if !UE_BUILD_SHIPPING
	ADigetherGameplayGameMode* GameMode = IsDevServerRpcAllowed() ? ResolveGameplayGameMode() : nullptr;
	if (!IsValid(GameMode))
	{
		Client_SaveResult(false, NSLOCTEXT("DigetherSave", "LoadRequestRejected", "저장한 진행을 불러오지 못했습니다."));
		return;
	}
	GameMode->LoadProgress(this);
#endif
}

void ADigetherGameplayPlayerController::Server_NewGame_Implementation()
{
#if !UE_BUILD_SHIPPING
	ADigetherGameplayGameMode* GameMode = IsDevServerRpcAllowed() ? ResolveGameplayGameMode() : nullptr;
	if (!IsValid(GameMode))
	{
		Client_SaveResult(false, NSLOCTEXT("DigetherSave", "NewGameRejected", "새 게임을 시작하지 못했습니다."));
		return;
	}
	GameMode->StartNewGame(this);
#endif
}

void ADigetherGameplayPlayerController::Client_SaveResult_Implementation(const bool bSucceeded, const FText& Message)
{
	UE_LOG(LogDigetherFramework, Display, TEXT("진행 요청 결과: Succeeded=%d Message=%s"),
		bSucceeded, *Message.ToString());
}

ADigetherGameplayGameMode* ADigetherGameplayPlayerController::ResolveGameplayGameMode() const
{
	if (!HasAuthority())
	{
		return nullptr;
	}
	UWorld* World = GetWorld();
	ADigetherGameplayGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<ADigetherGameplayGameMode>() : nullptr;
	return IsValid(GameMode) ? GameMode : nullptr;
}

ADigetherGameplayGameMode* ADigetherGameplayPlayerController::ResolveDepositGameMode(
	const EDigetherDepositOperation Operation, FDigetherDepositResponse& OutResponse)
{
	if (!HasAuthority())
	{
		OutResponse.Result = EDigetherEconomyResult::NotAuthority;
		return nullptr;
	}

	const double NowSeconds = FPlatformTime::Seconds();
	const double* LastRequestSeconds = LastDepositRequestSeconds.Find(Operation);
	if (LastRequestSeconds != nullptr &&
		NowSeconds - *LastRequestSeconds < DigetherFrameworkDefaults::DepositRequestIntervalSeconds)
	{
		OutResponse.Result = EDigetherEconomyResult::InvalidInput;
		OutResponse.bRateLimited = true;
		return nullptr;
	}
	LastDepositRequestSeconds.Add(Operation, NowSeconds);

	UWorld* World = GetWorld();
	ADigetherGameplayGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<ADigetherGameplayGameMode>() : nullptr;
	if (!IsValid(GameMode))
	{
		OutResponse.Result = EDigetherEconomyResult::InvalidInput;
		UE_LOG(LogDigetherFramework, Warning, TEXT("납부 요청을 처리할 GameMode가 없습니다: %s"),
			*UEnum::GetValueAsString(Operation));
		return nullptr;
	}
	return GameMode;
}

void ADigetherGameplayPlayerController::Server_BeginPour_Implementation(ADigetherDepositStationActor* Station)
{
	FDigetherDepositResponse Response;
	Response.StationId = IsValid(Station) ? Station->GetStationId() : NAME_None;
	if (ADigetherGameplayGameMode* GameMode = ResolveDepositGameMode(EDigetherDepositOperation::PourStarted, Response))
	{
		Response = GameMode->BeginPour(this, Station);
	}
	Client_DepositResult(EDigetherDepositOperation::PourStarted, Response);
}

void ADigetherGameplayPlayerController::Server_EndPour_Implementation()
{
	// 끝 요청은 빈도 제한을 두지 않는다. 놓치면 손을 뗀 뒤에도 붓기가 남는다.
	ADigetherGameplayGameMode* GameMode = ResolveGameplayGameMode();
	if (!IsValid(GameMode))
	{
		return;
	}
	FDigetherDepositResponse Response = GameMode->EndPour(this);
	if (Response.Result == EDigetherEconomyResult::Success)
	{
		Client_DepositResult(EDigetherDepositOperation::PourStopped, Response);
	}
}

void ADigetherGameplayPlayerController::Server_PickupFloorCoin_Implementation(ADigetherFloorCoinActor* Coin)
{
	FDigetherDepositResponse Response;
	if (ADigetherGameplayGameMode* GameMode = ResolveDepositGameMode(EDigetherDepositOperation::Pickup, Response))
	{
		Response = GameMode->PickupFloorCoin(this, Coin);
	}
	Client_DepositResult(EDigetherDepositOperation::Pickup, Response);
}

void ADigetherGameplayPlayerController::Server_DevSetQuotaProgress_Implementation(const int32 DepositedCredits)
{
#if !UE_BUILD_SHIPPING
	if (!IsDevServerRpcAllowed())
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("개발 서버 RPC 거절: 호스트가 아니고 서버에 -DigetherDevRpc가 없습니다."));
		return;
	}
	ADigetherGameplayGameMode* GameMode = ResolveGameplayGameMode();
	const bool bApplied = IsValid(GameMode) && GameMode->SetQuotaProgressForDev(this, DepositedCredits);
	UE_LOG(LogDigetherFramework, Display, TEXT("DigetherSetQuotaProgress 서버 결과: Applied=%d"), bApplied);
#endif
}

void ADigetherGameplayPlayerController::Server_BeginLeverPull_Implementation(ADigetherElevatorLeverActor* Lever)
{
	if (!IsValid(Lever))
	{
		return;
	}
	if (ADigetherGameplayGameMode* GameMode = ResolveGameplayGameMode())
	{
		GameMode->BeginLeverPull(this, Lever);
	}
}

void ADigetherGameplayPlayerController::Server_ReleaseLeverPull_Implementation(ADigetherElevatorLeverActor* Lever)
{
	if (!IsValid(Lever))
	{
		return;
	}
	if (ADigetherGameplayGameMode* GameMode = ResolveGameplayGameMode())
	{
		GameMode->ReleaseLeverPull(this, Lever);
	}
}

void ADigetherGameplayPlayerController::Server_CycleSpectateTarget_Implementation(const bool bNext)
{
	if (ADigetherGameplayGameMode* GameMode = ResolveGameplayGameMode())
	{
		GameMode->CycleSpectateTarget(this, bNext);
	}
}

void ADigetherGameplayPlayerController::Client_SetLifeSpectating_Implementation(const bool bSpectating)
{
	if (!IsLocalController() || bLifeInputLocked == bSpectating)
	{
		return;
	}
	SetIgnoreMoveInput(bSpectating);
	bLifeInputLocked = bSpectating;
	if (bSpectating)
	{
		APawn* ControlledPawn = GetPawn();
		UPawnMovementComponent* Movement = IsValid(ControlledPawn) ? ControlledPawn->GetMovementComponent() : nullptr;
		if (IsValid(Movement))
		{
			Movement->StopMovementImmediately();
		}
	}
}

void ADigetherGameplayPlayerController::RequestCreditFastForward(const bool bFastForward)
{
	if (IsLocalController())
	{
		Server_SetCreditFastForward(bFastForward);
	}
}

void ADigetherGameplayPlayerController::Server_SetCreditFastForward_Implementation(const bool bFastForward)
{
	// 호스트 여부·크레딧 단계 판정은 GameMode가 한다. 참가자 요청은 상태를 바꾸지 않는다(T33).
	if (ADigetherGameplayGameMode* GameMode = ResolveGameplayGameMode())
	{
		const bool bApplied = GameMode->SetCreditFastForward(this, bFastForward);
		UE_LOG(LogDigetherFramework, Verbose, TEXT("Credit fast-forward request: FastForward=%d Applied=%d"),
			bFastForward, bApplied);
	}
}

void ADigetherGameplayPlayerController::Client_SetGameOverAbsorbed_Implementation(const bool bAbsorbed)
{
	if (!IsLocalController() || bGameOverInputLocked == bAbsorbed)
	{
		return;
	}
	// 흡입 대기 중에는 이동만 막고 시점 조작은 그대로 둔다(S33 234행, 공통 카메라·조작 잠금 컷신 없음).
	SetIgnoreMoveInput(bAbsorbed);
	bGameOverInputLocked = bAbsorbed;
	ReceiveGameOverAbsorbed(bAbsorbed);
}

#if !UE_BUILD_SHIPPING
bool ADigetherGameplayPlayerController::IsDevServerRpcAllowed() const
{
	// 기획 S33: 게임 오버(실패 사건·크레딧) 중에는 개발 서버 RPC(지급·이동·진행 조작)를 받지 않는다.
	const ADigetherGameplayGameMode* GameMode = HasAuthority() && GetWorld() != nullptr
		? GetWorld()->GetAuthGameMode<ADigetherGameplayGameMode>() : nullptr;
	if (IsValid(GameMode) && GameMode->IsGameOverActive())
	{
		return false;
	}
	return IsLocalController() || FParse::Param(FCommandLine::Get(), TEXT("DigetherDevRpc"));
}
#endif

void ADigetherGameplayPlayerController::Server_RunSmoke_Implementation()
{
#if !UE_BUILD_SHIPPING
	if (!IsDevServerRpcAllowed())
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("개발 서버 RPC 거절: 호스트가 아니고 서버에 -DigetherDevRpc가 없습니다."));
		return;
	}
	if (ADigetherGameplayGameMode* GameMode = ResolveGameplayGameMode())
	{
		const bool bStarted = GameMode->StartRunSmoke(this);
		UE_LOG(LogDigetherFramework, Display, TEXT("DigetherRunSmoke 서버 결과: Started=%d"), bStarted);
	}
#endif
}

void ADigetherGameplayPlayerController::Server_DevTeleportOut_Implementation()
{
#if !UE_BUILD_SHIPPING
	if (!IsDevServerRpcAllowed())
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("개발 서버 RPC 거절: 호스트가 아니고 서버에 -DigetherDevRpc가 없습니다."));
		return;
	}
	APawn* ControlledPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!IsValid(ControlledPawn) || World == nullptr)
	{
		return;
	}
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		const FVector Target = It->GetActorLocation() + FVector(0.f, 0.f, 50.f);
		const bool bMoved = ControlledPawn->TeleportTo(Target, ControlledPawn->GetActorRotation());
		UE_LOG(LogDigetherFramework, Display, TEXT("DigetherTeleportOut 서버 결과: Moved=%d Target=%s"),
			bMoved, *Target.ToString());
		return;
	}
	UE_LOG(LogDigetherFramework, Warning, TEXT("DigetherTeleportOut: PlayerStart가 없습니다."));
#endif
}

void ADigetherGameplayPlayerController::Server_DevTeleportToDeposit_Implementation(const FName StationId)
{
#if !UE_BUILD_SHIPPING
	if (!IsDevServerRpcAllowed())
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("개발 서버 RPC 거절: 호스트가 아니고 서버에 -DigetherDevRpc가 없습니다."));
		return;
	}
	APawn* ControlledPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!IsValid(ControlledPawn) || !IsValid(World) || StationId.IsNone())
	{
		return;
	}
	ADigetherDepositStationActor* MatchedStation = nullptr;
	for (TActorIterator<ADigetherDepositStationActor> Iterator(World); Iterator; ++Iterator)
	{
		if (IsValid(*Iterator) && Iterator->GetStationId() == StationId)
		{
			if (MatchedStation)
			{
				UE_LOG(LogDigetherFramework, Warning, TEXT("DigetherTeleportToDeposit: 납부대 ID가 중복됐습니다: %s"), *StationId.ToString());
				return;
			}
			MatchedStation = *Iterator;
		}
	}
	if (!IsValid(MatchedStation))
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("DigetherTeleportToDeposit: 납부대를 찾지 못했습니다: %s"), *StationId.ToString());
		return;
	}
	const FVector Target = MatchedStation->GetActorLocation() + FVector(0.f, 0.f, 100.f);
	const bool bMoved = ControlledPawn->TeleportTo(Target, ControlledPawn->GetActorRotation());
	UE_LOG(LogDigetherFramework, Display, TEXT("DigetherTeleportToDeposit 서버 결과: Station=%s Moved=%d Target=%s"),
		*StationId.ToString(), bMoved, *Target.ToString());
#endif
}

void ADigetherGameplayPlayerController::Server_GrantCredits_Implementation(const int32 Amount)
{
#if !UE_BUILD_SHIPPING
	if (!IsDevServerRpcAllowed())
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("개발 서버 RPC 거절: 호스트가 아니고 서버에 -DigetherDevRpc가 없습니다."));
		return;
	}
	if (ADigetherGameplayGameMode* GameMode = ResolveGameplayGameMode())
	{
		const bool bGranted = GameMode->GrantCredits(this, Amount);
		UE_LOG(LogDigetherFramework, Display, TEXT("DigetherGrantCredits 서버 결과: Amount=%d Granted=%d"),
			Amount, bGranted);
	}
#endif
}

void ADigetherGameplayPlayerController::Client_DepositResult_Implementation(
	const EDigetherDepositOperation Operation, const FDigetherDepositResponse& Response)
{
	// 본인 결과만 받는다. 붓기 주기의 본인 차감·인정액은 본인에게 보여 줄 수 있다(S24). 공동 누적액은 담지 않는다.
	UE_LOG(LogDigetherFramework, Display,
		TEXT("납부 결과: Operation=%s Result=%s RateLimited=%d Station=%s Period=%d Accepted=%d Credited=%d"),
		*UEnum::GetValueAsString(Operation), *UEnum::GetValueAsString(Response.Result), Response.bRateLimited,
		*Response.StationId.ToString(), Response.PeriodGeneration, Response.AcceptedCredits, Response.CreditedCredits);
	if (!IsLocalController())
	{
		return;
	}
	const bool bStartRejected = Operation == EDigetherDepositOperation::PourStarted
		&& Response.Result != EDigetherEconomyResult::Success;
	if (bStartRejected || Operation == EDigetherDepositOperation::PourStopped)
	{
		// 서버가 붓기를 멈췄다. 계속 누르고 있어도 다시 붓지 않으며 다시 눌러야 한다(S24 ④).
		HeldPourStation.Reset();
#if !UE_BUILD_SHIPPING
		GetWorldTimerManager().ClearTimer(DevPourTimer);
#endif
	}
	OnDepositResult.Broadcast(Operation, Response);
}

#if !UE_BUILD_SHIPPING
bool ADigetherGameplayPlayerController::ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor)
{
	const TCHAR* Command = Cmd;
	if (FParse::Command(&Command, TEXT("DigetherHubTeleport")))
	{
		Server_DevTeleportToHubFacility(FName(*FString(Command).TrimStartAndEnd()));
		return true;
	}
	if (FParse::Command(&Command, TEXT("DigetherHubAction")))
	{
		TArray<FString> Args;
		FString(Command).ParseIntoArrayWS(Args);
		if (Args.Num() >= 2 && Args.Num() <= 3)
		{
			const int64 ActionValue = StaticEnum<EDigetherHubAction>()->GetValueByNameString(Args[1]);
			ADigetherHubFacilityActor* Match = nullptr;
			bool bDuplicate = false;
			for (TActorIterator<ADigetherHubFacilityActor> It(GetWorld()); It; ++It)
			{
				if (It->FacilityId == FName(*Args[0]))
				{
					bDuplicate |= Match != nullptr;
					Match = *It;
				}
			}
			if (IsValid(Match) && !bDuplicate && ActionValue != INDEX_NONE)
			{
				Server_HubFacilityRequest(Match, static_cast<EDigetherHubAction>(ActionValue),
					Args.Num() == 3 ? FName(*Args[2]) : NAME_None, FGuid::NewGuid());
			}
		}
		return true;
	}
	if (FParse::Command(&Command, TEXT("DigetherShowRunResult")))
	{
		ShowLastRunResult();
		return true;
	}
	if (FParse::Command(&Command, TEXT("DigetherPour")))
	{
		DigetherPour(FString(Command));
		return true;
	}
	if (FParse::Command(&Command, TEXT("DigetherPourStop")))
	{
		DigetherPourStop();
		return true;
	}
	if (FParse::Command(&Command, TEXT("DigetherSetQuotaProgress")))
	{
		DigetherSetQuotaProgress(FString(Command));
		return true;
	}
	if (FParse::Command(&Command, TEXT("DigetherCreditFastForward")))
	{
		DigetherCreditFastForward(FString(Command));
		return true;
	}
	if (FParse::Command(&Command, TEXT("DigetherLever")))
	{
		DigetherLever(FString(Command));
		return true;
	}
	if (FParse::Command(&Command, TEXT("DigetherRunSmoke")))
	{
		if (FString(Command).TrimStartAndEnd().IsEmpty())
		{
			DigetherRunSmoke();
		}
		else
		{
			UE_LOG(LogDigetherFramework, Warning, TEXT("사용법: DigetherRunSmoke"));
		}
		return true;
	}
	if (FParse::Command(&Command, TEXT("DigetherGrantCredits")))
	{
		DigetherGrantCredits(FString(Command));
		return true;
	}
	if (FParse::Command(&Command, TEXT("DigetherDumpState")))
	{
		DigetherDumpState();
		return true;
	}
	if (FParse::Command(&Command, TEXT("DigetherTeleportOut")))
	{
		DigetherTeleportOut();
		return true;
	}
	if (FParse::Command(&Command, TEXT("DigetherTeleportToDeposit")))
	{
		DigetherTeleportToDeposit(FString(Command));
		return true;
	}
	return Super::ProcessConsoleExec(Cmd, Ar, Executor);
}

void ADigetherGameplayPlayerController::DigetherLever(const FString& Arguments)
{
	if (!IsLocalController())
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("DigetherLever는 로컬 플레이어만 실행할 수 있습니다."));
		return;
	}

	TArray<FString> Tokens;
	Arguments.ParseIntoArrayWS(Tokens);
	const bool bDepartureToken = Tokens.Num() >= 1 && Tokens[0].Equals(TEXT("Departure"), ESearchCase::IgnoreCase);
	const bool bAscentToken = Tokens.Num() >= 1 && Tokens[0].Equals(TEXT("Ascent"), ESearchCase::IgnoreCase);
	// 세 번째 인자는 갱도 상승 레버의 안·밖 위치다. 생략하면 안쪽(Inside)이다. 출발 레버에는 쓰지 않는다.
	const bool bPlacementToken = Tokens.Num() == 3;
	const bool bValidPlacement = !bPlacementToken
		|| (bAscentToken && (Tokens[2].Equals(TEXT("Inside"), ESearchCase::IgnoreCase)
			|| Tokens[2].Equals(TEXT("Outside"), ESearchCase::IgnoreCase)));
	if ((Tokens.Num() != 2 && Tokens.Num() != 3) || (!bDepartureToken && !bAscentToken) ||
		(!Tokens[1].Equals(TEXT("Hold"), ESearchCase::IgnoreCase) &&
		 !Tokens[1].Equals(TEXT("Release"), ESearchCase::IgnoreCase)) || !bValidPlacement)
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("사용법: DigetherLever <Departure|Ascent> <Hold|Release> [Inside|Outside]"));
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	const EDigetherElevatorLeverKind LeverKind = bDepartureToken
		? EDigetherElevatorLeverKind::Departure : EDigetherElevatorLeverKind::Ascent;
	const EDigetherElevatorLeverPlacement Placement = bPlacementToken
		&& Tokens[2].Equals(TEXT("Outside"), ESearchCase::IgnoreCase)
		? EDigetherElevatorLeverPlacement::Outside : EDigetherElevatorLeverPlacement::Inside;
	ADigetherElevatorLeverActor* MatchingLever = nullptr;
	for (TActorIterator<ADigetherElevatorLeverActor> It(World); It; ++It)
	{
		ADigetherElevatorLeverActor* Lever = *It;
		if (IsValid(Lever) && Lever->GetLeverKind() == LeverKind
			&& (LeverKind != EDigetherElevatorLeverKind::Ascent || Lever->GetPlacement() == Placement))
		{
			if (MatchingLever != nullptr)
			{
				UE_LOG(LogDigetherFramework, Warning, TEXT("같은 종류의 레버가 여러 개여서 요청을 거절했습니다: %s"),
					*Tokens[0]);
				return;
			}
			MatchingLever = Lever;
		}
	}
	if (!IsValid(MatchingLever))
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("월드에서 레버를 찾지 못했습니다: %s"), *Tokens[0]);
		return;
	}

	if (Tokens[1].Equals(TEXT("Hold"), ESearchCase::IgnoreCase))
	{
		Server_BeginLeverPull(MatchingLever);
	}
	else
	{
		Server_ReleaseLeverPull(MatchingLever);
	}
}

void ADigetherGameplayPlayerController::DigetherRunSmoke()
{
	if (!IsLocalController())
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("DigetherRunSmoke는 로컬 플레이어만 실행할 수 있습니다."));
		return;
	}
	Server_RunSmoke();
}

void ADigetherGameplayPlayerController::DigetherGrantCredits(const FString& AmountInput)
{
	if (!IsLocalController())
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("DigetherGrantCredits는 로컬 플레이어만 실행할 수 있습니다."));
		return;
	}

	int32 Amount = 0;
	if (!TryParseDevCredits(AmountInput, Amount) || Amount <= 0)
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("DigetherGrantCredits 금액 입력 거절: 1 이상의 정수만 받습니다."));
		return;
	}
	Server_GrantCredits(Amount);
}

void ADigetherGameplayPlayerController::DigetherPour(const FString& Arguments)
{
	if (!IsLocalController() || !IsValid(GetPawn()))
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("DigetherPour는 몸이 있는 로컬 플레이어만 실행할 수 있습니다."));
		return;
	}
	TArray<FString> Tokens;
	Arguments.ParseIntoArrayWS(Tokens);
	const double Seconds = Tokens.Num() >= 1 && Tokens[0].IsNumeric() ? FCString::Atod(*Tokens[0]) : -1.0;
	if (Tokens.Num() < 1 || Tokens.Num() > 2 || !FMath::IsFinite(Seconds) || Seconds <= 0.0)
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("사용법: DigetherPour <초> [StationId]"));
		return;
	}
	const FName RequestedStation = Tokens.Num() == 2 ? FName(*Tokens[1]) : NAME_None;
	ADigetherDepositStationActor* Target = nullptr;
	double ClosestDistanceSquared = TNumericLimits<double>::Max();
	for (TActorIterator<ADigetherDepositStationActor> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		ADigetherDepositStationActor* Station = *Iterator;
		if (!IsValid(Station) || Station->GetStationId().IsNone()
			|| (!RequestedStation.IsNone() && Station->GetStationId() != RequestedStation))
		{
			continue;
		}
		const double DistanceSquared = FVector::DistSquared(GetPawn()->GetActorLocation(), Station->GetActorLocation());
		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			Target = Station;
		}
	}
	if (!IsValid(Target))
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("DigetherPour: 정산기를 찾지 못했습니다."));
		return;
	}
	// 실제 입력과 같은 RPC를 쓴다. 서버가 거리·입구 방향·가림을 그대로 검사한다.
	HandlePourReleased();
	StartPour(Target);
	if (IsPouring())
	{
		GetWorldTimerManager().SetTimer(DevPourTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			HandlePourReleased();
		}), static_cast<float>(Seconds), false);
	}
}

void ADigetherGameplayPlayerController::DigetherCreditFastForward(const FString& Arguments)
{
	const FString Trimmed = Arguments.TrimStartAndEnd();
	if (!IsLocalController() || (Trimmed != TEXT("0") && Trimmed != TEXT("1")))
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("사용법: DigetherCreditFastForward 1|0 (로컬 플레이어, 서버가 호스트만 반영)"));
		return;
	}
	RequestCreditFastForward(Trimmed == TEXT("1"));
}

void ADigetherGameplayPlayerController::DigetherPourStop()
{
	if (IsLocalController())
	{
		HandlePourReleased();
	}
}

void ADigetherGameplayPlayerController::DigetherSetQuotaProgress(const FString& AmountInput)
{
	if (!IsLocalController())
	{
		return;
	}
	int32 Amount = 0;
	if (!TryParseDevCredits(AmountInput, Amount))
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("사용법: DigetherSetQuotaProgress <0 이상 목표 미만의 금액>"));
		return;
	}
	Server_DevSetQuotaProgress(Amount);
}

void ADigetherGameplayPlayerController::DigetherTeleportOut()
{
	if (!IsLocalController())
	{
		return;
	}
	Server_DevTeleportOut();
}

void ADigetherGameplayPlayerController::DigetherTeleportToDeposit(const FString& StationInput)
{
	if (!IsLocalController())
	{
		return;
	}
	const FString Trimmed = StationInput.TrimStartAndEnd();
	Server_DevTeleportToDeposit(Trimmed.IsEmpty() ? DepositTestStationId : FName(*Trimmed));
}

void ADigetherGameplayPlayerController::DigetherDumpState()
{
	UWorld* World = GetWorld();
	const ADigetherGameplayGameState* GameplayState = World != nullptr
		? World->GetGameState<ADigetherGameplayGameState>() : nullptr;
	if (GameplayState == nullptr)
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("DigetherDumpState: GameState가 없습니다."));
		return;
	}

	auto ExportStruct = [](const UScriptStruct* Struct, const void* Data)
	{
		FString Text;
		Struct->ExportText(Text, Data, nullptr, nullptr, PPF_None, nullptr);
		return Text;
	};

	const FDigetherRunPublicState RunState = GameplayState->GetRunPublicStateComponent()->CaptureState();
	const FDigetherQuotaPublicState QuotaState = GameplayState->GetQuotaComponent()->CaptureState();
	const ADigetherGameplayPlayerState* SelfState = GetPlayerState<ADigetherGameplayPlayerState>();
	UE_LOG(LogDigetherFramework, Display, TEXT("Dump Self=%d Authority=%d Run=%s Quota=%s"),
		SelfState != nullptr ? SelfState->GetPlayerId() : -1, HasAuthority(),
		*ExportStruct(FDigetherRunPublicState::StaticStruct(), &RunState),
		*ExportStruct(FDigetherQuotaPublicState::StaticStruct(), &QuotaState));

	for (TActorIterator<ADigetherElevatorActor> It(World); It; ++It)
	{
		const FDigetherElevatorPublicState ElevatorState = It->GetElevatorComponent()->GetPublicState();
		UE_LOG(LogDigetherFramework, Display, TEXT("Dump Elevator=%s"),
			*ExportStruct(FDigetherElevatorPublicState::StaticStruct(), &ElevatorState));
	}

	for (const APlayerState* ListedPlayerState : GameplayState->PlayerArray)
	{
		const ADigetherGameplayPlayerState* GameplayPlayer = Cast<ADigetherGameplayPlayerState>(ListedPlayerState);
		if (GameplayPlayer == nullptr)
		{
			continue;
		}
		const FDigetherWalletState Wallet = GameplayPlayer->GetWalletComponent()->CaptureState();
		const FDigetherLifeComponentState Life = GameplayPlayer->GetLifeComponent()->CaptureState();
		const FDigetherRunResult RunResult = GameplayPlayer->GetLastRunResult();
		UE_LOG(LogDigetherFramework, Display, TEXT("Dump Player=%d IsSelf=%d Wallet=%s Life=%s RunResult=%s"),
			GameplayPlayer->GetPlayerId(), GameplayPlayer == SelfState,
			*ExportStruct(FDigetherWalletState::StaticStruct(), &Wallet),
			*ExportStruct(FDigetherLifeComponentState::StaticStruct(), &Life),
			*ExportStruct(FDigetherRunResult::StaticStruct(), &RunResult));
	}

	// 바닥 금화는 위치만 복제된다. 금액은 서버(호스트) 프로세스에서만 보인다.
	for (TActorIterator<ADigetherFloorCoinActor> It(World); It; ++It)
	{
		const ADigetherFloorCoinActor* Coin = *It;
		if (!IsValid(Coin))
		{
			continue;
		}
		UE_LOG(LogDigetherFramework, Display, TEXT("Dump FloorCoin Location=%s Station=%s Amount=%s"),
			*Coin->GetActorLocation().ToString(), *Coin->GetStationId().ToString(),
			Coin->HasAuthority() ? *FString::FromInt(Coin->GetAmountCredits()) : TEXT("hidden"));
	}
}

void ADigetherGameplayPlayerController::ScheduleAutoCommands()
{
	// 프로세스마다 한 번만 예약한다(월드 이동 뒤 반복 방지).
	static bool bAutoCommandsScheduled = false;
	if (bAutoCommandsScheduled || !IsLocalController())
	{
		return;
	}
	FString Spec;
	if (!FParse::Value(FCommandLine::Get(), TEXT("DigetherAutoCmds="), Spec) || Spec.IsEmpty())
	{
		return;
	}
	bAutoCommandsScheduled = true;

	TArray<FString> Entries;
	Spec.ParseIntoArray(Entries, TEXT("|"));
	for (const FString& Entry : Entries)
	{
		FString DelayText;
		FString CommandText;
		if (!Entry.Split(TEXT(":"), &DelayText, &CommandText) || CommandText.TrimStartAndEnd().IsEmpty())
		{
			UE_LOG(LogDigetherFramework, Warning, TEXT("DigetherAutoCmds 항목 형식 오류: %s"), *Entry);
			continue;
		}
		const float DelaySeconds = FMath::Max(0.1f, FCString::Atof(*DelayText));
		const FString Command = CommandText.TrimStartAndEnd();
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Command]()
		{
			UE_LOG(LogDigetherFramework, Display, TEXT("AutoCmd 실행: %s"), *Command);
			ConsoleCommand(Command);
		}), DelaySeconds, false);
	}
	UE_LOG(LogDigetherFramework, Display, TEXT("DigetherAutoCmds 예약: %d개"), Entries.Num());
}
#endif
