#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "DigetherRoomEntryWidget.generated.h"

class UCommonTextBlock;

// 방 목록 한 줄, 선택·클릭은 CommonListView 담당, 선택 모양은 버튼 스타일 Selected 값
UCLASS(Abstract)
class DIGETHER_API UDigetherRoomEntryWidget : public UCommonButtonBase, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UDigetherRoomEntryWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_Name;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_Host;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_Players;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_State;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> Text_Ping;
};
