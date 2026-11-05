#include "Game/Hub/DigetherHubStateActor.h"

#include "Components/LightComponent.h"
#include "Engine/Light.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Economy/DigetherWalletComponent.h"
#include "Game/Elevator/DigetherElevatorActor.h"
#include "Game/Elevator/DigetherElevatorComponent.h"
#include "Game/Elevator/DigetherElevatorLeverActor.h"
#include "Game/Framework/DigetherGameplayGameMode.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Hub/DigetherHubCatalog.h"
#include "Game/Hub/DigetherHubFacilityActor.h"
#include "Game/Hub/DigetherHubStageAnchor.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Run/DigetherRunPublicStateComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDigetherHub, Log, All);

namespace
{
	// 엘리베이터 1대와 연결 레버 최소 2개, 밖 상승 레버 포함 이동
	constexpr int32 MinElevatorTransformCount = 3;

	ADigetherHubStageAnchor* FindStageAnchor(const UWorld* World, FName StageId)
	{
		ADigetherHubStageAnchor* Found = nullptr;
		for (TActorIterator<ADigetherHubStageAnchor> It(World); It; ++It)
		{
			if (It->StageId == StageId)
			{
				if (Found)
				{
					return nullptr;
				}
				Found = *It;
			}
		}
		return Found;
	}

	FText ResultText(EDigetherHubResult Result)
	{
		switch (Result)
		{
		case EDigetherHubResult::Success:
			return NSLOCTEXT("DigetherHub", "Completed", "처리했습니다.");
		case EDigetherHubResult::InsufficientFunds:
			return NSLOCTEXT("DigetherHub", "InsufficientFunds", "잔고가 부족합니다.");
		case EDigetherHubResult::CapacityExceeded:
			return NSLOCTEXT("DigetherHub", "CapacityExceeded", "보관 한도를 초과합니다.");
		case EDigetherHubResult::NothingToClaim:
			return NSLOCTEXT("DigetherHub", "NothingToClaim", "수령할 구매품이 없습니다.");
		case EDigetherHubResult::AlreadyUnlocked:
			return NSLOCTEXT("DigetherHub", "AlreadyUnlocked", "팀에서 이미 해금한 목적지입니다. 추가 비용은 차감하지 않았습니다.");
		case EDigetherHubResult::StageLocked:
			return NSLOCTEXT("DigetherHub", "StageLocked", "먼저 이 목적지를 해금하세요.");
		case EDigetherHubResult::DestinationLocked:
			return NSLOCTEXT("DigetherHub", "DestinationLocked", "출발 준비 중에는 목적지를 변경할 수 없습니다.");
		default:
			return NSLOCTEXT("DigetherHub", "InvalidRequest", "시설의 현재 상태와 요청 항목을 확인하세요.");
		}
	}

	FText FacilityTitle(EDigetherHubFacilityKind Kind)
	{
		switch (Kind)
		{
		case EDigetherHubFacilityKind::Shop: return NSLOCTEXT("DigetherHub", "Shop", "상점");
		case EDigetherHubFacilityKind::Delivery: return NSLOCTEXT("DigetherHub", "Delivery", "물품 수령함");
		case EDigetherHubFacilityKind::Bed: return NSLOCTEXT("DigetherHub", "Bed", "침대 휴식");
		case EDigetherHubFacilityKind::Altar: return NSLOCTEXT("DigetherHub", "Altar", "제단");
		case EDigetherHubFacilityKind::Stage: return NSLOCTEXT("DigetherHub", "Stage", "스테이지 구입 · 선택");
		default: return FText::GetEmpty();
		}
	}

	bool StopAndTeleport(APawn* Pawn, APlayerController* Controller, const FTransform& Transform)
	{
		if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
		{
			Movement->StopMovementImmediately();
		}
		if (!Pawn->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics))
		{
			return false;
		}
		if (IsValid(Controller))
		{
			Controller->ClientSetLocation(Transform.GetLocation(), Transform.Rotator());
		}
		return true;
	}
}

ADigetherHubStateActor::ADigetherHubStateActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
}

void ADigetherHubStateActor::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		InitializeCatalogDefaults();
		CacheElevatorTransforms();
		GetWorldTimerManager().SetTimer(RestCleanupTimer, this, &ThisClass::CleanupRestingPlayers, .5f, true);
	}
	if (OfferingPulseCounter > 0)
	{
		OnRep_OfferingPulseCounter();
	}
}

