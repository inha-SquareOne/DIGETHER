#include "DigetherPrototypeGameMode.h"
#include "DigetherPrototypeGameState.h"
#include "DigetherPrototypePlayerState.h"
#include "DigetherPrototypeCorpse.h"
#include "DigetherPrototypeRules.h"
#include "DigetherPrototypeSaveGame.h"
#include "Player/DigetherPrototypePlayerController.h"
#include "Player/DigetherPrototypeCharacter.h"
#include "Player/DigetherPrototypeHUD.h"
#include "World/DigetherPrototypeWorld.h"
#include "World/DigetherExcavationTerrain.h"
#include "World/DigetherExcavationField.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "OnlineSubsystem.h"

namespace
{
// Prototype-only starting kit: a visible shovel must be usable on the first run.
// This is a trial fixture, not the unresolved production starting-equipment policy.
void GivePrototypeStarterTool(FPrototypePrivateState& State)
{
	FPrototypeItem Tool;
	Tool.Id = FGuid::NewGuid();
	Tool.bEquipment = true;
	Tool.WeightKg = 2.f;
	State.Items.Add(Tool);
}
}

ADigetherPrototypeGameMode::ADigetherPrototypeGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	GameStateClass = ADigetherPrototypeGameState::StaticClass();
	PlayerStateClass = ADigetherPrototypePlayerState::StaticClass();
	PlayerControllerClass = ADigetherPrototypePlayerController::StaticClass();
	DefaultPawnClass = ADigetherPrototypeCharacter::StaticClass();
	HUDClass = ADigetherPrototypeHUD::StaticClass();
}

ADigetherPrototypeGameState* ADigetherPrototypeGameMode::PrototypeState() const
{
	return GetGameState<ADigetherPrototypeGameState>();
}

ADigetherPrototypePlayerState* ADigetherPrototypeGameMode::PrototypePlayer(APlayerController* Controller) const
{
	return Controller ? Controller->GetPlayerState<ADigetherPrototypePlayerState>() : nullptr;
}

float ADigetherPrototypeGameMode::GetBoardingWeight(AActor* Elevator) const
{
	const ADigetherPrototypeStation* Station = Cast<ADigetherPrototypeStation>(Elevator);
	const ADigetherPrototypeElevatorCab* Cab = Station ? Station->GetElevatorCab() : nullptr;
	if (!IsValid(Cab)) return 0.f;
	float Total = 0.f;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		const ADigetherPrototypePlayerState* Player = PC ? PC->GetPlayerState<ADigetherPrototypePlayerState>() : nullptr;
		if (!Player || !PC->GetPawn() || (Player->PrivateState.Life != EPrototypeLife::Active && Player->PrivateState.Life != EPrototypeLife::Buried) || !Cab->ContainsLocation(PC->GetPawn()->GetActorLocation())) continue;
		for (const FPrototypeItem& Item : Player->PrivateState.Items) Total += Item.WeightKg;
	}
	for (const auto& Entry : DisconnectedPlayers)
	{
		const FDisconnectedPlayer& Record = Entry.Value;
		const APawn* Pawn = Record.Pawn.Get();
		if (!RunParticipants.Contains(Entry.Key) || !Pawn || (Record.State.Life != EPrototypeLife::Active && Record.State.Life != EPrototypeLife::Buried) || !Cab->ContainsLocation(Pawn->GetActorLocation())) continue;
		for (const FPrototypeItem& Item : Record.State.Items) Total += Item.WeightKg;
	}
	for (TActorIterator<ADigetherPrototypePickup> It(GetWorld()); It; ++It)
	{
		if (!It->bReserved && Cab->ContainsLocation(It->GetActorLocation())) Total += It->StoredItem.WeightKg;
	}
	return Total;
}

FString ADigetherPrototypeGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
	const FString& Options, const FString& Portal)
{
	const ADigetherPrototypeGameState* State = PrototypeState();
	FString FixtureId;
	const IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem && Subsystem->GetSubsystemName() == STEAM_SUBSYSTEM)
	{
		// Steam 연결에서는 클라이언트 URL의 임의 ID로 재접속 상태를 선택하지 않는다.
		if (!UniqueId.IsValid() || UniqueId.GetType() != STEAM_SUBSYSTEM)
		{
			return TEXT("유효한 Steam 참가자 ID가 필요합니다.");
		}
		FixtureId = UniqueId.ToString();
	}
	else
	{
		FixtureId = UGameplayStatics::ParseOption(Options, TEXT("PrototypePlayerId"));
		if (FixtureId.IsEmpty() && NewPlayerController && NewPlayerController->GetNetConnection() == nullptr)
		{
			FParse::Value(FCommandLine::Get(), TEXT("PrototypePlayerId="), FixtureId);
			if (FixtureId.IsEmpty()) FixtureId = TEXT("Host");
		}
		if (FixtureId.IsEmpty() && UniqueId.IsValid()) FixtureId = UniqueId.ToString();
	}
	if (FixtureId.IsEmpty() || FixtureId.Len() > 64)
	{
		return TEXT("시험 참가자 ID가 필요합니다. URL에 PrototypePlayerId를 지정하세요.");
	}
	if (ConnectedPlayers.Contains(FixtureId) && ConnectedPlayers[FixtureId].IsValid())
	{
		return TEXT("같은 시험 참가자 ID가 이미 연결되어 있습니다.");
	}
	const bool bReturning = DisconnectedPlayers.Contains(FixtureId);
	if (State && State->PublicState.Phase != EPrototypePhase::Base && (!bReturning || !RunParticipants.Contains(FixtureId)))
	{
		return TEXT("탐사 중에는 새로 참가할 수 없습니다.");
	}
	int32 ActiveCount = 0;
	for (const auto& Entry : ConnectedPlayers) if (Entry.Value.IsValid()) ++ActiveCount;
	if (!bReturning && ActiveCount + DisconnectedPlayers.Num() >= 4)
	{
		return TEXT("방 인원이 가득 찼습니다.");
	}
	const FString Error = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	if (!Error.IsEmpty()) return Error;
	if (ADigetherPrototypePlayerState* Player = PrototypePlayer(NewPlayerController))
	{
		Player->FixtureId = FixtureId;
		ConnectedPlayers.Add(FixtureId, NewPlayerController);
	}
	return FString();
}

void ADigetherPrototypeGameMode::PostLogin(APlayerController* NewPlayer)
{
	if (ADigetherPrototypePlayerState* Player = PrototypePlayer(NewPlayer))
	{
		const ADigetherPrototypeGameState* State = PrototypeState();
		if (State && State->PublicState.RunNumber == 0 && State->PublicState.Phase == EPrototypePhase::Base &&
			!DisconnectedPlayers.Contains(Player->FixtureId) && Player->PrivateState.Items.IsEmpty())
		{
			GivePrototypeStarterTool(Player->PrivateState);
			Player->ForceNetUpdate();
		}
	}
	Super::PostLogin(NewPlayer);
	if (ADigetherPrototypeGameState* State = PrototypeState())
	{
		if (State->PublicState.RunNumber == 0)
		{
			State->PublicState.QuotaTarget = FMath::Max(1, InitialQuota);
			State->PublicState.Message = TEXT("시험용: 거점에서 출발 장치를 사용하세요.");
			State->ForceNetUpdate();
		}
	}
}

void ADigetherPrototypeGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (ADigetherPrototypePlayerState* Player = PrototypePlayer(NewPlayer))
	{
		if (FDisconnectedPlayer* Previous = DisconnectedPlayers.Find(Player->FixtureId))
		{
			Player->PrivateState = Previous->State;
			HiddenPayout.Add(Player, Previous->Hidden);
			DeathLoss.Add(Player, Previous->Death);
			if (Previous->bBodyRecovered) RecoveredBodies.Add(Player);
			if (APawn* SavedPawn = Previous->Pawn.Get()) NewPlayer->Possess(SavedPawn);
			for (TActorIterator<ADigetherPrototypeCorpse> It(GetWorld()); It; ++It)
			{
				if (It->DeceasedFixtureId == Player->FixtureId) { It->Deceased = Player; It->ForceNetUpdate(); }
			}
			DisconnectedPlayers.Remove(Player->FixtureId);
			UpdatePlayer(Player);
		}
	}
	if (NewPlayer && NewPlayer->GetPawn()) return;
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

bool ADigetherPrototypeGameMode::HandleControllerDisconnect(APlayerController* Caller)
{
	ADigetherPrototypePlayerState* Player = PrototypePlayer(Caller);
	if (!HasAuthority() || !Player || Player->FixtureId.IsEmpty()) return false;
	if (DisconnectedPlayers.Contains(Player->FixtureId)) return true;
	CancelInteraction(Caller);
	FDisconnectedPlayer& Record = DisconnectedPlayers.Add(Player->FixtureId);
	Record.State = Player->PrivateState;
	Record.Pawn = Caller->GetPawn();
	Record.Hidden = HiddenPayout.FindRef(Player);
	Record.Death = DeathLoss.FindRef(Player);
	Record.bBodyRecovered = RecoveredBodies.Contains(Player);
	for (TActorIterator<ADigetherPrototypeCorpse> It(GetWorld()); It; ++It)
	{
		if (APawn* Pawn = Caller->GetPawn(); Pawn && It->Carrier == Pawn)
		{
			It->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			It->SetActorLocation(Pawn->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
			It->Carrier = nullptr;
			It->ForceNetUpdate();
		}
		if (It->Deceased == Player)
		{
			It->Deceased = nullptr;
			It->ForceNetUpdate();
		}
	}
	ConnectedPlayers.Remove(Player->FixtureId);
	HiddenPayout.Remove(Player);
	DeathLoss.Remove(Player);
	RecoveredBodies.Remove(Player);
	return true;
}

void ADigetherPrototypeGameMode::Logout(AController* Exiting)
{
	if (APlayerController* Caller = Cast<APlayerController>(Exiting)) HandleControllerDisconnect(Caller);
	Super::Logout(Exiting);
}

void ADigetherPrototypeGameMode::UpdatePlayer(ADigetherPrototypePlayerState* Player)
{
	if (Player)
	{
		Player->PublicLife = Player->PrivateState.Life;
		Player->PublicBurialLevel = Player->PrivateState.BurialLevel;
		Player->ForceNetUpdate();
	}
}

bool ADigetherPrototypeGameMode::CanInteract(APlayerController* Caller, AActor* Target, FString& OutMessage) const
{
	if (!HasAuthority() || !IsValid(Caller) || !IsValid(Target) || !IsValid(Caller->GetPawn()))
	{
		OutMessage = TEXT("상호작용 대상을 확인할 수 없습니다.");
		return false;
	}
	const ADigetherPrototypePlayerState* Player = PrototypePlayer(Caller);
	if (!Player || Player->PrivateState.Life != EPrototypeLife::Active)
	{
		OutMessage = TEXT("현재 행동할 수 없습니다.");
		return false;
	}
	if (FVector::DistSquared(Caller->GetPawn()->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(InteractionDistance))
	{
		OutMessage = TEXT("대상에 더 가까이 다가가세요.");
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PrototypeInteraction), false, Caller->GetPawn());
	const FVector Origin = Caller->GetPawn()->GetActorLocation() + FVector(0.f, 0.f, 50.f);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, Target->GetActorLocation(), ECC_Visibility, Params) && Hit.GetActor() != Target)
	{
		OutMessage = TEXT("대상이 장애물에 가려져 있습니다.");
		return false;
	}
	return true;
}

