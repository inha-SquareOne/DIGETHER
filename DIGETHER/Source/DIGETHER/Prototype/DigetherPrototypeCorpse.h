#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DigetherPrototypeTypes.h"
#include "DigetherPrototypeCorpse.generated.h"

class ADigetherPrototypePlayerState;
class UStaticMeshComponent;

UCLASS()
class DIGETHER_API ADigetherPrototypeCorpse : public AActor, public IDigetherPrototypeInteractable
{
	GENERATED_BODY()

public:
	ADigetherPrototypeCorpse();
	virtual bool InteractPrototype(APlayerController* Caller, bool bAlternate, FString& OutMessage) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Prototype")
	TObjectPtr<ADigetherPrototypePlayerState> Deceased;

	// 재접속 상태 연결용 시험 ID는 서버에서만 사용한다.
	UPROPERTY()
	FString DeceasedFixtureId;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Prototype")
	TObjectPtr<APawn> Carrier;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Prototype")
	bool bReserved = false;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;
};
