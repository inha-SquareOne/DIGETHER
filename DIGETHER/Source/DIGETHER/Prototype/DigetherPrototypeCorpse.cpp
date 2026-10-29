#include "DigetherPrototypeCorpse.h"
#include "DigetherPrototypeGameMode.h"
#include "DigetherPrototypePlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ADigetherPrototypeCorpse::ADigetherPrototypeCorpse()
{
	bReplicates = true;
	SetReplicateMovement(true);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CorpseMesh"));
	SetRootComponent(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) Mesh->SetStaticMesh(Cube.Object);
	Mesh->SetWorldScale3D(FVector(0.6f, 0.3f, 0.2f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void ADigetherPrototypeCorpse::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADigetherPrototypeCorpse, Deceased);
	DOREPLIFETIME(ADigetherPrototypeCorpse, Carrier);
	DOREPLIFETIME(ADigetherPrototypeCorpse, bReserved);
}

bool ADigetherPrototypeCorpse::InteractPrototype(APlayerController* Caller, bool bAlternate, FString& OutMessage)
{
	ADigetherPrototypeGameMode* Mode = GetWorld()->GetAuthGameMode<ADigetherPrototypeGameMode>();
	if (!Mode || !Mode->CanInteract(Caller, this, OutMessage)) return false;
	if (bReserved) { OutMessage = TEXT("운행에 예약된 시체입니다."); return false; }
	APawn* Pawn = Caller->GetPawn();
	if (Carrier == Pawn)
	{
		FVector Location;
		// The 60 x 30 x 20 cm fixture fits within a 35 cm sphere at any rotation.
		if (!Mode->FindSafeDropLocation(Pawn, this, 35.f, Location))
		{
			OutMessage = TEXT("시체를 안전하게 내려놓을 공간이 없습니다.");
			return false;
		}
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		Carrier = nullptr;
		OutMessage = TEXT("시험 시체를 내려놓았습니다.");
		ForceNetUpdate();
		return true;
	}
	if (Carrier)
	{
		OutMessage = TEXT("다른 플레이어가 시체를 운반 중입니다.");
		return false;
	}
	const ADigetherPrototypePlayerState* Player = Caller->GetPlayerState<ADigetherPrototypePlayerState>();
	if (!Player) return false;
	for (const FPrototypeItem& Item : Player->PrivateState.Items)
	{
		if (Item.bTwoHanded)
		{
			OutMessage = TEXT("양손 물품을 내려놓아야 시체를 들 수 있습니다.");
			return false;
		}
	}
	for (TActorIterator<ADigetherPrototypeCorpse> It(GetWorld()); It; ++It)
	{
		if (It->Carrier == Pawn)
		{
			OutMessage = TEXT("이미 시체를 운반 중입니다.");
			return false;
		}
	}
	Carrier = Pawn;
	AttachToActor(Pawn, FAttachmentTransformRules::KeepWorldTransform);
	SetActorRelativeLocation(FVector(90.f, 0.f, 0.f));
	OutMessage = TEXT("시험 시체를 양손으로 운반합니다.");
	ForceNetUpdate();
	return true;
}
