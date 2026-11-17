#include "Game/Inventory/DigetherWorldItemActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Game/Data/DigetherItemDefinition.h"
#include "Game/Inventory/DigetherInventoryComponent.h"
#include "Game/Inventory/DigetherInventoryLog.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 정의에 메시가 없을 때 쓰는 기본 도형의 크기(엔진 기본 상자는 한 변 100cm), 임시 표시용
	constexpr float PlaceholderBoxScale = 0.3f;
	// 물리 상자의 최소 반 크기(cm), 아주 얇은 메시가 바닥을 뚫지 않게 함
	constexpr float MinPhysicsHalfExtentCm = 3.f;
	// 버릴 때 던지는 속도(cm/s): 앞으로·위로, 손에서 놓아 툭 떨어지는 느낌의 시험값
	constexpr float DropForwardSpeed = 150.f;
	constexpr float DropUpSpeed = 80.f;
	// 떨어지며 살짝 도는 각속도 최대(도/s), 시험값
	constexpr float DropMaxSpinDeg = 180.f;
	// 멀리 굴러가지 않고 자리 잡게 하는 감쇠, 시험값
	constexpr float ItemLinearDamping = 0.2f;
	constexpr float ItemAngularDamping = 1.5f;
}

ADigetherWorldItemActor::ADigetherWorldItemActor()
{
	bReplicates = true;
	// 서버가 시뮬레이션하는 물리 위치·속도를 복제하면 클라이언트도 물리를 켜고 보정함(엔진 AActor 물리 복제)
	SetReplicateMovement(true);

	PhysicsBox = CreateDefaultSubobject<UBoxComponent>(TEXT("PhysicsBox"));
	SetRootComponent(PhysicsBox);
	PhysicsBox->SetMobility(EComponentMobility::Movable);
	PhysicsBox->InitBoxExtent(FVector(50.f * PlaceholderBoxScale));
	// 바닥·벽·다른 아이템과만 부딪힘, 플레이어(Pawn)는 통과하고 조준·시야(Visibility·Camera)는 막지 않음
	// PhysicsBody 종류를 쓰지 않는 이유: 근접 판정 대상(Pawn·PhysicsBody)에 들어가 삽에 맞지 않게 함
	PhysicsBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PhysicsBox->SetCollisionObjectType(ECC_WorldDynamic);
	PhysicsBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	PhysicsBox->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	PhysicsBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	PhysicsBox->SetGenerateOverlapEvents(false);
	PhysicsBox->SetLinearDamping(ItemLinearDamping);
	PhysicsBox->SetAngularDamping(ItemAngularDamping);
	// 물리는 서버 BeginPlay에서 켬(클라이언트는 복제된 물리 상태를 받으면 엔진이 켬)
	PhysicsBox->SetSimulatePhysics(false);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(PhysicsBox);
	// 줍기 조준(Visibility)에만 반응하고 이동·물리는 막지 않음, 겹치면 다른 대상도 함께 모아 우선순위로 고름
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComponent->SetCollisionObjectType(ECC_WorldDynamic);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Overlap);
	MeshComponent->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaceholderMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (PlaceholderMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(PlaceholderMesh.Object);
		MeshComponent->SetRelativeScale3D(FVector(PlaceholderBoxScale));
	}
}

void ADigetherWorldItemActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADigetherWorldItemActor, Item);
}

ADigetherWorldItemActor* ADigetherWorldItemActor::DropItem(UWorld* World, const FDigetherItemInstance& Item, const FVector& Location, const FRotator& Rotation, const FVector& Forward, const FVector& InheritedVelocity)
{
	if (!IsValid(World) || World->GetNetMode() == NM_Client || !Item.IsSet())
	{
		return nullptr;
	}
	const FTransform SpawnTransform(Rotation, Location);
	ADigetherWorldItemActor* Actor = World->SpawnActorDeferred<ADigetherWorldItemActor>(
		ADigetherWorldItemActor::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(Actor))
	{
		return nullptr;
	}
	Actor->Item = Item;
	// BeginPlay에서 메시·상자 크기를 정하고 물리를 켬
	Actor->FinishSpawning(SpawnTransform);
	if (Actor->PhysicsBox->IsSimulatingPhysics())
	{
		const FVector Toss = Forward.GetSafeNormal2D() * DropForwardSpeed + FVector::UpVector * DropUpSpeed;
		Actor->PhysicsBox->SetPhysicsLinearVelocity(InheritedVelocity + Toss);
		Actor->PhysicsBox->SetPhysicsAngularVelocityInDegrees(FVector(
			FMath::FRandRange(-DropMaxSpinDeg, DropMaxSpinDeg),
			FMath::FRandRange(-DropMaxSpinDeg, DropMaxSpinDeg),
			FMath::FRandRange(-DropMaxSpinDeg, DropMaxSpinDeg)));
	}
	return Actor;
}

