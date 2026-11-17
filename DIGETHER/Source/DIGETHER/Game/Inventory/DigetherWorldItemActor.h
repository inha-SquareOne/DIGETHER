#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/Interaction/DigetherInteractable.h"
#include "Game/Inventory/DigetherInventoryTypes.h"
#include "DigetherWorldItemActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

// 바닥에 놓인 아이템(줍기 대상), 한 인스턴스는 월드 액터이거나 인벤토리 슬롯이거나 둘 중 한 곳에만 있음
// 서버가 만들어 복제하고, 모습은 아이템 정의의 WorldMesh를 쓰며 없으면 기본 상자로 대신함
// 서버가 물리로 떨어뜨리고(중력·충돌은 메시 크기의 상자로 계산) 위치는 엔진 물리 복제로 모든 머신에 맞춤, 플레이어와는 부딪히지 않음
// 줍기는 E 상호작용(IDigetherInteractable): 서버가 인벤토리에 넣는 데 성공하면 이 액터를 없앰, 가득 차면 그대로 남음
UCLASS()
class DIGETHER_API ADigetherWorldItemActor : public AActor, public IDigetherInteractable
{
	GENERATED_BODY()

public:
	ADigetherWorldItemActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버 전용: 인스턴스를 담은 월드 아이템을 Location(손 높이 등 공중)에 만들고 Forward 방향으로 살짝 던져 중력으로 떨어뜨림
	// InheritedVelocity는 버린 사람의 속도(달리며 버리면 같이 밀려 나감), 생성에 실패하면 nullptr이고 인스턴스는 호출한 쪽이 책임짐
	static ADigetherWorldItemActor* DropItem(UWorld* World, const FDigetherItemInstance& Item, const FVector& Location, const FRotator& Rotation, const FVector& Forward, const FVector& InheritedVelocity);

	const FDigetherItemInstance& GetItem() const { return Item; }

	// IDigetherInteractable
	virtual EDigetherInteractionKind GetInteractionKind() const override { return EDigetherInteractionKind::Pickup; }
	virtual bool CanInteract(const APawn* Interactor) const override;
	virtual FText GetInteractionPrompt() const override;
	virtual void Interact(APawn* Interactor) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_Item();

	// 정의를 읽어 메시와 물리 상자 크기를 정함(모든 머신)
	void RefreshVisuals();

	// 물리 몸체(루트), 메시 경계 크기의 상자로 중력·바닥 충돌을 계산함(메시에 충돌이 없어도 떨어지게)
	UPROPERTY(VisibleAnywhere, Category="Digether|Item")
	TObjectPtr<UBoxComponent> PhysicsBox;

	// 보이는 모습이자 줍기 조준 대상(Visibility 겹침만)
	UPROPERTY(VisibleAnywhere, Category="Digether|Item")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	// 레벨에 직접 놓는 아이템용: 서버가 시작할 때 이 정의 이름(예: DA_Item_Pickaxe)으로 인스턴스를 새로 만듦
	// 코드로 만든 아이템(버리기·시험 생성)은 이미 인스턴스가 있어 쓰지 않음
	UPROPERTY(EditInstanceOnly, Category="Digether|Item")
	FName PlacedDefinitionId;

	UPROPERTY(ReplicatedUsing=OnRep_Item)
	FDigetherItemInstance Item;

	// 서버 전용: 줍기가 확정돼 곧 사라질 액터, 같은 틱의 두 번째 요청을 거절함
	bool bConsumed = false;
};
