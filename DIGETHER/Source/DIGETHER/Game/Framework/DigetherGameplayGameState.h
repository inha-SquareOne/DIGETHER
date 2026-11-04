#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "DigetherGameplayGameState.generated.h"

class UDigetherQuotaStateComponent;
class UDigetherRunPublicStateComponent;

// 공개 상태만 보관하고 게임 진행 판정은 조율자에게 맡긴다.
UCLASS()
class DIGETHER_API ADigetherGameplayGameState : public AGameState
{
	GENERATED_BODY()

public:
	ADigetherGameplayGameState();

	UFUNCTION(BlueprintPure, Category="Digether|Economy")
	UDigetherQuotaStateComponent* GetQuotaComponent() const;

	UFUNCTION(BlueprintPure, Category="Digether|Run")
	UDigetherRunPublicStateComponent* GetRunPublicStateComponent() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Economy")
	TObjectPtr<UDigetherQuotaStateComponent> QuotaComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Run")
	TObjectPtr<UDigetherRunPublicStateComponent> RunPublicStateComponent;
};
