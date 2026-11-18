#pragma once

#include "CoreMinimal.h"
#include "DigetherJihunGridGraphTypes.generated.h"

class UWorld;

// SpineEdge와 코드를 공유하지 않는 독립 시험 방식(사용자 결정 2026-11-18)
// 시험 단계라 GameplayTag 대신 UENUM 사용(공유 태그 ini 변경 회피), 본편 이관 시 전환

// 모듈 로컬 기준 방향, yaw 90은 E→N(언리얼 +X→+Y)
UENUM(BlueprintType)
enum class EDigetherJihunGridGraphDir : uint8
{
	East,
	North,
	West,
	South
};

UENUM(BlueprintType)
enum class EDigetherJihunGridGraphRole : uint8
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
enum class EDigetherJihunGridGraphEdgeType : uint8
{
	Open,
	Door,
	Window,
	Wall,
	// 건물 밖(갱도 쪽)으로 나가는 출입구, 이웃 모듈 연결 불필요
	Entrance
};

// 모듈 한 면의 구간. N·S면은 로컬 X, W·E면은 로컬 Y를 따라 cm로 잰다
USTRUCT(BlueprintType)
struct FDigetherJihunGridGraphEdge
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	EDigetherJihunGridGraphDir Side = EDigetherJihunGridGraphDir::North;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	EDigetherJihunGridGraphEdgeType Type = EDigetherJihunGridGraphEdgeType::Open;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	int32 StartCm = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	int32 EndCm = 0;

	// 여러 층을 차지하는 모듈(계단)에서 몇 번째 층의 면인지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	int32 FloorOffset = 0;
};

// 생성기가 쓰는 모듈 한 종류의 정의. 기획 umap은 바꾸지 않고 참조만 함
USTRUCT(BlueprintType)
struct FDigetherJihunGridGraphModuleSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	TSoftObjectPtr<UWorld> Level;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	EDigetherJihunGridGraphRole Role = EDigetherJihunGridGraphRole::Corridor;

	// 로컬 yaw 0 기준 점유 칸 수(X, Y), 피벗은 남서 모서리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	FIntPoint SizeUnits = FIntPoint(3, 3);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph", meta = (ClampMin = "1"))
	int32 FloorSpan = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	TArray<FDigetherJihunGridGraphEdge> Edges;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	// 수동 제외용. 형태 규칙에 맞지 않는 모듈은 꺼 두지 않아도 생성기가 고르지 않음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	bool bEnabled = true;
};

// 생성 규칙 수치. 기획 답변(2026-11-17) 기준, 미답변 항목은 임시 가정
USTRUCT(BlueprintType)
struct FDigetherJihunGridGraphRules
{
	GENERATED_BODY()

	// GleyBox 실측: 피벗이 놓이는 격자 간격
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Grid", meta = (ClampMin = "1"))
	int32 UnitSizeCm = 200;

	// GleyBox 실측: 층 간격
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Grid", meta = (ClampMin = "1"))
	int32 FloorHeightCm = 600;

	// 큰 격자 교차점 수. 모든 층이 같은 격자를 씀
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Grid", meta = (ClampMin = "2"))
	int32 LatticeColumns = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Grid", meta = (ClampMin = "2"))
	int32 LatticeRows = 10;

	// 교차점 사이 복도 모듈 수. 2 = 평행 복도 사이에 교실 한 줄(I_School_Map 시안의 복도-교실-복도), 겹치는 교실 띠는 점유 검사로 거름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Grid", meta = (ClampMin = "2"))
	int32 MinRunModules = 2;

	// 시안은 교실 앞 3칸 세트 뒤 꺾임이 많고 일부 긴 줄(13칸) 있음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Grid", meta = (ClampMin = "2"))
	int32 MaxRunModules = 6;

	// 격자 간격을 교실 깊이(복도 2개)로 고정할 확률. 평행 복도 사이에 교실 한 줄이 끼어 통로 교실(복도-교실-복도)이 생김
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Grid", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ClassroomDepthGapChance = 0.4f;