bool ADigetherPrototypeGameMode::RequestStartRun(APlayerController* Caller, FString& OutMessage)
{
	ADigetherPrototypeGameState* State = PrototypeState();
	if (!HasAuthority() || !State || !PrototypePlayer(Caller) || State->PublicState.Phase != EPrototypePhase::Base)
	{
		OutMessage = TEXT("지금은 출발할 수 없습니다.");
		return false;
	}
	if (!bCompletingHold)
	{
		if (HoldingPlayer.IsValid() || !Caller->GetPawn() || !HoldingStation.IsValid())
		{
			OutMessage = TEXT("다른 레버가 사용 중입니다.");
			return false;
		}
		HoldingPlayer = Caller;
		HoldingElevator.Reset();
		HoldStartLocation = Caller->GetPawn()->GetActorLocation();
		HoldElapsed = 0.f;
		HoldDuration = State->PublicState.Day == 4 ? 4.f : 2.f;
		HoldRunId = State->PublicState.RunId;
		bHoldElevator = false;
		bHoldEmergency = false;
		OutMessage = State->PublicState.Day == 4 ? TEXT("시험 경고: 4일차 출발 레버를 계속 누르세요.") : TEXT("출발 레버를 계속 누르세요.");
		return true;
	}
	if (APlayerController* Depositor = DepositHolder.Get()) CancelInteraction(Depositor);
	DepartureCount = 0;
	RunParticipants.Reset();
	CollectedValue = 0;
	ReturnedThisRun.Reset();
	HiddenPayout.Reset();
	DeathLoss.Reset();
	RecoveredBodies.Reset();
	TimeAccumulator = 0.f;
	bRunFinalized = false;
	bQuotaFailedAtDeparture = State->PublicState.Day == 4 && State->PublicState.QuotaProgress < State->PublicState.QuotaTarget;
	TArray<TPair<TWeakObjectPtr<APawn>, FTransform>> Teleported;
	TArray<TPair<TWeakObjectPtr<ADigetherPrototypePlayerState>, FPrototypePrivateState>> PreviousStates;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		ADigetherPrototypePlayerState* Player = PrototypePlayer(PC);
		if (!Player || !PC->GetPawn()) continue;
		const FTransform Previous = PC->GetPawn()->GetActorTransform();
		const FVector Destination(2700.f, 120.f * (DepartureCount - 1), 110.f);
		if (!PC->GetPawn()->TeleportTo(Destination, PC->GetPawn()->GetActorRotation(), false, false))
		{
			for (const auto& Moved : Teleported)
			{
				if (APawn* Pawn = Moved.Key.Get()) Pawn->SetActorTransform(Moved.Value, false, nullptr, ETeleportType::TeleportPhysics);
			}
			for (const auto& Entry : PreviousStates)
			{
				if (ADigetherPrototypePlayerState* SavedPlayer = Entry.Key.Get())
				{
					SavedPlayer->PrivateState = Entry.Value;
					UpdatePlayer(SavedPlayer);
				}
			}
			bRunFinalized = true;
			OutMessage = TEXT("시험 스폰 위치가 막혀 출발을 취소했습니다.");
			return false;
		}
		Teleported.Emplace(PC->GetPawn(), Previous);
		PreviousStates.Emplace(Player, Player->PrivateState);
		++DepartureCount;
		RunParticipants.Add(Player->FixtureId);
		Player->PrivateState.Life = EPrototypeLife::Active;
		Player->PrivateState.BurialLevel = 0;
		Player->PrivateState.Health = 100.f;
		Player->PrivateState.WoodCount = 2;
		Player->PrivateState.LastPayout = 0;
		Player->PrivateState.Message = TEXT("시험 탐사 시작: 물품을 회수하고 엘리베이터로 돌아오세요.");
		UpdatePlayer(Player);
	}
	if (DepartureCount == 0)
	{
		bRunFinalized = true;
		OutMessage = TEXT("출발할 플레이어가 없습니다.");
		return false;
	}
	++State->PublicState.RunNumber;
	State->PublicState.RunId = FGuid::NewGuid();
	State->PublicState.Phase = EPrototypePhase::Expedition;
	State->PublicState.RemainingSeconds = FMath::Max(1.f, RunSeconds);
	State->PublicState.ElevatorTrips = 0;
	State->PublicState.MaxElevatorTrips = FMath::Clamp(FixtureElevatorTrips, 1, 2);
	State->PublicState.ElevatorState = EPrototypeElevatorState::Ground;
	State->PublicState.ElevatorProgress = 0.f;
	bTimeoutPending = false;
	bEmptyReturn = false;
	for (TActorIterator<ADigetherPrototypeElevatorCab> It(GetWorld()); It; ++It) It->SetTravelProgress(0.f);
	State->PublicState.Message = TEXT("시험 탐사 중: 회수 후 엘리베이터를 이용하세요.");
	State->ForceNetUpdate();
	OutMessage = TEXT("시험 탐사를 시작했습니다.");
	return true;
}

bool ADigetherPrototypeGameMode::RequestStartRunAt(APlayerController* Caller, AActor* Station, FString& OutMessage)
{
	if (!CanInteract(Caller, Station, OutMessage)) return false;
	if (HoldingPlayer.IsValid()) { OutMessage = TEXT("다른 레버가 사용 중입니다."); return false; }
	HoldingStation = Station;
	if (!RequestStartRun(Caller, OutMessage)) { HoldingStation.Reset(); return false; }
	return true;
}

bool ADigetherPrototypeGameMode::RequestPickup(APlayerController* Caller, AActor* Pickup, int32 Value,
	float WeightKg, bool bHiddenSlot, bool bTwoHanded, FString& OutMessage)
{
	FPrototypeItem Item;
	Item.Id = FGuid::NewGuid();
	Item.Value = Value;
	Item.WeightKg = WeightKg;
	Item.bTwoHanded = bTwoHanded;
	return RequestPickupItem(Caller, Pickup, Item, bHiddenSlot, OutMessage);
}

bool ADigetherPrototypeGameMode::RequestPickupItem(APlayerController* Caller, AActor* Pickup,
	const FPrototypeItem& SourceItem, bool bHiddenSlot, FString& OutMessage)
{
	ADigetherPrototypeGameState* State = PrototypeState();
	ADigetherPrototypePlayerState* Player = PrototypePlayer(Caller);
	if (!State || (State->PublicState.Phase != EPrototypePhase::Expedition &&
		!(State->PublicState.Phase == EPrototypePhase::Base && SourceItem.bEquipment && Pickup && Pickup->ActorHasTag(TEXT("PrototypeReturnedEquipment")))) ||
		!Player || !CanInteract(Caller, Pickup, OutMessage)) return false;
	if (!SourceItem.Id.IsValid() || SourceItem.Value < 0 || !FMath::IsFinite(SourceItem.WeightKg) || SourceItem.WeightKg < 0.f || (bHiddenSlot && SourceItem.bTwoHanded))
	{
		OutMessage = TEXT("이 물품은 해당 슬롯에 넣을 수 없습니다.");
		return false;
	}
	int32 NormalCount = 0;
	bool bHasHidden = false;
	for (TActorIterator<ADigetherPrototypeCorpse> It(GetWorld()); It; ++It)
	{
		if (It->Carrier == Caller->GetPawn())
		{
			OutMessage = TEXT("시체를 운반하는 동안 다른 물품을 주울 수 없습니다.");
			return false;
		}
	}
	for (const FPrototypeItem& Item : Player->PrivateState.Items)
	{
		NormalCount += !Item.bHidden;
		bHasHidden |= Item.bHidden;
		if (Item.bTwoHanded)
		{
			OutMessage = TEXT("양손 물품을 들고 있어 다른 물품을 주울 수 없습니다.");
			return false;
		}
	}
	if ((bHiddenSlot && bHasHidden) || (!bHiddenSlot && NormalCount >= 4))
	{
		OutMessage = TEXT("빈 슬롯이 없습니다.");
		return false;
	}
	FPrototypeItem Item = SourceItem;
	Item.bHidden = bHiddenSlot;
	Player->PrivateState.Items.Add(Item);
	OutMessage = bHiddenSlot ? TEXT("시험 물품을 히든 슬롯에 넣었습니다.") : TEXT("시험 물품을 획득했습니다.");
	Player->PrivateState.Message = OutMessage;
	UpdatePlayer(Player);
	return true;
}

