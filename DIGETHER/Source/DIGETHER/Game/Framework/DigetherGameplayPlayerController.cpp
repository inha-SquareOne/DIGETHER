#include "Game/Framework/DigetherGameplayPlayerController.h"

#include "Engine/World.h"
#include "Game/Dev/DigetherCheatManager.h"
#include "Game/Elevator/DigetherElevatorLeverActor.h"
#include "Game/Framework/DigetherFrameworkDefaults.h"
#include "Game/Framework/DigetherFrameworkLog.h"
#include "Game/Framework/DigetherGameplayGameMode.h"
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
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Run/DigetherRunPublicStateComponent.h"
#include "Game/Economy/DigetherDepositInput.h"
#include "Misc/Parse.h"

namespace
{
	// 임시 개발 시험용 식별자이며 실제 월드 납부대 배치와는 별개다.
	const FName DepositTestStationId(TEXT("GameTestDeposit"));
}
#endif

ADigetherGameplayPlayerController::ADigetherGameplayPlayerController()
{
	CheatClass = UDigetherCheatManager::StaticClass();
}

void ADigetherGameplayPlayerController::BeginPlay()
{
	Super::BeginPlay();
#if !UE_BUILD_SHIPPING
	ScheduleAutoCommands();
#endif
}

void ADigetherGameplayPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bLifeInputLocked)
	{
		SetIgnoreMoveInput(false);
		bLifeInputLocked = false;
	}
	LastDepositRequestSeconds.Empty();
#if !UE_BUILD_SHIPPING
	ResetDepositTest();
#endif
	Super::EndPlay(EndPlayReason);
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

void ADigetherGameplayPlayerController::Server_OpenDeposit_Implementation(const FName StationId)
{
	FDigetherDepositResponse Response;
	Response.StationId = StationId;
	if (ADigetherGameplayGameMode* GameMode = ResolveDepositGameMode(EDigetherDepositOperation::Open, Response))
	{
		Response = GameMode->OpenDeposit(this, StationId);
	}
	Client_DepositResult(EDigetherDepositOperation::Open, Response);
}

void ADigetherGameplayPlayerController::Server_SubmitDeposit_Implementation(const FGuid SessionId,
	const FName StationId, const int32 PeriodGeneration, const int32 Amount)
{
	FDigetherDepositResponse Response;
	Response.SessionId = SessionId;
	Response.StationId = StationId;
	Response.PeriodGeneration = PeriodGeneration;
	if (ADigetherGameplayGameMode* GameMode = ResolveDepositGameMode(EDigetherDepositOperation::Submit, Response))
	{
		Response = GameMode->SubmitDeposit(this, SessionId, StationId, PeriodGeneration, Amount);
	}
	Client_DepositResult(EDigetherDepositOperation::Submit, Response);
}

