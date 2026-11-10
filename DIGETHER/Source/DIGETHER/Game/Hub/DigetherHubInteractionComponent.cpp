#include "Game/Hub/DigetherHubInteractionComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Economy/DigetherDepositStationActor.h"
#include "Game/Economy/DigetherFloorCoinActor.h"
#include "Game/Elevator/DigetherElevatorLeverActor.h"
#include "Game/Elevator/DigetherElevatorLeverComponent.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Hub/DigetherHubFacilityActor.h"
#include "Game/Input/DigetherInputPrompt.h"
#include "Game/UI/Hub/DigetherHubFacilityWidget.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

namespace
{
	// 시설 패널 주기 조회 간격(초)
	constexpr float HubViewRefreshSeconds = .75f;

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

UDigetherHubInteractionComponent::UDigetherHubInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

ADigetherGameplayPlayerController* UDigetherHubInteractionComponent::GetController() const
{
	return Cast<ADigetherGameplayPlayerController>(GetOwner());
}

AActor* UDigetherHubInteractionComponent::FindInteractionTarget() const
{
	const ADigetherGameplayPlayerController* Controller = GetController();
	const APawn* ControlledPawn = IsValid(Controller) ? Controller->GetPawn() : nullptr;
	UWorld* World = GetWorld();
	if (!IsValid(Controller) || !Controller->IsLocalController() || !IsValid(ControlledPawn) || !IsValid(World))
	{
		return nullptr;
	}
	FVector Eye;
	FRotator Look;
	Controller->GetPlayerViewPoint(Eye, Look);
	const FVector Forward = Look.Vector();
	AActor* Best = nullptr;
	double BestScore = TNumericLimits<double>::Max();
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
		if (World->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Params)
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
	for (TActorIterator<ADigetherHubFacilityActor> It(World); It; ++It)
	{
		Consider(*It, *It, false);
	}
	for (TActorIterator<ADigetherDepositStationActor> It(World); It; ++It)
	{
		Consider(*It, nullptr, false);
	}
	for (TActorIterator<ADigetherElevatorLeverActor> It(World); It; ++It)
	{
		Consider(*It, nullptr, false);
	}
	for (TActorIterator<ADigetherFloorCoinActor> It(World); It; ++It)
	{
		Consider(*It, nullptr, true);
	}
	return Best;
}

FText UDigetherHubInteractionComponent::GetInteractionHint() const
{
	const ADigetherGameplayPlayerController* Controller = GetController();
	if (!IsValid(Controller) || bHubInputModeApplied || Controller->IsRunResultInputModeApplied())
	{
		return FText::GetEmpty();
	}
	// 고정 키 대신 현재 적용 매핑의 키 이름(키 재설정 반영)
	const FText InteractKey = DigetherInputPrompt::GetActionKeyText(Controller, Controller->GetInteractAction());
	if (const ADigetherElevatorLeverActor* Lever = HeldInteractionLever.Get())
	{
		if (ShouldShowOtherLeverActive(*Lever))
		{
			return NSLOCTEXT("DigetherHub", "OtherLeverActive", "다른 레버가 작동 중입니다");
		}
		const FDigetherElevatorLeverPublicState State = Lever->GetLeverComponent()->GetPublicState();
		return FText::Format(NSLOCTEXT("DigetherHub", "HoldingLever", "{0} 계속 누르기 · 레버 작동 {1}%"),
			InteractKey, FText::AsNumber(FMath::RoundToInt(State.Progress * 100)));
	}
	const FText PourKey = DigetherInputPrompt::GetActionKeyText(Controller, Controller->GetPourAction());
	if (Controller->IsPouring())
	{
		return FText::Format(NSLOCTEXT("DigetherHub", "Pouring", "{0} 유지 · 금화를 붓는 중"), PourKey);
	}
	AActor* Target = FindInteractionTarget();
	if (Target != nullptr && Target->IsA<ADigetherFloorCoinActor>())
	{
		return FText::Format(NSLOCTEXT("DigetherHub", "FloorCoinPrompt", "{0} · 금화 줍기"), InteractKey);
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
		return FText::Format(NSLOCTEXT("DigetherHub", "UsePrompt", "{0} · {1}"), InteractKey, Name);
	}
	if (const ADigetherElevatorLeverActor* Lever = Cast<ADigetherElevatorLeverActor>(Target))
	{
		if (Lever->GetLeverKind() == EDigetherElevatorLeverKind::Departure)
		{
			return FText::Format(NSLOCTEXT("DigetherHub", "DeparturePrompt", "{0} 길게 누르기 · 출발 레버"), InteractKey);
		}
		if (ShouldShowOtherLeverActive(*Lever))
		{
			return NSLOCTEXT("DigetherHub", "OtherLeverActive", "다른 레버가 작동 중입니다");
		}
		return FText::Format(Lever->GetPlacement() == EDigetherElevatorLeverPlacement::Outside
			? NSLOCTEXT("DigetherHub", "AscentPromptOutside", "{0} 길게 누르기 · 상승 레버(밖) · 당긴 사람은 타지 않습니다")
			: NSLOCTEXT("DigetherHub", "AscentPromptInside", "{0} 길게 누르기 · 상승 레버(안)"), InteractKey);
	}
	// 이동·시점은 임시 Pawn의 엔진 기본 입력이라 정식 캐릭터 입력 전까지 고정 표기(설계 G0-4)
	return IsValid(Target)
		? FText::Format(NSLOCTEXT("DigetherHub", "PourPrompt", "{0} 길게 누르기 · 정산기에 금화 붓기"), PourKey)
		: FText::Format(NSLOCTEXT("DigetherHub", "ApproachPrompt", "시설에 다가가 바라본 뒤 {0} · 이동 WASD / 시점 마우스"), InteractKey);
}

void UDigetherHubInteractionComponent::HandleInteractStarted()
{
	ADigetherGameplayPlayerController* Controller = GetController();
	if (!IsValid(Controller) || !Controller->IsLocalController() || bHubInputModeApplied
		|| Controller->IsRunResultInputModeApplied() || Controller->IsLifeInputLocked())
	{
		return;
	}
	// 대상 전환 전 이전 레버 당김 해제
	HandleInteractReleased();
	AActor* Target = FindInteractionTarget();
	if (ADigetherElevatorLeverActor* Lever = Cast<ADigetherElevatorLeverActor>(Target))
	{
		HeldInteractionLever = Lever;
		Controller->Server_BeginLeverPull(Lever);
	}
	else if (ADigetherFloorCoinActor* Coin = Cast<ADigetherFloorCoinActor>(Target))
	{
		// 거리·생존·거점 단계·중복 획득은 서버 판정
		Controller->Server_PickupFloorCoin(Coin);
	}
	else if (ADigetherHubFacilityActor* Facility = Cast<ADigetherHubFacilityActor>(Target))
	{
		StartHubInteraction(Facility);
	}
}

void UDigetherHubInteractionComponent::HandleInteractReleased()
{
	ADigetherGameplayPlayerController* Controller = GetController();
	if (HeldInteractionLever.IsValid() && IsValid(Controller) && Controller->IsLocalController())
	{
		Controller->Server_ReleaseLeverPull(HeldInteractionLever.Get());
	}
	HeldInteractionLever.Reset();
}

void UDigetherHubInteractionComponent::StartHubInteraction(ADigetherHubFacilityActor* Facility)
{
	ADigetherGameplayPlayerController* Controller = GetController();
	if (!IsValid(Controller) || !Controller->IsLocalController() || !IsValid(Facility)
		|| Facility->GetWorld() != GetWorld() || !Controller->GetHubFacilityWidgetClass() || IsValid(HubFacilityWidget))
	{
		return;
	}
	// 패널 진입 전 유지 입력과 결과 화면 정리
	HandleInteractReleased();
	Controller->StopPour();
	Controller->CloseRunResult();
	HubFacilityWidget = CreateWidget<UDigetherHubFacilityWidget>(Controller, Controller->GetHubFacilityWidgetClass());
	if (!IsValid(HubFacilityWidget) || !HubFacilityWidget->AddToPlayerScreen(100))
	{
		HubFacilityWidget = nullptr;
		return;
	}
	ActiveHubFacility = Facility;
	bHubPreviousCursorVisible = Controller->bShowMouseCursor;
	Controller->SetShowMouseCursor(true);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(HubFacilityWidget->TakeWidget());
	Controller->SetInputMode(Mode);
	Controller->SetIgnoreMoveInput(true);
	bHubInputModeApplied = true;
	HubFacilityWidget->ActivateWidget();
	HubFacilityWidget->FocusDefaultButton();
	// 거래 요청과 조회의 경합 방지, 다른 사용자의 시설 변경 반영
	GetWorld()->GetTimerManager().SetTimer(HubViewRefreshTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (IsValid(HubFacilityWidget) && !bHubRequestPending)
		{
			SendHubRequest(EDigetherHubAction::Inspect);
		}
	}), HubViewRefreshSeconds, true);
	SendHubRequest(EDigetherHubAction::Inspect);
}