void ADigetherHubStateActor::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HasAuthority())
	{
		EndAllRest();
	}
	GetWorldTimerManager().ClearTimer(RestCleanupTimer);
	GetWorldTimerManager().ClearTimer(OfferingPulseTimer);
	RestoreOfferingLight();
	Super::EndPlay(Reason);
}

void ADigetherHubStateActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADigetherHubStateActor, UnlockedStageIds);
	DOREPLIFETIME(ADigetherHubStateActor, SelectedStageId);
	DOREPLIFETIME(ADigetherHubStateActor, bDestinationLocked);
	DOREPLIFETIME(ADigetherHubStateActor, TotalOfferingCredits);
	DOREPLIFETIME(ADigetherHubStateActor, OfferingPulseCounter);
	DOREPLIFETIME(ADigetherHubStateActor, OfferingPulseEndsAtServerTime);
}

ADigetherHubStateActor* ADigetherHubStateActor::Find(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	ADigetherHubStateActor* Found = nullptr;
	for (TActorIterator<ADigetherHubStateActor> It(World); It; ++It)
	{
		if (Found)
		{
			return nullptr;
		}
		Found = *It;
	}
	return Found;
}

void ADigetherHubStateActor::InitializeCatalogDefaults()
{
	if (!HasAuthority() || bProgressInitialized || !IsValid(Catalog) || !Catalog->IsCatalogValid())
	{
		return;
	}
	UnlockedStageIds.Reset();
	for (const FDigetherHubStage& Stage : Catalog->Stages)
	{
		if (Stage.bInitiallyUnlocked)
		{
			UnlockedStageIds.Add(Stage.StageId);
		}
	}
	SelectedStageId = UnlockedStageIds[0];
	bProgressInitialized = true;
}

bool ADigetherHubStateActor::IsRequesterAllowed(const ADigetherGameplayPlayerController* Requester,
	const ADigetherHubFacilityActor* Facility) const
{
	// 거래 실행·개인 화면 생성의 서버 권한·게임 상태 경계
	if (!HasAuthority() || !bProgressInitialized || !IsValid(Catalog) || !Catalog->IsCatalogValid()
		|| !IsValid(Requester) || !Requester->HasAuthority() || Requester->GetWorld() != GetWorld()
		|| !IsValid(Facility) || Facility->GetWorld() != GetWorld() || Find(GetWorld()) != this)
	{
		return false;
	}
	const ADigetherGameplayPlayerState* Player = Requester->GetPlayerState<ADigetherGameplayPlayerState>();
	const ADigetherGameplayGameMode* Mode = GetWorld()->GetAuthGameMode<ADigetherGameplayGameMode>();
	const ADigetherGameplayGameState* State = GetWorld()->GetGameState<ADigetherGameplayGameState>();
	if (!IsValid(Player) || !Player->IsGameplayInitialized() || Player->IsInactive()
		|| !Player->GetGameplayPlayerId().IsValid() || Player->GetOwner() != Requester
		|| Player->GetGameplayPawn() != Requester->GetPawn() || !IsValid(Player->GetWalletComponent())
		|| !IsValid(Mode) || !Mode->IsRunReady() || !IsValid(State) || !IsValid(State->GetRunPublicStateComponent()))
	{
		return false;
	}
	const FDigetherRunPublicState Run = State->GetRunPublicStateComponent()->CaptureState();
	if (Run.Phase == EDigetherRunPhase::InProgress || Run.bGameOver)
	{
		return false;
	}
	int32 Matches = 0;
	for (TActorIterator<ADigetherHubFacilityActor> It(GetWorld()); It; ++It)
	{
		if (It->FacilityId == Facility->FacilityId)
		{
			++Matches;
		}
	}
	if (Matches != 1)
	{
		return false;
	}
	for (TActorIterator<ADigetherElevatorActor> It(GetWorld()); It; ++It)
	{
		if (!IsValid(It->GetElevatorComponent())
			|| It->GetElevatorComponent()->GetPublicState().Phase != EDigetherElevatorPhase::HubWaiting)
		{
			return false;
		}
	}
	// 침대 외형 안의 휴식 Pawn에 대한 자기 침대 조회·해제 허용
	for (const FRestRecord& Rest : RestingPlayers)
	{
		if (Rest.Controller.Get() == Requester && Rest.Bed.Get() == Facility && Rest.Pawn.Get() == Requester->GetPawn())
		{
			return true;
		}
	}
	return Facility->CanInteract(Requester);
}

