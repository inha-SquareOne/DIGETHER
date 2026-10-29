#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Prototype/DigetherPrototypeTypes.h"
#include "DigetherPrototypeHUD.generated.h"

// 그레이박스 시험용 HUD. 남은 시간 숫자와 엘리베이터 중량은 표시하지 않는다.
// 서버 RPC 메시지는 소유 Controller만 받으므로 이 HUD는 로컬 소유 Controller/PlayerState만 읽는다.
UCLASS()
class DIGETHER_API ADigetherPrototypeHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	// 부모가 코드 완료 후 BP로 파생해 외형을 바꿀 수 있는 확장 지점.
	UFUNCTION(BlueprintNativeEvent, Category = "Prototype")
	void DrawPrototypeHUD();
	virtual void DrawPrototypeHUD_Implementation();

	UPROPERTY(BlueprintReadOnly, Category = "Prototype")
	FString NoticeLine;

	UPROPERTY(BlueprintReadOnly, Category = "Prototype")
	FString StatusLine;

	UPROPERTY(BlueprintReadOnly, Category = "Prototype")
	FString PlayerLine;

	UPROPERTY(BlueprintReadOnly, Category = "Prototype")
	FString TargetLine;

	// 로컬 Controller/PlayerState가 받은 개인 알림.
	UPROPERTY(BlueprintReadOnly, Category = "Prototype")
	FString MessageLine;

	// GameState 공개 메시지(모든 참가자 공통).
	UPROPERTY(BlueprintReadOnly, Category = "Prototype")
	FString PublicMessageLine;

	UPROPERTY(BlueprintReadOnly, Category = "Prototype")
	FString ControlsLine;

	// 승강 상태·진행도(공개 값). 해당 없으면 빈 문자열.
	UPROPERTY(BlueprintReadOnly, Category = "Prototype")
	FString ElevatorLine;

	// 시험 목표 체크리스트(조작 설명과 구분).
	UPROPERTY(BlueprintReadOnly, Category = "Prototype")
	FString ChecklistTitle;

	UPROPERTY(BlueprintReadOnly, Category = "Prototype")
	TArray<FString> ChecklistLines;

	// 로컬 소유자의 유지 상호작용 진행도(0~1). 다른 플레이어 값은 쓰지 않는다.
	UPROPERTY(BlueprintReadOnly, Category = "Prototype")
	float HoldFraction = 0.f;

	// 좌상단 한 줄: 일차 · 목표 · Steam 상태.
	UPROPERTY(BlueprintReadOnly, Category = "Prototype")
	FString CompactStatusLine;

	// 우상단 한 줄: 현재 목표.
	UPROPERTY(BlueprintReadOnly, Category = "Prototype")
	FString ObjectiveLine;

	// 로컬 소유자의 체력·스태미나(0~1). 값이 없으면 막대를 그리지 않는다.
	UPROPERTY(BlueprintReadOnly, Category = "Prototype")
	bool bHasHealth = false;

	UPROPERTY(BlueprintReadOnly, Category = "Prototype")
	float HealthFraction = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Prototype")
	float HealthValue = 100.f;

	UPROPERTY(BlueprintReadOnly, Category = "Prototype")
	bool bHasStamina = false;

	UPROPERTY(BlueprintReadOnly, Category = "Prototype")
	float StaminaFraction = 1.f;

	// 체력 막대의 분모(표시용 시험값). 게임 규칙 값이 아니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prototype")
	float DisplayMaxHealth = 100.f;

	// 선택 슬롯 1~5(5는 히든).
	UPROPERTY(BlueprintReadOnly, Category = "Prototype")
	int32 SelectedSlotIndex = 1;

	// F1로 접고 펴는 상세 도움말·체크리스트.
	UPROPERTY(BlueprintReadOnly, Category = "Prototype")
	bool bShowHelpPanel = false;

private:
	void RefreshDisplayText();

	// 일반 슬롯 4칸의 표시 문자열(빈 칸은 빈 문자열). 소유 Controller의 개인 데이터만 쓴다.
	TArray<FString> SlotTexts;
	bool bHiddenSlotFilled = false;
	bool bHasSlotData = false;
};
