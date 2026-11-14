// 붓기·바닥 금화·납부 결과, 폐기된 금액 입력 호환
#include "Game/Framework/DigetherGameplayPlayerController.h"

#include "Engine/World.h"
#include "Game/Economy/DigetherDepositStationActor.h"
#include "Game/Economy/DigetherFloorCoinActor.h"
#include "Game/Framework/DigetherFrameworkDefaults.h"
#include "Game/Framework/DigetherFrameworkLog.h"
#include "Game/Framework/DigetherGameplayGameMode.h"
#include "Game/Hub/DigetherHubInteractionComponent.h"
#include "HAL/PlatformTime.h"
#include "TimerManager.h"
#include "UObject/Class.h"

void ADigetherGameplayPlayerController::RequestOpenDeposit(const FName StationId)
{
	// 폐기된 금액 입력, BP 참조 호환용 빈 구현
}

void ADigetherGameplayPlayerController::RequestSubmitDeposit(const FGuid SessionId,
	const FName StationId, const int32 PeriodGeneration, const int32 Amount)
{
	// 폐기된 금액 입력, BP 참조 호환용 빈 구현
}

void ADigetherGameplayPlayerController::RequestCancelDeposit(const FGuid SessionId)
{
	// 폐기된 금액 입력, BP 참조 호환용 빈 구현
}

void ADigetherGameplayPlayerController::StartDepositInteraction(ADigetherDepositStationActor* Station)
{
	// 폐기된 금액 입력, 납부는 StartPour(우클릭 유지)
}

void ADigetherGameplayPlayerController::SubmitDepositInput(const FString& AmountInput)
{
	// 사용 중단 금액 입력 위젯 호환용 빈 구현
}

void ADigetherGameplayPlayerController::CloseDepositInteraction()
{
	// 사용 중단 금액 입력 위젯 호환용, 진행 중 붓기 입력만 종료
	HandlePourReleased();
}

void ADigetherGameplayPlayerController::StartPour(ADigetherDepositStationActor* Station)
{
	if (!IsLocalController() || !IsValid(Station) || Station->GetWorld() != GetWorld()
		|| Station->GetStationId().IsNone() || (HubInteractionComponent && HubInteractionComponent->IsHubPanelOpen())
		|| IsRunResultInputModeApplied() || bLifeInputLocked)
	{
		return;
	}
	if (HubInteractionComponent)
	{
		HubInteractionComponent->HandleInteractReleased();
	}
	HeldPourStation = Station;
	Server_BeginPour(Station);
}

void ADigetherGameplayPlayerController::StopPour()
{
	HandlePourReleased();
}

void ADigetherGameplayPlayerController::HandlePourStarted()
{
	// 런 플로우 v1.1.0 4.6: 관전 중 우클릭은 다음 관전 대상, 붓기는 관전 중 원래도 거절(StartPour)
	if (bLifeInputLocked)
	{
		if (IsLocalController())
		{
			Server_CycleSpectateTarget(true);
		}
		return;
	}
	if (HubInteractionComponent)
	{
		StartPour(Cast<ADigetherDepositStationActor>(HubInteractionComponent->FindInteractionTarget()));
	}
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
	// 끝 요청은 빈도 제한 없음, 놓치면 손을 뗀 뒤에도 붓기 유지
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

void ADigetherGameplayPlayerController::Client_DepositResult_Implementation(
	const EDigetherDepositOperation Operation, const FDigetherDepositResponse& Response)
{
	// 본인 결과만 수신, 본인 차감·인정액은 표시 가능(S24), 공동 누적액 미포함
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
		// 서버 붓기 중단, 계속 눌러도 재개하지 않고 다시 눌러야 함(S24 ④)
		HeldPourStation.Reset();
#if !UE_BUILD_SHIPPING
		GetWorldTimerManager().ClearTimer(DevPourTimer);
#endif
	}
	OnDepositResult.Broadcast(Operation, Response);
}
