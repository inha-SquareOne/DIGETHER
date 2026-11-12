#pragma once

#include "CoreMinimal.h"
#include "DigetherJihunSpineEdgeTypes.generated.h"

class UWorld;

// 시험 단계라 GameplayTag 대신 UENUM 사용(공유 태그 ini 변경 회피), 본편 이관 시 전환

// 모듈 로컬 기준 방향, yaw 90은 E→N(언리얼 +X→+Y)
UENUM(BlueprintType)
enum class EDigetherJihunSpineEdgeDir : uint8
{
	East,
	North,
	West,
	South
};

UENUM(BlueprintType)
enum class EDigetherJihunSpineEdgeRole : uint8
{
	Lobby,
	Corridor,
	Classroom,
	ClassroomInterior,
	Stairs,
	Bridge
};

// 지정하지 않은 면 구간은 Wall로 취급
UENUM(BlueprintType)
enum class EDigetherJihunSpineEdgeType : uint8
{
	Open,
	Door,
	Window,
	Wall,
	// 건물 밖으로 나가는 출입구, 이웃 모듈 연결 불필요
	Entrance
};

// 모듈 한 면의 구간. N·S면은 로컬 X, W·E면은 로컬 Y를 따라 cm로 잰다
USTRUCT(BlueprintType)
struct FDigetherJihunSpineEdgeEdge
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge")
	EDigetherJihunSpineEdgeDir Side = EDigetherJihunSpineEdgeDir::North;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge")
	EDigetherJihunSpineEdgeType Type = EDigetherJihunSpineEdgeType::Open;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge")
	int32 StartCm = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge")
	int32 EndCm = 0;

	// 여러 층을 차지하는 모듈(계단)에서 몇 번째 층의 면인지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge")
	int32 FloorOffset = 0;
};

// 생성기가 쓰는 모듈 한 종류의 정의. 기획 umap은 바꾸지 않고 참조만 함
USTRUCT(BlueprintType)
struct FDigetherJihunSpineEdgeModuleSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge")
	TSoftObjectPtr<UWorld> Level;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge")
	EDigetherJihunSpineEdgeRole Role = EDigetherJihunSpineEdgeRole::Corridor;

	// 로컬 yaw 0 기준 점유 칸 수(X, Y), 피벗은 남서 모서리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge")
	FIntPoint SizeUnits = FIntPoint(3, 3);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge", meta = (ClampMin = "1"))
	int32 FloorSpan = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge")
	TArray<FDigetherJihunSpineEdgeEdge> Edges;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	// 수동 제외용. 형태 규칙에 맞지 않는 모듈은 꺼 두지 않아도 생성기가 고르지 않음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge")
	bool bEnabled = true;
};

// 생성 규칙 수치. 모두 시험용 임시값, 정식 기획 확정 전
USTRUCT(BlueprintType)
struct FDigetherJihunSpineEdgeRules
{
	GENERATED_BODY()

	// GleyBox 실측: 피벗이 놓이는 격자 간격
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge|Grid", meta = (ClampMin = "1"))
	int32 UnitSizeCm = 200;

	// GleyBox 실측: 층 간격
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge|Grid", meta = (ClampMin = "1"))
	int32 FloorHeightCm = 600;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge|Shape", meta = (ClampMin = "1"))
	int32 MinFloors = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge|Shape", meta = (ClampMin = "1"))
	int32 MaxFloors = 3;

	// 한 갈래 복도의 요소 수(복도 칸·계단·꺾임 각각 1)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge|Shape", meta = (ClampMin = "1"))
	int32 MinArmElements = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge|Shape", meta = (ClampMin = "1"))
	int32 MaxArmElements = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge|Shape", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CornerChance = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge|Shape", meta = (ClampMin = "0"))
	int32 MaxCornersPerArm = 1;

	// 위층 층계참 양쪽이 모두 비었을 때 양쪽 문을 함께 열 확률, 아니면 한쪽만
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge|Shape", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UpperBothSidesChance = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge|Rooms", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ClassroomChance = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge|Rooms", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InteriorChance = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge|Rooms", meta = (ClampMin = "0"))
	int32 MinClassrooms = 2;

	// 검증 실패 시 Seed+1로 다시 시도하는 최대 횟수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge|Retry", meta = (ClampMin = "1"))
	int32 MaxAttempts = 20;
};

// 모듈 하나의 배치 결과. 월드 위치 = 원점 + PivotUnits*UnitSizeCm + Floor*FloorHeightCm
USTRUCT(BlueprintType)
struct FDigetherJihunSpineEdgePlacement
{
	GENERATED_BODY()

	// 생성에 넘긴 Catalog 배열 인덱스
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge")
	int32 ModuleIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge")
	FIntPoint PivotUnits = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge")
	int32 Floor = 0;

	// yaw = 90 * QuarterTurns
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge")
	int32 QuarterTurns = 0;

	bool operator==(const FDigetherJihunSpineEdgePlacement& Other) const
	{
		return ModuleIndex == Other.ModuleIndex && PivotUnits == Other.PivotUnits
			&& Floor == Other.Floor && QuarterTurns == Other.QuarterTurns;
	}
};

USTRUCT(BlueprintType)
struct FDigetherJihunSpineEdgeLayout
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge")
	bool bSuccess = false;

	// 요청 Seed + 실패한 시도 수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge")
	int32 SeedUsed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge")
	int32 Attempts = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge")
	int32 FloorCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge")
	int32 ClassroomCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge")
	TArray<FDigetherJihunSpineEdgePlacement> Placements;

	// 마지막 실패 이유, 성공 시 비어 있음
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|SpineEdge")
	FString FailureReason;
};

// 회전·좌표 변환 공용 함수. 생성기와 테스트가 함께 씀
namespace DigetherJihunSpineEdge
{
	DIGETHER_API EDigetherJihunSpineEdgeDir RotateDir(EDigetherJihunSpineEdgeDir Dir, int32 QuarterTurns);
	DIGETHER_API EDigetherJihunSpineEdgeDir OppositeDir(EDigetherJihunSpineEdgeDir Dir);
	DIGETHER_API FIntPoint DirStep(EDigetherJihunSpineEdgeDir Dir);

	// 로컬 칸(X, Y)을 회전 후 피벗 기준 칸으로. 칸 [i,i+1) 구간을 그대로 옮김
	DIGETHER_API FIntPoint RotateCell(const FIntPoint& LocalCell, int32 QuarterTurns);

	// 로컬 점(cm)을 회전 후 피벗 기준 점으로
	DIGETHER_API FIntPoint RotatePointCm(const FIntPoint& LocalCm, int32 QuarterTurns);

	// 회전한 모듈이 차지하는 사각형의 최소 모서리(피벗 기준 칸)
	DIGETHER_API FIntPoint RotatedMinOffset(const FIntPoint& SizeUnits, int32 QuarterTurns);
	DIGETHER_API FIntPoint RotatedSize(const FIntPoint& SizeUnits, int32 QuarterTurns);
}
