#include "Game/Framework/DigetherGameplayPlayerController.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Economy/DigetherDepositStationActor.h"
#include "Game/Economy/DigetherFloorCoinActor.h"
#include "Game/Elevator/DigetherElevatorLeverActor.h"
#include "Game/Elevator/DigetherElevatorLeverComponent.h"
#include "Game/Framework/DigetherFrameworkLog.h"
#include "Game/Framework/DigetherGameplayGameMode.h"
#include "Game/Hub/DigetherHubFacilityActor.h"
#include "Game/Hub/DigetherHubStateActor.h"
#include "Game/UI/Hub/DigetherHubFacilityWidget.h"
#include "GameFramework/Pawn.h"
#include "HAL/PlatformTime.h"
#include "TimerManager.h"

namespace
{
	// 복제 진행률 기준의 다른 상승 레버 사용 안내
	// 실제 시작 허용 여부는 서버 점유 검사로 확정
	bool IsOtherAscentLeverActive(const ADigetherElevatorLeverActor& Lever)
	{
		if (Lever.GetLeverKind() != EDigetherElevatorLeverKind::Ascent || !IsValid(Lever.GetWorld()))
		{
			return false;
		}
		for (TActorIterator<ADigetherElevatorLeverActor> It(Lever.GetWorld()); It; ++It)
		{
			const ADigetherElevatorLeverActor* Other = *It;
			if (Other != &Lever && Other->GetLeverKind() == EDigetherElevatorLeverKind::Ascent
				&& Other->GetElevatorActor() == Lever.GetElevatorActor() && IsValid(Other->GetLeverComponent())
				&& Other->GetLeverComponent()->GetPublicState().Progress > 0.f)
			{
				return true;
			}
		}
		return false;
	}

	// 아직 시작하지 않은 레버에만 사용 중 안내
	bool ShouldShowOtherLeverActive(const ADigetherElevatorLeverActor& Lever)
	{
		return IsValid(Lever.GetLeverComponent()) && Lever.GetLeverComponent()->GetPublicState().Progress <= 0.f
			&& IsOtherAscentLeverActive(Lever);
	}
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
	HandleInteractReleased();
	HandlePourReleased();
	CloseHubInteraction();
}

void ADigetherGameplayPlayerController::Client_ChainSummoned_Implementation(const float SummonSeconds)
{
	const float SafeSeconds = FMath::IsFinite(SummonSeconds) ? FMath::Max(0.f, SummonSeconds) : 0.f;
	ReceiveChainSummoned(SafeSeconds);
	OnChainSummoned.Broadcast(SafeSeconds);
}

AActor* ADigetherGameplayPlayerController::FindInteractionTarget() const
{
	const APawn* ControlledPawn = GetPawn();
	if (!IsLocalController() || !IsValid(ControlledPawn) || !IsValid(GetWorld()))
	{
		return nullptr;
	}
	FVector Eye;
	FRotator Look;
	GetPlayerViewPoint(Eye, Look);
	const FVector Forward = Look.Vector();
	AActor* Best = nullptr;
	double BestScore = TNumericLimits<double>::Max();
	// 로컬 표시·입력용 후보 선택, 실제 상호작용은 서버 재검사
	auto Consider = [&](AActor* Candidate, const ADigetherHubFacilityActor* Facility, const bool bFloorCoin)
	{
		// 시설·금화는 원점, 레버·정산기는 몸통 높이 기준
		const FVector Target = Candidate->GetActorLocation() + (Facility || bFloorCoin ? FVector::ZeroVector : FVector(0, 0, 80));
		const double Range = Facility ? Facility->InteractionDistanceCm : 200.0;
		const double Distance = FVector::Dist(ControlledPawn->GetActorLocation(), Candidate->GetActorLocation());
		const FVector ToTarget = Target - Eye;
		const double Dot = FVector::DotProduct(Forward, ToTarget.GetSafeNormal());
		// 조작면 가까이에서는 시선각 조건 완화
		if (Distance > Range || (Dot < .2 && Distance > 60))
		{
			return;
		}
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(HubInteractionSight), false, ControlledPawn);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Params)
			&& Hit.GetActor() != Candidate && Hit.Distance + 4 < ToTarget.Size())
		{
			return;
		}
		const double Score = Distance + (1 - Dot) * 150;
		if (Score < BestScore)
		{
			Best = Candidate;
			BestScore = Score;
		}
	};
	// 후보 4종은 서로 다른 AActor 직계 계열, 클래스별 순회로 전체 Actor 순회 회피
	for (TActorIterator<ADigetherHubFacilityActor> It(GetWorld()); It; ++It)
	{
		Consider(*It, *It, false);
	}
	for (TActorIterator<ADigetherDepositStationActor> It(GetWorld()); It; ++It)
	{
		Consider(*It, nullptr, false);
	}
	for (TActorIterator<ADigetherElevatorLeverActor> It(GetWorld()); It; ++It)
	{
		Consider(*It, nullptr, false);
	}
	for (TActorIterator<ADigetherFloorCoinActor> It(GetWorld()); It; ++It)
	{
		Consider(*It, nullptr, true);
	}
	return Best;
}

