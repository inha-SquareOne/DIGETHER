// 진행 저장·불러오기·새 게임 요청
#include "Game/Framework/DigetherGameplayPlayerController.h"

#include "Game/Framework/DigetherFrameworkLog.h"
#include "Game/Framework/DigetherGameplayGameMode.h"

void ADigetherGameplayPlayerController::RequestManualSave()
{
#if !UE_BUILD_SHIPPING
	if (IsLocalController())
	{
		Server_ManualSave();
	}
#endif
}

void ADigetherGameplayPlayerController::RequestLoadGame()
{
#if !UE_BUILD_SHIPPING
	if (IsLocalController())
	{
		Server_LoadGame();
	}
#endif
}

void ADigetherGameplayPlayerController::RequestNewGame()
{
#if !UE_BUILD_SHIPPING
	if (IsLocalController())
	{
		Server_NewGame();
	}
#endif
}

void ADigetherGameplayPlayerController::Server_ManualSave_Implementation()
{
#if !UE_BUILD_SHIPPING
	// 결과 전송은 GameMode가 한 번, 여기서는 GameMode 도달 전 거절만 전송
	ADigetherGameplayGameMode* GameMode = IsDevServerRpcAllowed() ? ResolveGameplayGameMode() : nullptr;
	if (!IsValid(GameMode))
	{
		Client_SaveResult(false, NSLOCTEXT("DigetherSave", "ManualSaveRejected", "진행 저장을 완료하지 못했습니다."));
		return;
	}
	GameMode->SaveProgress(this);
#endif
}

void ADigetherGameplayPlayerController::Server_LoadGame_Implementation()
{
#if !UE_BUILD_SHIPPING
	ADigetherGameplayGameMode* GameMode = IsDevServerRpcAllowed() ? ResolveGameplayGameMode() : nullptr;
	if (!IsValid(GameMode))
	{
		Client_SaveResult(false, NSLOCTEXT("DigetherSave", "LoadRequestRejected", "저장한 진행을 불러오지 못했습니다."));
		return;
	}
	GameMode->LoadProgress(this);
#endif
}

void ADigetherGameplayPlayerController::Server_NewGame_Implementation()
{
#if !UE_BUILD_SHIPPING
	ADigetherGameplayGameMode* GameMode = IsDevServerRpcAllowed() ? ResolveGameplayGameMode() : nullptr;
	if (!IsValid(GameMode))
	{
		Client_SaveResult(false, NSLOCTEXT("DigetherSave", "NewGameRejected", "새 게임을 시작하지 못했습니다."));
		return;
	}
	GameMode->StartNewGame(this);
#endif
}

void ADigetherGameplayPlayerController::Client_SaveResult_Implementation(const bool bSucceeded, const FText& Message)
{
	UE_LOG(LogDigetherFramework, Display, TEXT("진행 요청 결과: Succeeded=%d Message=%s"),
		bSucceeded, *Message.ToString());
}
