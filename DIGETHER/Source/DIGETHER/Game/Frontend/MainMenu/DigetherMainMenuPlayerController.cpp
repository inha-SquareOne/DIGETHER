#include "Game/Frontend/MainMenu/DigetherMainMenuPlayerController.h"

#include "CommonActivatableWidget.h"
#include "Game/Frontend/DigetherFrontendSettings.h"
#include "Game/Online/DigetherSessionSubsystem.h"
#include "Game/Save/DigetherSaveRules.h"
#include "Game/Save/DigetherSaveTuning.h"

TSoftClassPtr<UCommonActivatableWidget> ADigetherMainMenuPlayerController::GetScreenClass() const
{
	return UDigetherFrontendSettings::Get()->MainMenuScreenClass;
}

// UHT가 생성한 Exec 진입점이 정의를 요구하므로 함수는 남기고 본문만 출시 빌드에서 뺀다.
// 방 코드 정규화는 HostRoom·JoinByCode 입구가 화면 입력과 같은 경로로 처리한다.
void ADigetherMainMenuPlayerController::RoomHost(const FString& RoomCode, const FString& SaveOption)
{
#if !UE_BUILD_SHIPPING
	FDigetherSaveSlotSelection Selection;
	if (!DigetherSaveRules::TryParseSlotOption(SaveOption, Selection))
	{
		// 개발 콘솔 안내라 String Table 문구를 쓰지 않는다.
		ClientMessage(FString::Printf(TEXT("RoomHost 저장 옵션 '%s'를 읽지 못했습니다. new·newN·load·loadN (N=0..%d)"),
			*SaveOption, DigetherSaveTuning::MaxProgressSlots - 1));
		return;
	}
	if (UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this))
	{
		Session->HostRoom(FString(), true, RoomCode, Selection);
	}
#endif
}

void ADigetherMainMenuPlayerController::RoomJoin(const FString& RoomCode, float TimeoutSeconds)
{
#if !UE_BUILD_SHIPPING
	if (UDigetherSessionSubsystem* Session = UDigetherSessionSubsystem::Get(this))
	{
		Session->JoinByCode(RoomCode, TimeoutSeconds);
	}
#endif
}
