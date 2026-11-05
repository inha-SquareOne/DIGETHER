#include "Game/Frontend/DigetherFrontendPlayerController.h"

#include "CommonActivatableWidget.h"
#include "Game/Frontend/DigetherFrontendSettings.h"
#include "Game/UI/Common/DigetherFrontendLayoutWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogDigetherFrontend, Log, All);

void ADigetherFrontendPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController()) return;

	const UDigetherFrontendSettings* Settings = UDigetherFrontendSettings::Get();
	const TSubclassOf<UDigetherFrontendLayoutWidget> LayoutClass = Settings->FrontendLayoutClass.LoadSynchronous();
	const TSubclassOf<UCommonActivatableWidget> ScreenClass = GetScreenClass().LoadSynchronous();
	if (!LayoutClass || !ScreenClass)
	{
		UE_LOG(LogDigetherFrontend, Error, TEXT("프런트엔드 화면 클래스를 불러오지 못했습니다. 프로젝트 설정 > Game > DIGETHER Frontend의 UI 항목을 확인하세요."));
		return;
	}

	Layout = CreateWidget<UDigetherFrontendLayoutWidget>(this, LayoutClass);
	if (!Layout) return;
	Layout->AddToPlayerScreen();
	Layout->PushScreen(ScreenClass);
	SetShowMouseCursor(true);
}

void ADigetherFrontendPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsLocalController())
	{
		if (Layout) Layout->RemoveFromParent();
		Layout = nullptr;
		// 게임 맵에는 프런트엔드 화면이 없으므로 게임 입력으로 되돌린다.
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
	Super::EndPlay(EndPlayReason);
}
