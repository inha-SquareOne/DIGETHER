#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "DigetherPlayerVoiceRowWidget.generated.h"

class APlayerState;
class UCheckBox;
class UCommonTextBlock;
class USlider;
class UDigetherVoiceComponent;

// 동료 1명의 목소리 음량·나만 음소거 한 줄, 값은 바로 적용(이 PC만, 저장 안 함)
UCLASS(Abstract)
class DIGETHER_API UDigetherPlayerVoiceRowWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetPlayer(APlayerState* InPlayer, UDigetherVoiceComponent* InVoice);
	APlayerState* GetPlayer() const { return Player.Get(); }

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_PlayerName;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider> Slider_Volume;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_VolumeValue;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCheckBox> Check_Mute;

private:
	UFUNCTION() void HandleVolumeChanged(float Value);
	UFUNCTION() void HandleMuteChanged(bool bChecked);
	void UpdateValueText();

	TWeakObjectPtr<APlayerState> Player;
	TWeakObjectPtr<UDigetherVoiceComponent> Voice;
};
