#include "DigetherPrototypeWorld.h"
#include "DigetherExcavationTerrain.h"
#include "DigetherPrototypeMineScene.h"
#include "DigetherPrototypePortal.h"

#include "Prototype/DigetherPrototypeGameMode.h"
#include "Prototype/DigetherPrototypeGameState.h"
#include "Prototype/DigetherPrototypePlayerState.h"
#include "Prototype/DigetherPrototypeCorpse.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
// TextRender의 앞면(+X)이 각 로컬 카메라를 향하도록 해 뒤집힌 뒷면을 보지 않게 한다.
void FaceLocalCamera(UTextRenderComponent* Text, UWorld* World)
{
	if (!Text || !World || World->GetNetMode() == NM_DedicatedServer || !GEngine) { return; }
	APlayerController* LocalController = GEngine->GetFirstLocalPlayerController(World);
	if (!LocalController) { return; }
	FVector CameraLocation; FRotator CameraRotation;
	LocalController->GetPlayerViewPoint(CameraLocation, CameraRotation);
	FVector Direction = CameraLocation - Text->GetComponentLocation();
	Direction.Z = 0.f;
	if (!Direction.IsNearlyZero()) { Text->SetWorldRotation(FRotator(0.f, Direction.Rotation().Yaw, 0.f)); }
}

void SetCube(UStaticMeshComponent* Mesh)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) { Mesh->SetStaticMesh(Cube.Object); }
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

bool ValidateInteraction(AActor* Target, APlayerController* Caller, const FVector& AimPoint, FString& OutMessage)
{
	UWorld* World = Target ? Target->GetWorld() : nullptr;
	ADigetherPrototypeGameMode* Mode = World ? World->GetAuthGameMode<ADigetherPrototypeGameMode>() : nullptr;
	if (!Target || !Target->HasAuthority() || !Mode || !Mode->CanInteract(Caller, Target, OutMessage)) { return false; }
	APawn* Pawn = Caller ? Caller->GetPawn() : nullptr;
	if (!Pawn) { OutMessage = TEXT("캐릭터를 찾을 수 없습니다."); return false; }
	FVector Eye; FRotator Rotation;
	Pawn->GetActorEyesViewPoint(Eye, Rotation);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PrototypeInteraction), false, Pawn);
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Eye, AimPoint, ECC_Visibility, Params) && Hit.GetActor() != Target)
	{
		OutMessage = TEXT("대상이 가려져 있습니다.");
		return false;
	}
	return true;
}

bool IsExpedition(UWorld* World)
{
	const ADigetherPrototypeGameState* State = World ? World->GetGameState<ADigetherPrototypeGameState>() : nullptr;
	return State && State->PublicState.Phase == EPrototypePhase::Expedition;
}
}

ADigetherPrototypeStation::ADigetherPrototypeStation()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	SetCube(Mesh);
	Mesh->SetRelativeScale3D(FVector(.6f, .6f, 1.f));
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Mesh);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(24.f);
}

void ADigetherPrototypeStation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FaceLocalCamera(Label, GetWorld());
}

void ADigetherPrototypeStation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	OnRep_Kind();
}

void ADigetherPrototypeStation::OnRep_Kind()
{
	const TCHAR* Text = TEXT("INFO");
	switch (Kind)
	{
	case EPrototypeStationKind::Start: Text = TEXT("START"); break;
	case EPrototypeStationKind::Elevator: Text = TEXT("ELEVATOR"); break;
	case EPrototypeStationKind::Deposit: Text = TEXT("DEPOSIT"); break;
	case EPrototypeStationKind::EmergencyExit: Text = TEXT("EXIT"); break;
	case EPrototypeStationKind::Shop: Text = TEXT("SHOP"); break;
	default: break;
	}
	Label->SetText(FText::FromString(Text));
}

void ADigetherPrototypeStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADigetherPrototypeStation, Kind);
	DOREPLIFETIME(ADigetherPrototypeStation, ElevatorCab);
}

