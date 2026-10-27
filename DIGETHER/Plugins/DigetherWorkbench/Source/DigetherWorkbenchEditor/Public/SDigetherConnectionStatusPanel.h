// 'DIGETHER 연결 상태' 패널 위젯. 합성 예제이며 실게임 provider에 연결되지 않은 상태를 그대로 보여준다.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SDigetherConnectionStatusPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDigetherConnectionStatusPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};
