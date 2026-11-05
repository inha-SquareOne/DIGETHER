#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "DigetherGameplayGameState.generated.h"

class UDigetherQuotaStateComponent;
class UDigetherRunPublicStateComponent;
class UDigetherGameOverPublicStateComponent;

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

	// 기획 S33 게임 오버 단계·흡입·결과 크레딧 공개 상태다.
	UFUNCTION(BlueprintPure, Category="Digether|Game Over")
	UDigetherGameOverPublicStateComponent* GetGameOverPublicStateComponent() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Economy")
	TObjectPtr<UDigetherQuotaStateComponent> QuotaComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Run")
	TObjectPtr<UDigetherRunPublicStateComponent> RunPublicStateComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Game Over")
	TObjectPtr<UDigetherGameOverPublicStateComponent> GameOverPublicStateComponent;
};