bool ADigetherPrototypeGameMode::RequestElevator(APlayerController* Caller, AActor* Elevator, FString& OutMessage)
{
	ADigetherPrototypeGameState* State = PrototypeState();
	ADigetherPrototypeStation* Station = Cast<ADigetherPrototypeStation>(Elevator);
	ADigetherPrototypeElevatorCab* Cab = Station ? Station->GetElevatorCab() : nullptr;
	const bool bAutomaticTimeout = bCompletingHold && bTimeoutPending;
	if (!HasAuthority() || !State || State->PublicState.Phase != EPrototypePhase::Expedition ||
		State->PublicState.ElevatorState != EPrototypeElevatorState::Ground || !Cab ||
		(!bAutomaticTimeout && !CanInteract(Caller, Elevator, OutMessage))) return false;
	if (GetBoardingWeight(Elevator) > ElevatorCapacityKg)
	{
		OutMessage = TEXT("시험 중량 한도를 초과했습니다. 물품을 내려놓으세요.");
		return false;
	}
	if (!bCompletingHold)
	{
		if (HoldingPlayer.IsValid())
		{
			OutMessage = TEXT("다른 레버가 사용 중입니다.");
			return false;
		}
		HoldingPlayer = Caller;
		HoldingElevator = Elevator;
		HoldStartLocation = Caller->GetPawn()->GetActorLocation();
		HoldElapsed = 0.f;
		HoldDuration = 2.f;
		HoldRunId = State->PublicState.RunId;
		bHoldElevator = true;
		bHoldEmergency = false;
		OutMessage = TEXT("상승 레버를 계속 누르세요.");
		return true;
	}

	TransitCab = Cab;
	TransitPlayers.Reset();
	TransitPreviousLives.Reset();
	TransitPickups.Reset();
	TransitPickupItems.Reset();
	TransitPickupTransforms.Reset();
	TransitCorpses.Reset();
	TransitCorpseIds.Reset();
	TransitCorpseTransforms.Reset();
	TransitOriginalTransforms.Reset();
	NonBoardingPositions.Reset();
	for (TActorIterator<APawn> It(GetWorld()); It; ++It) NonBoardingPositions.Add(*It, It->GetActorTransform());

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		ADigetherPrototypePlayerState* Player = PrototypePlayer(PC);
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Player || !Pawn || (Player->PrivateState.Life != EPrototypeLife::Active &&
			Player->PrivateState.Life != EPrototypeLife::Buried) || !Cab->ContainsLocation(Pawn->GetActorLocation())) continue;
		TransitPlayers.Add(Player->FixtureId);
		TransitPreviousLives.Add(Player->FixtureId, Player->PrivateState.Life);
		TransitOriginalTransforms.Add(Pawn, Pawn->GetActorTransform());
		Player->PrivateState.Life = EPrototypeLife::InTransit;
		UpdatePlayer(Player);
		if (!Pawn->AttachToActor(Cab, FAttachmentTransformRules::KeepWorldTransform))
		{
			CancelElevatorTransit();
			OutMessage = TEXT("시험 승강 탑승에 실패해 운행을 취소했습니다.");
			return false;
		}
	}
	for (auto& Entry : DisconnectedPlayers)
	{
		FDisconnectedPlayer& Record = Entry.Value;
		APawn* Pawn = Record.Pawn.Get();
		if (!RunParticipants.Contains(Entry.Key) || !Pawn || (Record.State.Life != EPrototypeLife::Active && Record.State.Life != EPrototypeLife::Buried) ||
			!Cab->ContainsLocation(Pawn->GetActorLocation())) continue;
		TransitPlayers.Add(Entry.Key);
		TransitPreviousLives.Add(Entry.Key, Record.State.Life);
		TransitOriginalTransforms.Add(Pawn, Pawn->GetActorTransform());
		Record.State.Life = EPrototypeLife::InTransit;
		if (!Pawn->AttachToActor(Cab, FAttachmentTransformRules::KeepWorldTransform))
		{
			CancelElevatorTransit();
			OutMessage = TEXT("이탈 참가자의 시험 탑승에 실패해 운행을 취소했습니다.");
			return false;
		}
	}
	for (TActorIterator<ADigetherPrototypePickup> It(GetWorld()); It; ++It)
	{
		ADigetherPrototypePickup* Pickup = *It;
		if (Pickup->bReserved || !Cab->ContainsLocation(Pickup->GetActorLocation())) continue;
		TransitPickups.Add(Pickup);
		TransitPickupItems.Add(Pickup->StoredItem);
		TransitPickupTransforms.Add(Pickup->GetActorTransform());
		Pickup->bReserved = true;
		Pickup->ForceNetUpdate();
		if (!Pickup->AttachToActor(Cab, FAttachmentTransformRules::KeepWorldTransform))
		{
			CancelElevatorTransit();
			OutMessage = TEXT("시험 화물 예약에 실패해 운행을 취소했습니다.");
			return false;
		}
	}
	for (TActorIterator<ADigetherPrototypeCorpse> It(GetWorld()); It; ++It)
	{
		ADigetherPrototypeCorpse* Corpse = *It;
		if (Corpse->bReserved || !Cab->ContainsLocation(Corpse->GetActorLocation())) continue;
		TransitCorpses.Add(Corpse);
		TransitCorpseIds.Add(Corpse->DeceasedFixtureId);
		TransitCorpseTransforms.Add(Corpse->GetActorTransform());
		Corpse->bReserved = true;
		Corpse->ForceNetUpdate();
		if (!Corpse->Carrier && !Corpse->AttachToActor(Cab, FAttachmentTransformRules::KeepWorldTransform))
		{
			CancelElevatorTransit();
			OutMessage = TEXT("시험 시체 예약에 실패해 운행을 취소했습니다.");
			return false;
		}
	}
	TSet<FGuid> SeenItems;
	for (const FString& Id : TransitPlayers)
	{
		const APlayerController* PC = ConnectedPlayers.FindRef(Id).Get();
		const ADigetherPrototypePlayerState* Player = PC ? PC->GetPlayerState<ADigetherPrototypePlayerState>() : nullptr;
		const FDisconnectedPlayer* Offline = DisconnectedPlayers.Find(Id);
		const FPrototypePrivateState* Private = Player ? &Player->PrivateState : Offline ? &Offline->State : nullptr;
		if (!Private) { CancelElevatorTransit(); OutMessage = TEXT("시험 승객 정보를 확인할 수 없습니다."); return false; }
		for (const FPrototypeItem& Item : Private->Items)
		{
			if (!Item.Id.IsValid() || SeenItems.Contains(Item.Id)) { CancelElevatorTransit(); OutMessage = TEXT("중복된 시험 물품이 있어 운행을 취소했습니다."); return false; }
			SeenItems.Add(Item.Id);
		}
	}
	for (const FPrototypeItem& Item : TransitPickupItems)
	{
		if (!Item.Id.IsValid() || SeenItems.Contains(Item.Id)) { CancelElevatorTransit(); OutMessage = TEXT("중복된 시험 물품이 있어 운행을 취소했습니다."); return false; }
		SeenItems.Add(Item.Id);
	}
	ElevatorElapsed = 0.f;
	bEmptyReturn = false;
	State->PublicState.ElevatorProgress = 0.f;
	State->PublicState.ElevatorState = State->PublicState.ElevatorTrips + 1 >= State->PublicState.MaxElevatorTrips
		? EPrototypeElevatorState::FinalAscending : EPrototypeElevatorState::Ascending;
	TransitRemainingBeforePenalty = State->PublicState.RemainingSeconds;
	TransitElapsedSeconds = 0.f;
	bTransitPenaltyApplied = true;
	State->PublicState.RemainingSeconds = FMath::Max(0.f, State->PublicState.RemainingSeconds - FMath::Max(0.f, FixtureAscentPenaltySeconds));
	bTimeoutPending = State->PublicState.RemainingSeconds <= 0.f;
	State->PublicState.Message = TEXT("시험 승강 중: 도착 후 회수 결과가 확정됩니다.");
	State->ForceNetUpdate();
	OutMessage = TEXT("시험 엘리베이터 상승 중입니다.");
	return true;
}
void ADigetherPrototypeGameMode::CancelElevatorTransit()
{
	ADigetherPrototypeGameState* State = PrototypeState();
	if (!State) return;
	if (bTransitPenaltyApplied)
	{
		State->PublicState.RemainingSeconds = FMath::Max(0.f, TransitRemainingBeforePenalty - TransitElapsedSeconds);
		bTimeoutPending = State->PublicState.RemainingSeconds <= 0.f;
		bTransitPenaltyApplied = false;
	}
	if (ADigetherPrototypeElevatorCab* Cab = TransitCab.Get()) Cab->SetTravelProgress(0.f);
	// 모든 예약 Actor를 먼저 확보해야 복원 중간에 한 아이템만 사라지지 않는다.
	for (int32 Index = 0; Index < TransitPickups.Num(); ++Index)
	{
		if (TransitPickups[Index].IsValid()) continue;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADigetherPrototypePickup* Pickup = GetWorld()->SpawnActor<ADigetherPrototypePickup>(
			ADigetherPrototypePickup::StaticClass(), TransitPickupTransforms[Index], Params);
		if (!Pickup)
		{
			bTransitRollbackPending = true;
			State->PublicState.ElevatorState = EPrototypeElevatorState::Top;
			State->PublicState.Message = TEXT("시험 승강 취소 대기: 화물 복원을 다시 시도합니다.");
			State->ForceNetUpdate();
			return;
		}
		Pickup->StoredItem = TransitPickupItems[Index];
		Pickup->bReserved = true;
		TransitPickups[Index] = Pickup;
	}
	for (int32 Index = 0; Index < TransitCorpses.Num(); ++Index)
	{
		if (TransitCorpses[Index].IsValid()) continue;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADigetherPrototypeCorpse* Corpse = GetWorld()->SpawnActor<ADigetherPrototypeCorpse>(
			ADigetherPrototypeCorpse::StaticClass(), TransitCorpseTransforms[Index], Params);
		if (!Corpse)
		{
			bTransitRollbackPending = true;
			State->PublicState.ElevatorState = EPrototypeElevatorState::Top;
			State->PublicState.Message = TEXT("시험 승강 취소 대기: 시체 복원을 다시 시도합니다.");
			State->ForceNetUpdate();
			return;
		}
		Corpse->DeceasedFixtureId = TransitCorpseIds[Index];
		if (APlayerController* PC = ConnectedPlayers.FindRef(Corpse->DeceasedFixtureId).Get())
			Corpse->Deceased = PrototypePlayer(PC);
		Corpse->bReserved = true;
		TransitCorpses[Index] = Corpse;
	}
	for (int32 Index = 0; Index < TransitPickups.Num(); ++Index)
	{
		ADigetherPrototypePickup* Pickup = TransitPickups[Index].Get();
		Pickup->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Pickup->SetActorTransform(TransitPickupTransforms[Index], false, nullptr, ETeleportType::TeleportPhysics);
		Pickup->bReserved = false;
		Pickup->ForceNetUpdate();
	}
	for (int32 Index = 0; Index < TransitCorpses.Num(); ++Index)
	{
		ADigetherPrototypeCorpse* Corpse = TransitCorpses[Index].Get();
		if (!Corpse->Carrier)
		{
			Corpse->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Corpse->SetActorTransform(TransitCorpseTransforms[Index], false, nullptr, ETeleportType::TeleportPhysics);
		}
		Corpse->bReserved = false;
		Corpse->ForceNetUpdate();
	}
	TransitPickups.Reset();
	TransitPickupItems.Reset();
	TransitPickupTransforms.Reset();
	TransitCorpses.Reset();
	TransitCorpseIds.Reset();
	TransitCorpseTransforms.Reset();
	for (auto It = TransitOriginalTransforms.CreateIterator(); It; ++It)
	{
		APawn* Pawn = Cast<APawn>(It.Key().Get());
		if (!Pawn) continue;
		Pawn->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		const FTransform& Original = It.Value();
		bool bRestored = Pawn->TeleportTo(Original.GetLocation(), Original.Rotator(), false, false);
		for (int32 Attempt = 0; !bRestored && Attempt < 8; ++Attempt)
		{
			const float Angle = 2.f * PI * Attempt / 8.f;
			const FVector Offset(FMath::Cos(Angle) * 120.f, FMath::Sin(Angle) * 120.f, 0.f);
			bRestored = Pawn->TeleportTo(Original.GetLocation() + Offset, Original.Rotator(), false, false);
		}
		if (bRestored) It.RemoveCurrent();
	}
	if (!TransitOriginalTransforms.IsEmpty())
	{
		bTransitRollbackPending = true;
		State->PublicState.ElevatorState = EPrototypeElevatorState::Top;
		State->PublicState.Message = TEXT("시험 승강 취소 대기: 승객 원위치를 찾고 있습니다.");
		State->ForceNetUpdate();
		return;
	}
	for (const auto& Entry : TransitPreviousLives)
	{
		APlayerController* PC = ConnectedPlayers.FindRef(Entry.Key).Get();
		ADigetherPrototypePlayerState* Player = PrototypePlayer(PC);
		if (Player) { Player->PrivateState.Life = Entry.Value; UpdatePlayer(Player); }
		else if (FDisconnectedPlayer* Record = DisconnectedPlayers.Find(Entry.Key)) Record->State.Life = Entry.Value;
	}
	bTransitRollbackPending = false;
	TransitPlayers.Reset();
	TransitPreviousLives.Reset();
	TransitCab.Reset();
	State->PublicState.ElevatorState = EPrototypeElevatorState::Ground;
	State->PublicState.ElevatorProgress = 0.f;
	State->PublicState.Message = TEXT("시험 승강이 취소되었습니다.");
	State->ForceNetUpdate();
}

