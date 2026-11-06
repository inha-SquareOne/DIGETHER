#pragma once

#include "CoreMinimal.h"
#include "CommonTabListWidgetBase.h"
#include "DigetherTabListWidget.generated.h"

class UHorizontalBox;

// CommonUI 탭 목록, 등록 탭 버튼은 Box_Tabs에 가로 배치, 배치는 WBP_TabList
UCLASS(Abstract)
class DIGETHER_API UDigetherTabListWidget : public UCommonTabListWidgetBase
{
	GENERATED_BODY()

protected:
	virtual void HandleTabCreation_Implementation(FName TabNameID, UCommonButtonBase* TabButton) override;
	virtual void HandleTabRemoval_Implementation(FName TabNameID, UCommonButtonBase* TabButton) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> Box_Tabs;

	// 탭 버튼 사이 간격
	UPROPERTY(EditAnywhere, Category = "Digether|Tabs")
	FMargin TabPadding = FMargin(0.f, 0.f, 12.f, 0.f);
};