void UDigetherHubInteractionComponent::SendHubRequest(const EDigetherHubAction Action, const FName OptionId)
{
	ADigetherGameplayPlayerController* Controller = GetController();
	if (!ActiveHubFacility.IsValid() || !IsValid(Controller) || !Controller->IsLocalController())
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
	Controller->Server_HubFacilityRequest(ActiveHubFacility.Get(), Action, OptionId, ActiveHubRequestId);
}

void UDigetherHubInteractionComponent::SubmitHubOption(const int32 OptionIndex)
{
	if (!bHubRequestPending && CurrentHubView.Options.IsValidIndex(OptionIndex)
		&& CurrentHubView.Options[OptionIndex].bEnabled)
	{
		const FDigetherHubOption Option = CurrentHubView.Options[OptionIndex];
		SendHubRequest(Option.Action, Option.Id);
	}
}

void UDigetherHubInteractionComponent::CloseHubInteraction()
{
	ADigetherGameplayPlayerController* Controller = GetController();
	const bool bLocal = IsValid(Controller) && Controller->IsLocalController();
	// 패널 재오픈 뒤 이전 조회·응답 적용 방지
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HubViewRefreshTimer);
	}
	ActiveHubRequestId.Invalidate();
	bHubRequestPending = false;
	if (ActiveHubFacility.IsValid() && bLocal)
	{
		Controller->Server_HubFacilityRequest(ActiveHubFacility.Get(), EDigetherHubAction::Stand, NAME_None, FGuid::NewGuid());
	}
	ActiveHubFacility.Reset();
	if (IsValid(HubFacilityWidget))
	{
		HubFacilityWidget->DeactivateWidget();
		HubFacilityWidget->RemoveFromParent();
	}
	HubFacilityWidget = nullptr;
	CurrentHubView = FDigetherHubView();
	if (bHubInputModeApplied && bLocal)
	{
		Controller->SetInputMode(FInputModeGameOnly());
		Controller->SetShowMouseCursor(bHubPreviousCursorVisible);
		Controller->SetIgnoreMoveInput(false);
	}
	bHubInputModeApplied = false;
}

void UDigetherHubInteractionComponent::ApplyHubFacilityResult(const FDigetherHubView& View)
{
	const ADigetherGameplayPlayerController* Controller = GetController();
	// 닫힌 패널과 이전 요청의 지연 응답 제외
	if (!IsValid(Controller) || !Controller->IsLocalController() || !IsValid(HubFacilityWidget)
		|| View.RequestId != ActiveHubRequestId)
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