ADigetherPrototypeElevatorCab::ADigetherPrototypeElevatorCab()
{
	bReplicates = true;
	SetReplicateMovement(true);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Floor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Floor"));
	Floor->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Disc(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Disc.Succeeded()) { Floor->SetStaticMesh(Disc.Object); }
	Floor->SetRelativeLocation(FVector(0.f, 0.f, 5.f));
	Floor->SetRelativeScale3D(FVector(7.f, 7.f, .1f));
	Floor->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Floor->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	auto CageBar = [this](const FVector& Location, const FVector& Size)
	{
		UStaticMeshComponent* Bar = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("CageBar%d"), CageParts.Num()));
		Bar->SetupAttachment(RootComponent);
		SetCube(Bar);
		Bar->SetRelativeLocation(Location);
		Bar->SetRelativeScale3D(Size / 100.f);
		// Decorative cage follows the physical cab; boarding remains the tested
		// floor/volume rule and cannot be obstructed by a new invisible gate.
		Bar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CageParts.Add(Bar);
	};
	for (float Side : {-1.f, 1.f})
	{
		for (float X : {-280.f, -140.f, 0.f, 140.f, 280.f}) CageBar(FVector(X, Side * 280.f, 130.f), FVector(8.f, 8.f, 240.f));
		for (float Z : {20.f, 250.f}) CageBar(FVector(0.f, Side * 280.f, Z), FVector(580.f, 15.f, 15.f));
		CageBar(FVector(Side * 280.f, 0.f, 250.f), FVector(15.f, 580.f, 15.f));
	}
	for (float Y : {-280.f, -140.f, 0.f, 140.f, 280.f}) CageBar(FVector(280.f, Y, 130.f), FVector(8.f, 8.f, 240.f));
}

void ADigetherPrototypeElevatorCab::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (CageMaterial)
	{
		Floor->SetMaterial(0, CageMaterial);
		for (UStaticMeshComponent* Part : CageParts) Part->SetMaterial(0, CageMaterial);
	}
}

void ADigetherPrototypeElevatorCab::BeginPlay()
{
	Super::BeginPlay();
	GroundLocation = GetActorLocation();
}

void ADigetherPrototypeElevatorCab::SetTravelProgress(float Progress)
{
	if (!HasAuthority() || !FMath::IsFinite(Progress)) { return; }
	SetActorLocation(GroundLocation + FVector(0.f, 0.f, FMath::Clamp(Progress, 0.f, 1.f) * FMath::Max(0.f, TravelHeight)), false, nullptr, ETeleportType::TeleportPhysics);
	ForceNetUpdate();
}

bool ADigetherPrototypeElevatorCab::ContainsLocation(const FVector& Location) const
{
	const FVector Delta = Location - GetActorLocation();
	return Delta.SizeSquared2D() <= FMath::Square(FMath::Max(0.f, BoardingRadius)) && Delta.Z >= -30.f && Delta.Z <= 240.f;
}

bool ADigetherPrototypeStation::InteractPrototype(APlayerController* Caller, bool bAlternate, FString& OutMessage)
{
	if (!ValidateInteraction(this, Caller, Mesh->GetComponentLocation(), OutMessage)) { return false; }
	ADigetherPrototypeGameMode* Mode = GetWorld()->GetAuthGameMode<ADigetherPrototypeGameMode>();
	switch (Kind)
	{
	case EPrototypeStationKind::Start:
	{
		const ADigetherPrototypeGameState* State = GetWorld()->GetGameState<ADigetherPrototypeGameState>();
		if (State && State->PublicState.Phase == EPrototypePhase::GameOver)
		{
			if (bAlternate) { return Mode->RequestRestartGame(Caller, OutMessage); }
			OutMessage = TEXT("시험 게임 오버: 보조 상호작용으로 새 게임을 시작하세요.");
			return true;
		}
		return Mode->RequestStartRunAt(Caller, this, OutMessage);
	}
	case EPrototypeStationKind::Elevator: return Mode->RequestElevator(Caller, this, OutMessage);
	case EPrototypeStationKind::Deposit: return Mode->RequestBeginDeposit(Caller, this, OutMessage);
	case EPrototypeStationKind::EmergencyExit: return Mode->RequestBeginEmergencyExit(Caller, this, OutMessage);
	case EPrototypeStationKind::Shop: return bAlternate ? Mode->RequestPurchaseEmergency(Caller, OutMessage) : Mode->RequestPurchase(Caller, OutMessage);
	default: OutMessage = TEXT("시험 안내: 출발 후 굴착하고 회수품을 가져오세요."); return true;
	}
}

ADigetherPrototypeDigCell::ADigetherPrototypeDigCell()
{
	bReplicates = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	SetCube(Mesh);
	SupportMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SupportMesh"));
	SupportMesh->SetupAttachment(RootComponent);
	SetCube(SupportMesh);
	SupportMesh->SetRelativeLocation(FVector(0.f, 68.f, 0.f));
	SupportMesh->SetRelativeScale3D(FVector(.18f, .18f, 1.8f));
	SupportMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ApplyAppearance();
}

