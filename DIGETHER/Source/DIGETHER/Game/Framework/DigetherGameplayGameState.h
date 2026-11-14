#pragma once

#include "CoreMinimal.h"
#include "Game/Elevator/DigetherElevatorTypes.h"
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

	// 기획 런 플로우 v1.1.0 5.2: 출발 레버 공개 경고(LeverWarning)와 런 공개 상태로 문구용 경고 종류 판정
	// 4일차 마감 확정 뒤 출발 전·공통 로딩 실패 뒤 마감 유지 중이면 DeadlineConfirmed, 문구는 레버 컴포넌트 함수 사용
	UFUNCTION(BlueprintPure, Category="Digether|Elevator")
	EDigetherDepartureWarningKind GetDepartureWarningKind(EDigetherDepartureWarningKind LeverWarning) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Economy")
	TObjectPtr<UDigetherQuotaStateComponent> QuotaComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Run")
	TObjectPtr<UDigetherRunPublicStateComponent> RunPublicStateComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Game Over")
	TObjectPtr<UDigetherGameOverPublicStateComponent> GameOverPublicStateComponent;
};
