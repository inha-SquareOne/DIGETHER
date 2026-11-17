#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DigetherInteractionComponent.generated.h"

class APawn;

// 플레이어 Pawn의 E 조준 상호작용: 시선 방향으로 IDigetherInteractable 대상을 찾고, 요청을 서버에 보내 서버가 확정함
// - 조준 대상 선택은 소유 클라이언트의 화면용이고, 서버는 요청받은 대상의 거리·시선 가림·CanInteract를 다시 검사함
// - 겹친 대상은 EDigetherInteractionKind 순서(구조 → 줍기 → 수리 → 보강), 같으면 가까운 쪽
UCLASS(ClassGroup=(Digether))
class DIGETHER_API UDigetherInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 소유 클라이언트: 조준 중인 대상에게 상호작용을 요청함
	void RequestInteract();

	// 소유 클라이언트의 화면 안내용, 대상이 없으면 빈 문구
	FText GetFocusedPrompt() const;
	AActor* GetFocusedTarget() const { return FocusedTarget.Get(); }

protected:
	// 상호작용 가능 거리(눈 위치 기준, cm), 시험값
	UPROPERTY(EditDefaultsOnly, Category="Digether|Interaction", meta=(ClampMin="1"))
	float InteractionRangeCm = 300.f;

	// 조준 허용 반경(cm), 작은 물건을 십자선에 정확히 맞추지 않아도 되게 함
	UPROPERTY(EditDefaultsOnly, Category="Digether|Interaction", meta=(ClampMin="0"))
	float AimRadiusCm = 20.f;

	// 서버가 거리를 검사할 때 지연으로 인한 오차를 봐주는 여유(cm)
	UPROPERTY(EditDefaultsOnly, Category="Digether|Interaction", meta=(ClampMin="0"))
	float ServerRangeToleranceCm = 100.f;

private:
	UFUNCTION(Server, Reliable)
	void Server_Interact(AActor* Target);

	// 눈 위치·시선 방향 기준으로 가장 우선하는 대상을 찾음
	AActor* FindBestTarget() const;

	bool IsOwnerLocallyControlled() const;

	TWeakObjectPtr<AActor> FocusedTarget;
};