void ADigetherPrototypeDigCell::BeginPlay()
{
	Super::BeginPlay();
	ApplyAppearance();
	if (HasAuthority()) { GetWorldTimerManager().SetTimer(WorldTimer, this, &ADigetherPrototypeDigCell::CheckTremor, 1.f, true); }
}

void ADigetherPrototypeDigCell::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(WorldTimer);
	Super::EndPlay(Reason);
}

void ADigetherPrototypeDigCell::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADigetherPrototypeDigCell, bExcavated);
	DOREPLIFETIME(ADigetherPrototypeDigCell, SupportStrength);
	DOREPLIFETIME(ADigetherPrototypeDigCell, bHasSupport);
}

void ADigetherPrototypeDigCell::OnRep_CellState() { ApplyAppearance(); }

void ADigetherPrototypeDigCell::ApplyAppearance()
{
	Mesh->SetRelativeScale3D(bExcavated ? FVector(.35f, .35f, .15f) : FVector(1.5f, 1.5f, 2.f));
	Mesh->SetRelativeLocation(bExcavated ? FVector(0.f, 0.f, -90.f) : FVector::ZeroVector);
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, bExcavated ? ECR_Ignore : ECR_Block);
	SupportMesh->SetVisibility(bExcavated && bHasSupport);
}

bool ADigetherPrototypeDigCell::InteractPrototype(APlayerController* Caller, bool bAlternate, FString& OutMessage)
{
	if (!ValidateInteraction(this, Caller, Mesh->GetComponentLocation(), OutMessage)) { return false; }
	if (!IsExpedition(GetWorld())) { OutMessage = TEXT("탐사 중에 사용할 수 있습니다."); return false; }
	const ADigetherPrototypePlayerState* Player = Caller->GetPlayerState<ADigetherPrototypePlayerState>();
	if (!Player) { OutMessage = TEXT("플레이어 상태를 확인할 수 없습니다."); return false; }
	for (const FPrototypeItem& Item : Player->PrivateState.Items)
	{
		if (Item.bTwoHanded) { OutMessage = TEXT("양손 화물을 내려놓아야 굴착하거나 보강할 수 있습니다."); return false; }
	}
	for (TActorIterator<ADigetherPrototypeCorpse> It(GetWorld()); It; ++It)
	{
		if (It->Carrier == Caller->GetPawn()) { OutMessage = TEXT("시체를 내려놓아야 굴착하거나 보강할 수 있습니다."); return false; }
	}
	const ADigetherPrototypeGameState* State = GetWorld()->GetGameState<ADigetherPrototypeGameState>();
	if (State && LastRunId != State->PublicState.RunId)
	{
		LastRunId = State->PublicState.RunId;
		bExcavated = false; SupportStrength = 0; bHasSupport = false; SecondsSinceTremor = 0.f;
		ApplyAppearance();
	}
	if (bAlternate)
	{
		if (!bExcavated) { OutMessage = TEXT("굴착한 구간에 보강을 설치하세요."); return false; }
		if (SupportStrength >= 2) { OutMessage = TEXT("보강이 이미 튼튼합니다."); return false; }
		if (!bHasSupport)
		{
			ADigetherPrototypeGameMode* Mode = GetWorld()->GetAuthGameMode<ADigetherPrototypeGameMode>();
			if (!Mode || !Mode->RequestUseWood(Caller, OutMessage)) { return false; }
			bHasSupport = true;
			OutMessage = TEXT("시험용 목재 1개로 보강했습니다.");
		}
		else { OutMessage = TEXT("보강을 무료로 보수했습니다."); }
		SupportStrength = 2;
	}
	else
	{
		if (bExcavated) { OutMessage = TEXT("이미 굴착한 구간입니다."); return false; }
		bExcavated = true;
		SupportStrength = 0;
		bHasSupport = false;
		SecondsSinceTremor = 0.f;
		OutMessage = TEXT("굴착했습니다. 보조 상호작용으로 보강하세요.");
	}
	ApplyAppearance();
	ForceNetUpdate();
	return true;
}

