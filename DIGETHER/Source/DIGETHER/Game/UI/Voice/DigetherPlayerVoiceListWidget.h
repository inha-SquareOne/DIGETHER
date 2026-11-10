#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "DigetherPlayerVoiceListWidget.generated.h"

class UDigetherPlayerVoiceRowWidget;
class UPanelWidget;

// 동료 목소리 음량 목록(일시정지·인원 화면에 배치), 참가자 변동은 1초마다 확인
UCLASS(Abstract)
class DIGETHER_API UDigetherPlayerVoiceListWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Digether|Voice")
	void RefreshPlayers();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> Panel_Players;

	UPROPERTY(EditDefaultsOnly, Category = "Digether|Voice")
	TSubclassOf<UDigetherPlayerVoiceRowWidget> RowClass;

private:
	FTimerHandle RefreshTimer;
};
