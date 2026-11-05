#include "Game/Frontend/MainMenu/DigetherMainMenuGameMode.h"

#include "Game/Frontend/MainMenu/DigetherMainMenuPlayerController.h"

ADigetherMainMenuGameMode::ADigetherMainMenuGameMode()
{
	PlayerControllerClass = ADigetherMainMenuPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}

void ADigetherMainMenuGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// 메뉴에는 조종할 캐릭터가 없어 Pawn 미생성
}
