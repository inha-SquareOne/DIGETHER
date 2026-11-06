#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DigetherHubWellVisualComponent.generated.h"

class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;

// 우물 범람·흡입 시험 연출, 서버 단계 이벤트의 시간만 소비·판정과 충돌 변경 없음
UCLASS(ClassGroup=(Digether), meta=(BlueprintSpawnableComponent))
class DIGETHER_API UDigetherHubWellVisualComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherHubWellVisualComponent();

	// 선택 컴포넌트가 모두 없거나 시간이 비유한·음수면 기존 연출을 복원하고 false
	UFUNCTION(BlueprintCallable, Category="Digether|Hub|Well Visual")
	bool StartFlood(float FloodSeconds, float FailMotionSeconds, UStaticMeshComponent* FloodSurface,
		UStaticMeshComponent* SuctionShape, UPointLightComponent* Glow);

	// 서버 흡입 시작 이벤트용, 범람 시작 전이면 false·이미 흡입 중이면 진행 유지
	UFUNCTION(BlueprintCallable, Category="Digether|Hub|Well Visual")
	bool StartSuction();

	UFUNCTION(BlueprintCallable, Category="Digether|Hub|Well Visual")
	void StopVisuals();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	// 아래 값은 기본 Plane·Cone 시험용 임시 비주얼 값, 게임 튜닝·판정 반경 아님
	UPROPERTY(EditDefaultsOnly, Category="Digether|Hub|Well Visual", meta=(ClampMin="0.01"))
	float FloodStartScaleMultiplier = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category="Digether|Hub|Well Visual", meta=(ClampMin="0.01"))
	float FloodEndScaleMultiplier = 18.f;

	UPROPERTY(EditDefaultsOnly, Category="Digether|Hub|Well Visual", meta=(ClampMin="0.0"))
	float FloodRiseCm = 130.f;

	UPROPERTY(EditDefaultsOnly, Category="Digether|Hub|Well Visual")
	FLinearColor FloodGlowColor = FLinearColor(0.02f, 0.8f, 0.65f);

	UPROPERTY(EditDefaultsOnly, Category="Digether|Hub|Well Visual", meta=(ClampMin="0.0"))
	float FloodGlowIntensity = 1800.f;

	UPROPERTY(EditDefaultsOnly, Category="Digether|Hub|Well Visual")
	FLinearColor SuctionGlowColor = FLinearColor(0.04f, 0.3f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category="Digether|Hub|Well Visual", meta=(ClampMin="0.0"))
	float SuctionGlowIntensity = 4000.f;

	UPROPERTY(EditDefaultsOnly, Category="Digether|Hub|Well Visual", meta=(ClampMin="0.0"))
	float SuctionRotationDegreesPerSecond = 135.f;

	UPROPERTY(EditDefaultsOnly, Category="Digether|Hub|Well Visual", meta=(ClampMin="0.0"))
	float SuctionPulseHz = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category="Digether|Hub|Well Visual", meta=(ClampMin="0.0", ClampMax="0.9"))
	float SuctionPulseScale = 0.12f;

	// 회전이 보이도록 대칭 Cone의 Y축만 줄이는 시험용 타원 비율
	UPROPERTY(EditDefaultsOnly, Category="Digether|Hub|Well Visual", meta=(ClampMin="0.01", ClampMax="1.0"))
	float SuctionEllipseRatio = 0.75f;

	UPROPERTY(EditDefaultsOnly, Category="Digether|Hub|Well Visual", meta=(ClampMin="0.01"))
	float SuctionEndScaleMultiplier = 0.5f;

private:
	enum class EVisualPhase : uint8
	{
		Inactive,
		Flood,
		Suction
	};

	struct FSceneSnapshot
	{
		FTransform RelativeTransform = FTransform::Identity;
		bool bVisible = false;
		bool bHiddenInGame = false;

		void Capture(const USceneComponent& Component);
		void Restore(USceneComponent& Component) const;
	};

	void ApplyVisuals();
	bool HasVisualComponents() const;
	// 실제 게임에서는 공개 서버 시계로 재구성, 프레임워크 없는 프리뷰만 Delta 보간
	bool RefreshPublicTiming();

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ActiveFloodSurface;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ActiveSuctionShape;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> ActiveGlow;

	FSceneSnapshot FloodSnapshot;
	FSceneSnapshot SuctionSnapshot;
	FSceneSnapshot GlowSnapshot;
	FColor OriginalGlowColor = FColor::White;
	float OriginalGlowIntensity = 0.f;
	double FloodElapsedSeconds = 0.0;
	double SuctionElapsedSeconds = 0.0;
	float FloodDurationSeconds = 0.f;
	float SuctionDurationSeconds = 0.f;
	bool bUsingPublicTiming = false;
	EVisualPhase Phase = EVisualPhase::Inactive;
};