void ADigetherPrototypeGameMode::CompleteElevatorAscent()
{
	ADigetherPrototypeGameState* State = PrototypeState();
	if (!State || !TransitCab.IsValid()) { CancelElevatorTransit(); return; }
	for (const auto& Pickup : TransitPickups)
	{
		if (!Pickup.IsValid()) { CancelElevatorTransit(); return; }
	}
	for (const auto& Corpse : TransitCorpses)
	{
		if (!Corpse.IsValid()) { CancelElevatorTransit(); return; }
	}
	for (const FString& Id : TransitPlayers)
	{
		APlayerController* PC = ConnectedPlayers.FindRef(Id).Get();
		ADigetherPrototypePlayerState* Player = PrototypePlayer(PC);
		FDisconnectedPlayer* Offline = DisconnectedPlayers.Find(Id);
		FPrototypePrivateState* Private = Player ? &Player->PrivateState : Offline ? &Offline->State : nullptr;
		if (!Private || Private->Life != EPrototypeLife::InTransit) { CancelElevatorTransit(); return; }
	}
	bTransitPenaltyApplied = false;
	for (const FString& Id : TransitPlayers)
	{
		APlayerController* PC = ConnectedPlayers.FindRef(Id).Get();
		ADigetherPrototypePlayerState* Player = PrototypePlayer(PC);
		FDisconnectedPlayer* Offline = DisconnectedPlayers.Find(Id);
		FPrototypePrivateState* Private = Player ? &Player->PrivateState : &Offline->State;
		for (const FPrototypeItem& Item : Private->Items)
		{
			if (Item.bEquipment) continue;
			if (Item.bHidden)
			{
				if (Player) HiddenPayout.FindOrAdd(Player) = static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(HiddenPayout.FindRef(Player)) + Item.Value));
				else Offline->Hidden = static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(Offline->Hidden) + Item.Value));
			}
			else CollectedValue = static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(CollectedValue) + Item.Value));
		}
		Private->Items.RemoveAll([](const FPrototypeItem& Item) { return !Item.bEquipment; });
		Private->Life = EPrototypeLife::Escaped;
		Private->Message = TEXT("시험 탈출 완료: 나머지 참가자를 기다립니다.");
		if (Player) UpdatePlayer(Player);
		APawn* Pawn = nullptr;
		if (PC) Pawn = PC->GetPawn();
		else if (Offline) Pawn = Offline->Pawn.Get();
		if (Pawn) Pawn->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
	for (int32 Index = 0; Index < TransitPickups.Num(); ++Index)
	{
		ADigetherPrototypePickup* Pickup = TransitPickups[Index].Get();
		const FPrototypeItem Item = TransitPickupItems[Index];
		if (Item.bEquipment)
		{
			Pickup->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Pickup->SetActorLocation(FVector(-500.f, 350.f + Index * 45.f, 50.f), false, nullptr, ETeleportType::TeleportPhysics);
			Pickup->StoredItem.bHidden = false;
			Pickup->bReserved = false;
			Pickup->Tags.AddUnique(TEXT("PrototypeReturnedEquipment"));
			ReturnedThisRun.Add(Pickup);
			Pickup->ForceNetUpdate();
		}
		else
		{
			CollectedValue = static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(CollectedValue) + Item.Value));
			Pickup->Destroy();
		}
	}
	for (const auto& WeakCorpse : TransitCorpses)
	{
		ADigetherPrototypeCorpse* Corpse = WeakCorpse.Get();
		if (ADigetherPrototypePlayerState* Dead = Corpse->Deceased)
		{
			if (!RecoveredBodies.Contains(Dead)) RecoveredBodies.Add(Dead);
		}
		else if (FDisconnectedPlayer* DeadRecord = DisconnectedPlayers.Find(Corpse->DeceasedFixtureId))
		{
			DeadRecord->bBodyRecovered = true;
		}
		Corpse->Destroy();
	}
	++State->PublicState.ElevatorTrips;
	TransitPlayers.Reset();
	TransitPreviousLives.Reset();
	TransitOriginalTransforms.Reset();
	TransitPickups.Reset();
	TransitPickupItems.Reset();
	TransitPickupTransforms.Reset();
	TransitCorpses.Reset();
	TransitCorpseIds.Reset();
	TransitCorpseTransforms.Reset();
	bool bAnyRemaining = false;
	for (APlayerState* BasePlayer : State->PlayerArray)
	{
		const ADigetherPrototypePlayerState* Player = Cast<ADigetherPrototypePlayerState>(BasePlayer);
		if (Player && (Player->PrivateState.Life == EPrototypeLife::Active || Player->PrivateState.Life == EPrototypeLife::Buried)) bAnyRemaining = true;
	}
	for (const auto& Entry : DisconnectedPlayers)
	{
		if (RunParticipants.Contains(Entry.Key) && (Entry.Value.State.Life == EPrototypeLife::Active || Entry.Value.State.Life == EPrototypeLife::Buried)) bAnyRemaining = true;
	}
	if (bTimeoutPending || !bAnyRemaining || State->PublicState.ElevatorTrips >= State->PublicState.MaxElevatorTrips)
	{
		FinishRun();
		return;
	}
	ElevatorElapsed = 0.f;
	State->PublicState.ElevatorState = EPrototypeElevatorState::Top;
	State->PublicState.ElevatorProgress = 1.f;
	State->PublicState.Message = TEXT("시험 1차 회수 완료: 승강기가 자동으로 내려옵니다.");
	State->ForceNetUpdate();
}

void ADigetherPrototypeGameMode::UpdateElevator(float DeltaSeconds)
{
	ADigetherPrototypeGameState* State = PrototypeState();
	if (!State) return;
	if (bTransitRollbackPending)
	{
		CancelElevatorTransit();
		return;
	}
	ADigetherPrototypeElevatorCab* Cab = TransitCab.Get();
	if (!Cab)
	{
		if (State->PublicState.ElevatorState != EPrototypeElevatorState::Ground) CancelElevatorTransit();
		return;
	}
	const EPrototypeElevatorState ElevatorState = State->PublicState.ElevatorState;
	if (ElevatorState == EPrototypeElevatorState::Top)
	{
		ElevatorElapsed += DeltaSeconds;
		if (ElevatorElapsed >= 0.5f)
		{
			ElevatorElapsed = 0.f;
			State->PublicState.ElevatorState = EPrototypeElevatorState::Descending;
		}
		return;
	}
	if (ElevatorState != EPrototypeElevatorState::Ascending && ElevatorState != EPrototypeElevatorState::FinalAscending &&
		ElevatorState != EPrototypeElevatorState::Descending) return;
	if (bTransitPenaltyApplied) TransitElapsedSeconds += DeltaSeconds;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Pawn = *It;
		if (TransitOriginalTransforms.Contains(Pawn) || !Cab->ContainsLocation(Pawn->GetActorLocation())) continue;
		const FTransform* Previous = NonBoardingPositions.Find(Pawn);
		FVector Destination = Previous ? Previous->GetLocation() : Pawn->GetActorLocation();
		if (Cab->ContainsLocation(Destination)) Destination = Cab->GetActorLocation() + FVector(Cab->BoardingRadius + 150.f, 0.f, 110.f);
		if (!Pawn->TeleportTo(Destination, Pawn->GetActorRotation(), false, false))
		{
			State->PublicState.Message = TEXT("시험 승강 대기: 탑승 영역을 비우고 있습니다.");
			State->ForceNetUpdate();
			return;
		}
	}
	ElevatorElapsed += DeltaSeconds;
	const float Fraction = FMath::Clamp(ElevatorElapsed / FMath::Max(0.1f, FixtureElevatorTravelSeconds), 0.f, 1.f);
	State->PublicState.ElevatorProgress = ElevatorState == EPrototypeElevatorState::Descending ? 1.f - Fraction : Fraction;
	Cab->SetTravelProgress(State->PublicState.ElevatorProgress);
	if (Fraction < 1.f) return;
	if (ElevatorState == EPrototypeElevatorState::Descending)
	{
		State->PublicState.ElevatorState = EPrototypeElevatorState::Ground;
		State->PublicState.Message = TEXT("시험 승강기가 다시 내려왔습니다.");
		TransitCab.Reset();
	}
	else if (bEmptyReturn)
	{
		bEmptyReturn = false;
		FinishRun();
	}
	else CompleteElevatorAscent();
	State->ForceNetUpdate();
}
void ADigetherPrototypeGameMode::KillPlayer(ADigetherPrototypePlayerState* Player)
{
	if (!Player || Player->PrivateState.Life == EPrototypeLife::Dead || Player->PrivateState.Life == EPrototypeLife::Escaped) return;
	APawn* Pawn = Player->GetPawn();
	if (Pawn)
	{
		for (TActorIterator<ADigetherPrototypeCorpse> It(GetWorld()); It; ++It)
		{
			if (It->Carrier == Pawn)
			{
				It->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
				It->SetActorLocation(Pawn->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
				It->Carrier = nullptr;
				It->ForceNetUpdate();
			}
		}
		for (const FPrototypeItem& Item : Player->PrivateState.Items)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FVector Location = Pawn->GetActorLocation();
			if (ADigetherPrototypePickup* Dropped = GetWorld()->SpawnActor<ADigetherPrototypePickup>(Location, FRotator::ZeroRotator, Params))
			{
				Dropped->StoredItem = Item;
			}
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ADigetherPrototypeCorpse* Corpse = GetWorld()->SpawnActor<ADigetherPrototypeCorpse>(Pawn->GetActorLocation(), FRotator::ZeroRotator, Params))
		{
			Corpse->Deceased = Player;
			Corpse->DeceasedFixtureId = Player->FixtureId;
			Corpse->ForceNetUpdate();
		}
	}
	DeathLoss.Add(Player, Player->PrivateState.Money);
	Player->PrivateState.Money = 0;
	Player->PrivateState.Health = 0.f;
	Player->PrivateState.Life = EPrototypeLife::Dead;
	Player->PrivateState.BurialLevel = 0;
	Player->PrivateState.Items.Reset();
	Player->PrivateState.Message = TEXT("시험 사망: 잔고를 잃었습니다.");
	UpdatePlayer(Player);
}

void ADigetherPrototypeGameMode::KillDisconnectedPlayer(const FString& FixtureId)
{
	FDisconnectedPlayer* Record = DisconnectedPlayers.Find(FixtureId);
	if (!Record || Record->State.Life == EPrototypeLife::Dead || Record->State.Life == EPrototypeLife::Escaped) return;
	if (APawn* Pawn = Record->Pawn.Get())
	{
		for (const FPrototypeItem& Item : Record->State.Items)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ADigetherPrototypePickup* Dropped = GetWorld()->SpawnActor<ADigetherPrototypePickup>(
				Pawn->GetActorLocation(), FRotator::ZeroRotator, Params)) Dropped->StoredItem = Item;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ADigetherPrototypeCorpse* Corpse = GetWorld()->SpawnActor<ADigetherPrototypeCorpse>(Pawn->GetActorLocation(), FRotator::ZeroRotator, Params))
		{
			Corpse->DeceasedFixtureId = FixtureId;
			Corpse->ForceNetUpdate();
		}
	}
	Record->Death = Record->State.Money;
	Record->State.Money = 0;
	Record->State.Health = 0.f;
	Record->State.Life = EPrototypeLife::Dead;
	Record->State.BurialLevel = 0;
	Record->State.Items.Reset();
}

void ADigetherPrototypeGameMode::FinishRun()
{
	ADigetherPrototypeGameState* State = PrototypeState();
	if (!State || bRunFinalized || State->PublicState.Phase != EPrototypePhase::Expedition) return;
	bRunFinalized = true;
	int32 Survivors = 0;
	for (APlayerState* BasePlayer : State->PlayerArray)
	{
		if (ADigetherPrototypePlayerState* Player = Cast<ADigetherPrototypePlayerState>(BasePlayer))
		{
			if (Player->PrivateState.Life == EPrototypeLife::Escaped) ++Survivors;
			else KillPlayer(Player);
		}
	}
	for (auto& Entry : DisconnectedPlayers)
	{
		FDisconnectedPlayer& Record = Entry.Value;
		if (!RunParticipants.Contains(Entry.Key)) continue;
		if (Record.State.Life == EPrototypeLife::Escaped) ++Survivors;
		else if (Record.State.Life != EPrototypeLife::Dead) KillDisconnectedPlayer(Entry.Key);
	}
	if (Survivors == 0)
	{
		for (const auto& Pickup : ReturnedThisRun) if (Pickup.IsValid()) Pickup->Destroy();
	}
	ReturnedThisRun.Reset();
	const int32 Share = DigetherPrototypeRules::GeneralShare(CollectedValue, DepartureCount, Survivors);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		ADigetherPrototypePlayerState* Player = PrototypePlayer(PC);
		if (!Player) continue;
		const bool bSurvived = Player->PrivateState.Life == EPrototypeLife::Escaped;
		if (!bSurvived && Survivors > 0 && RecoveredBodies.Contains(Player))
			Player->PrivateState.Money = DigetherPrototypeRules::CorpseRefund(DeathLoss.FindRef(Player));
		const int32 Hidden = bSurvived ? HiddenPayout.FindRef(Player) : 0;
		Player->PrivateState.LastPayout = bSurvived ? static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(Share) + Hidden)) : 0;
		Player->PrivateState.Money = static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(Player->PrivateState.Money) + Player->PrivateState.LastPayout));
		Player->PrivateState.Life = EPrototypeLife::Active;
		Player->PrivateState.BurialLevel = 0;
		Player->PrivateState.Health = 100.f;
		Player->PrivateState.Message = FString::Printf(TEXT("시험 정산: %d원. 거점에서 납부하거나 다시 출발하세요."), Player->PrivateState.LastPayout);
		if (APawn* Pawn = PC->GetPawn())
		{
			if (!TryReturnPawn(Pawn, It.GetIndex()))
			{
				PendingReturns.Add(Pawn, It.GetIndex());
				Player->PrivateState.Message += TEXT(" 거점 이동 위치를 다시 찾고 있습니다.");
			}
		}
		UpdatePlayer(Player);
	}
	int32 OfflineIndex = 0;
	for (auto& Entry : DisconnectedPlayers)
	{
		FDisconnectedPlayer& Record = Entry.Value;
		if (!RunParticipants.Contains(Entry.Key)) continue;
		const bool bSurvived = Record.State.Life == EPrototypeLife::Escaped;
		if (!bSurvived && Survivors > 0 && Record.bBodyRecovered)
			Record.State.Money = DigetherPrototypeRules::CorpseRefund(Record.Death);
		Record.State.LastPayout = bSurvived ? static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(Share) + Record.Hidden)) : 0;
		Record.State.Money = static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(Record.State.Money) + Record.State.LastPayout));
		Record.State.Life = EPrototypeLife::Active;
		Record.State.BurialLevel = 0;
		Record.State.Health = 100.f;
		Record.State.Message = TEXT("연결이 끊긴 동안 탐사가 끝났습니다. 시험 정산을 확인하세요.");
		if (APawn* Pawn = Record.Pawn.Get())
		{
			const int32 Index = 4 + OfflineIndex++;
			if (!TryReturnPawn(Pawn, Index)) PendingReturns.Add(Pawn, Index);
		}
	}
	for (TActorIterator<ADigetherPrototypeCorpse> It(GetWorld()); It; ++It) It->Destroy();
	for (TActorIterator<ADigetherPrototypePickup> It(GetWorld()); It; ++It)
	{
		if (!It->ActorHasTag(TEXT("PrototypeReturnedEquipment"))) It->Destroy();
	}
	++State->PublicState.Day;
	State->PublicState.RemainingSeconds = 0.f;
	if (!PendingReturns.IsEmpty())
	{
		State->PublicState.Phase = EPrototypePhase::Returning;
		State->PublicState.Message = TEXT("시험 귀환 대기: 안전한 거점 위치를 찾고 있습니다.");
		State->ForceNetUpdate();
		return;
	}
	CompleteReturn();
}

