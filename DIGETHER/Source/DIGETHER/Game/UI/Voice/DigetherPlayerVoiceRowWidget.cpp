#include "Game/UI/Voice/DigetherPlayerVoiceRowWidget.h"

#include "CommonTextBlock.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Game/Voice/DigetherVoiceComponent.h"
#include "Game/Voice/DigetherVoiceTypes.h"
#include "GameFramework/PlayerState.h"

void UDigetherPlayerVoiceRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Slider_Volume->SetMinValue(0.f);
	Slider_Volume->SetMaxValue(DigetherVoiceDefaults::MaxPlayerVolume);
	Slider_Volume->OnValueChanged.AddDynamic(this, &ThisClass::HandleVolumeChanged);
	if (Check_Mute)
	{
		Check_Mute->OnCheckStateChanged.AddDynamic(this, &ThisClass::HandleMuteChanged);
	}
}

void UDigetherPlayerVoiceRowWidget::SetPlayer(APlayerState* InPlayer, UDigetherVoiceComponent* InVoice)
{
	Player = InPlayer;
	Voice = InVoice;
	Text_PlayerName->SetText(IsValid(InPlayer) ? FText::FromString(InPlayer->GetPlayerName()) : FText::GetEmpty());
	Slider_Volume->SetValue(InVoice != nullptr ? InVoice->GetPlayerVoiceVolume(InPlayer) : 1.f);
	if (Check_Mute)
	{
		Check_Mute->SetIsChecked(InVoice != nullptr && InVoice->IsPlayerVoiceMuted(InPlayer));
	}
	UpdateValueText();
}

void UDigetherPlayerVoiceRowWidget::HandleVolumeChanged(const float Value)
{
	if (UDigetherVoiceComponent* VoicePtr = Voice.Get())
	{
		VoicePtr->SetPlayerVoiceVolume(Player.Get(), Value);
	}
	UpdateValueText();
}

void UDigetherPlayerVoiceRowWidget::HandleMuteChanged(const bool bChecked)
{
	if (UDigetherVoiceComponent* VoicePtr = Voice.Get())
	{
		VoicePtr->SetPlayerVoiceMuted(Player.Get(), bChecked);
	}
}

void UDigetherPlayerVoiceRowWidget::UpdateValueText()
{
	if (Text_VolumeValue)
	{
		Text_VolumeValue->SetText(FText::AsPercent(Slider_Volume->GetValue()));
	}
}