FDigetherHubProgress ADigetherHubStateActor::CaptureProgress() const
{
	FDigetherHubProgress Progress;
	Progress.bInitialized = bProgressInitialized;
	Progress.Players = PrivatePlayers;
	Progress.UnlockedStageIds = UnlockedStageIds;
	Progress.SelectedStageId = SelectedStageId;
	Progress.TotalOfferingCredits = TotalOfferingCredits;
	return Progress;
}

bool ADigetherHubStateActor::ValidateProgress(const FDigetherHubProgress& Candidate) const
{
	// 구조 검증 후 현행 카탈로그의 품목·목적지 참조 확인
	if (!DigetherHubRules::IsProgressStructurallyValid(Candidate) || !IsValid(Catalog) || !Catalog->IsCatalogValid())
	{
		return false;
	}
	if (!Candidate.bInitialized)
	{
		return true;
	}
	for (FName StageId : Candidate.UnlockedStageIds)
	{
		if (!Catalog->FindStage(StageId))
		{
			return false;
		}
	}
	for (const FDigetherHubPlayerProgress& Player : Candidate.Players)
	{
		for (const TArray<FDigetherHubItemStack>* Stacks : {&Player.Inventory, &Player.PendingDelivery})
		{
			for (const FDigetherHubItemStack& Stack : *Stacks)
			{
				if (!Catalog->FindOffer(Stack.ItemId))
				{
					return false;
				}
			}
		}
	}
	return true;
}

bool ADigetherHubStateActor::RestoreProgress(const FDigetherHubProgress& Candidate)
{
	if (!HasAuthority() || !ValidateProgress(Candidate))
	{
		return false;
	}
	// 휴식·출발 잠금·중복 요청 기록은 저장 복원 대상 제외
	EndAllRest();
	ReturnElevatorToHub();
	PrivatePlayers = Candidate.Players;
	UnlockedStageIds = Candidate.UnlockedStageIds;
	SelectedStageId = Candidate.SelectedStageId;
	TotalOfferingCredits = Candidate.TotalOfferingCredits;
	bProgressInitialized = Candidate.bInitialized;
	bDestinationLocked = false;
	AppliedRequests.Reset();
	InitializeCatalogDefaults();
	ForceNetUpdate();
	return true;
}

bool ADigetherHubStateActor::IsResting(const ADigetherGameplayPlayerController* Requester) const
{
	return RestingPlayers.ContainsByPredicate([Requester](const FRestRecord& Rest)
	{
		return Rest.Controller.Get() == Requester;
	});
}