	// 교실 깊이 간격을 가로지르는 짧은 복도(2칸, 교실을 못 붙여 벽 복도가 됨)를 만들 확률 배율. 시안은 그 자리를 통로 교실이 대신함
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Grid", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DepthGapEdgeChance = 0.25f;

	// 격자 간격 하나에 복도 중간 계단(복도-계단-복도) 자리를 둘 확률. 그 간격의 간선은 위·아래 두 층이 같은 계단을 지남
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Grid", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InlineStairGapChance = 0.25f;

	// 기획: 2층 시작, 아래 1층·위 3층 = 5층
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Floors", meta = (ClampMin = "1"))
	int32 FloorCount = 5;

	// 0부터 셈. 1 = 2층(임시 가정: 로비가 시작 층)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Floors", meta = (ClampMin = "0"))
	int32 LobbyFloor = 1;

	// 맨 아래·맨 위 층을 계단 하나에 짧은 복도만 있는 막다른 층으로
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Floors")
	bool bDeadEndOuterFloors = true;

	// 막다른 층에서 계단 칸 뒤로 이어지는 복도 모듈 수(기획: 1~3개 또는 교실 하나)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Floors", meta = (ClampMin = "0"))
	int32 DeadEndMinModules = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Floors", meta = (ClampMin = "0"))
	int32 DeadEndMaxModules = 3;

	// 일반 층 사이 계단 수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Floors", meta = (ClampMin = "1"))
	int32 MinStairsBetweenFloors = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Floors", meta = (ClampMin = "1"))
	int32 MaxStairsBetweenFloors = 2;

	// 일반 층 하나에 넣을 교차점 수 목표(거리 한도에 걸리면 덜 생김)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Shape", meta = (ClampMin = "2"))
	int32 MinNodesPerFloor = 16;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Shape", meta = (ClampMin = "2"))
	int32 MaxNodesPerFloor = 28;

	// 일반 층(막다른 층 제외) 하나의 최소 모듈 수(가구층 제외, 계단은 아래층에 셈)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Shape", meta = (ClampMin = "0"))
	int32 MinModulesPerMainFloor = 20;

	// 나무 모양 경로에 이웃 교차점끼리 간선을 더해 고리를 만들 확률(거미줄 정도)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Shape", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LoopChance = 0.35f;

	// 기획: T자 복도 층당 0~3개
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Shape", meta = (ClampMin = "0"))
	int32 MaxTeesPerFloor = 3;

	// 간선 하나를 연결 다리(201·202)가 낀 구간으로 바꿀 확률
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Shape", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BridgeChance = 0.1f;

	// 기획: 시작 지점에서 가장 먼 곳까지 가장 빠른 길 약 40모듈(조정 예정)
	// 5층을 계단으로 잇기만 해도 약 40이 들어 48 이하로는 전체 약 250에서 멈춤(2026-11-18 측정), 임시로 60까지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Distance", meta = (ClampMin = "1"))
	int32 MinFarthestModules = 36;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Distance", meta = (ClampMin = "1"))
	int32 MaxFarthestModules = 60;

	// 기획: 전체 약 500(가구층 제외 모듈 수), 추후 조정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Distance", meta = (ClampMin = "1"))
	int32 MinTotalModules = 250;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Distance", meta = (ClampMin = "1"))
	int32 MaxTotalModules = 800;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Rooms", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ClassroomChance = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Rooms", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InteriorChance = 1.0f;

	// 검증 실패 시 Seed+1로 다시 시도하는 최대 횟수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Retry", meta = (ClampMin = "1"))
	int32 MaxAttempts = 20;
};

// 모듈 하나의 배치 결과. 월드 위치 = 원점 + PivotUnits*UnitSizeCm + Floor*FloorHeightCm
USTRUCT(BlueprintType)
struct FDigetherJihunGridGraphPlacement
{
	GENERATED_BODY()

	// 생성에 넘긴 Catalog 배열 인덱스
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	int32 ModuleIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	FIntPoint PivotUnits = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	int32 Floor = 0;

