#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Hub/DigetherHubTypes.h"
#include "DigetherHubInteractionComponent.generated.h"

class ADigetherElevatorLeverActor;
class ADigetherGameplayPlayerController;
class ADigetherHubFacilityActor;
class UDigetherHubFacilityWidget;

// 소유 PlayerController의 로컬 상호작용 상태: 시선 대상·E 입력 레버 유지·거점 시설 패널
// 서버 판정·RPC·입력 에셋·위젯 클래스는 PlayerController 담당, 복제 없음
UCLASS(ClassGroup=(Digether))
class DIGETHER_API UDigetherHubInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherHubInteractionComponent();

	void HandleInteractStarted();
	// Completed·Canceled 입력과 패널 종료의 공통 레버 해제
	void HandleInteractReleased();
	// 로컬 표시·입력용 후보 선택, 실제 상호작용은 서버 재검사
	AActor* FindInteractionTarget() const;
	FText GetInteractionHint() const;

	void StartHubInteraction(ADigetherHubFacilityActor* Facility);
	void SubmitHubOption(int32 OptionIndex);
	void CloseHubInteraction();
	// Client_HubFacilityResult 도착 결과 적용
	void ApplyHubFacilityResult(const FDigetherHubView& View);

	bool IsHubPanelOpen() const { return bHubInputModeApplied; }

private:
	ADigetherGameplayPlayerController* GetController() const;
	void SendHubRequest(EDigetherHubAction Action, FName OptionId = NAME_None);

	UPROPERTY(Transient)
	TObjectPtr<UDigetherHubFacilityWidget> HubFacilityWidget;
	TWeakObjectPtr<ADigetherHubFacilityActor> ActiveHubFacility;
	TWeakObjectPtr<ADigetherElevatorLeverActor> HeldInteractionLever;
	FDigetherHubView CurrentHubView;
	FGuid ActiveHubRequestId;
	bool bHubInputModeApplied = false;
	bool bHubPreviousCursorVisible = false;
	bool bHubRequestPending = false;
	EDigetherHubAction LastHubAction = EDigetherHubAction::Inspect;
	FTimerHandle HubViewRefreshTimer;
};