void ADigetherWorldItemActor::BeginPlay()
{
	Super::BeginPlay();
	// 레벨에 놓인 아이템은 서버가 인스턴스를 만들어 복제함(줍고 버려도 같은 인스턴스가 이어짐)
	if (HasAuthority() && !Item.IsSet() && !PlacedDefinitionId.IsNone())
	{
		Item = FDigetherItemInstance::MakeNew(PlacedDefinitionId);
	}
	// 아이템 크기는 정의(WorldMeshScale)로만 정함: 레벨에 놓을 때 바뀐 액터 배율은 무시함(모든 머신)
	// 예: 루트가 메시였던 이전 구조에서 놓은 액터는 기본 상자 배율 0.3이 액터 배율로 남아 있음
	SetActorScale3D(FVector::OneVector);
	RefreshVisuals();
	// 물리 원본은 서버, 레벨에 놓인 아이템도 바닥으로 내려앉음
	if (HasAuthority())
	{
		PhysicsBox->SetSimulatePhysics(true);
	}
}

void ADigetherWorldItemActor::OnRep_Item()
{
	RefreshVisuals();
}

void ADigetherWorldItemActor::RefreshVisuals()
{
	const UDigetherItemDefinition* Definition = UDigetherItemDefinition::FindDefinition(Item.DefinitionId);
	if (IsValid(Definition) && !Definition->WorldMesh.IsNull())
	{
		if (UStaticMesh* Mesh = Definition->WorldMesh.LoadSynchronous())
		{
			MeshComponent->SetStaticMesh(Mesh);
			MeshComponent->SetRelativeScale3D(FVector(FMath::Max(Definition->WorldMeshScale, 0.01f)));
		}
	}
	// 물리 상자를 메시 경계에 맞추고 메시를 상자 가운데로 옮김(메시 원점이 손잡이 끝이어도 무게 중심이 가운데)
	const UStaticMesh* Mesh = MeshComponent->GetStaticMesh();
	if (!IsValid(Mesh))
	{
		return;
	}
	const FBoxSphereBounds LocalBounds = Mesh->GetBounds();
	const FVector Scale = MeshComponent->GetRelativeScale3D();
	const FVector Extent = (LocalBounds.BoxExtent * Scale).ComponentMax(FVector(MinPhysicsHalfExtentCm));
	PhysicsBox->SetBoxExtent(Extent);
	MeshComponent->SetRelativeLocation(-LocalBounds.Origin * Scale);
}

bool ADigetherWorldItemActor::CanInteract(const APawn* Interactor) const
{
	return IsValid(Interactor) && Item.IsSet() && !bConsumed;
}

FText ADigetherWorldItemActor::GetInteractionPrompt() const
{
	const UDigetherItemDefinition* Definition = UDigetherItemDefinition::FindDefinition(Item.DefinitionId);
	const FText Name = IsValid(Definition) && !Definition->DisplayName.IsEmpty() ? Definition->DisplayName : FText::FromName(Item.DefinitionId);
	return FText::Format(NSLOCTEXT("DigetherWorldItem", "PickupPrompt", "줍기: {0}"), Name);
}

void ADigetherWorldItemActor::Interact(APawn* Interactor)
{
	if (!HasAuthority() || bConsumed || !IsValid(Interactor))
	{
		return;
	}
	const APlayerState* PlayerState = Interactor->GetPlayerState();
	UDigetherInventoryComponent* Inventory = IsValid(PlayerState) ? PlayerState->FindComponentByClass<UDigetherInventoryComponent>() : nullptr;
	if (!IsValid(Inventory))
	{
		UE_LOG(LogDigetherInventory, Warning, TEXT("줍기 실패: %s의 PlayerState에 인벤토리가 없다"), *GetNameSafe(Interactor));
		return;
	}
	// 확정: 인벤토리에 넣는 데 성공한 요청만 월드 아이템을 없앰(늦게 온 요청은 bConsumed로 이미 거절됨)
	int32 SlotIndex = INDEX_NONE;
	const EDigetherInventoryResult Result = Inventory->TryAddItem(Item, SlotIndex);
	if (Result != EDigetherInventoryResult::Success)
	{
		UE_LOG(LogDigetherInventory, Log, TEXT("%s가 %s를 줍지 못했다: 결과 %d"), *GetNameSafe(Interactor), *Item.DefinitionId.ToString(), static_cast<int32>(Result));
		return;
	}
	bConsumed = true;
	UE_LOG(LogDigetherInventory, Log, TEXT("%s가 %s를 주웠다(슬롯 %d)"), *GetNameSafe(Interactor), *Item.DefinitionId.ToString(), SlotIndex + 1);
	Destroy();
}
