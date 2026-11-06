#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Run/DigetherRunResult.h"
#include "DigetherPlayerScreenComponent.generated.h"

class ADigetherGameplayPlayerController;
class ADigetherGameplayPlayerState;
class UDigetherGameOverCreditsWidget;
class UDigetherRunHudWidget;
class UDigetherRunResultWidget;

// 소유 PlayerController의 로컬 화면: HUD·런 결과·결과 크레딧 위젯 수명과 결과 화면 커서·입력 모드
// 위젯 클래스는 PlayerController UPROPERTY(BP 기본값) 사용, 복제 없음
UCLASS(ClassGroup=(Digether))
class DIGETHER_API UDigetherPlayerScreenComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherPlayerScreenComponent();

	// 소유 PC BeginPlay에서 호출, 로컬 Controller만 HUD·크레딧 위젯 생성
	void CreatePersistentWidgets();
	// 소유 PC EndPlay에서 호출, 결과 구독 해제·결과 화면 닫기·HUD·크레딧 제거
	void ReleaseWidgets();
	// PlayerState 교체·빙의 시 결과 알림 재구독과 현재 스냅샷 적용
	void RefreshRunResultBinding();
	bool ShowLastRunResult();
	void CloseRunResult();

	bool IsRunResultInputModeApplied() const { return bRunResultInputModeApplied; }

private:
	ADigetherGameplayPlayerController* GetController() const;
	void HandleRunResultChanged(const FDigetherRunResult& Result);

	UPROPERTY(Transient)
	TObjectPtr<UDigetherRunHudWidget> HudWidget;

	UPROPERTY(Transient)
	TObjectPtr<UDigetherRunResultWidget> RunResultWidget;

	UPROPERTY(Transient)
	TObjectPtr<UDigetherGameOverCreditsWidget> GameOverCreditsWidget;

	TWeakObjectPtr<ADigetherGameplayPlayerState> RunResultPlayerState;
	FGuid LastPresentedRunResultId;
	bool bRunResultPreviousCursorVisible = false;
	bool bRunResultInputModeApplied = false;
};