FText ADigetherGameplayPlayerController::GetInteractionHint() const
{
	if (bHubInputModeApplied || bRunResultInputModeApplied)
	{
		return FText::GetEmpty();
	}
	if (const ADigetherElevatorLeverActor* Lever = HeldInteractionLever.Get())
	{
		if (ShouldShowOtherLeverActive(*Lever))
		{
			return NSLOCTEXT("DigetherHub", "OtherLeverActive", "다른 레버가 작동 중입니다");
		}
		const FDigetherElevatorLeverPublicState State = Lever->GetLeverComponent()->GetPublicState();
		return FText::Format(NSLOCTEXT("DigetherHub", "HoldingLever", "E를 계속 눌러 레버 작동 · {0}%"),
			FText::AsNumber(FMath::RoundToInt(State.Progress * 100)));
	}
	if (HeldPourStation.IsValid())
	{
		return NSLOCTEXT("DigetherHub", "Pouring", "우클릭 유지 · 금화를 붓는 중");
	}
	AActor* Target = FindInteractionTarget();
	if (Target != nullptr && Target->IsA<ADigetherFloorCoinActor>())
	{
		return NSLOCTEXT("DigetherHub", "FloorCoinPrompt", "E · 금화 줍기");
	}
	if (const ADigetherHubFacilityActor* Facility = Cast<ADigetherHubFacilityActor>(Target))
	{
		FText Name;
		switch (Facility->Kind)
		{
		case EDigetherHubFacilityKind::Shop: Name = NSLOCTEXT("DigetherHub", "ShopPrompt", "상점"); break;
		case EDigetherHubFacilityKind::Delivery: Name = NSLOCTEXT("DigetherHub", "DeliveryPrompt", "물품 수령함"); break;
		case EDigetherHubFacilityKind::Bed: Name = NSLOCTEXT("DigetherHub", "BedPrompt", "침대"); break;
		case EDigetherHubFacilityKind::Altar: Name = NSLOCTEXT("DigetherHub", "AltarPrompt", "제단"); break;
		case EDigetherHubFacilityKind::Stage: Name = NSLOCTEXT("DigetherHub", "StagePrompt", "스테이지 구입·선택"); break;
		}
		return FText::Format(NSLOCTEXT("DigetherHub", "UsePrompt", "E · {0}"), Name);
	}
	if (const ADigetherElevatorLeverActor* Lever = Cast<ADigetherElevatorLeverActor>(Target))
	{
		if (Lever->GetLeverKind() == EDigetherElevatorLeverKind::Departure)
		{
			return NSLOCTEXT("DigetherHub", "DeparturePrompt", "E 길게 누르기 · 출발 레버");
		}
		if (ShouldShowOtherLeverActive(*Lever))
		{
			return NSLOCTEXT("DigetherHub", "OtherLeverActive", "다른 레버가 작동 중입니다");
		}
		return Lever->GetPlacement() == EDigetherElevatorLeverPlacement::Outside
			? NSLOCTEXT("DigetherHub", "AscentPromptOutside", "E 길게 누르기 · 상승 레버(밖) · 당긴 사람은 타지 않습니다")
			: NSLOCTEXT("DigetherHub", "AscentPromptInside", "E 길게 누르기 · 상승 레버(안)");
	}
	return IsValid(Target) ? NSLOCTEXT("DigetherHub", "PourPrompt", "우클릭 길게 누르기 · 정산기에 금화 붓기")
		: NSLOCTEXT("DigetherHub", "ApproachPrompt", "시설에 다가가 바라본 뒤 E · 이동 WASD / 시점 마우스");
}

