#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "DigetherPrototypeTypes.h"
#include "DigetherPrototypeGameState.generated.h"

UCLASS()
class DIGETHER_API ADigetherPrototypeGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Prototype")
	FPrototypePublicState PublicState;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