void ADigetherGameplayPlayerController::Server_CancelDeposit_Implementation(const FGuid SessionId)
{
	FDigetherDepositResponse Response;
	Response.SessionId = SessionId;
	if (ADigetherGameplayGameMode* GameMode = ResolveDepositGameMode(EDigetherDepositOperation::Cancel, Response))
	{
		Response = GameMode->CancelDeposit(this, SessionId);
	}
	Client_DepositResult(EDigetherDepositOperation::Cancel, Response);
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

#if !UE_BUILD_SHIPPING
bool ADigetherGameplayPlayerController::IsDevServerRpcAllowed() const
{
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
	UE_LOG(LogDigetherFramework, Display,
		TEXT("납부 결과: Operation=%s Result=%s RateLimited=%d Session=%s Station=%s Period=%d Accepted=%d Credited=%d"),
		*UEnum::GetValueAsString(Operation), *UEnum::GetValueAsString(Response.Result), Response.bRateLimited,
		*Response.SessionId.ToString(), *Response.StationId.ToString(), Response.PeriodGeneration,
		Response.AcceptedCredits, Response.CreditedCredits);

#if !UE_BUILD_SHIPPING
	if (!IsLocalController())
	{
		return;
	}

	if (DepositTestPhase == EDigetherDepositTestPhase::AwaitingOpen &&
		Operation == EDigetherDepositOperation::Open && Response.StationId == DepositTestStationId)
	{
		if (Response.Result != EDigetherEconomyResult::Success || !Response.SessionId.IsValid())
		{
			ResetDepositTest();
			return;
		}

		PendingDepositTestSessionId = Response.SessionId;
		DepositTestPhase = EDigetherDepositTestPhase::AwaitingSubmit;
		Server_SubmitDeposit(Response.SessionId, Response.StationId, Response.PeriodGeneration,
			PendingDepositTestAmount);
	}
	else if (DepositTestPhase == EDigetherDepositTestPhase::AwaitingSubmit &&
		Operation == EDigetherDepositOperation::Submit && Response.SessionId == PendingDepositTestSessionId)
	{
		ResetDepositTest();
	}
#endif
}

#if !UE_BUILD_SHIPPING
bool ADigetherGameplayPlayerController::ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor)
{
	const TCHAR* Command = Cmd;
	if (FParse::Command(&Command, TEXT("DigetherDepositTest")))
	{
		DigetherDepositTest(FString(Command));
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
	if (Tokens.Num() != 2 ||
		(!Tokens[0].Equals(TEXT("Departure"), ESearchCase::IgnoreCase) &&
		 !Tokens[0].Equals(TEXT("Ascent"), ESearchCase::IgnoreCase)) ||
		(!Tokens[1].Equals(TEXT("Hold"), ESearchCase::IgnoreCase) &&
		 !Tokens[1].Equals(TEXT("Release"), ESearchCase::IgnoreCase)))
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("사용법: DigetherLever <Departure|Ascent> <Hold|Release>"));
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	const EDigetherElevatorLeverKind LeverKind = Tokens[0].Equals(TEXT("Departure"), ESearchCase::IgnoreCase)
		? EDigetherElevatorLeverKind::Departure : EDigetherElevatorLeverKind::Ascent;
	ADigetherElevatorLeverActor* MatchingLever = nullptr;
	for (TActorIterator<ADigetherElevatorLeverActor> It(World); It; ++It)
	{
		ADigetherElevatorLeverActor* Lever = *It;
		if (IsValid(Lever) && Lever->GetLeverKind() == LeverKind)
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
	const EDigetherEconomyResult ParseResult = FDigetherDepositInput::TryParseAmount(AmountInput, Amount);
	if (ParseResult != EDigetherEconomyResult::Success)
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("DigetherGrantCredits 금액 입력 거절: %s"),
			*UEnum::GetValueAsString(ParseResult));
		return;
	}
	Server_GrantCredits(Amount);
}

void ADigetherGameplayPlayerController::DigetherDepositTest(const FString& AmountInput)
{
	if (!IsLocalController())
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("DigetherDepositTest는 로컬 플레이어만 실행할 수 있습니다."));
		return;
	}
	if (DepositTestPhase != EDigetherDepositTestPhase::Idle)
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("이전 DigetherDepositTest의 결과를 기다리는 중입니다."));
		return;
	}

	int32 Amount = 0;
	const EDigetherEconomyResult ParseResult = FDigetherDepositInput::TryParseAmount(AmountInput, Amount);
	if (ParseResult != EDigetherEconomyResult::Success)
	{
		ResetDepositTest();
		UE_LOG(LogDigetherFramework, Warning, TEXT("DigetherDepositTest 금액 입력 거절: %s"),
			*UEnum::GetValueAsString(ParseResult));
		return;
	}

	PendingDepositTestAmount = Amount;
	DepositTestPhase = EDigetherDepositTestPhase::AwaitingOpen;
	Server_OpenDeposit(DepositTestStationId);
}

void ADigetherGameplayPlayerController::ResetDepositTest()
{
	DepositTestPhase = EDigetherDepositTestPhase::Idle;
	PendingDepositTestAmount = 0;
	PendingDepositTestSessionId.Invalidate();
}

void ADigetherGameplayPlayerController::DigetherTeleportOut()
{
	if (!IsLocalController())
	{
		return;
	}
	Server_DevTeleportOut();
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
		UE_LOG(LogDigetherFramework, Display, TEXT("Dump Player=%d IsSelf=%d Wallet=%s Life=%s"),
			GameplayPlayer->GetPlayerId(), GameplayPlayer == SelfState,
			*ExportStruct(FDigetherWalletState::StaticStruct(), &Wallet),
			*ExportStruct(FDigetherLifeComponentState::StaticStruct(), &Life));
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
