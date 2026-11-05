#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Prototype/DigetherPrototypeTypes.h"
#include "DigetherPrototypePortal.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;
class UMaterialInterface;

// 시험용 양방향 이동. 목적지는 서버의 월드 빌더가 고정한다.
UCLASS()
class DIGETHER_API ADigetherPrototypePortal : public AActor, public IDigetherPrototypeInteractable
{
	GENERATED_BODY()
public:
	ADigetherPrototypePortal();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool InteractPrototype(APlayerController* Caller, bool bAlternate, FString& OutMessage) override;
	void SetArrivalCandidates(const TArray<FVector>& Candidates, const FRotator& Facing);
private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> Frame;
	UPROPERTY(EditDefaultsOnly, Category="Prototype|Appearance")
	TObjectPtr<UMaterialInterface> PortalSurfaceMaterial;
	// 서버 전용이며 클라이언트는 목적지를 지정할 수 없다.
	TArray<FVector> ArrivalCandidates;
	FRotator ArrivalFacing = FRotator::ZeroRotator;
};
