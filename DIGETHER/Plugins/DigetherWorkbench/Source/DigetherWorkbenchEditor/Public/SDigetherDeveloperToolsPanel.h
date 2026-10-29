// 'DIGETHER 개발 도구' 패널. 환경 조회와 엔진 UI 바로가기만 제공하며 설정·패키지를 바꾸지 않는다.
// 조회는 생성과 새로고침 버튼에서만 하고 Tick·타이머·전역 델리게이트를 쓰지 않는다.
#pragma once

#include "CoreMinimal.h"
#include "DigetherDeveloperToolsModel.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;

class SDigetherDeveloperToolsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDigetherDeveloperToolsPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void Rebuild();
	FReply HandleRefreshClicked();
	FReply HandleShortcutClicked(FDigetherShortcut Shortcut);
	FText GetActionMessage() const;

	TSharedPtr<SVerticalBox> ContentBox;
	FText LastActionMessage;
};
