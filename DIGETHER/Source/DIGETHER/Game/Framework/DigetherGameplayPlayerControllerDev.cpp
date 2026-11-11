// 개발용 서버 RPC·콘솔 명령, Shipping 구현은 상태 불변
#include "Game/Framework/DigetherGameplayPlayerController.h"

#include "Engine/World.h"
#include "Game/Economy/DigetherDepositStationActor.h"
#include "Game/Economy/DigetherFloorCoinActor.h"
#include "Game/Elevator/DigetherElevatorLeverActor.h"
#include "Game/Framework/DigetherFrameworkLog.h"
#include "Game/Framework/DigetherGameplayGameMode.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Hub/DigetherHubFacilityActor.h"
#include "GameFramework/Pawn.h"

#if !UE_BUILD_SHIPPING
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "Game/Economy/DigetherQuotaStateComponent.h"
#include "Game/Economy/DigetherWalletComponent.h"
#include "Game/Elevator/DigetherElevatorActor.h"
#include "Game/Elevator/DigetherElevatorComponent.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Run/DigetherRunPublicStateComponent.h"

namespace
{
	// 개발 시험용 식별자, 실제 월드 납부대 배치와 별개
	const FName DepositTestStationId(TEXT("GameTestDeposit"));
	// 개발 명령 금액 입력, 앞뒤 공백 제외 ASCII 숫자만 허용·int32 초과 거절
	constexpr int32 MaxDevCreditDigits = 10;
	// 반경 35cm 시험 Pawn 4명의 붓기 준비용 원호, 정식 캐릭터 배치·게임 규칙과 별개
	constexpr float DevPourStandingRadiusCm = 120.f;
	constexpr float DevPourStandingAnglesDegrees[] = { -60.f, -20.f, 20.f, 60.f };

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

#if !UE_BUILD_SHIPPING
bool ADigetherGameplayPlayerController::IsDevServerRpcAllowed() const
{
	// 기획 S33: 게임 오버(실패 사건·크레딧) 중 개발 서버 RPC(지급·이동·진행 조작) 거절
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
	const FTransform Inlet = MatchedStation->GetPourInletTransform();
	const ADigetherGameplayPlayerState* State = GetPlayerState<ADigetherGameplayPlayerState>();
	FVector Target = ControlledPawn->GetActorLocation();
	bool bMoved = false;
	for (const float AngleDegrees : DevPourStandingAnglesDegrees)
	{
		const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
		const FVector Direction = Inlet.GetUnitAxis(EAxis::X) * FMath::Cos(AngleRadians)
			+ Inlet.GetUnitAxis(EAxis::Y) * FMath::Sin(AngleRadians);
		Target = Inlet.GetLocation() + Direction * DevPourStandingRadiusCm;
		const FRotator FacingInlet = (Inlet.GetLocation() - Target).Rotation();
		// TeleportTo의 주변 보정으로 점유된 자리를 쓰지 않도록 빈 후보만 시도
		if (World->EncroachingBlockingGeometry(ControlledPawn, Target, FacingInlet)
			|| !ControlledPawn->TeleportTo(Target, FacingInlet))
		{
			continue;
		}
		const FVector ActualLocation = ControlledPawn->GetActorLocation();
		const FRotator ActualFacingInlet = (Inlet.GetLocation() - ActualLocation).Rotation();
		ControlledPawn->SetActorRotation(ActualFacingInlet);
		SetControlRotation(ActualFacingInlet);
		// 소유 클라이언트의 위치·회전과 카메라 전송도 엔진 기본 이동 경로로 갱신
		ClientSetLocation(ActualLocation, ActualFacingInlet);
		ControlledPawn->ForceNetUpdate();
		bMoved = true;
		break;
	}
	UE_LOG(LogDigetherFramework, Display,
		TEXT("DigetherTeleportToDeposit 서버 결과: Station=%s Player=%d Moved=%d Target=%s Actual=%s Rotation=%s"),
		*StationId.ToString(), State != nullptr ? State->GetPlayerId() : -1, bMoved, *Target.ToString(),
		*ControlledPawn->GetActorLocation().ToString(), *GetControlRotation().ToString());
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

#if !UE_BUILD_SHIPPING
void ADigetherGameplayPlayerController::DigetherHubTeleport(const FString& FacilityInput)
{
	if (IsLocalController())
	{
		Server_DevTeleportToHubFacility(FName(*FacilityInput.TrimStartAndEnd()));
	}
}

void ADigetherGameplayPlayerController::DigetherHubAction(const FString& Arguments)
{
	TArray<FString> Args;
	Arguments.ParseIntoArrayWS(Args);
	if (!IsLocalController() || Args.Num() < 2 || Args.Num() > 3)
	{
		UE_LOG(LogDigetherFramework, Warning, TEXT("사용법: DigetherHubAction <FacilityId> <Action> [OptionId]"));
		return;
	}
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
	// 세 번째 인자: 상승 레버 안·밖 위치, 생략 시 Inside, 출발 레버에는 미사용
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
	// 실제 입력과 같은 RPC, 거리·입구 방향·가림은 서버 검사
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

	// 서버의 원격 카메라 캐시와 소유 클라이언트의 실제 시선 비교용, 판정·복제 변경 없음
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const APlayerController* Controller = Iterator->Get();
		const APawn* ViewedPawn = IsValid(Controller) ? Controller->GetPawn() : nullptr;
		if (!IsValid(ViewedPawn))
		{
			continue;
		}
		const APlayerState* ViewPlayer = Controller->GetPlayerState<APlayerState>();
		FVector Eye;
		FRotator ViewRotation;
		Controller->GetPlayerViewPoint(Eye, ViewRotation);
		for (TActorIterator<ADigetherDepositStationActor> StationIterator(World); StationIterator; ++StationIterator)
		{
			const ADigetherDepositStationActor* Station = *StationIterator;
			if (!IsValid(Station))
			{
				continue;
			}
			const FTransform Inlet = Station->GetPourInletTransform();
			FHitResult Hit;
			const FCollisionQueryParams Params(SCENE_QUERY_STAT(DevPourView), false, ViewedPawn);
			const bool bHit = World->LineTraceSingleByChannel(Hit, Eye, Inlet.GetLocation(), ECC_Visibility, Params);
			UE_LOG(LogDigetherFramework, Display,
				TEXT("Dump PourView Player=%d IsSelf=%d Pawn=%s Eye=%s Rotation=%s Control=%s Station=%s StationActor=%s StationLocation=%s Inlet=%s Forward=%s Hit=%s HitDistance=%.3f"),
				ViewPlayer != nullptr ? ViewPlayer->GetPlayerId() : -1, Controller == this,
				*ViewedPawn->GetActorLocation().ToString(), *Eye.ToString(), *ViewRotation.ToString(),
				*Controller->GetControlRotation().ToString(), *Station->GetStationId().ToString(), *Station->GetName(),
				*Station->GetActorLocation().ToString(),
				*Inlet.GetLocation().ToString(), *Inlet.GetUnitAxis(EAxis::X).ToString(),
				bHit ? *GetNameSafe(Hit.GetActor()) : TEXT("None"), bHit ? Hit.Distance : -1.f);
		}
	}

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

	// 바닥 금화는 위치만 복제, 금액은 서버(호스트) 프로세스에서만 확인 가능
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
	// 프로세스당 한 번만 예약(월드 이동 뒤 반복 방지)
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
