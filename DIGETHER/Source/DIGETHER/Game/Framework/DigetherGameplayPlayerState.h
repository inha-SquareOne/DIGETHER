#pragma once

#include "CoreMinimal.h"
#include "Game/Economy/DigetherEconomyTypes.h"
#include "GameFramework/PlayerState.h"
#include "DigetherGameplayPlayerState.generated.h"

class UDigetherLifeStateComponent;
class UDigetherWalletComponent;

// Pawn 교체와 분리해 지갑·생존 상태를 유지한다.
UCLASS()
class DIGETHER_API ADigetherGameplayPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ADigetherGameplayPlayerState();

	UFUNCTION(BlueprintPure, Category="Digether|Economy")
	UDigetherWalletComponent* GetWalletComponent() const;

	UFUNCTION(BlueprintPure, Category="Digether|Life")
	UDigetherLifeStateComponent* GetLifeComponent() const;

	// 서버 월드 안의 조율용 식별자다. 인증 ID·재접속 복원을 대신하지 않는다.
	void InitializeGameplayIdentity();
	FGuid GetGameplayPlayerId() const;
	FString GetGameplayPlayerKey() const;

	FDigetherWalletState CaptureState() const;
	EDigetherEconomyResult RestoreState(const FDigetherWalletState& State, bool bNotify = true);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Economy")
	TObjectPtr<UDigetherWalletComponent> WalletComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Life")
	TObjectPtr<UDigetherLifeStateComponent> LifeComponent;

private:
	// 생존·엘리베이터 서버 판정만 공유하며 클라이언트에 복제하지 않는다.
	FGuid GameplayPlayerId;
};