void ADigetherPrototypeDigCell::CheckTremor()
{
	if (!HasAuthority()) { return; }
	ADigetherPrototypeGameState* State = GetWorld()->GetGameState<ADigetherPrototypeGameState>();
	if (!State || State->PublicState.Phase != EPrototypePhase::Expedition) { return; }
	if (LastRunId != State->PublicState.RunId)
	{
		LastRunId = State->PublicState.RunId;
		bExcavated = false; SupportStrength = 0; bHasSupport = false; SecondsSinceTremor = 0.f;
		ApplyAppearance(); ForceNetUpdate(); return;
	}
	if (!bExcavated) { return; }
	const ADigetherPrototypeGameMode* ModeForTiming = GetWorld()->GetAuthGameMode<ADigetherPrototypeGameMode>();
	const float Duration = ModeForTiming ? FMath::Max(1.f, ModeForTiming->RunSeconds) : 180.f;
	const float RemainingFraction = FMath::Clamp(State->PublicState.RemainingSeconds / Duration, 0.f, 1.f);
	const float AscentFactor = State->PublicState.ElevatorTrips > 0 ? .7f : 1.f;
	const float Interval = FMath::Max(1.f, TremorSeconds * FMath::Lerp(.3f, 1.f, RemainingFraction) * AscentFactor);
	SecondsSinceTremor += 1.f;
	if (SecondsSinceTremor < Interval) { return; }
	SecondsSinceTremor = 0.f;
	if (SupportStrength > 0)
	{
		--SupportStrength;
		State->PublicState.Message = TEXT("여진이 발생했습니다. 굴착 구간과 보강 상태를 확인하세요.");
		State->ForceNetUpdate(); ForceNetUpdate(); return;
	}
	// 벽이 캐릭터 내부에 생성되는 것은 피한다. 비어 있을 때만 충돌을 복원한다.
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const APawn* Pawn = *It;
		if (Pawn && FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) < FMath::Square(150.f)) { SecondsSinceTremor = FMath::Max(0.f, Interval - 1.f); return; }
	}
	bExcavated = false;
	bHasSupport = false;
	State->PublicState.Message = TEXT("여진이 발생했습니다. 굴착 구간과 보강 상태를 확인하세요.");
	State->ForceNetUpdate();
	ApplyAppearance(); ForceNetUpdate();
	ADigetherPrototypeGameMode* Mode = GetWorld()->GetAuthGameMode<ADigetherPrototypeGameMode>();
	int32 PlayersWithPawns = 0;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (Mode && Mode->IsHazardTarget(*It)) { ++PlayersWithPawns; }
	}
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Pawn = *It;
		if (Mode && Pawn && Mode->IsHazardTarget(Pawn) && FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) < FMath::Square(275.f))
		{
			// 1인 시험에서는 구조 불가능한 교착을 피하도록 3단계를 시험한다.
			Mode->RequestWorldBurialPawn(Pawn, PlayersWithPawns == 1 ? 3 : 1);
		}
	}
}

ADigetherPrototypePickup::ADigetherPrototypePickup()
{
	bReplicates = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh")); RootComponent = Mesh;
	SetCube(Mesh); Mesh->SetRelativeScale3D(FVector(.35f));
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
}

void ADigetherPrototypePickup::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && !StoredItem.Id.IsValid())
	{
		StoredItem.Id = FGuid::NewGuid();
		StoredItem.Value = Value;
		StoredItem.WeightKg = WeightKg;
		StoredItem.bTwoHanded = bTwoHanded;
	}
}

void ADigetherPrototypePickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADigetherPrototypePickup, bReserved);
}

bool ADigetherPrototypePickup::InteractPrototype(APlayerController* Caller, bool bAlternate, FString& OutMessage)
{
	if (!ValidateInteraction(this, Caller, Mesh->GetComponentLocation(), OutMessage)) { return false; }
	if (bReserved) { OutMessage = TEXT("승강 중인 화물은 조작할 수 없습니다."); return false; }
	ADigetherPrototypeGameMode* Mode = GetWorld()->GetAuthGameMode<ADigetherPrototypeGameMode>();
	if (!Mode->RequestPickupItem(Caller, this, StoredItem, bAlternate, OutMessage)) { return false; }
	Destroy();
	return true;
}

ADigetherPrototypeHazard::ADigetherPrototypeHazard()
{
	bReplicates = true;
	SetReplicateMovement(true);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh")); RootComponent = Mesh;
	SetCube(Mesh); Mesh->SetRelativeScale3D(FVector(.6f));
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
}

void ADigetherPrototypeHazard::BeginPlay()
{
	Super::BeginPlay();
	SpawnLocation = GetActorLocation();
	if (HasAuthority()) { GetWorldTimerManager().SetTimer(HazardTimer, this, &ADigetherPrototypeHazard::ApplyHazard, .25f, true); }
}