FDigetherHubView ADigetherHubStateActor::BuildView(const ADigetherGameplayPlayerController* Requester,
	const ADigetherHubFacilityActor* Facility) const
{
	FDigetherHubView View;
	if (IsValid(Facility))
	{
		View.FacilityId = Facility->FacilityId;
		View.Kind = Facility->Kind;
		View.Title = FacilityTitle(Facility->Kind);
	}
	View.bInteractionAllowed = IsRequesterAllowed(Requester, Facility);
	if (!View.bInteractionAllowed)
	{
		View.Message = NSLOCTEXT("DigetherHub", "InteractionUnavailable", "시설에 가까이 서서 거점 대기 상태에서 이용하세요.");
		return View;
	}
	View.bSuccess = true;
	View.bResting = IsResting(Requester);
	const ADigetherGameplayPlayerState* Player = Requester->GetPlayerState<ADigetherGameplayPlayerState>();
	View.Balance = Player->CaptureState().BalanceCredits;
	const FDigetherHubPlayerProgress* Private = PrivatePlayers.FindByPredicate([Player](const FDigetherHubPlayerProgress& Record)
	{
		return Record.PlayerId == Player->GetGameplayPlayerId();
	});
	if (Facility->Kind == EDigetherHubFacilityKind::Shop || Facility->Kind == EDigetherHubFacilityKind::Delivery)
	{
		TArray<FText> Lines;
		for (const FDigetherHubOffer& Offer : Catalog->Offers)
		{
			const int32 Owned = Private ? DigetherHubRules::GetItemCount(Private->Inventory, Offer.ItemId) : 0;
			const int32 Pending = Private ? DigetherHubRules::GetItemCount(Private->PendingDelivery, Offer.ItemId) : 0;
			Lines.Add(FText::Format(NSLOCTEXT("DigetherHub", "InventoryLine", "{0}: 보유 {1} / 배송 {2}"),
				Offer.DisplayName, FText::AsNumber(Owned), FText::AsNumber(Pending)));
			if (Facility->Kind == EDigetherHubFacilityKind::Shop)
			{
				FDigetherHubOption& Option = View.Options.AddDefaulted_GetRef();
				Option.Id = Offer.ItemId;
				Option.Label = FText::Format(NSLOCTEXT("DigetherHub", "BuyLabel", "{0} ×{1} · {2} 크레딧"),
					Offer.DisplayName, FText::AsNumber(Offer.Quantity), FText::AsNumber(Offer.PriceCredits));
				Option.Detail = NSLOCTEXT("DigetherHub", "BuyDetail", "구매 후 수령함에서 받습니다.");
				Option.Cost = Offer.PriceCredits;
				Option.Action = EDigetherHubAction::Purchase;
				Option.bEnabled = View.Balance >= Offer.PriceCredits;
			}
		}
		View.Detail = FText::Join(FText::FromString(TEXT("\n")), Lines);
		if (Facility->Kind == EDigetherHubFacilityKind::Delivery)
		{
			FDigetherHubOption& Option = View.Options.AddDefaulted_GetRef();
			Option.Label = NSLOCTEXT("DigetherHub", "ClaimAll", "내 배송 물품 모두 수령");
			Option.Action = EDigetherHubAction::ClaimDelivery;
			Option.bEnabled = Private && !Private->PendingDelivery.IsEmpty();
		}
	}
	else if (Facility->Kind == EDigetherHubFacilityKind::Stage)
	{
		const FDigetherHubStage* Selected = Catalog->FindStage(SelectedStageId);
		View.Detail = FText::Format(NSLOCTEXT("DigetherHub", "DestinationDetail", "현재 목적지: {0}\n{1}"),
			Selected ? Selected->DisplayName : FText::GetEmpty(), bDestinationLocked
			? NSLOCTEXT("DigetherHub", "DestinationFixed", "출발 준비 중 · 선택 고정")
			: NSLOCTEXT("DigetherHub", "SharedUnlock", "해금은 팀 공용이며 요청자의 잔고에서 한 번만 차감합니다."));
		for (const FDigetherHubStage& Stage : Catalog->Stages)
		{
			FDigetherHubOption& Option = View.Options.AddDefaulted_GetRef();
			Option.Id = Stage.StageId;
			const bool bUnlocked = UnlockedStageIds.Contains(Stage.StageId);
			Option.Action = bUnlocked ? EDigetherHubAction::SelectStage : EDigetherHubAction::UnlockStage;
			Option.Cost = bUnlocked ? 0 : Stage.UnlockCostCredits;
			Option.Label = bUnlocked
				? FText::Format(NSLOCTEXT("DigetherHub", "SelectLabel", "{0} · {1}"), Stage.DisplayName,
					Stage.StageId == SelectedStageId ? NSLOCTEXT("DigetherHub", "Selected", "선택됨") : NSLOCTEXT("DigetherHub", "Select", "선택"))
				: FText::Format(NSLOCTEXT("DigetherHub", "UnlockLabel", "{0} · 해금 {1} 크레딧"), Stage.DisplayName, FText::AsNumber(Stage.UnlockCostCredits));
			Option.bEnabled = !bDestinationLocked && (bUnlocked ? Stage.StageId != SelectedStageId : View.Balance >= Stage.UnlockCostCredits)
				&& IsValid(FindStageAnchor(GetWorld(), Stage.StageId));
		}
	}
	else if (Facility->Kind == EDigetherHubFacilityKind::Bed)
	{
		const bool bOccupied = RestingPlayers.ContainsByPredicate([Facility, Requester](const FRestRecord& Rest)
		{
			return Rest.Bed.Get() == Facility && Rest.Controller.Get() != Requester;
		});
		View.Detail = View.bResting ? NSLOCTEXT("DigetherHub", "Resting", "휴식 중입니다. 일어나거나 화면을 닫으면 원래 위치로 돌아갑니다.")
			: NSLOCTEXT("DigetherHub", "BedDetails", "한 명씩 침대에서 휴식할 수 있습니다. 화면을 닫으면 일어납니다.");
		FDigetherHubOption& Option = View.Options.AddDefaulted_GetRef();
		Option.Label = View.bResting ? NSLOCTEXT("DigetherHub", "Stand", "일어나기") : NSLOCTEXT("DigetherHub", "Rest", "휴식 시작");
		Option.Action = View.bResting ? EDigetherHubAction::Stand : EDigetherHubAction::Rest;
		Option.bEnabled = !bOccupied;
	}
	else if (Facility->Kind == EDigetherHubFacilityKind::Altar)
	{
		View.Detail = FText::Format(NSLOCTEXT("DigetherHub", "AltarDetails", "팀 누적 헌납: {0} 크레딧\n헌납하면 잠시 제단이 빛납니다. 할당량 납부와는 별개입니다."), FText::AsNumber(TotalOfferingCredits));
		FDigetherHubOption& Option = View.Options.AddDefaulted_GetRef();
		Option.Label = FText::Format(NSLOCTEXT("DigetherHub", "OfferLabel", "헌납 · {0} 크레딧"), FText::AsNumber(Catalog->OfferingCostCredits));
		Option.Cost = Catalog->OfferingCostCredits;
		Option.Action = EDigetherHubAction::Offer;
		Option.bEnabled = View.Balance >= Catalog->OfferingCostCredits;
	}
	return View;
}