void ADigetherGameplayPlayerController::HandleInteractStarted()
{
	if (!IsLocalController() || bHubInputModeApplied || bRunResultInputModeApplied || bLifeInputLocked)
	{
		return;
	}
	// 대상 전환 전 이전 레버 당김 해제
	HandleInteractReleased();
	AActor* Target = FindInteractionTarget();
	if (ADigetherElevatorLeverActor* Lever = Cast<ADigetherElevatorLeverActor>(Target))
	{
		HeldInteractionLever = Lever;
		Server_BeginLeverPull(Lever);
	}
	else if (ADigetherFloorCoinActor* Coin = Cast<ADigetherFloorCoinActor>(Target))
	{
		// 거리·생존·거점 단계·중복 획득의 서버 판정
		Server_PickupFloorCoin(Coin);
	}
	else if (ADigetherHubFacilityActor* Facility = Cast<ADigetherHubFacilityActor>(Target))
	{
		StartHubInteraction(Facility);
	}
}

void ADigetherGameplayPlayerController::HandleInteractReleased()
{
	// Completed·Canceled 입력과 패널 종료의 공통 레버 해제
	if (HeldInteractionLever.IsValid() && IsLocalController())
	{
		Server_ReleaseLeverPull(HeldInteractionLever.Get());
	}
	HeldInteractionLever.Reset();
}

void ADigetherGameplayPlayerController::StartHubInteraction(ADigetherHubFacilityActor* Facility)
{
	if (!IsLocalController() || !IsValid(Facility) || Facility->GetWorld() != GetWorld()
		|| !HubFacilityWidgetClass || IsValid(HubFacilityWidget))
	{
		return;
	}
	// 패널 진입 전 유지 입력과 결과 화면 정리
	HandleInteractReleased();
	HandlePourReleased();
	CloseRunResult();
	HubFacilityWidget = CreateWidget<UDigetherHubFacilityWidget>(this, HubFacilityWidgetClass);
	if (!IsValid(HubFacilityWidget) || !HubFacilityWidget->AddToPlayerScreen(100))
	{
		HubFacilityWidget = nullptr;
		return;
	}
	ActiveHubFacility = Facility;
	bHubPreviousCursorVisible = bShowMouseCursor;
	SetShowMouseCursor(true);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(HubFacilityWidget->TakeWidget());
	SetInputMode(Mode);
	SetIgnoreMoveInput(true);
	bHubInputModeApplied = true;
	HubFacilityWidget->ActivateWidget();
	HubFacilityWidget->FocusDefaultButton();
	// 거래 요청과 조회의 경합 방지, 다른 사용자의 시설 변경 반영
	GetWorldTimerManager().SetTimer(HubViewRefreshTimer, [this]()
	{
		if (IsValid(HubFacilityWidget) && !bHubRequestPending)
		{
			SendHubRequest(EDigetherHubAction::Inspect);
		}
	}, .75f, true);
	SendHubRequest(EDigetherHubAction::Inspect);
}

void ADigetherGameplayPlayerController::SendHubRequest(const EDigetherHubAction Action, const FName OptionId)
{
	if (!ActiveHubFacility.IsValid() || !IsLocalController())
	{
		CloseHubInteraction();
		return;
	}
	// 이전 패널·요청의 지연 응답 구분용 식별자
	ActiveHubRequestId = FGuid::NewGuid();
	LastHubAction = Action;
	bHubRequestPending = true;
	if (IsValid(HubFacilityWidget) && Action != EDigetherHubAction::Inspect)
	{
		HubFacilityWidget->SetPending();
	}
	Server_HubFacilityRequest(ActiveHubFacility.Get(), Action, OptionId, ActiveHubRequestId);
}