void ADigetherPrototypeHazard::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(HazardTimer);
	Super::EndPlay(Reason);
}

void ADigetherPrototypeHazard::ApplyHazard()
{
	if (!HasAuthority() || !IsExpedition(GetWorld())) { return; }
	const ADigetherPrototypeGameState* State = GetWorld()->GetGameState<ADigetherPrototypeGameState>();
	if (State && LastRunId != State->PublicState.RunId)
	{
		LastRunId = State->PublicState.RunId;
		SetActorLocation(SpawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
		AttackCooldown = 0.f;
	}
	ADigetherPrototypeGameMode* Mode = GetWorld()->GetAuthGameMode<ADigetherPrototypeGameMode>();
	AttackCooldown = FMath::Max(0.f, AttackCooldown - .25f);
	APawn* Nearest = nullptr;
	float BestDistanceSquared = FMath::Square(Radius);
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Pawn = *It;
		if (!Mode || !Mode->IsHazardTarget(Pawn)) { continue; }
		const float DistanceSquared = FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			FCollisionQueryParams Params(SCENE_QUERY_STAT(PrototypeHazardSight), false, this);
			FHitResult Hit;
			if (!GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation(), Pawn->GetActorLocation(), ECC_Visibility, Params) || Hit.GetActor() == Pawn)
			{
				BestDistanceSquared = DistanceSquared;
				Nearest = Pawn;
			}
		}
	}
	if (!Nearest || !Mode) { return; }
	if (BestDistanceSquared <= FMath::Square(125.f))
	{
		if (AttackCooldown <= 0.f) { Mode->RequestWorldDamagePawn(Nearest, Damage); AttackCooldown = 2.f; }
		return;
	}
	FVector Direction = Nearest->GetActorLocation() - GetActorLocation();
	Direction.Z = 0.f;
	AddActorWorldOffset(Direction.GetSafeNormal() * 25.f, true);
}

ADigetherPrototypeWorldBuilder::ADigetherPrototypeWorldBuilder()
{
	bReplicates = true;
	// Each peer creates its own nonreplicated scene in BeginPlay, including
	// players joining at the distant school, so keep the builder relevant.
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	ElevatorCabClass = ADigetherPrototypeElevatorCab::StaticClass();
	StationClass = ADigetherPrototypeStation::StaticClass();
	DigCellClass = ADigetherPrototypeDigCell::StaticClass();
	TerrainClass = ADigetherExcavationTerrain::StaticClass();
	MineSceneClass = ADigetherPrototypeMineScene::StaticClass();
	PortalClass = ADigetherPrototypePortal::StaticClass();
	PickupClass = ADigetherPrototypePickup::StaticClass();
	HazardClass = ADigetherPrototypeHazard::StaticClass();
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Floor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Floor"));
	Floor->SetupAttachment(RootComponent);
	SetCube(Floor);
	Floor->SetRelativeLocation(FVector(2500.f, 0.f, -25.f));
	Floor->SetRelativeScale3D(FVector(70.f, 24.f, .5f));
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UStaticMeshComponent* Segment = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("ExcavationFloor%d"), Index));
		Segment->SetupAttachment(RootComponent);
		SetCube(Segment);
		ExcavationFloorSegments.Add(Segment);
	}
	NorthPartition = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NorthPartition"));
	NorthPartition->SetupAttachment(RootComponent);
	SetCube(NorthPartition);
	NorthPartition->SetRelativeLocation(FVector(3900.f, 675.f, 150.f));
	NorthPartition->SetRelativeScale3D(FVector(.5f, 10.5f, 3.f));
	SouthPartition = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SouthPartition"));
	SouthPartition->SetupAttachment(RootComponent);
	SetCube(SouthPartition);
	SouthPartition->SetRelativeLocation(FVector(3900.f, -675.f, 150.f));
	SouthPartition->SetRelativeScale3D(FVector(.5f, 10.5f, 3.f));
	ZoneOneLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ZoneOneLabel"));
	ZoneOneLabel->SetupAttachment(RootComponent);
	ZoneOneLabel->SetRelativeLocation(FVector(3700.f, -300.f, 220.f));
	ZoneOneLabel->SetWorldSize(35.f);
	ZoneOneLabel->SetText(FText::FromString(TEXT("TEST AREA 1")));
	ZoneTwoLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ZoneTwoLabel"));
	ZoneTwoLabel->SetupAttachment(RootComponent);
	ZoneTwoLabel->SetRelativeLocation(FVector(4400.f, -300.f, 220.f));
	ZoneTwoLabel->SetWorldSize(35.f);
	ZoneTwoLabel->SetText(FText::FromString(TEXT("TEST AREA 2")));
	BaseBarrier = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseBarrier"));
	BaseBarrier->SetupAttachment(RootComponent);
	SetCube(BaseBarrier);
	BaseBarrier->SetRelativeLocation(FVector(1500.f, 0.f, 200.f));
	BaseBarrier->SetRelativeScale3D(FVector(.5f, 24.f, 4.f));
	ElevatorPlatform = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ElevatorPlatform"));
	ElevatorPlatform->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Disc(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Disc.Succeeded()) { ElevatorPlatform->SetStaticMesh(Disc.Object); }
	ElevatorPlatform->SetRelativeLocation(FVector(3000.f, 220.f, 3.f));
	ElevatorPlatform->SetRelativeScale3D(FVector(7.f, 7.f, .05f));
	ElevatorPlatform->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ElevatorPlatform->SetVisibility(false);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UStaticMeshComponent* Wall = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("SeedRoomWall%d"), Index));
		Wall->SetupAttachment(RootComponent);
		SetCube(Wall);
		RoomWalls.Add(Wall);
	}
}