FDigetherHubView ADigetherHubStateActor::Execute(ADigetherGameplayPlayerController* Requester,
	ADigetherHubFacilityActor* Facility, EDigetherHubAction Action, FName OptionId, FGuid RequestId)
{
	// 사망·거리 이탈·런 전환 뒤에도 자기 침대 점유 해제 허용
	if (HasAuthority() && IsValid(Requester) && Requester->HasAuthority() && Requester->GetWorld() == GetWorld()
		&& Action == EDigetherHubAction::Stand)
	{
		EndRest(Requester);
		FDigetherHubView View = BuildView(Requester, Facility);
		View.RequestId = RequestId;
		View.bSuccess = true;
		return View;
	}
	FDigetherHubView View = BuildView(Requester, Facility);
	View.RequestId = RequestId;
	if (!View.bInteractionAllowed || !RequestId.IsValid())
	{
		View.bSuccess = false;
		return View;
	}
	ADigetherGameplayPlayerState* Player = Requester->GetPlayerState<ADigetherGameplayPlayerState>();
	const FGuid PlayerId = Player->GetGameplayPlayerId();
	if (DigetherHubRules::HasAppliedRequest(AppliedRequests, PlayerId, RequestId))
	{
		View.Message = NSLOCTEXT("DigetherHub", "RequestAlreadyApplied", "이미 처리한 요청입니다. 추가 비용은 차감하지 않았습니다.");
		return View;
	}
	FDigetherHubPlayerProgress* Private = PrivatePlayers.FindByPredicate([PlayerId](const FDigetherHubPlayerProgress& Record)
	{
		return Record.PlayerId == PlayerId;
	});
	if (Action == EDigetherHubAction::Inspect)
	{
		if (!Private || !Private->bStarterGrantConsidered)
		{
			FDigetherHubProgress Candidate = CaptureProgress();
			FDigetherWalletState Wallet = Player->CaptureState();
			int32 GrantedCredits = 0;
#if UE_BUILD_SHIPPING
			// Shipping 지원금 지급 제외, 최초 확인 기록만 유지
			constexpr int32 TrialStarterCredits = 0;
#else
			const int32 TrialStarterCredits = Catalog->TrialStarterCredits;
#endif
			const EDigetherHubResult GrantResult = DigetherHubRules::ConsiderTrialStarterGrant(Candidate,
				PlayerId, Wallet, TrialStarterCredits, GrantedCredits);
			if (GrantResult != EDigetherHubResult::Success)
			{
				View.bSuccess = false;
				View.Message = ResultText(GrantResult);
				return View;
			}
			if (Player->RestoreState(Wallet, false) != EDigetherEconomyResult::Success)
			{
				View.bSuccess = false;
				return View;
			}
			PrivatePlayers = MoveTemp(Candidate.Players);
			View = BuildView(Requester, Facility);
			View.RequestId = RequestId;
			View.bProgressChanged = true;
			if (GrantedCredits > 0)
			{
				View.Message = FText::Format(NSLOCTEXT("DigetherHub", "TrialStarterGrant", "시설 시험 지원금 {0} 크레딧을 한 번 지급했습니다."), FText::AsNumber(Catalog->TrialStarterCredits));
				Player->GetWalletComponent()->NotifyStateChanged();
			}
			DigetherHubRules::RememberAppliedRequest(AppliedRequests, PlayerId, RequestId);
		}
		return View;
	}
	if (!Private || !Private->bStarterGrantConsidered)
	{
		View.bSuccess = false;
		View.Message = NSLOCTEXT("DigetherHub", "InspectFirst", "시설 화면을 먼저 열어 주세요.");
		return View;
	}
	if (Action == EDigetherHubAction::Rest && Facility->Kind == EDigetherHubFacilityKind::Bed)
	{
		const bool bRestStarted = StartRest(Requester, Facility);
		View = BuildView(Requester, Facility);
		View.RequestId = RequestId;
		View.bSuccess = bRestStarted;
		View.Message = bRestStarted ? NSLOCTEXT("DigetherHub", "RestStarted", "휴식을 시작했습니다.")
			: NSLOCTEXT("DigetherHub", "BedOccupied", "다른 플레이어가 사용 중이거나 휴식 위치를 사용할 수 없습니다.");
		return View;
	}
	FDigetherHubProgress Candidate = CaptureProgress();
	FDigetherWalletState Wallet = Player->CaptureState();
	EDigetherHubResult Result = EDigetherHubResult::InvalidState;
	if (Action == EDigetherHubAction::Purchase && Facility->Kind == EDigetherHubFacilityKind::Shop)
	{
		if (const FDigetherHubOffer* Offer = Catalog->FindOffer(OptionId))
		{
			Result = DigetherHubRules::Purchase(Candidate, PlayerId, Wallet, *Offer);
		}
	}
	else if (Action == EDigetherHubAction::ClaimDelivery && Facility->Kind == EDigetherHubFacilityKind::Delivery)
	{
		Result = DigetherHubRules::ClaimDelivery(Candidate, PlayerId);
	}
	else if (Action == EDigetherHubAction::UnlockStage && Facility->Kind == EDigetherHubFacilityKind::Stage)
	{
		if (const FDigetherHubStage* Stage = Catalog->FindStage(OptionId))
		{
			Result = IsValid(FindStageAnchor(GetWorld(), OptionId))
				? DigetherHubRules::UnlockStage(Candidate, Wallet, *Stage, bDestinationLocked) : EDigetherHubResult::InvalidState;
		}
	}
	else if (Action == EDigetherHubAction::SelectStage && Facility->Kind == EDigetherHubFacilityKind::Stage)
	{
		if (Catalog->FindStage(OptionId) && IsValid(FindStageAnchor(GetWorld(), OptionId)))
		{
			Result = DigetherHubRules::SelectStage(Candidate, OptionId, bDestinationLocked);
		}
	}
	else if (Action == EDigetherHubAction::Offer && Facility->Kind == EDigetherHubFacilityKind::Altar)
	{
		Result = DigetherHubRules::Offer(Candidate, Wallet, Catalog->OfferingCostCredits);
	}
	if (Result == EDigetherHubResult::Success)
	{
		const bool bWalletChanged = Wallet.BalanceCredits != Player->CaptureState().BalanceCredits;
		// 지갑 복원 성공 후 구매품·팀 진행 일괄 반영
		if (Player->RestoreState(Wallet, false) != EDigetherEconomyResult::Success)
		{
			Result = EDigetherHubResult::InvalidState;
		}
		else
		{
			PrivatePlayers = MoveTemp(Candidate.Players);
			UnlockedStageIds = MoveTemp(Candidate.UnlockedStageIds);
			SelectedStageId = Candidate.SelectedStageId;
			TotalOfferingCredits = Candidate.TotalOfferingCredits;
			DigetherHubRules::RememberAppliedRequest(AppliedRequests, PlayerId, RequestId);
			if (Action == EDigetherHubAction::Offer)
			{
				OfferingPulseCounter = OfferingPulseCounter == MAX_int32 ? 1 : OfferingPulseCounter + 1;
				OfferingPulseEndsAtServerTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds() + Catalog->OfferingPulseSeconds;
				OnRep_OfferingPulseCounter();
			}
			ForceNetUpdate();
			if (bWalletChanged)
			{
				Player->GetWalletComponent()->NotifyStateChanged();
			}
		}
	}
	View = BuildView(Requester, Facility);
	View.RequestId = RequestId;
	View.bSuccess = Result == EDigetherHubResult::Success;
	View.bProgressChanged = View.bSuccess;
	View.Message = ResultText(Result);
	return View;
}