bool ADigetherPrototypeGameMode::TryReturnPawn(APawn* Pawn, int32 Index) const
{
	if (!IsValid(Pawn)) return false;
	for (int32 Attempt = 0; Attempt < 12; ++Attempt)
	{
		const int32 Slot = (Index + Attempt) % 12;
		const FVector Destination(-300.f - 150.f * (Slot / 4), 140.f * (Slot % 4 - 1), 110.f);
		if (Pawn->TeleportTo(Destination, Pawn->GetActorRotation(), false, false)) return true;
	}
	return false;
}

void ADigetherPrototypeGameMode::CompleteReturn()
{
	ADigetherPrototypeGameState* State = PrototypeState();
	if (!State) return;
	State->PublicState.Phase = bQuotaFailedAtDeparture ? EPrototypePhase::GameOver : EPrototypePhase::Base;
	State->PublicState.Message = bQuotaFailedAtDeparture
		? TEXT("시험 게임 오버: 4일차 출발 시 목표 미달")
		: TEXT("시험 정산 완료. 거점에서 납부하거나 다시 출발하세요.");
	FString SaveMessage;
	if (!SaveFixture(SaveMessage)) State->PublicState.Message += TEXT(" (시험 자동 저장 실패)");
	State->ForceNetUpdate();
}

void ADigetherPrototypeGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ADigetherPrototypeGameState* State = PrototypeState();
	if (!HasAuthority() || !State) return;
	if (State->PublicState.Phase == EPrototypePhase::Returning)
	{
		for (auto It = PendingReturns.CreateIterator(); It; ++It)
		{
			if (TryReturnPawn(It.Key().Get(), It.Value())) It.RemoveCurrent();
		}
		if (PendingReturns.IsEmpty()) CompleteReturn();
		return;
	}
	if (State->PublicState.Phase == EPrototypePhase::Expedition)
	{
		UpdateElevator(DeltaSeconds);
		if (State->PublicState.Phase != EPrototypePhase::Expedition || bRunFinalized) return;
	}
	if (APlayerController* Depositor = DepositHolder.Get())
	{
		FString Message;
		ADigetherPrototypePlayerState* Player = PrototypePlayer(Depositor);
		if (State->PublicState.Phase != EPrototypePhase::Base || DepositPeriodGeneration != PeriodGeneration || !Player || !DepositStation.IsValid() ||
			!CanInteract(Depositor, DepositStation.Get(), Message) || Player->PrivateState.Money <= 0)
		{
			CancelInteraction(Depositor);
		}
		else
		{
			DepositElapsed += DeltaSeconds;
			int32 Ticks = 0;
			const float Interval = FMath::Max(0.1f, FixtureDepositInterval);
			while (DepositElapsed >= Interval && DepositHolder.Get() == Depositor && Ticks++ < 10)
			{
				DepositElapsed -= Interval;
				if (!RequestDeposit(Depositor, 1, Message)) { CancelInteraction(Depositor); break; }
			}
		}
	}
	for (auto It = RescueJobs.CreateIterator(); It; ++It)
	{
		FRescueJob& Job = It.Value();
		APlayerController* Rescuer = Job.Rescuer.Get();
		APlayerController* Target = Job.Target.Get();
		ADigetherPrototypePlayerState* TargetState = It.Key().Get();
		FString Ignored;
		if (!Rescuer || !Target || !TargetState || TargetState->PrivateState.Life != EPrototypeLife::Buried ||
			!Target->GetPawn() || !CanInteract(Rescuer, Target->GetPawn(), Ignored))
		{
			if (ADigetherPrototypePlayerState* RescuerState = PrototypePlayer(Rescuer))
			{
				RescuerState->PrivateState.HoldProgress = 0.f;
				RescuerState->PrivateState.Message = TEXT("구조가 중단되었습니다.");
				UpdatePlayer(RescuerState);
			}
			It.RemoveCurrent();
			continue;
		}
		Job.Elapsed += DeltaSeconds;
		if (ADigetherPrototypePlayerState* RescuerState = PrototypePlayer(Rescuer))
		{
			RescuerState->PrivateState.HoldProgress = FMath::Clamp(Job.Elapsed / 2.f, 0.f, 1.f);
			UpdatePlayer(RescuerState);
		}
		if (Job.Elapsed >= 2.f)
		{
			if (!TryMoveBuriedPawnToSafety(Target->GetPawn()))
			{
				TargetState->PrivateState.Message = TEXT("안전한 구조 위치가 없어 매몰 상태를 유지합니다.");
				UpdatePlayer(TargetState);
				if (ADigetherPrototypePlayerState* RescuerState = PrototypePlayer(Rescuer))
				{
					RescuerState->PrivateState.HoldProgress = 0.f;
					RescuerState->PrivateState.Message = TEXT("안전한 구조 위치를 찾지 못했습니다.");
					UpdatePlayer(RescuerState);
				}
				It.RemoveCurrent();
				continue;
			}
			TargetState->PrivateState.Life = EPrototypeLife::Active;
			TargetState->PrivateState.BurialLevel = 0;
			TargetState->PrivateState.Message = TEXT("시험 구조가 완료되었습니다.");
			UpdatePlayer(TargetState);
			if (ADigetherPrototypePlayerState* RescuerState = PrototypePlayer(Rescuer))
			{
				RescuerState->PrivateState.HoldProgress = 0.f;
				RescuerState->PrivateState.Message = TEXT("동료를 구조했습니다.");
				UpdatePlayer(RescuerState);
			}
			It.RemoveCurrent();
		}
	}
	if (APlayerController* Holder = HoldingPlayer.Get())
	{
		ADigetherPrototypePlayerState* Player = PrototypePlayer(Holder);
		FString HoldMessage;
		AActor* HoldTarget = bHoldElevator ? HoldingElevator.Get() : HoldingStation.Get();
		if (!Player || !Holder->GetPawn() || Player->PrivateState.Life != EPrototypeLife::Active ||
			HoldRunId != State->PublicState.RunId ||
			!HoldTarget || !CanInteract(Holder, HoldTarget, HoldMessage) ||
			FVector::DistSquared(Holder->GetPawn()->GetActorLocation(), HoldStartLocation) > FMath::Square(150.f) ||
			(bHoldElevator && (!HoldingElevator.IsValid() || GetBoardingWeight(HoldingElevator.Get()) > ElevatorCapacityKg)) ||
			(bHoldEmergency && !CanUseEmergency(Holder, HoldMessage)))
		{
			CancelInteraction(Holder);
		}
		else
		{
			HoldElapsed += DeltaSeconds;
			Player->PrivateState.HoldProgress = FMath::Clamp(HoldElapsed / HoldDuration, 0.f, 1.f);
			UpdatePlayer(Player);
			if (HoldElapsed >= HoldDuration)
			{
				const bool bElevator = bHoldElevator;
				const bool bEmergency = bHoldEmergency;
				AActor* Elevator = HoldingElevator.Get();
				HoldingPlayer.Reset();
				HoldingElevator.Reset();
				HoldingStation.Reset();
				Player->PrivateState.HoldProgress = 0.f;
				bCompletingHold = true;
				FString Message;
				if (bElevator) RequestElevator(Holder, Elevator, Message);
				else if (bEmergency) RequestEmergencyExit(Holder, Message);
				else RequestStartRun(Holder, Message);
				bCompletingHold = false;
				Player->PrivateState.Message = Message;
				UpdatePlayer(Player);
			}
		}
	}
	if (State->PublicState.Phase != EPrototypePhase::Expedition || bRunFinalized) return;
	TimeAccumulator += DeltaSeconds;
	if (TimeAccumulator < 1.f) return;
	const float Elapsed = TimeAccumulator;
	TimeAccumulator = 0.f;
	State->PublicState.RemainingSeconds = FMath::Max(0.f, State->PublicState.RemainingSeconds - Elapsed);
	if (State->PublicState.ElevatorState == EPrototypeElevatorState::Ground)
	{
		for (TActorIterator<ADigetherPrototypeStation> It(GetWorld()); It; ++It)
		{
			if (It->Kind != EPrototypeStationKind::Elevator) continue;
			const float Weight = GetBoardingWeight(*It);
			const float Capacity = FMath::Max(1.f, ElevatorCapacityKg);
			State->PublicState.ElevatorWeightStage = Weight > Capacity ? 3 : Weight >= Capacity * .75f ? 2 : Weight > 0.f ? 1 : 0;
			break;
		}
	}
	for (APlayerState* BasePlayer : State->PlayerArray)
	{
		ADigetherPrototypePlayerState* Player = Cast<ADigetherPrototypePlayerState>(BasePlayer);
		if (!Player || Player->PrivateState.Life != EPrototypeLife::Buried) continue;
		Player->PrivateState.Health -= 5.f * Elapsed;
		if (Player->PrivateState.Health <= 0.f) KillPlayer(Player);
		else UpdatePlayer(Player);
	}
	for (auto& Entry : DisconnectedPlayers)
	{
		FDisconnectedPlayer& Record = Entry.Value;
		if (!RunParticipants.Contains(Entry.Key) || Record.State.Life != EPrototypeLife::Buried) continue;
		Record.State.Health -= 5.f * Elapsed;
		if (Record.State.Health <= 0.f) KillDisconnectedPlayer(Entry.Key);
	}
	if (State->PublicState.RemainingSeconds <= 0.f)
	{
		if (APlayerController* Holder = HoldingPlayer.Get()) CancelInteraction(Holder);
		bTimeoutPending = true;
		if (State->PublicState.ElevatorState == EPrototypeElevatorState::Ascending ||
			State->PublicState.ElevatorState == EPrototypeElevatorState::FinalAscending) return;
		if (State->PublicState.ElevatorState == EPrototypeElevatorState::Top) { FinishRun(); return; }
		if (State->PublicState.ElevatorState == EPrototypeElevatorState::Descending)
		{
			bEmptyReturn = true;
			ElevatorElapsed = State->PublicState.ElevatorProgress * FMath::Max(0.1f, FixtureElevatorTravelSeconds);
			State->PublicState.ElevatorState = EPrototypeElevatorState::Ascending;
			State->PublicState.Message = TEXT("시간 종료: 빈 시험 승강기가 거점으로 복귀합니다.");
			State->ForceNetUpdate();
			return;
		}
		bool bStartedAutomatic = false;
		for (TActorIterator<ADigetherPrototypeStation> It(GetWorld()); It; ++It)
		{
			ADigetherPrototypeStation* Elevator = *It;
			if (Elevator->Kind != EPrototypeStationKind::Elevator || GetBoardingWeight(Elevator) > ElevatorCapacityKg) continue;
			FString Ignored;
			bCompletingHold = true;
			bStartedAutomatic = RequestElevator(nullptr, Elevator, Ignored);
			bCompletingHold = false;
			break;
		}
		if (!bStartedAutomatic) FinishRun();
	}
	else
	{
		if (State->PublicState.ElevatorState != EPrototypeElevatorState::Ground) { State->ForceNetUpdate(); return; }
		bool bAnyActive = false;
		for (APlayerState* BasePlayer : State->PlayerArray)
		{
			const ADigetherPrototypePlayerState* Player = Cast<ADigetherPrototypePlayerState>(BasePlayer);
			if (Player && (Player->PrivateState.Life == EPrototypeLife::Active || Player->PrivateState.Life == EPrototypeLife::Buried)) bAnyActive = true;
		}
		for (const auto& Entry : DisconnectedPlayers)
		{
			if (RunParticipants.Contains(Entry.Key) && (Entry.Value.State.Life == EPrototypeLife::Active || Entry.Value.State.Life == EPrototypeLife::Buried)) bAnyActive = true;
		}
		if (!bAnyActive) FinishRun();
		else State->ForceNetUpdate();
	}
}

