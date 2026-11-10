#include "Game/Framework/DigetherGameplayPlayerController.h"

#include "Game/UI/DigetherPlayerScreenComponent.h"
#include "Game/UI/Pause/DigetherPauseMenuWidget.h"

namespace
{
	// 거점 시설 패널(100)·런 결과 화면 위
	constexpr int32 PauseMenuZOrder = 200;
}

void ADigetherGameplayPlayerController::HandlePausePressed()
{
	// 메뉴가 열린 동안은 UI 전용 입력이라 여기 오지 않음, 닫기는 메뉴의 뒤로 동작
	if (!IsPauseMenuOpen())
	{
		OpenPauseMenu();
	}
}

bool ADigetherGameplayPlayerController::IsPauseMenuOpen() const
{
	return IsValid(PauseMenuWidget);
}

void ADigetherGameplayPlayerController::OpenPauseMenu()
{
	if (!IsLocalController() || !PauseMenuWidgetClass || IsPauseMenuOpen())
	{
		return;
	}
	// 메뉴 진입 전 레버·붓기 유지 입력과 시설 패널 정리
	ReleaseHeldInputsAndHubPanel();
	UDigetherPauseMenuWidget* Widget = CreateWidget<UDigetherPauseMenuWidget>(this, PauseMenuWidgetClass);
	if (!IsValid(Widget) || !Widget->AddToPlayerScreen(PauseMenuZOrder))
	{
		return;
	}
	PauseMenuWidget = Widget;
	bPauseMenuPreviousCursorVisible = bShowMouseCursor;
	SetShowMouseCursor(true);
	Widget->OnDeactivated().AddUObject(this, &ThisClass::HandlePauseMenuDeactivated);
	Widget->ActivateWidget();
	// 루트 활성화 위젯은 포커스 불가, 메뉴가 지정한 기본 버튼에 포커스
	FInputModeUIOnly Mode;
	if (UWidget* FocusTarget = Widget->GetDesiredFocusTarget())
	{
		Mode.SetWidgetToFocus(FocusTarget->TakeWidget());
	}
	SetInputMode(Mode);
}

void ADigetherGameplayPlayerController::HandlePauseMenuDeactivated()
{
	ClosePauseMenu();
}

void ADigetherGameplayPlayerController::ClosePauseMenu()
{
	if (!IsPauseMenuOpen())
	{
		return;
	}
	// 비활성화 알림 재진입 방지용으로 먼저 비움
	UDigetherPauseMenuWidget* Widget = PauseMenuWidget;
	PauseMenuWidget = nullptr;
	Widget->OnDeactivated().RemoveAll(this);
	if (Widget->IsActivated())
	{
		Widget->DeactivateWidget();
	}
	Widget->RemoveFromParent();
	if (!IsLocalController())
	{
		return;
	}
	// 메뉴가 열린 동안 결과 화면이 떴으면 결과 화면의 입력 모드 유지
	if (PlayerScreenComponent && PlayerScreenComponent->IsRunResultInputModeApplied())
	{
		return;
	}
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(bPauseMenuPreviousCursorVisible);
}
