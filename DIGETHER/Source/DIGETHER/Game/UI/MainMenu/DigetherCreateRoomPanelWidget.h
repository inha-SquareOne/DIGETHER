#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Game/Save/DigetherSaveSlotTypes.h"
#include "Types/SlateEnums.h"
#include "DigetherCreateRoomPanelWidget.generated.h"

class UCheckBox;
class UComboBoxString;
class UEditableTextBox;
class UTextBlock;
class UDigetherButtonWidget;

// 방 만들기 패널(방 이름·공개 여부·진행 슬롯 새 게임/이어하기), 세션 서브시스템에 생성 요청
// 슬롯 위젯은 선택 바인딩, WBP에 없으면 슬롯 0 이어하기(슬롯 도입 전 동작)
UCLASS(Abstract)
class DIGETHER_API UDigetherCreateRoomPanelWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UDigetherCreateRoomPanelWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UEditableTextBox> Input_RoomName;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCheckBox> Check_Public;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_Create;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDigetherButtonWidget> Btn_Close;

	// 진행 슬롯 선택(기획 S01 2행·S30 205행), 항목은 패널 활성화 시 C++이 슬롯 수만큼 생성
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_SaveSlot;
	// 켜면 고른 슬롯 새 게임 초기화, 끄면 이어하기(빈 슬롯은 새 진행)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCheckBox> Check_NewGame;
	// 고른 슬롯 요약(전체·기간 일차, 목표, 저장 시각 또는 비어 있음·읽을 수 없음)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SaveSlotSummary;

private:
	UFUNCTION()
	void HandleSaveSlotChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	void HandleSessionStatusChanged();
	void CreateRoom();
	void RefreshState();
	// 디스크 슬롯 요약 재조회 후 콤보·요약 갱신, 디스크 읽기라 패널 활성화 시에만 호출
	void RefreshSaveSlots();
	void RefreshSaveSlotSummary();
	FDigetherSaveSlotSelection MakeSaveSelection() const;

	FDelegateHandle StatusChangedHandle;
	TArray<FDigetherSaveSlotSummary> SlotSummaries;
};
