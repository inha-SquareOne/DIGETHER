#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DigetherHubVisualActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

// 외형 교체와 기능 기준점·통행 충돌을 분리한 시설별 비주얼 부모
UCLASS(Blueprintable)
class DIGETHER_API ADigetherHubVisualActor : public AActor
{
	GENERATED_BODY()

public:
	ADigetherHubVisualActor();
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Digether|Hub|Visual")
	void RefreshVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Hub|Visual")
	TObjectPtr<USceneComponent> SceneRoot;

	// 아트 위치·회전·크기 보정 전용 계층
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Hub|Visual")
	TObjectPtr<USceneComponent> ArtRoot;

	// 단일 메시·재질 교체 슬롯
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Hub|Visual")
	TObjectPtr<UStaticMeshComponent> ReplacementMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Hub|Visual")
	TObjectPtr<USceneComponent> PlaceholderRoot;

	// 외형 메시와 독립된 통행 판정용 충돌 계층
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Hub|Visual")
	TObjectPtr<USceneComponent> CollisionRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Hub|Visual")
	TObjectPtr<USceneComponent> InteractionAnchor;

	// ArtRoot 하위 조립 아트 사용 시 임시 외형 숨김
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Digether|Hub|Visual")
	bool bUseCustomArt = false;

	// 외형 갱신 시 이동·수정하지 않는 기능 Actor 참조
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Digether|Hub|Visual")
	TWeakObjectPtr<AActor> GameplayActor;

protected:
	virtual void BeginPlay() override;
};
