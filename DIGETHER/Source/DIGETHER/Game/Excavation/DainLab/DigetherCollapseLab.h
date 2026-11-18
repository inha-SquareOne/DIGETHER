#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/Excavation/Dain/DigetherDiggableSoil.h"
#include "DigetherCollapseLab.generated.h"

class ADigetherCeilingProbe;
class APawn;
class ADigetherExcavationLabCharacter;
class UBoxComponent;
class UInstancedStaticMeshComponent;
class UMaterialBillboardComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// 개인 시험용 천장 낙하·퇴적. 수동 입력과 여진 모두 같은 적용 경로 사용
UCLASS()
class DIGETHER_API ADigetherCollapseLab : public AActor
{
	GENERATED_BODY()

public:
	ADigetherCollapseLab();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Digether|Collapse Lab")
	TObjectPtr<ADigetherCeilingProbe> CeilingProbe;

	// 제거 영역은 새 굴착 천장과 겹쳐야 함. 두 영역의 크기·양은 승인 전 시험값
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|Collapse Lab")
	TObjectPtr<UBoxComponent> CeilingRemoval;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|Collapse Lab")
	TObjectPtr<UBoxComponent> Deposit;

	// 마지막 성공 기록. 재굴착 후 다음 수동 요청 허용, 같은 낙하의 중복 요청은 거절
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Digether|Collapse Lab")
	bool bCollapsed = false;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Digether|Collapse Lab")
	int32 RemovedSamples = 0;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Digether|Collapse Lab")
	int32 DepositedSamples = 0;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Digether|Collapse Lab")
	FText LastStatus;

	// 개인 시험 연출값. S09의 3~10초 예고 시간이 아니라 실제 낙하·잔류 먼지 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digether|Collapse Lab", meta = (ClampMin = "0.1", ClampMax = "5", Units = "s"))
	float FallSeconds = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digether|Collapse Lab", meta = (ClampMin = "0.1", ClampMax = "5", Units = "s"))
	float DustSeconds = 2.0f;
	UPROPERTY(EditAnywhere, Category = "Digether|Collapse Lab")
	TObjectPtr<UMaterialInterface> DustMaterial;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Digether|Collapse Lab")
	int32 CollapseCount = 0;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Digether|Collapse Lab")
	int32 LastLandedStage = 0;
	// 마지막으로 착지에 성공한 붕괴의 후보. 이후 출입은 반영하지 않는다.
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Digether|Collapse Lab")
	TArray<TWeakObjectPtr<APawn>> LastBurialTargets;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Digether|Collapse Lab")
	bool bFalling = false;

	// 상세 패널의 즉시 낙하 시험용. 자동 여진은 예고 때 확정한 단계를 전달한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digether|Collapse Lab", meta = (ClampMin="1", ClampMax="3"))
	int32 PreviewStage = 3;

	// 유효 요청만 낙하 시작. true는 적용 성공이 아닌 연출 시작, 완료는 CollapseCount로 확인
	UFUNCTION(BlueprintCallable, Category = "Digether|Collapse Lab")
	bool StartCollapse(int32 Stage = 0);
	// 예고 중에는 플레이어가 피할 수 있다. 매몰 후보는 실제 착지 시점에 수집한다.
	bool GetWarningSurface(TArray<FVector>& OutVertices, int32 Stage);
	// 도착 시점의 즉시 적용 경로. 새 수동 입력은 StartCollapse를 사용
	// 실행 직전 천장·보호 영역 검사 → 천장 감소와 퇴적 증가를 한 후보로 적용
	UFUNCTION(BlueprintCallable, Category = "Digether|Collapse Lab")
	EDigetherSoilEditResult TryCollapse();
	// PIE 상세 패널 버튼. 편집 월드와 Shipping에서는 지형 변경 금지
	UFUNCTION(CallInEditor, Category = "Digether|Collapse Lab")
	void TriggerCollapse();
	void UpdateBurialExposure(ADigetherExcavationLabCharacter& Victim, ADigetherDiggableSoil& Soil, bool bAllowRescue);


protected:
	virtual void BeginPlay() override;

private:
	// 낙하 중 배치 변경과 다른 흙 참조를 감지, 지형 데이터와 분리된 시험 계약
	struct FCollapseContext
	{
		TWeakObjectPtr<ADigetherDiggableSoil> Soil;
		TWeakObjectPtr<UObject> ChunkBuffer;
		FTransform SoilTransform, RemovalTransform, DepositTransform;
		FVector RemovalExtent, DepositExtent;
		FIntVector Cells;
		double Spacing = 0;
	};
	struct FDepositSample
	{
		int32 Index;
		FVector WorldPosition;
	};
	// 이 구간의 착지로 공기에서 흙이 된 표본만 추적한다.
	TArray<int32> DepositSamples;
	int32 FallingStage = 0;
	EDigetherSoilEditResult ApplyLanding(int32 Stage);
	FCollapseContext CaptureContext() const;
	bool MatchesContext(const FCollapseContext& Context) const;
	// 후보만 계산, 확정 밀도/충돌은 변경하지 않음. Applied는 후보 준비 완료 의미
	EDigetherSoilEditResult PrepareCollapse(TArray<uint8>& OutCandidate, FBox& OutChanged, int32& OutRemoved, int32& OutAdded, TArray<FVector>* WarningSurface = nullptr, TArray<FDepositSample>* NewSolids = nullptr, int32 Stage = 3);
	void ShowStatus() const;

	UPROPERTY(VisibleAnywhere, Category = "Digether|Collapse Lab")
	TObjectPtr<UInstancedStaticMeshComponent> FallingSoil;
	UPROPERTY(VisibleAnywhere, Category = "Digether|Collapse Lab")
	TObjectPtr<UMaterialBillboardComponent> Dust;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DustInstance;
	FCollapseContext FallingContext, LastContext;
	uint32 FallingRevision = 0, LastCollapseRevision = 0;
	struct FFallingClod
	{
		FVector Source, Landing, Scale, Spin;
		FQuat Rotation;
		float Release, Duration;
	};
	TArray<FFallingClod> Clods;
	void SpawnFallingClods(const TArray<FVector>& CeilingVertices);
	void AnimateFallingClods(float Alpha);
	float FallElapsed = 0, DustElapsed = 0;
	float ActiveFallSeconds = 1, ActiveDustSeconds = 2;
	bool bDustActive = false;
};
