#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DigetherRunHudWidget.generated.h"

class AGameStateBase;
class ADigetherGameplayPlayerController;
class UDigetherQuotaStateComponent;
class UDigetherRunPublicStateComponent;
class UDigetherWalletComponent;
class UTextBlock;
struct FDigetherQuotaPublicState;
struct FDigetherRunPublicState;
struct FDigetherWalletState;

// 공개 런·목표 상태와 본인 지갑 표시
// 일차·목표·잔고·런 단계는 변경 알림으로만 갱신, 상호작용 힌트만 시선 따라 주기 갱신
UCLASS(Abstract)
class DIGETHER_API UDigetherRunHudWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 화면별 생략 가능한 선택 바인딩
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Day;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Quota;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Balance;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RunPhase;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Interaction;

private:
	// GameState는 늦게 복제·교체될 수 있어 GameStateSetEvent로 재연결
	void HandleGameStateSet(AGameStateBase* GameState);
	void BindGameState(AGameStateBase* GameState);
	void UnbindGameState();
	// PlayerState는 늦은 복제·재접속 교체 시 PC 알림으로 재연결
	void HandlePlayerStateChanged();
	void BindWallet();
	void UnbindWallet();

	void HandleQuotaChanged(const FDigetherQuotaPublicState& State);
	void HandleRunChanged(const FDigetherRunPublicState& State);
	void HandleWalletChanged(const FDigetherWalletState& State);
	void RefreshQuota();
	void RefreshRun();
	void RefreshBalance();
	void RefreshInteraction();

	TWeakObjectPtr<UWorld> BoundWorld;
	FDelegateHandle GameStateSetHandle;
	TWeakObjectPtr<UDigetherQuotaStateComponent> BoundQuota;
	FDelegateHandle QuotaChangedHandle;
	TWeakObjectPtr<UDigetherRunPublicStateComponent> BoundRun;
	FDelegateHandle RunChangedHandle;
	TWeakObjectPtr<ADigetherGameplayPlayerController> BoundController;
	FDelegateHandle PlayerStateChangedHandle;
	TWeakObjectPtr<UDigetherWalletComponent> BoundWallet;
	FDelegateHandle WalletChangedHandle;
	FTimerHandle InteractionTimer;
};