void ADigetherPrototypeGameMode::CancelInteraction(APlayerController* Caller)
{
	if (!HasAuthority() || !Caller) return;
	if (DepositHolder.Get() == Caller)
	{
		DepositHolder.Reset();
		DepositStation.Reset();
		DepositElapsed = 0.f;
	}
	for (auto It = RescueJobs.CreateIterator(); It; ++It)
	{
		if (It.Value().Rescuer.Get() == Caller) It.RemoveCurrent();
	}
	if (HoldingPlayer.Get() != Caller)
	{
		if (ADigetherPrototypePlayerState* Player = PrototypePlayer(Caller))
		{
			Player->PrivateState.HoldProgress = 0.f;
			UpdatePlayer(Player);
		}
		return;
	}
	HoldingPlayer.Reset();
	HoldingElevator.Reset();
	HoldingStation.Reset();
	if (ADigetherPrototypePlayerState* Player = PrototypePlayer(Caller))
	{
		Player->PrivateState.HoldProgress = 0.f;
		Player->PrivateState.Message = TEXT("레버 작동이 취소되었습니다.");
		UpdatePlayer(Player);
	}
}

bool ADigetherPrototypeGameMode::RequestDeposit(APlayerController* Caller, int32 Amount, FString& OutMessage)
{
	ADigetherPrototypeGameState* State = PrototypeState();
	ADigetherPrototypePlayerState* Player = PrototypePlayer(Caller);
	if (!HasAuthority() || !State || State->PublicState.Phase != EPrototypePhase::Base || !Player || Amount <= 0 || Amount > Player->PrivateState.Money)
	{
		OutMessage = TEXT("납부할 수 없습니다. 단계와 잔고를 확인하세요.");
		return false;
	}
	const int32 Remaining = State->PublicState.QuotaTarget - State->PublicState.QuotaProgress;
	const int32 Paid = FMath::Min(Amount, Remaining);
	if (Paid <= 0) return false;
	Player->PrivateState.Money -= Paid;
	State->PublicState.QuotaProgress += Paid;
	if (State->PublicState.QuotaProgress >= State->PublicState.QuotaTarget)
	{
		if (APlayerController* Holder = HoldingPlayer.Get()) CancelInteraction(Holder);
		if (APlayerController* Depositor = DepositHolder.Get()) CancelInteraction(Depositor);
		State->PublicState.Day = 1;
		++PeriodGeneration;
		State->PublicState.QuotaProgress = 0;
		State->PublicState.QuotaTarget = static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(State->PublicState.QuotaTarget) + InitialQuota));
		State->PublicState.Message = TEXT("시험 목표 달성: 새 목표와 1일차가 시작됩니다.");
		FString SaveMessage;
		if (!SaveFixture(SaveMessage)) State->PublicState.Message += TEXT(" (시험 자동 저장 실패)");
	}
	Player->PrivateState.Message = FString::Printf(TEXT("시험 납부 %d원"), Paid);
	OutMessage = Player->PrivateState.Message;
	UpdatePlayer(Player);
	State->ForceNetUpdate();
	return true;
}

bool ADigetherPrototypeGameMode::RequestBeginDeposit(APlayerController* Caller, AActor* Station, FString& OutMessage)
{
	ADigetherPrototypeGameState* State = PrototypeState();
	ADigetherPrototypePlayerState* Player = PrototypePlayer(Caller);
	if (!HasAuthority() || !State || State->PublicState.Phase != EPrototypePhase::Base || !Player ||
		Player->PrivateState.Money <= 0 || !CanInteract(Caller, Station, OutMessage))
	{
		if (OutMessage.IsEmpty()) OutMessage = TEXT("시험 납부를 시작할 수 없습니다. 잔고를 확인하세요.");
		return false;
	}
	if (DepositHolder.IsValid() || HoldingPlayer.Get() == Caller)
	{
		OutMessage = TEXT("정산대를 다른 작업자가 사용 중입니다.");
		return false;
	}
	DepositHolder = Caller;
	DepositStation = Station;
	DepositElapsed = 0.f;
	DepositPeriodGeneration = PeriodGeneration;
	Player->PrivateState.HoldProgress = 0.001f;
	UpdatePlayer(Player);
	OutMessage = TEXT("시험 납부 중: E를 놓으면 중단됩니다. 인정률은 임시 100%입니다.");
	return true;
}

bool ADigetherPrototypeGameMode::RequestDamage(APlayerController* Caller, APlayerController* Target, float Amount, FString& OutMessage)
{
	ADigetherPrototypeGameState* State = PrototypeState();
	ADigetherPrototypePlayerState* Player = PrototypePlayer(Target);
	if (!HasAuthority() || !State || State->PublicState.Phase != EPrototypePhase::Expedition || !Player ||
		Player->PrivateState.Life == EPrototypeLife::Dead || Player->PrivateState.Life == EPrototypeLife::Escaped || Player->PrivateState.Life == EPrototypeLife::InTransit ||
		!FMath::IsFinite(Amount) || Amount <= 0.f || (Caller && !CanInteract(Caller, Target->GetPawn(), OutMessage))) return false;
	Player->PrivateState.Health -= Amount;
	if (Player->PrivateState.Health <= 0.f) KillPlayer(Player);
	else UpdatePlayer(Player);
	OutMessage = TEXT("시험 피해가 적용되었습니다.");
	return true;
}

bool ADigetherPrototypeGameMode::RequestWorldDamagePawn(APawn* Target, float Amount)
{
	if (!HasAuthority() || !Target || !FMath::IsFinite(Amount) || Amount <= 0.f) return false;
	if (APlayerController* Controller = Cast<APlayerController>(Target->GetController()))
	{
		FString Ignored;
		return RequestDamage(nullptr, Controller, Amount, Ignored);
	}
	const ADigetherPrototypeGameState* State = PrototypeState();
	if (!State || State->PublicState.Phase != EPrototypePhase::Expedition) return false;
	for (auto& Entry : DisconnectedPlayers)
	{
		FDisconnectedPlayer& Record = Entry.Value;
		if (Record.Pawn.Get() != Target || (Record.State.Life != EPrototypeLife::Active && Record.State.Life != EPrototypeLife::Buried)) continue;
		Record.State.Health -= Amount;
		if (Record.State.Health <= 0.f) KillDisconnectedPlayer(Entry.Key);
		return true;
	}
	return false;
}

bool ADigetherPrototypeGameMode::IsHazardTarget(APawn* Pawn) const
{
	if (!HasAuthority() || !IsValid(Pawn)) return false;
	if (APlayerController* Controller = Cast<APlayerController>(Pawn->GetController()))
	{
		const ADigetherPrototypePlayerState* Player = PrototypePlayer(Controller);
		return Player && (Player->PrivateState.Life == EPrototypeLife::Active || Player->PrivateState.Life == EPrototypeLife::Buried);
	}
	for (const auto& Entry : DisconnectedPlayers)
	{
		if (Entry.Value.Pawn.Get() == Pawn) return Entry.Value.State.Life == EPrototypeLife::Active || Entry.Value.State.Life == EPrototypeLife::Buried;
	}
	return false;
}

bool ADigetherPrototypeGameMode::RequestWorldBurialPawn(APawn* Pawn, int32 Level)
{
	const ADigetherPrototypeGameState* State = PrototypeState();
	if (!HasAuthority() || !State || State->PublicState.Phase != EPrototypePhase::Expedition || Level < 1 || Level > 3 || !IsValid(Pawn)) return false;
	if (APlayerController* Controller = Cast<APlayerController>(Pawn->GetController()))
	{
		FString Ignored;
		return RequestBurial(Controller, Level, Ignored);
	}
	for (auto& Entry : DisconnectedPlayers)
	{
		FDisconnectedPlayer& Record = Entry.Value;
		if (Record.Pawn.Get() != Pawn || Record.State.Life != EPrototypeLife::Active) continue;
		if (DepartureCount == 1 && Level == 3) KillDisconnectedPlayer(Entry.Key);
		else { Record.State.Life = EPrototypeLife::Buried; Record.State.BurialLevel = Level; }
		return true;
	}
	return false;
}

bool ADigetherPrototypeGameMode::RequestBurial(APlayerController* Target, int32 Level, FString& OutMessage)
{
	ADigetherPrototypeGameState* State = PrototypeState();
	ADigetherPrototypePlayerState* Player = PrototypePlayer(Target);
	if (!HasAuthority() || !State || State->PublicState.Phase != EPrototypePhase::Expedition || !Player ||
		Player->PrivateState.Life != EPrototypeLife::Active || Level < 1 || Level > 3) return false;
	if (DepartureCount == 1 && Level == 3) KillPlayer(Player);
	else
	{
		Player->PrivateState.Life = EPrototypeLife::Buried;
		Player->PrivateState.BurialLevel = Level;
		Player->PrivateState.Message = TEXT("시험 매몰: 동료의 구조가 필요합니다.");
		UpdatePlayer(Player);
	}
	OutMessage = TEXT("시험 매몰이 발생했습니다.");
	return true;
}

bool ADigetherPrototypeGameMode::RequestRescue(APlayerController* Caller, APlayerController* Target, FString& OutMessage)
{
	ADigetherPrototypePlayerState* Player = PrototypePlayer(Target);
	if (!Player || Caller == Target || Player->PrivateState.Life != EPrototypeLife::Buried || !Target->GetPawn() || !CanInteract(Caller, Target->GetPawn(), OutMessage)) return false;
	if (RescueJobs.Contains(Player) || HoldingPlayer.Get() == Caller)
	{
		OutMessage = TEXT("구조 대상이나 플레이어가 이미 작업 중입니다.");
		return false;
	}
	for (const auto& Entry : RescueJobs)
	{
		if (Entry.Value.Rescuer.Get() == Caller)
		{
			OutMessage = TEXT("이미 다른 사람을 구조 중입니다.");
			return false;
		}
	}
	FRescueJob& Job = RescueJobs.Add(Player);
	Job.Rescuer = Caller;
	Job.Target = Target;
	OutMessage = TEXT("구조 입력을 계속 누르세요.");
	return true;
}