	// yaw = 90 * QuarterTurns
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	int32 QuarterTurns = 0;

	bool operator==(const FDigetherJihunGridGraphPlacement& Other) const
	{
		return ModuleIndex == Other.ModuleIndex && PivotUnits == Other.PivotUnits
			&& Floor == Other.Floor && QuarterTurns == Other.QuarterTurns;
	}
};

USTRUCT(BlueprintType)
struct FDigetherJihunGridGraphLayout
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	bool bSuccess = false;

	// 요청 Seed + 실패한 시도 수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	int32 SeedUsed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	int32 Attempts = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	int32 FloorCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	int32 ClassroomCount = 0;

	// 뒤쪽 열린 끝이 뒤편 복도 문과 이어진 교실(복도 사이 통로)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	int32 ThroughClassroomCount = 0;

	// 복도 중간 계단(복도-계단-복도)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	int32 InlineStairCount = 0;

	// 로비에서 가장 먼 모듈까지 가장 빠른 길의 모듈 수(로비 제외, 가구층 제외)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	int32 FarthestModules = 0;

	// 가구층을 뺀 모듈 수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	int32 TotalModules = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	TArray<FDigetherJihunGridGraphPlacement> Placements;

	// 마지막 실패 이유, 성공 시 비어 있음
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	FString FailureReason;
};

// 배치된 모듈 면 하나를 월드 cm 좌표로 옮긴 것. 연결 검사·디버그 표시용
struct FDigetherJihunGridGraphWorldEdge
{
	int32 Floor = 0;
	// 면이 바라보는 월드 방향
	EDigetherJihunGridGraphDir Dir = EDigetherJihunGridGraphDir::North;
	// N·S면은 월드 Y, W·E면은 월드 X
	int32 LineCm = 0;
	int32 StartCm = 0;
	int32 EndCm = 0;
	EDigetherJihunGridGraphEdgeType Type = EDigetherJihunGridGraphEdgeType::Open;
};

// 회전·좌표 변환 공용 함수. 생성기와 테스트가 함께 씀
namespace DigetherJihunGridGraph
{
	DIGETHER_API EDigetherJihunGridGraphDir RotateDir(EDigetherJihunGridGraphDir Dir, int32 QuarterTurns);
	DIGETHER_API EDigetherJihunGridGraphDir OppositeDir(EDigetherJihunGridGraphDir Dir);
	DIGETHER_API FIntPoint DirStep(EDigetherJihunGridGraphDir Dir);

	// 로컬 칸(X, Y)을 회전 후 피벗 기준 칸으로. 칸 [i,i+1) 구간을 그대로 옮김
	DIGETHER_API FIntPoint RotateCell(const FIntPoint& LocalCell, int32 QuarterTurns);

	// 로컬 점(cm)을 회전 후 피벗 기준 점으로
	DIGETHER_API FIntPoint RotatePointCm(const FIntPoint& LocalCm, int32 QuarterTurns);

	// 회전한 모듈이 차지하는 사각형의 최소 모서리(피벗 기준 칸)
	DIGETHER_API FIntPoint RotatedMinOffset(const FIntPoint& SizeUnits, int32 QuarterTurns);
	DIGETHER_API FIntPoint RotatedSize(const FIntPoint& SizeUnits, int32 QuarterTurns);

	// 배치가 차지하는 칸 사각형(층 범위는 Placement.Floor부터 Spec.FloorSpan개)
	DIGETHER_API void GetPlacementRect(
		const FDigetherJihunGridGraphModuleSpec& Spec,
		const FDigetherJihunGridGraphPlacement& Placement,
		FIntPoint& OutMin,
		FIntPoint& OutSize);

	DIGETHER_API void GetWorldEdges(
		const FDigetherJihunGridGraphModuleSpec& Spec,
		const FDigetherJihunGridGraphPlacement& Placement,
		int32 UnitSizeCm,
		TArray<FDigetherJihunGridGraphWorldEdge>& OutEdges);
}