bool ADigetherHubStateActor::StartRest(ADigetherGameplayPlayerController* Requester, ADigetherHubFacilityActor* Facility)
{
	if (IsResting(Requester))
	{
		return true;
	}
	if (RestingPlayers.ContainsByPredicate([Facility](const FRestRecord& Record) { return Record.Bed.Get() == Facility; }))
	{
		return false;
	}
	APawn* Pawn = Requester->GetPawn();
	if (Facility->RestLocationOffset.ContainsNaN())
	{
		return false;
	}
	// 외형 보정과 독립된 시설 로컬 좌표로 휴식 위치 계산
	FTransform Target = Facility->GetActorTransform();
	Target.SetLocation(Facility->GetActorTransform().TransformPosition(Facility->RestLocationOffset));
	FVector AdjustedLocation = Target.GetLocation();
	if (!GetWorld()->FindTeleportSpot(Pawn, AdjustedLocation, Target.Rotator()))
	{
		return false;
	}
	Target.SetLocation(AdjustedLocation);
	FRestRecord Record;
	Record.Controller = Requester;
	Record.Pawn = Pawn;
	Record.Bed = Facility;
	Record.ReturnTransform = Pawn->GetActorTransform();
	if (!StopAndTeleport(Pawn, Requester, Target))
	{
		return false;
	}
	RestingPlayers.Add(MoveTemp(Record));
	return true;
}