bool ADigetherPrototypeGameMode::CanUseEmergency(APlayerController* Caller, FString& OutMessage) const
{
	const ADigetherPrototypeGameState* State = PrototypeState();
	const ADigetherPrototypePlayerState* Player = PrototypePlayer(Caller);
	if (!HasAuthority() || !State || State->PublicState.Phase != EPrototypePhase::Expedition || !Player || Player->PrivateState.Life != EPrototypeLife::Active) return false;
	for (TActorIterator<ADigetherPrototypeCorpse> It(GetWorld()); It; ++It)
	{
		if (It->Carrier == Caller->GetPawn())
		{
			OutMessage = TEXT("시체를 운반 중에는 긴급 탈출할 수 없습니다.");
			return false;
		}
	}
	bool bHasDevice = false;
	float WeightKg = 0.f;
	for (const FPrototypeItem& Item : Player->PrivateState.Items)
	{
		if (!Item.bEquipment)
		{
			OutMessage = TEXT("회수품을 든 채로 긴급 탈출할 수 없습니다.");
			return false;
		}
		bHasDevice |= Item.bEmergencyDevice;
		WeightKg += Item.WeightKg;
	}
	if (!bHasDevice) { OutMessage = TEXT("시험 긴급 탈출 장치가 필요합니다."); return false; }
	if (WeightKg > FixtureEmergencyWeightKg) { OutMessage = TEXT("시험 긴급 탈출 중량 한도를 초과했습니다."); return false; }
	return true;
}

bool ADigetherPrototypeGameMode::RequestBeginEmergencyExit(APlayerController* Caller, AActor* Station, FString& OutMessage)
{
	if (!CanInteract(Caller, Station, OutMessage) || !CanUseEmergency(Caller, OutMessage)) return false;
	if (HoldingPlayer.IsValid() || DepositHolder.Get() == Caller)
	{
		OutMessage = TEXT("다른 시험 상호작용이 진행 중입니다.");
		return false;
	}
	HoldingPlayer = Caller;
	HoldingStation = Station;
	HoldingElevator.Reset();
	HoldStartLocation = Caller->GetPawn()->GetActorLocation();
	HoldElapsed = 0.f;
	HoldDuration = 2.f;
	HoldRunId = PrototypeState()->PublicState.RunId;
	bHoldElevator = false;
	bHoldEmergency = true;
	OutMessage = TEXT("시험 긴급 장치를 계속 누르세요. 장치는 1회 사용합니다.");
	return true;
}

bool ADigetherPrototypeGameMode::RequestEmergencyExit(APlayerController* Caller, FString& OutMessage)
{
	if (!bCompletingHold || !CanUseEmergency(Caller, OutMessage)) return false;
	ADigetherPrototypeGameState* State = PrototypeState();
	ADigetherPrototypePlayerState* Player = PrototypePlayer(Caller);
	const int32 DeviceIndex = Player->PrivateState.Items.IndexOfByPredicate([](const FPrototypeItem& Item) { return Item.bEmergencyDevice; });
	if (DeviceIndex == INDEX_NONE) return false;
	Player->PrivateState.Items.RemoveAt(DeviceIndex);
	Player->PrivateState.Life = EPrototypeLife::Escaped;
	Player->PrivateState.Message = TEXT("시험 긴급 탈출 완료");
	UpdatePlayer(Player);
	OutMessage = Player->PrivateState.Message;
	bool bAnyoneActive = false;
	for (APlayerState* BasePlayer : State->PlayerArray)
	{
		const ADigetherPrototypePlayerState* Other = Cast<ADigetherPrototypePlayerState>(BasePlayer);
		if (Other && (Other->PrivateState.Life == EPrototypeLife::Active || Other->PrivateState.Life == EPrototypeLife::Buried || Other->PrivateState.Life == EPrototypeLife::InTransit)) bAnyoneActive = true;
	}
	for (const auto& Entry : DisconnectedPlayers)
	{
		if (RunParticipants.Contains(Entry.Key) && (Entry.Value.State.Life == EPrototypeLife::Active || Entry.Value.State.Life == EPrototypeLife::Buried || Entry.Value.State.Life == EPrototypeLife::InTransit)) bAnyoneActive = true;
	}
	if (!bAnyoneActive) FinishRun();
	return true;
}

bool ADigetherPrototypeGameMode::RequestPurchaseEmergency(APlayerController* Caller, FString& OutMessage)
{
	if (!RequestPurchase(Caller, OutMessage)) return false;
	ADigetherPrototypePlayerState* Player = PrototypePlayer(Caller);
	if (!Player || Player->PrivateState.Items.IsEmpty()) return false;
	Player->PrivateState.Items.Last().bEmergencyDevice = true;
	Player->PrivateState.Message = TEXT("시험 긴급 탈출 장치를 구매했습니다. (10원, 1회)");
	OutMessage = Player->PrivateState.Message;
	UpdatePlayer(Player);
	return true;
}

bool ADigetherPrototypeGameMode::RequestPurchase(APlayerController* Caller, FString& OutMessage)
{
	ADigetherPrototypeGameState* State = PrototypeState();
	ADigetherPrototypePlayerState* Player = PrototypePlayer(Caller);
	if (!HasAuthority() || !State || State->PublicState.Phase != EPrototypePhase::Base || !Player || Player->PrivateState.Money < 10)
	{
		OutMessage = TEXT("시험 장비 구매에는 10원이 필요합니다.");
		return false;
	}
	int32 Count = 0;
	for (const FPrototypeItem& Item : Player->PrivateState.Items) Count += !Item.bHidden;
	if (Count >= 4)
	{
		OutMessage = TEXT("일반 슬롯이 가득 찼습니다.");
		return false;
	}
	FPrototypeItem Item;
	Item.Id = FGuid::NewGuid();
	Item.bEquipment = true;
	Item.WeightKg = 1.f;
	Player->PrivateState.Money -= 10;
	Player->PrivateState.Items.Add(Item);
	OutMessage = TEXT("시험 장비를 구매했습니다. (10원)");
	Player->PrivateState.Message = OutMessage;
	UpdatePlayer(Player);
	return true;
}

bool ADigetherPrototypeGameMode::RequestUseWood(APlayerController* Caller, FString& OutMessage)
{
	ADigetherPrototypeGameState* State = PrototypeState();
	ADigetherPrototypePlayerState* Player = PrototypePlayer(Caller);
	if (!HasAuthority() || !State || State->PublicState.Phase != EPrototypePhase::Expedition || !Player ||
		Player->PrivateState.Life != EPrototypeLife::Active || Player->PrivateState.WoodCount <= 0)
	{
		OutMessage = TEXT("시험용 목재가 부족하거나 보강할 수 없는 상태입니다.");
		return false;
	}
	--Player->PrivateState.WoodCount;
	UpdatePlayer(Player);
	return true;
}

bool ADigetherPrototypeGameMode::RequestRestartGame(APlayerController* Caller, FString& OutMessage)
{
	ADigetherPrototypeGameState* State = PrototypeState();
	if (!HasAuthority() || !State || State->PublicState.Phase != EPrototypePhase::GameOver || !PrototypePlayer(Caller))
	{
		OutMessage = TEXT("지금은 새 시험 게임을 시작할 수 없습니다.");
		return false;
	}
	FString NewGameSlot;
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		const FString Candidate = TEXT("DIGETHER_Prototype_Game_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (!UGameplayStatics::DoesSaveGameExist(Candidate, 0)) { NewGameSlot = Candidate; break; }
	}
	if (NewGameSlot.IsEmpty())
	{
		OutMessage = TEXT("새 시험 게임의 독립 저장 슬롯을 만들 수 없습니다.");
		return false;
	}
	if (APlayerController* Holder = HoldingPlayer.Get()) CancelInteraction(Holder);
	if (APlayerController* Depositor = DepositHolder.Get()) CancelInteraction(Depositor);
	RescueJobs.Reset();
	CollectedValue = 0;
	DepartureCount = 0;
	HiddenPayout.Reset();
	DeathLoss.Reset();
	RecoveredBodies.Reset();
	State->PublicState = FPrototypePublicState();
	ActiveGameSaveSlot = NewGameSlot;
	++PeriodGeneration;
	for (TActorIterator<ADigetherPrototypePickup> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("PrototypeReturnedEquipment"))) It->Destroy();
	}
	State->PublicState.QuotaTarget = FMath::Max(1, InitialQuota);
	State->PublicState.Message = FString::Printf(TEXT("새 시험 게임: 거점에서 출발할 수 있습니다. 저장 슬롯: %s"), *ActiveGameSaveSlot);
	for (APlayerState* BasePlayer : State->PlayerArray)
	{
		if (ADigetherPrototypePlayerState* Player = Cast<ADigetherPrototypePlayerState>(BasePlayer))
		{
			Player->PrivateState = FPrototypePrivateState();
			GivePrototypeStarterTool(Player->PrivateState);
			UpdatePlayer(Player);
		}
	}
	for (auto& Entry : DisconnectedPlayers)
	{
		Entry.Value.State = FPrototypePrivateState();
		GivePrototypeStarterTool(Entry.Value.State);
	}
	State->ForceNetUpdate();
	OutMessage = FString::Printf(TEXT("새 시험 게임 생성. 저장 슬롯: %s (정식 크레딧 미구현)"), *ActiveGameSaveSlot);
	return true;
}

bool ADigetherPrototypeGameMode::FindSafeDropLocation(APawn* Pawn, const AActor* IgnoredActor, float Radius, FVector& OutLocation) const
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !IsValid(Pawn) || !FMath::IsFinite(Radius) || Radius <= 0.f) { return false; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PrototypeDropSafety), false, Pawn);
	if (IgnoredActor) { Params.AddIgnoredActor(IgnoredActor); }
	for (TActorIterator<ADigetherPrototypeCorpse> It(World); It; ++It)
		if (It->Carrier == Pawn) { Params.AddIgnoredActor(*It); }
	const FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
	const FVector Start = Pawn->GetActorLocation();
	const FVector Desired = Start + Pawn->GetActorForwardVector() * 100.f;
	FHitResult Hit;
	// The sphere encloses the whole fixture, including the carried corpse's rotation.
	if (World->SweepSingleByChannel(Hit, Start, Desired, FQuat::Identity, ECC_Visibility, Shape, Params) ||
		World->SweepSingleByChannel(Hit, Start, Desired, FQuat::Identity, ECC_Pawn, Shape, Params)) { return false; }
	// The elevator floor blocks Pawn but intentionally ignores Visibility.
	if (!World->SweepSingleByChannel(Hit, Desired, Desired - FVector(0, 0, 200.f),
		FQuat::Identity, ECC_Pawn, Shape, Params) || Hit.bStartPenetrating || Hit.ImpactNormal.Z < .65f) { return false; }
	const FVector Candidate = Hit.Location + FVector(0, 0, 2.f);
	if (World->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, ECC_Visibility, Shape, Params) ||
		World->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, ECC_Pawn, Shape, Params)) { return false; }
	// Sweeps detect surface intersections; the center also catches a sphere
	// wholly enclosed by soil. Sampling near the surface can falsely reject air
	// because IsSolidAtWorldPoint uses the nearest 50 cm grid sample.
	for (TActorIterator<ADigetherExcavationTerrain> It(World); It; ++It)
		if (It->IsSolidAtWorldPoint(Candidate)) { return false; }
	FVector EyeLocation;
	FRotator EyeRotation;
	Pawn->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	if (World->LineTraceSingleByChannel(Hit, EyeLocation, Candidate, ECC_Visibility, Params)) { return false; }
	OutLocation = Candidate;
	return true;
}

