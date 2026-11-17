#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DigetherInteractable.generated.h"

class APawn;

// E 조준 상호작용 종류, 값이 작을수록 우선함(기획: 구조 → 줍기 → 수리 → 보강)
// 조준 범위에 여러 대상이 겹치면 이 순서로 하나만 고름
UENUM()
enum class EDigetherInteractionKind : uint8
{
	Rescue = 0,
	Pickup = 1,
	Repair = 2,
	Reinforce = 3,
};

UINTERFACE(MinimalAPI)
class UDigetherInteractable : public UInterface
{
	GENERATED_BODY()
};

// E로 상호작용할 수 있는 월드 대상의 공통 인터페이스
// 조준·요청은 상호작용 컴포넌트(UDigetherInteractionComponent)가 하고 서버가 거리·상태를 다시 검사한 뒤 Interact를 부름
// 기존 거점 레버·시설 상호작용(UDigetherHubInteractionComponent)은 아직 이 인터페이스로 옮기지 않았음
class DIGETHER_API IDigetherInteractable
{
	GENERATED_BODY()

public:
	virtual EDigetherInteractionKind GetInteractionKind() const = 0;

	// 지금 이 플레이어가 상호작용할 수 있는지(화면 안내와 서버 재검사에 같이 씀)
	virtual bool CanInteract(const APawn* Interactor) const = 0;

	// 화면에 보일 안내 문구(예: "줍기: 곡괭이")
	virtual FText GetInteractionPrompt() const = 0;

	// 서버 전용: 상호작용을 확정함, 호출 전에 서버가 CanInteract·거리를 이미 확인함
	virtual void Interact(APawn* Interactor) = 0;
};
