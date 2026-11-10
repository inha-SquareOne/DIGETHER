// 거점 상호작용 RPC·BP/위젯 진입점, 로컬 상태는 UDigetherHubInteractionComponent 위임
#include "Game/Framework/DigetherGameplayPlayerController.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Framework/DigetherFrameworkLog.h"
#include "Game/Hub/DigetherHubFacilityActor.h"
#include "Game/Hub/DigetherHubInteractionComponent.h"
#include "Game/Hub/DigetherHubStateActor.h"
#include "GameFramework/Pawn.h"
#include "HAL/PlatformTime.h"

namespace
{
	// 시설 요청 서버 최소 간격(초), 휴식 해제(Stand)는 제외
	constexpr double HubRequestMinIntervalSeconds = .05;
}

void ADigetherGameplayPlayerController::StartHubInteraction(ADigetherHubFacilityActor* Facility)
{
	if (HubInteractionComponent)
	{
		HubInteractionComponent->StartHubInteraction(Facility);
	}
}

void ADigetherGameplayPlayerController::SubmitHubOption(const int32 OptionIndex)
{
	if (HubInteractionComponent)
	{
		HubInteractionComponent->SubmitHubOption(OptionIndex);
	}
}

void ADigetherGameplayPlayerController::CloseHubInteraction()
{
	if (HubInteractionComponent)
	{
		HubInteractionComponent->CloseHubInteraction();
	}
}

FText ADigetherGameplayPlayerController::GetInteractionHint() const
{
	return HubInteractionComponent ? HubInteractionComponent->GetInteractionHint() : FText::GetEmpty();
}

void ADigetherGameplayPlayerController::ReleaseHeldInputsAndHubPanel()
{
	// 순서 고정: 레버 해제 → 붓기 종료 → 시설 패널 닫기
	if (HubInteractionComponent)
	{
		HubInteractionComponent->HandleInteractReleased();
	}
	HandlePourReleased();
	CloseHubInteraction();
}

void ADigetherGameplayPlayerController::Server_DevTeleportToHubFacility_Implementation(const FName FacilityId)
{
#if !UE_BUILD_SHIPPING
	if (!IsDevServerRpcAllowed() || !IsValid(GetPawn())) return;
	ADigetherHubFacilityActor* Match = nullptr;
	for (TActorIterator<ADigetherHubFacilityActor> It(GetWorld()); It; ++It)
	{
		if (It->FacilityId == FacilityId)
		{
			if (Match) return;
			Match = *It;
		}
	}
	if (!IsValid(Match)) return;
	const FVector Location = Match->GetActorLocation() - Match->GetActorForwardVector() * 90 + FVector(0, 0, 35);
	const FRotator Rotation = Match->GetActorRotation();
	if (GetPawn()->TeleportTo(Location, Rotation))
	{
		ClientSetLocation(GetPawn()->GetActorLocation(), Rotation);
		UE_LOG(LogDigetherFramework, Display, TEXT("Hub test teleport: Facility=%s Position=%s"),
			*FacilityId.ToString(), *GetPawn()->GetActorLocation().ToString());
	}
#endif
}

void ADigetherGameplayPlayerController::Client_CloseHubForRun_Implementation()
{
	// 탐사 진입 전 유지 입력과 시설 패널 해제
	ReleaseHeldInputsAndHubPanel();
}

void ADigetherGameplayPlayerController::Client_ChainSummoned_Implementation(const float SummonSeconds)
{
	const float SafeSeconds = FMath::IsFinite(SummonSeconds) ? FMath::Max(0.f, SummonSeconds) : 0.f;
	ReceiveChainSummoned(SafeSeconds);
	OnChainSummoned.Broadcast(SafeSeconds);
}

void ADigetherGameplayPlayerController::Server_HubFacilityRequest_Implementation(ADigetherHubFacilityActor* Facility,
	const EDigetherHubAction Action, const FName OptionId, const FGuid RequestId)
{
	ADigetherHubStateActor* Hub = ADigetherHubStateActor::Find(GetWorld());
	FDigetherHubView View;
	View.RequestId = RequestId;
	// 요청 간격 제한과 무관한 휴식 점유 해제
	if (IsValid(Hub) && Action == EDigetherHubAction::Stand)
	{
		Hub->EndRest(this);
	}
	const double Now = FPlatformTime::Seconds();
	if (Action != EDigetherHubAction::Stand && LastHubRequestSeconds >= 0
		&& Now - LastHubRequestSeconds < HubRequestMinIntervalSeconds)
	{
		View.bInteractionAllowed = true;
		View.Message = NSLOCTEXT("DigetherHub", "RateLimited", "잠시 후 다시 시도하세요.");
	}
	else if (IsValid(Hub))
	{
		LastHubRequestSeconds = Now;
		// 패널 표시 이후 바뀐 생존·거리·시야의 서버 재검사
		View = Hub->Execute(this, Facility, Action, OptionId, RequestId);
		// 기획 답변 05(2026-11-06): 시설 변경은 메모리에 유지하고 공식 저장 시점에 함께 저장한다.
	}
	Client_HubFacilityResult(View);
}

void ADigetherGameplayPlayerController::Client_HubFacilityResult_Implementation(const FDigetherHubView& View)
{
	UE_LOG(LogDigetherFramework, Display, TEXT("Hub facility result: Facility=%s Success=%d Balance=%d Message=%s Detail=%s"),
		*View.FacilityId.ToString(), View.bSuccess, View.Balance, *View.Message.ToString(), *View.Detail.ToString());
	if (HubInteractionComponent)
	{
		HubInteractionComponent->ApplyHubFacilityResult(View);
	}
}