void ADigetherHubStateActor::EndRest(ADigetherGameplayPlayerController* Requester)
{
	if (!HasAuthority())
	{
		return;
	}
	for (int32 Index = RestingPlayers.Num() - 1; Index >= 0; --Index)
	{
		FRestRecord Record = RestingPlayers[Index];
		if (Record.Controller.Get() != Requester)
		{
			continue;
		}
		// 위치 복귀 실패·Pawn 교체 시에도 침대 점유 우선 해제
		RestingPlayers.RemoveAt(Index);
		if (Record.Pawn.IsValid() && Record.Pawn->GetWorld() == GetWorld())
		{
			if (!StopAndTeleport(Record.Pawn.Get(), Record.Controller.Get(), Record.ReturnTransform))
			{
				UE_LOG(LogDigetherHub, Warning, TEXT("Bed occupancy released but the retained Pawn could not return to its previous position."));
			}
		}
	}
}

void ADigetherHubStateActor::EndAllRest()
{
	if (!HasAuthority())
	{
		return;
	}
	while (!RestingPlayers.IsEmpty())
	{
		EndRest(RestingPlayers.Last().Controller.Get());
	}
}

void ADigetherHubStateActor::CleanupRestingPlayers()
{
	const ADigetherGameplayGameState* State = GetWorld()->GetGameState<ADigetherGameplayGameState>();
	if (!IsValid(State) || !IsValid(State->GetRunPublicStateComponent())
		|| State->GetRunPublicStateComponent()->CaptureState().Phase == EDigetherRunPhase::InProgress)
	{
		EndAllRest();
		return;
	}
	// 접속 해제·사망·Pawn 교체 시 남은 점유 정리
	for (int32 Index = RestingPlayers.Num() - 1; Index >= 0; --Index)
	{
		const FRestRecord& Record = RestingPlayers[Index];
		ADigetherGameplayPlayerController* Controller = Record.Controller.Get();
		const ADigetherGameplayPlayerState* Player = IsValid(Controller) ? Controller->GetPlayerState<ADigetherGameplayPlayerState>() : nullptr;
		if (!IsValid(Controller) || !IsValid(Player) || Player->IsInactive() || !Record.Bed.IsValid()
			|| !Record.Pawn.IsValid() || Controller->GetPawn() != Record.Pawn.Get()
			|| !IsValid(Player->GetLifeComponent())
			|| Player->GetLifeComponent()->CaptureState().PublicState.Life != EDigetherLifeState::Active)
		{
			EndRest(Controller);
			Index = FMath::Min(Index, RestingPlayers.Num());
		}
	}
}

void ADigetherHubStateActor::LockDestination(bool bLocked)
{
	if (HasAuthority())
	{
		bDestinationLocked = bLocked;
		ForceNetUpdate();
	}
}

bool ADigetherHubStateActor::GetSelectedStageTransform(FTransform& OutTransform) const
{
	if (!HasAuthority() || !bProgressInitialized || !IsValid(Catalog) || !Catalog->FindStage(SelectedStageId)
		|| !UnlockedStageIds.Contains(SelectedStageId))
	{
		return false;
	}
	ADigetherHubStageAnchor* Anchor = FindStageAnchor(GetWorld(), SelectedStageId);
	if (!IsValid(Anchor) || Anchor->GetActorTransform().ContainsNaN()
		|| !Anchor->GetActorScale3D().Equals(FVector::OneVector))
	{
		return false;
	}
	OutTransform = Anchor->GetActorTransform();
	return true;
}