void ADigetherPrototypeWorldBuilder::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FaceLocalCamera(ZoneOneLabel, GetWorld());
	FaceLocalCamera(ZoneTwoLabel, GetWorld());
}

void ADigetherPrototypeWorldBuilder::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLayout();
}

void ADigetherPrototypeWorldBuilder::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADigetherPrototypeWorldBuilder, TestSeed);
}

void ADigetherPrototypeWorldBuilder::OnRep_Layout() { ApplyLayout(); }

bool ADigetherPrototypeWorldBuilder::ApplyLayout()
{
	if (RoomWalls.Num() != 4) { return false; }
	// The excavation volume is real empty space after a cut. Never leave the old
	// monolithic floor underneath it: it would keep invisible collision in the pit.
	Floor->SetRelativeLocation(FVector(1500.f, 0.f, -25.f));
	Floor->SetRelativeScale3D(FVector(50.f, 24.f, .5f));
	const FVector Centers[] = { FVector(5400.f, 0.f, -25.f), FVector(4400.f, -800.f, -25.f), FVector(4400.f, 800.f, -25.f) };
	const FVector Scales[] = { FVector(12.f, 24.f, .5f), FVector(8.f, 8.f, .5f), FVector(8.f, 8.f, .5f) };
	for (int32 Index = 0; Index < ExcavationFloorSegments.Num(); ++Index)
	{
		ExcavationFloorSegments[Index]->SetRelativeLocation(Centers[Index]);
		ExcavationFloorSegments[Index]->SetRelativeScale3D(Scales[Index]);
		if (GroundMaterial) ExcavationFloorSegments[Index]->SetMaterial(0, GroundMaterial);
	}
	if (GroundMaterial) { Floor->SetMaterial(0, GroundMaterial); BaseBarrier->SetMaterial(0, GroundMaterial); }
	NorthPartition->SetVisibility(false); NorthPartition->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SouthPartition->SetVisibility(false); SouthPartition->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ZoneOneLabel->SetVisibility(false); ZoneTwoLabel->SetVisibility(false);
	for (UStaticMeshComponent* Wall : RoomWalls) { Wall->SetVisibility(false); Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
	FRandomStream Stream(TestSeed);
	// 고정 방 모듈을 순서대로 연결하고 출입구 위치만 seed로 고른다.
	// 모든 문은 폭 300cm이며 방의 양 끝을 제외하므로 연속 경로가 존재한다.
	constexpr float HalfWidth = 1200.f;
	constexpr float HalfDoor = 150.f;
	for (int32 Room = 0; Room < 2; ++Room)
	{
		const float DoorY = Stream.RandRange(-2, 2) * 100.f;
		const float LowerEnd = DoorY - HalfDoor;
		const float UpperStart = DoorY + HalfDoor;
		if (LowerEnd <= -HalfWidth || UpperStart >= HalfWidth || HalfDoor * 2.f < 100.f)
		{
			UE_LOG(LogTemp, Error, TEXT("Prototype room connection is invalid; seed %d"), TestSeed);
			return false;
		}
		const float X = 4600.f + Room * 600.f;
		UStaticMeshComponent* Lower = RoomWalls[Room * 2];
		UStaticMeshComponent* Upper = RoomWalls[Room * 2 + 1];
		Lower->SetRelativeLocation(FVector(X, (-HalfWidth + LowerEnd) * .5f, 150.f));
		Lower->SetRelativeScale3D(FVector(.5f, (LowerEnd + HalfWidth) / 100.f, 3.f));
		Upper->SetRelativeLocation(FVector(X, (UpperStart + HalfWidth) * .5f, 150.f));
		Upper->SetRelativeScale3D(FVector(.5f, (HalfWidth - UpperStart) / 100.f, 3.f));
	}
	return true;
}

void ADigetherPrototypeWorldBuilder::BeginPlay()
{
	Super::BeginPlay();
	ApplyLayout();
	for (TActorIterator<ADigetherPrototypeMineScene> It(GetWorld()); It; ++It)
	{
		if (It->GetOwner() == this) { LocalScene = *It; break; }
	}
	if (!LocalScene.IsValid())
	{
		FActorSpawnParameters SceneParams;
		SceneParams.Owner = this;
		SceneParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		LocalScene = GetWorld()->SpawnActor<ADigetherPrototypeMineScene>(MineSceneClass ? MineSceneClass.Get() : ADigetherPrototypeMineScene::StaticClass(), GetActorLocation(), GetActorRotation(), SceneParams);
	}
	if (!HasAuthority()) { return; }
	if (!ApplyLayout()) { UE_LOG(LogTemp, Error, TEXT("Prototype world was not spawned: invalid room layout.")); return; }
	const FVector Origin = GetActorLocation();
	FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADigetherPrototypeElevatorCab* Cab = GetWorld()->SpawnActor<ADigetherPrototypeElevatorCab>(ElevatorCabClass ? ElevatorCabClass.Get() : ADigetherPrototypeElevatorCab::StaticClass(), Origin + FVector(3000.f, 220.f, 0.f), FRotator::ZeroRotator, Params);
	if (Cab) { Cab->Tags.Add(TEXT("PrototypeElevatorCab")); }
	auto SpawnStation = [this, Origin, Cab](EPrototypeStationKind Kind, const FVector& Offset, const TCHAR* Tag)
	{
		const FTransform Transform(FRotator::ZeroRotator, Origin + Offset);
		ADigetherPrototypeStation* Station = GetWorld()->SpawnActorDeferred<ADigetherPrototypeStation>(StationClass ? StationClass.Get() : ADigetherPrototypeStation::StaticClass(), Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Station) { Station->Kind = Kind; if (Kind == EPrototypeStationKind::Elevator) { Station->SetElevatorCab(Cab); } Station->Tags.Add(FName(Tag)); Station->FinishSpawning(Transform); }
	};
	SpawnStation(EPrototypeStationKind::Start, FVector(150.f, 0.f, 100.f), TEXT("PrototypeStart"));
	SpawnStation(EPrototypeStationKind::Deposit, FVector(0.f, 220.f, 100.f), TEXT("PrototypeDeposit"));
	SpawnStation(EPrototypeStationKind::Terminal, FVector(-150.f, 0.f, 100.f), TEXT("PrototypeTerminal"));
	SpawnStation(EPrototypeStationKind::Shop, FVector(0.f, -220.f, 100.f), TEXT("PrototypeShop"));
	SpawnStation(EPrototypeStationKind::Elevator, FVector(3000.f, 220.f, 100.f), TEXT("PrototypeElevator"));
	SpawnStation(EPrototypeStationKind::EmergencyExit, FVector(3000.f, -220.f, 100.f), TEXT("PrototypeEmergencyExit"));
	ADigetherExcavationTerrain* Soil = GetWorld()->SpawnActor<ADigetherExcavationTerrain>(TerrainClass ? TerrainClass.Get() : ADigetherExcavationTerrain::StaticClass(), Origin + FVector(4400.f, 0.f, 0.f), FRotator::ZeroRotator, Params);
	if (Soil) Soil->Tags.Add(TEXT("PrototypeTerrainFloor"));
	ADigetherExcavationTerrain* DigWall = GetWorld()->SpawnActor<ADigetherExcavationTerrain>(TerrainClass ? TerrainClass.Get() : ADigetherExcavationTerrain::StaticClass(), Origin + FVector(5300.f, 0.f, 200.f), FRotator(90.f, 0.f, 0.f), Params);
	if (DigWall) DigWall->Tags.Add(TEXT("PrototypeTerrainWall"));
	// Both endpoints are server spawned; their destinations are configured only on authority.
	auto SpawnPortal = [this, Origin, &Params](const FVector& Offset, const TCHAR* Tag, const TArray<FVector>& Arrivals, const FRotator& Facing)
	{
		ADigetherPrototypePortal* Portal = GetWorld()->SpawnActor<ADigetherPrototypePortal>(PortalClass ? PortalClass.Get() : ADigetherPrototypePortal::StaticClass(), Origin + Offset, FRotator::ZeroRotator, Params);
		if (Portal)
		{
			Portal->Tags.Add(FName(Tag));
			TArray<FVector> WorldArrivals;
			for (const FVector& Arrival : Arrivals) WorldArrivals.Add(Origin + Arrival);
			Portal->SetArrivalCandidates(WorldArrivals, Facing);
		}
	};
	SpawnPortal(FVector(5600.f, 3000.f, 100.f), TEXT("PrototypeMinePortal"),
		{FVector(20100.f, 0.f, 96.f), FVector(20100.f, 160.f, 96.f), FVector(20100.f, -160.f, 96.f), FVector(20300.f, 0.f, 96.f)}, FRotator(0.f, 0.f, 0.f));
	SpawnPortal(FVector(19800.f, 0.f, 100.f), TEXT("PrototypeSchoolPortal"),
		{FVector(5400.f, 3000.f, 96.f), FVector(5400.f, 3160.f, 96.f), FVector(5400.f, 2840.f, 96.f), FVector(5500.f, 2750.f, 96.f)}, FRotator(0.f, 180.f, 0.f));
	ADigetherPrototypeHazard* Hazard = GetWorld()->SpawnActor<ADigetherPrototypeHazard>(HazardClass ? HazardClass.Get() : ADigetherPrototypeHazard::StaticClass(), Origin + FVector(5000.f, 950.f, 50.f), FRotator::ZeroRotator, Params);
	if (Hazard) { Hazard->Tags.Add(TEXT("PrototypeHazard")); }
	GetWorldTimerManager().SetTimer(ResetTimer, this, &ADigetherPrototypeWorldBuilder::RefreshRun, 1.f, true);
	RefreshRun();
}

void ADigetherPrototypeWorldBuilder::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(ResetTimer);
	if (LocalScene.IsValid()) LocalScene->Destroy();
	LocalScene.Reset();
	Super::EndPlay(Reason);
}

