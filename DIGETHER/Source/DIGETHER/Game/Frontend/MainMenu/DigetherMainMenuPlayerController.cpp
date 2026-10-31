#include "Game/Frontend/MainMenu/DigetherMainMenuPlayerController.h"

#include "CommonActivatableWidget.h"
#include "Game/Frontend/DigetherFrontendSettings.h"
#include "Game/Online/DigetherSessionSubsystem.h"

TSoftClassPtr<UCommonActivatableWidget> ADigetherMainMenuPlayerController::GetScreenClass() const
{
	return UDigetherFrontendSettings::Get()->MainMenuScreenClass;
}

void ADigetherMainMenuPlayerController::RoomHost(const FString& RoomCode)
{
	if (UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this))
	{
		Session->HostRoom(FString(), true, RoomCode);
	}
}

void ADigetherMainMenuPlayerController::RoomJoin(const FString& RoomCode, float TimeoutSeconds)
{
	if (UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this))
	{
		Session->JoinByCode(RoomCode, TimeoutSeconds);
	}
}
