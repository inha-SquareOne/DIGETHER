#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DigetherCeilingProbe.generated.h"

class ADigetherDiggableSoil;
class UBoxComponent;

// 개인 시험용 표면 관찰자, 밀도·충돌·손상 상태를 변경하지 않음
UCLASS()
class DIGETHER_API ADigetherCeilingProbe : public AActor
{
	GENERATED_BODY()

public:
	ADigetherCeilingProbe();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Digether|Ceiling")
	TObjectPtr<ADigetherDiggableSoil> Soil;

	// 박스 안에 세 꼭짓점이 모두 있는 삼각형만 후보, 경계는 보수적으로 제외
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|Ceiling")
	TObjectPtr<UBoxComponent> Region;

	// 월드 아래 방향과의 최대 각도, 확정 기획 수치 아님
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digether|Ceiling", meta = (ClampMin = "0", ClampMax = "89"))
	float CeilingAngleDegrees = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digether|Ceiling")
	bool bDrawCandidates = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Digether|Ceiling")
	int32 CandidateTriangles = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Digether|Ceiling")
	bool bBaselineReady = false;

	// 초기 Revision 0의 활성 표면만 캡처, 이미 굴착한 상태를 원래 지형으로 오인하지 않음
	UFUNCTION(BlueprintCallable, Category = "Digether|Ceiling")
	bool CaptureBaseline();

	// 확정 활성 메시만 검사, 반환값은 붕괴 가능 판정이 아닌 표시 삼각형 수
	UFUNCTION(BlueprintCallable, Category = "Digether|Ceiling")
	int32 RefreshCandidates();
	// 검사와 선 제출 비용을 분리해 비교, 판정 상태는 변경하지 않음
	void DrawCandidates() const;
	// 예고 시점의 월드 삼각형 복사, 이후 굴착·영역 이동과 독립된 표시용 기록
	void CopyCandidateVertices(TArray<FVector>& OutVertices) const { OutVertices = CandidateVertices; }

private:
	friend class FDigetherCeilingAccelerationTest;
	// 초기 표면의 공간 경계 계층. 판정식은 그대로 두고 먼 삼각형만 제외
	struct FInitialSurfaceNode
	{
		FBox Bounds = FBox(ForceInit);
		int32 First = 0, Count = 0, Left = INDEX_NONE, Right = INDEX_NONE;
	};
	int32 BuildInitialNode(int32 First, int32 Count);
	TArray<FInitialSurfaceNode> InitialNodes;
	// 원래 삼각형 번호 유지: 같은 거리일 때 기존 전체 순회의 첫 면을 선택
	TArray<int32> InitialTriangleOrder;
	TArray<FVector> InitialTriangles;
	TArray<FVector> CandidateVertices;
	TWeakObjectPtr<ADigetherDiggableSoil> BaselineSoil;
	uint32 LastRevision = 0;
	uint32 BaselineDensityHash = 0;
	uint32 LastDensityHash = 0;
	FTransform LastSoilTransform;
	FTransform LastRegionTransform;
	FVector LastExtent = FVector::ZeroVector;
	float LastAngle = -1.f;
	bool WasInsideInitialSoil(const FVector& Point) const;
};
