#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Game/Hub/DigetherHubTypes.h"
#include "DigetherHubFacilityWidget.generated.h"

class UButton;
class UTextBlock;

// 소유 Controller의 거래 응답 표시와 WBP 배치 연결
UCLASS(Abstract)
class DIGETHER_API UDigetherHubFacilityWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	void SetView(const FDigetherHubView& View);
	void SetPending();
	void FocusDefaultButton();

protected:
	virtual void NativeOnInitialized() override;
	virtual bool NativeOnHandleBackAction() override;

private:
	// 시설 패널 WBP의 필수 바인딩
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Title;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Status;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Balance;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Detail;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Button_Option1;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Button_Option2;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Button_Option3;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Option1;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Option2;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Option3;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Button_Close;
	UFUNCTION() void HandleOption1();
	UFUNCTION() void HandleOption2();
	UFUNCTION() void HandleOption3();
	UFUNCTION() void HandleClose();
	void Submit(int32 Index);
};