void ADigetherPrototypeWorldBuilder::RefreshRun()
{
	if (!HasAuthority()) { return; }
	const ADigetherPrototypeGameState* State = GetWorld()->GetGameState<ADigetherPrototypeGameState>();
	if (!State || State->PublicState.Phase != EPrototypePhase::Expedition || !State->PublicState.RunId.IsValid() || LastRunId == State->PublicState.RunId) { return; }
	LastRunId = State->PublicState.RunId;
	for (TActorIterator<ADigetherExcavationTerrain> It(GetWorld()); It; ++It)
	{
		if (!It->ResetTerrain()) UE_LOG(LogTemp, Error, TEXT("Excavation terrain reset failed"));
	}
	for (const TWeakObjectPtr<ADigetherPrototypePickup>& Existing : RunPickups)
	{
		if (Existing.IsValid()) { Existing->Destroy(); }
	}
	RunPickups.Reset();
	FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 Index = 0; Index < 10; ++Index)
	{
		const FVector SchoolLocations[] = {
			FVector(20500.f, 700.f, 75.f), FVector(20600.f, 700.f, 75.f), FVector(20400.f, 700.f, 75.f),
			FVector(21600.f, 700.f, 75.f), FVector(21700.f, 700.f, 75.f), FVector(21500.f, 700.f, 75.f)};
		const FVector Location = Index < 4 ? FVector(3650.f, -450.f + 100.f * Index, 75.f) : SchoolLocations[Index - 4];
		ADigetherPrototypePickup* Item = GetWorld()->SpawnActor<ADigetherPrototypePickup>(PickupClass ? PickupClass.Get() : ADigetherPrototypePickup::StaticClass(), GetActorLocation() + Location, FRotator::ZeroRotator, Params);
		if (Item)
		{
			Item->Tags.Add(FName(*FString::Printf(TEXT("PrototypePickup%02d"), Index)));
			RunPickups.Add(Item);
		}
	}
}