bool ADigetherPrototypeGameMode::RequestDrop(APlayerController* Caller, int32 ItemIndex, FString& OutMessage)
{
	ADigetherPrototypeGameState* State = PrototypeState();
	ADigetherPrototypePlayerState* Player = PrototypePlayer(Caller);
	if (!HasAuthority() || !State || State->PublicState.Phase != EPrototypePhase::Expedition || !Player || !Caller->GetPawn() ||
		Player->PrivateState.Life != EPrototypeLife::Active || !Player->PrivateState.Items.IsValidIndex(ItemIndex))
	{
		OutMessage = TEXT("내려놓을 물품이 없습니다.");
		return false;
	}
	FVector Location;
	// The prototype pickup is a 35 cm cube; its enclosing sphere has radius 30.4 cm.
	if (!FindSafeDropLocation(Caller->GetPawn(), nullptr, 31.f, Location))
	{
		OutMessage = TEXT("안전하게 내려놓을 공간이 없습니다.");
		return false;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADigetherPrototypePickup* Pickup = GetWorld()->SpawnActor<ADigetherPrototypePickup>(Location, FRotator::ZeroRotator, Params);
	if (!Pickup)
	{
		OutMessage = TEXT("물품을 내려놓지 못했습니다.");
		return false;
	}
	Pickup->StoredItem = Player->PrivateState.Items[ItemIndex];
	Player->PrivateState.Items.RemoveAt(ItemIndex);
	UpdatePlayer(Player);
	OutMessage = TEXT("시험 물품을 내려놓았습니다.");
	return true;
}


bool ADigetherPrototypeGameMode::TryMoveBuriedPawnToSafety(APawn* Pawn) const
{
	ACharacter* Character = Cast<ACharacter>(Pawn);
	UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	UWorld* World = GetWorld();
	if (!World || !Capsule || Capsule->GetCollisionEnabled() == ECollisionEnabled::NoCollision) { return false; }
	const float Radius = Capsule->GetScaledCapsuleRadius(), HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PrototypeBurialSafety), false, Pawn);
	const FName Profile = Capsule->GetCollisionProfileName();
	const auto TryCandidate = [&](const FVector& Candidate)
	{
		// Also test the sampled soil interior; a triangle surface alone cannot report
		// an overlap for a capsule completely enclosed by solid volume.
		for (TActorIterator<ADigetherExcavationTerrain> It(World); It; ++It)
		{
			// Only horizontal floor patches have a world-up local Z axis. A rescue
			// must not leave the player at the bottom of the original floor volume.
			const FTransform& TerrainTransform = It->GetActorTransform();
			if (FVector::DotProduct(TerrainTransform.GetUnitAxis(EAxis::Z), FVector::UpVector) > .99f)
			{
				const FVector LocalFeet = TerrainTransform.InverseTransformPosition(Candidate - FVector(0, 0, HalfHeight));
				const float HalfWidth = DigetherExcavation::CellsXY * DigetherExcavation::Spacing * .5f;
				if (FMath::Abs(LocalFeet.X) <= HalfWidth && FMath::Abs(LocalFeet.Y) <= HalfWidth &&
					LocalFeet.Z < -1.f) { return false; }
			}
			for (float Z : { -HalfHeight + Radius, 0.f, HalfHeight - Radius })
				for (const FVector& Offset : { FVector::ZeroVector, FVector(Radius, 0, 0), FVector(-Radius, 0, 0), FVector(0, Radius, 0), FVector(0, -Radius, 0) })
					if (It->IsSolidAtWorldPoint(Candidate + Offset + FVector(0, 0, Z))) { return false; }
		}
		if (World->OverlapBlockingTestByProfile(Candidate, FQuat::Identity, Profile, Shape, Params)) { return false; }
		FHitResult SweepHit;
		if (World->SweepSingleByProfile(SweepHit, Candidate + FVector(0, 0, 5.f), Candidate,
			FQuat::Identity, Profile, Shape, Params)) { return false; }
		return Pawn->TeleportTo(Candidate, Pawn->GetActorRotation(), false, false);
	};
	const FVector Origin = Pawn->GetActorLocation();
	if (TryCandidate(Origin)) { return true; }
	static const FVector2D Offsets[] = { {100,0}, {-100,0}, {0,100}, {0,-100}, {200,0}, {-200,0}, {0,200}, {0,-200},
		{400,0}, {-400,0}, {0,400}, {0,-400}, {600,0}, {-600,0}, {0,600}, {0,-600} };
	for (const FVector2D& Offset : Offsets)
	{
		const FVector Start = Origin + FVector(Offset.X, Offset.Y, 600.f);
		FHitResult FloorHit;
		if (!World->LineTraceSingleByChannel(FloorHit, Start, Start - FVector(0, 0, 1200.f), ECC_Visibility, Params) || FloorHit.ImpactNormal.Z < .65f) { continue; }
		if (TryCandidate(FloorHit.ImpactPoint + FVector(0, 0, HalfHeight + 4.f))) { return true; }
	}
	return false;
}

bool ADigetherPrototypeGameMode::RequestSelfEscape(APlayerController* Caller, FString& OutMessage)
{
	ADigetherPrototypeGameState* State = PrototypeState();
	ADigetherPrototypePlayerState* Player = PrototypePlayer(Caller);
	if (!HasAuthority() || !State || State->PublicState.Phase != EPrototypePhase::Expedition || !Player ||
		Player->PrivateState.Life != EPrototypeLife::Buried || Player->PrivateState.BurialLevel > 2)
	{
		OutMessage = TEXT("스스로 빠져나올 수 없는 상태입니다.");
		return false;
	}
	if (!Caller->GetPawn() || !TryMoveBuriedPawnToSafety(Caller->GetPawn()))
	{
		OutMessage = TEXT("안전한 탈출 위치가 없어 매몰 상태를 유지합니다.");
		Player->PrivateState.Message = OutMessage;
		UpdatePlayer(Player);
		return false;
	}
	Player->PrivateState.Life = EPrototypeLife::Active;
	Player->PrivateState.BurialLevel = 0;
	Player->PrivateState.Message = TEXT("시험 매몰에서 빠져나왔습니다.");
	UpdatePlayer(Player);
	OutMessage = Player->PrivateState.Message;
	return true;
}

FString ADigetherPrototypeGameMode::FixtureSlotName() const
{
	if (!ActiveGameSaveSlot.IsEmpty()) return ActiveGameSaveSlot;
	FString Suffix;
	if (FParse::Value(FCommandLine::Get(), TEXT("PrototypeSaveSlot="), Suffix) && !Suffix.IsEmpty() && Suffix.Len() <= 48)
	{
		bool bValid = true;
		for (TCHAR Character : Suffix) bValid &= FChar::IsAlnum(Character) || Character == TEXT('_') || Character == TEXT('-');
		if (bValid) return TEXT("DIGETHER_Prototype_") + Suffix;
	}
	return TEXT("DIGETHER_Prototype_Fixture");
}

bool ADigetherPrototypeGameMode::SaveFixture(FString& OutMessage)
{
	ADigetherPrototypeGameState* State = PrototypeState();
	if (!HasAuthority() || !State || State->PublicState.Phase == EPrototypePhase::Expedition)
	{
		OutMessage = TEXT("시험 저장은 완료된 거점 상태에서만 가능합니다.");
		return false;
	}
	UDigetherPrototypeSaveGame* Save = Cast<UDigetherPrototypeSaveGame>(UGameplayStatics::CreateSaveGameObject(UDigetherPrototypeSaveGame::StaticClass()));
	if (!Save) return false;
	Save->PublicState = State->PublicState;
	for (APlayerState* BasePlayer : State->PlayerArray)
	{
		ADigetherPrototypePlayerState* Player = Cast<ADigetherPrototypePlayerState>(BasePlayer);
		if (!Player) continue;
		if (Player->FixtureId.IsEmpty())
		{
			OutMessage = TEXT("안정된 참가자 ID가 없어 시험 저장을 건너뛰었습니다.");
			return false;
		}
		Save->Players.Add(Player->FixtureId, Player->PrivateState);
	}
	for (const auto& Entry : DisconnectedPlayers) Save->Players.Add(Entry.Key, Entry.Value.State);
	for (TActorIterator<ADigetherPrototypePickup> It(GetWorld()); It; ++It)
	{
		if (!It->ActorHasTag(TEXT("PrototypeReturnedEquipment"))) continue;
		FPrototypeSavedWorldItem& SavedItem = Save->ReturnedEquipment.AddDefaulted_GetRef();
		SavedItem.Item = It->StoredItem;
		SavedItem.Transform = It->GetActorTransform();
	}
	if (!UGameplayStatics::SaveGameToSlot(Save, FixtureSlotName(), 0))
	{
		OutMessage = TEXT("시험 저장 쓰기에 실패했습니다.");
		return false;
	}
	OutMessage = FString::Printf(TEXT("시험 슬롯 %s에 저장했습니다."), *FixtureSlotName());
	return true;
}

bool ADigetherPrototypeGameMode::RequestSave(APlayerController* Caller, FString& OutMessage)
{
	if (!Caller || !Caller->IsLocalController())
	{
		OutMessage = TEXT("시험 저장은 호스트만 할 수 있습니다.");
		return false;
	}
	return SaveFixture(OutMessage);
}

bool ADigetherPrototypeGameMode::RequestLoad(APlayerController* Caller, FString& OutMessage)
{
	ADigetherPrototypeGameState* State = PrototypeState();
	if (!HasAuthority() || !Caller || !Caller->IsLocalController() || !State || State->PublicState.Phase != EPrototypePhase::Base)
	{
		OutMessage = TEXT("시험 불러오기는 호스트의 거점에서만 가능합니다.");
		return false;
	}
	const UDigetherPrototypeSaveGame* Save = Cast<UDigetherPrototypeSaveGame>(UGameplayStatics::LoadGameFromSlot(FixtureSlotName(), 0));
	if (!Save || Save->Version != 2)
	{
		OutMessage = TEXT("시험 저장이 없거나 버전이 맞지 않습니다. 기존 저장은 유지됩니다.");
		return false;
	}
	for (APlayerState* BasePlayer : State->PlayerArray)
	{
		ADigetherPrototypePlayerState* Player = Cast<ADigetherPrototypePlayerState>(BasePlayer);
		if (!Player || Player->FixtureId.IsEmpty() || !Save->Players.Contains(Player->FixtureId))
		{
			OutMessage = TEXT("현재 참가자와 시험 저장의 ID가 일치하지 않습니다.");
			return false;
		}
	}
	TArray<TWeakObjectPtr<ADigetherPrototypePickup>> ExistingEquipment;
	for (TActorIterator<ADigetherPrototypePickup> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("PrototypeReturnedEquipment"))) ExistingEquipment.Add(*It);
	}
	TArray<TWeakObjectPtr<ADigetherPrototypePickup>> SpawnedEquipment;
	for (const FPrototypeSavedWorldItem& SavedItem : Save->ReturnedEquipment)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADigetherPrototypePickup* Pickup = GetWorld()->SpawnActor<ADigetherPrototypePickup>(ADigetherPrototypePickup::StaticClass(), SavedItem.Transform, Params);
		if (!Pickup)
		{
			for (const auto& Spawned : SpawnedEquipment) if (Spawned.IsValid()) Spawned->Destroy();
			OutMessage = TEXT("시험 저장 장비를 복원하지 못해 기존 상태를 유지했습니다.");
			return false;
		}
		Pickup->StoredItem = SavedItem.Item;
		Pickup->Tags.AddUnique(TEXT("PrototypeReturnedEquipment"));
		SpawnedEquipment.Add(Pickup);
	}
	for (const auto& Existing : ExistingEquipment) if (Existing.IsValid()) Existing->Destroy();
	if (APlayerController* Holder = HoldingPlayer.Get()) CancelInteraction(Holder);
	if (APlayerController* Depositor = DepositHolder.Get()) CancelInteraction(Depositor);
	RescueJobs.Reset();
	State->PublicState = Save->PublicState;
	++PeriodGeneration;
	TMap<FString, FDisconnectedPlayer> RestoredDisconnected;
	for (const auto& Entry : Save->Players)
	{
		if (ConnectedPlayers.Contains(Entry.Key) && ConnectedPlayers[Entry.Key].IsValid()) continue;
		FDisconnectedPlayer& Record = RestoredDisconnected.Add(Entry.Key);
		if (const FDisconnectedPlayer* Existing = DisconnectedPlayers.Find(Entry.Key)) Record.Pawn = Existing->Pawn;
		Record.State = Entry.Value;
	}
	DisconnectedPlayers = MoveTemp(RestoredDisconnected);
	for (APlayerState* BasePlayer : State->PlayerArray)
	{
		ADigetherPrototypePlayerState* Player = CastChecked<ADigetherPrototypePlayerState>(BasePlayer);
		Player->PrivateState = Save->Players.FindChecked(Player->FixtureId);
		UpdatePlayer(Player);
	}
	State->ForceNetUpdate();
	OutMessage = FString::Printf(TEXT("시험 슬롯 %s을 불러왔습니다."), *FixtureSlotName());
	return true;
}
