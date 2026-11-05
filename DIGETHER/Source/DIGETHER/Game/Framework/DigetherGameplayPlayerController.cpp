#include "Game/Framework/DigetherGameplayPlayerController.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Game/Dev/DigetherCheatManager.h"
#include "Game/Elevator/DigetherElevatorLeverActor.h"
#include "Game/Framework/DigetherFrameworkLog.h"
#include "Game/Framework/DigetherGameplayGameMode.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Hub/DigetherHubInteractionComponent.h"
#include "Game/UI/Deposit/DigetherDepositPanelWidget.h"
#include "Game/UI/DigetherPlayerScreenComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"

ADigetherGameplayPlayerController::ADigetherGameplayPlayerController()
{
	CheatClass = UDigetherCheatManager::StaticClass();
	HubInteractionComponent = CreateDefaultSubobject<UDigetherHubInteractionComponent>(TEXT("HubInteractionComponent"));
	PlayerScreenComponent = CreateDefaultSubobject<UDigetherPlayerScreenComponent>(TEXT("PlayerScreenComponent"));
}

void ADigetherGameplayPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (PlayerScreenComponent)
	{
		PlayerScreenComponent->CreatePersistentWidgets();
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
	ReleaseHeldInputsAndHubPanel();
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
	if (PlayerScreenComponent)
	{
		PlayerScreenComponent->ReleaseWidgets();
	}
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
	if (InteractAction && HubInteractionComponent)
	{
		if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
		{
			UDigetherHubInteractionComponent* Interaction = HubInteractionComponent;
			Input->BindAction(InteractAction, ETriggerEvent::Started, Interaction, &UDigetherHubInteractionComponent::HandleInteractStarted);
			Input->BindAction(InteractAction, ETriggerEvent::Completed, Interaction, &UDigetherHubInteractionComponent::HandleInteractReleased);
			Input->BindAction(InteractAction, ETriggerEvent::Canceled, Interaction, &UDigetherHubInteractionComponent::HandleInteractReleased);
		}
	}
	// 기획 S24: 우클릭 유지 중 붓기, 떼면 중단
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
	ReleaseHeldInputsAndHubPanel();
	Super::OnPossess(InPawn);
	RefreshRunResultBinding();
}

bool ADigetherGameplayPlayerController::ShowLastRunResult()
{
	return PlayerScreenComponent && PlayerScreenComponent->ShowLastRunResult();
}

void ADigetherGameplayPlayerController::CloseRunResult()
{
	if (PlayerScreenComponent)
	{
		PlayerScreenComponent->CloseRunResult();
	}
}

bool ADigetherGameplayPlayerController::IsRunResultInputModeApplied() const
{
	return PlayerScreenComponent && PlayerScreenComponent->IsRunResultInputModeApplied();
}

void ADigetherGameplayPlayerController::RefreshRunResultBinding()
{
	if (PlayerScreenComponent)
	{
		PlayerScreenComponent->RefreshRunResultBinding();
	}
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
		// 레버 점유는 Pawn·Controller 관계가 유효할 때 먼저 해제
		GameMode->NotifyPlayerDisconnecting(this);
	}
	State->RetainPawn(LeavingPawn);
	UnPossess();
	// UnPossess가 Pawn의 PlayerState도 지움, 사망·탈출 판정은 같은 최신 상태 유지 필요
	if (IsValid(LeavingPawn))
	{
		LeavingPawn->SetPlayerState(State);
	}
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
	// 호스트 여부·크레딧 단계 판정은 GameMode 담당, 참가자 요청은 상태 불변(T33)
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
	// 흡입 대기 중 이동만 잠금, 시점 조작 유지(S33 234행, 공통 카메라·조작 잠금 컷신 없음)
	SetIgnoreMoveInput(bAbsorbed);
	bGameOverInputLocked = bAbsorbed;
	ReceiveGameOverAbsorbed(bAbsorbed);
}