void ADigetherHubStateActor::CacheElevatorTransforms()
{
	if (!HasAuthority() || !ElevatorTransforms.IsEmpty())
	{
		return;
	}
	ADigetherElevatorActor* Elevator = nullptr;
	for (TActorIterator<ADigetherElevatorActor> It(GetWorld()); It; ++It)
	{
		if (Elevator)
		{
			return;
		}
		Elevator = *It;
	}
	if (!IsValid(Elevator))
	{
		return;
	}
	Elevator->SetReplicateMovement(true);
	ElevatorTransforms.Emplace(Elevator, Elevator->GetActorTransform());
	for (TActorIterator<ADigetherElevatorLeverActor> It(GetWorld()); It; ++It)
	{
		if (It->GetElevatorActor() == Elevator)
		{
			It->SetReplicateMovement(true);
			ElevatorTransforms.Emplace(*It, It->GetActorTransform());
		}
	}
	// 레버 구성은 GameMode에서 검사, 밖 레버 없는 개발 맵 허용
	if (ElevatorTransforms.Num() < MinElevatorTransformCount)
	{
		ElevatorTransforms.Reset();
		UE_LOG(LogDigetherHub, Error, TEXT("Trial stage travel requires one elevator and at least two matching levers."));
	}
}

bool ADigetherHubStateActor::MoveElevatorToSelectedStage()
{
	FTransform Destination;
	if (!HasAuthority() || !GetSelectedStageTransform(Destination))
	{
		return false;
	}
	CacheElevatorTransforms();
	if (ElevatorTransforms.Num() < MinElevatorTransformCount)
	{
		return false;
	}
	for (const TPair<TWeakObjectPtr<AActor>, FTransform>& Entry : ElevatorTransforms)
	{
		if (!Entry.Key.IsValid())
		{
			return false;
		}
	}
	EndAllRest();
	const FTransform HubTransform = ElevatorTransforms[0].Value;
	for (const TPair<TWeakObjectPtr<AActor>, FTransform>& Entry : ElevatorTransforms)
	{
		// 엘리베이터 기준 상대 배치 보존 후 목적지 좌표계로 이동
		const FTransform Relative = Entry.Value.GetRelativeTransform(HubTransform);
		if (!Entry.Key->SetActorTransform(Relative * Destination, false, nullptr, ETeleportType::TeleportPhysics))
		{
			ReturnElevatorToHub();
			return false;
		}
		Entry.Key->ForceNetUpdate();
	}
	return true;
}

void ADigetherHubStateActor::ReturnElevatorToHub()
{
	if (!HasAuthority())
	{
		return;
	}
	for (const TPair<TWeakObjectPtr<AActor>, FTransform>& Entry : ElevatorTransforms)
	{
		if (Entry.Key.IsValid())
		{
			Entry.Key->SetActorTransform(Entry.Value, false, nullptr, ETeleportType::TeleportPhysics);
			Entry.Key->ForceNetUpdate();
		}
	}
	LockDestination(false);
}

void ADigetherHubStateActor::OnRep_OfferingPulseCounter()
{
	if (!IsValid(OfferingLight) || !IsValid(OfferingLight->GetLightComponent()) || !IsValid(Catalog))
	{
		return;
	}
	ULightComponent* Light = OfferingLight->GetLightComponent();
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!IsValid(GameState))
	{
		return;
	}
	// 늦은 합류에도 서버 종료 시각 기준 남은 연출만 적용
	const double Remaining = OfferingPulseEndsAtServerTime - GameState->GetServerWorldTimeSeconds();
	if (Remaining <= 0.0)
	{
		RestoreOfferingLight();
		return;
	}
	if (!bOfferingLightIntensityCached)
	{
		OfferingLightBaseIntensity = Light->Intensity;
		bOfferingLightIntensityCached = true;
	}
	Light->SetIntensity(OfferingLightBaseIntensity * 3.f);
	GetWorldTimerManager().SetTimer(OfferingPulseTimer, this, &ThisClass::RestoreOfferingLight,
		static_cast<float>(Remaining), false);
}

void ADigetherHubStateActor::RestoreOfferingLight()
{
	if (bOfferingLightIntensityCached && IsValid(OfferingLight) && IsValid(OfferingLight->GetLightComponent()))
	{
		OfferingLight->GetLightComponent()->SetIntensity(OfferingLightBaseIntensity);
	}
}