void ADigetherGameplayPlayerController::SubmitHubOption(const int32 OptionIndex)
{
	if (!bHubRequestPending && CurrentHubView.Options.IsValidIndex(OptionIndex)
		&& CurrentHubView.Options[OptionIndex].bEnabled)
	{
		const FDigetherHubOption Option = CurrentHubView.Options[OptionIndex];
		SendHubRequest(Option.Action, Option.Id);
	}
}

void ADigetherGameplayPlayerController::CloseHubInteraction()
{
	// 패널 재오픈 뒤 이전 조회·응답 적용 방지
	GetWorldTimerManager().ClearTimer(HubViewRefreshTimer);
	ActiveHubRequestId.Invalidate();
	bHubRequestPending = false;
	if (ActiveHubFacility.IsValid() && IsLocalController())
	{
		Server_HubFacilityRequest(ActiveHubFacility.Get(), EDigetherHubAction::Stand, NAME_None, FGuid::NewGuid());
	}
	ActiveHubFacility.Reset();
	if (IsValid(HubFacilityWidget))
	{
		HubFacilityWidget->DeactivateWidget();
		HubFacilityWidget->RemoveFromParent();
	}
	HubFacilityWidget = nullptr;
	CurrentHubView = FDigetherHubView();
	if (bHubInputModeApplied && IsLocalController())
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(bHubPreviousCursorVisible);
		SetIgnoreMoveInput(false);
	}
	bHubInputModeApplied = false;
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
	if (Action != EDigetherHubAction::Stand && LastHubRequestSeconds >= 0 && Now - LastHubRequestSeconds < .05)
	{
		View.bInteractionAllowed = true;
		View.Message = NSLOCTEXT("DigetherHub", "RateLimited", "잠시 후 다시 시도하세요.");
	}
	else if (IsValid(Hub))
	{
		LastHubRequestSeconds = Now;
		// 패널 표시 이후 바뀐 생존·거리·시야의 서버 재검사
		View = Hub->Execute(this, Facility, Action, OptionId, RequestId);
		// 주기 조회의 불필요한 저장 방지
		if (View.bProgressChanged)
		{
			if (ADigetherGameplayGameMode* Mode = ResolveGameplayGameMode())
			{
				if (!Mode->SaveHubFacilityProgress())
				{
					View.Message = FText::Format(NSLOCTEXT("DigetherHub", "AutosaveFailed", "{0}\n자동 저장에 실패했습니다. 현재 진행은 유지됩니다."), View.Message);
				}
			}
		}
	}
	Client_HubFacilityResult(View);
}

void ADigetherGameplayPlayerController::Client_HubFacilityResult_Implementation(const FDigetherHubView& View)
{
	UE_LOG(LogDigetherFramework, Display, TEXT("Hub facility result: Facility=%s Success=%d Balance=%d Message=%s Detail=%s"),
		*View.FacilityId.ToString(), View.bSuccess, View.Balance, *View.Message.ToString(), *View.Detail.ToString());
	// 닫힌 패널과 이전 요청의 지연 응답 제외
	if (!IsLocalController() || !IsValid(HubFacilityWidget) || View.RequestId != ActiveHubRequestId)
	{
		return;
	}
	bHubRequestPending = false;
	if (!View.bInteractionAllowed)
	{
		CloseHubInteraction();
		return;
	}
	FDigetherHubView Display = View;
	// 주기 조회 중 직전 거래 결과 안내 유지
	if (LastHubAction == EDigetherHubAction::Inspect && CurrentHubView.FacilityId == View.FacilityId
		&& !CurrentHubView.Message.IsEmpty())
	{
		Display.Message = CurrentHubView.Message;
	}
	CurrentHubView = Display;
	HubFacilityWidget->SetView(Display);
}
