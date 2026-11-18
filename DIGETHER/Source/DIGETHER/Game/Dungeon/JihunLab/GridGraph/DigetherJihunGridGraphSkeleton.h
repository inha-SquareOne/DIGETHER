#pragma once

#include "CoreMinimal.h"
#include "DigetherJihunGridGraphTypes.h"

// 큰 격자 위의 층별 연결 그래프. 모듈을 고르기 전 단계라 칸 점유·모듈 면은 다루지 않음
// 좌표: 교차점 = (열, 행), 위치는 칸(unit, 200cm). 복도 칸(block) = 복도 모듈 하나(3칸)
// 계단 폭(4칸)이 복도 칸 배수가 아니라 격자 선 위치는 칸 단위로 둠

// 생성기가 카탈로그에서 읽어 넘기는 크기
struct FDigetherJihunGridGraphSkeletonDims
{
	int32 BlockUnits = 3;
	FIntPoint StairsUnits = FIntPoint(4, 8);
	int32 LobbyBlocks = 4;
	int32 LobbyDepthUnits = 9;
	int32 BridgeBlocks = 2;
	int32 ClassroomDepthUnits = 6;
};

// 교차점 두 개를 잇는 복도 구간
struct FDigetherJihunGridGraphSkeletonEdge
{
	int32 Floor = 0;
	FIntPoint From = FIntPoint::ZeroValue;
	// From → To 방향
	EDigetherJihunGridGraphDir Dir = EDigetherJihunGridGraphDir::East;
	// 교실을 붙이는 쪽. 반대쪽이 외부 창
	EDigetherJihunGridGraphDir RoomSide = EDigetherJihunGridGraphDir::North;
	// From 다음 복도 칸부터 센 다리 시작 위치, 없으면 INDEX_NONE
	int32 BridgeOffset = INDEX_NONE;
	// 복도 중간 계단 앞의 복도 칸 수, 없으면 INDEX_NONE. 위·아래 짝 간선이 같은 값
	int32 StairOffset = INDEX_NONE;
	// 짝 간선 중 계단 모듈을 내놓는 아래층 간선
	bool bStairLower = false;
	bool bLobby = false;

	FIntPoint To() const;
};

// 계단 하나. 막다른 교차점 앞(Cap) 또는 복도 중간(Inline)
struct FDigetherJihunGridGraphSkeletonStair
{
	int32 LowerFloor = 0;
	bool bInline = false;
	// Cap: 계단이 붙는 교차점과 방향. Inline: 간선 방향
	FIntPoint Node = FIntPoint::ZeroValue;
	EDigetherJihunGridGraphDir Dir = EDigetherJihunGridGraphDir::East;
	// 층계참 뒤 꺾임 계단이 뻗는 쪽
	EDigetherJihunGridGraphDir BackSide = EDigetherJihunGridGraphDir::North;
	FIntPoint RectMin = FIntPoint::ZeroValue;
	FIntPoint RectSize = FIntPoint::ZeroValue;
};

// 막다른 층의 짧은 복도. 계단 칸에서 Dir 쪽으로 Modules개
struct FDigetherJihunGridGraphSkeletonStub
{
	int32 Floor = 0;
	FIntPoint Node = FIntPoint::ZeroValue;
	EDigetherJihunGridGraphDir Dir = EDigetherJihunGridGraphDir::East;
	EDigetherJihunGridGraphDir RoomSide = EDigetherJihunGridGraphDir::North;
	int32 Modules = 0;
};

struct FDigetherJihunGridGraphSkeletonFloor
{
	bool bDeadEnd = false;
	// 추가된 순서. 반복 순서를 Seed마다 고정하려고 TMap 대신 배열로 순회
	TArray<FIntPoint> Nodes;
	// 비트 = 방향, 격자 간선과 막다른 층 짧은 복도만(막다른 끝 계단 제외)
	TMap<FIntPoint, uint8> Masks;
	// 로비에서 그 교차점 칸까지 모듈 수 상한 추정(고리로 더 짧아질 수 있음)
	TMap<FIntPoint, int32> Distance;
	// 막다른 끝 계단(Cap)이 붙은 방향
	TMap<FIntPoint, EDigetherJihunGridGraphDir> StairDirs;
	int32 TeeCount = 0;
};

struct FDigetherJihunGridGraphSkeleton
{
	int32 BlockUnits = 3;
	int32 StairsAlongUnits = 4;
	// 격자 선의 교차점 칸 최소 좌표(칸 단위)
	TArray<int32> ColumnUnits;
	TArray<int32> RowUnits;
	// 간격 i(선 i와 i+1 사이)에 복도 중간 계단이 있는지와, 그 계단 아래층의 짝수·홀수
	TArray<bool> ColumnStairGap;
	TArray<bool> RowStairGap;
	TArray<int32> ColumnStairParity;
	TArray<int32> RowStairParity;
	TArray<FDigetherJihunGridGraphSkeletonFloor> Floors;
	TArray<FDigetherJihunGridGraphSkeletonEdge> Edges;
	TArray<FDigetherJihunGridGraphSkeletonStair> Stairs;
	TArray<FDigetherJihunGridGraphSkeletonStub> Stubs;
	int32 LobbyFloor = 0;
	int32 LobbyEdge = INDEX_NONE;
	// 로비 간선 From 다음 칸부터 센 로비 시작 위치(복도 칸 수)
	int32 LobbyOffset = 0;
	// (층, 교차점, 방향) → Edges 인덱스. 간선 양 끝에서 모두 찾을 수 있음
	TMap<uint64, int32> EdgeLookup;

	bool IsInLattice(const FIntPoint& Node) const;
	FIntPoint NodeUnits(const FIntPoint& Node) const;
	bool IsStairGap(const FIntPoint& From, EDigetherJihunGridGraphDir Dir) const;
	int32 StairParity(const FIntPoint& From, EDigetherJihunGridGraphDir Dir) const;
	// 두 교차점 사이 복도 칸 수(중간 계단 자리 제외)
	int32 RunBlocks(const FIntPoint& From, EDigetherJihunGridGraphDir Dir) const;
	// 간선 안 복도 칸 Index의 최소 좌표. 중간 계단 뒤 칸은 계단 폭만큼 밀림
	FIntPoint RunBlockMin(const FDigetherJihunGridGraphSkeletonEdge& Edge, int32 Index) const;
	int32 FindEdge(int32 Floor, const FIntPoint& Node, EDigetherJihunGridGraphDir Dir) const;
};

namespace DigetherJihunGridGraph
{
	DIGETHER_API uint8 DirBit(EDigetherJihunGridGraphDir Dir);
	DIGETHER_API int32 CountDirs(uint8 Mask);

	// 칸 사각형 계산. 반환 = 복도 칸(최소 좌표 BlockMin)의 뒤쪽(H 반대)·바깥쪽(C 반대) 모서리 점
	DIGETHER_API FIntPoint BlockBackOuterCorner(const FIntPoint& BlockMin, EDigetherJihunGridGraphDir H, EDigetherJihunGridGraphDir C, int32 BlockUnits);
	DIGETHER_API void RunRect(const FIntPoint& Origin, EDigetherJihunGridGraphDir H, EDigetherJihunGridGraphDir C,
		int32 Along0, int32 Along1, int32 Cross0, int32 Cross1, FIntPoint& OutMin, FIntPoint& OutSize);

	// 교차점 칸 앞(H 쪽)에 붙는 막다른 끝 계단 사각형. 층계참이 복도 띠와 같은 줄
	DIGETHER_API void StairRect(const FIntPoint& NodeMin, EDigetherJihunGridGraphDir H, EDigetherJihunGridGraphDir BackSide,
		const FDigetherJihunGridGraphSkeletonDims& Dims, FIntPoint& OutMin, FIntPoint& OutSize);

	// 간선 From 교차점 칸 기준 Offset칸 뒤에서 시작하는 Length칸 구간(복도 띠 + 교실 쪽 Cross)
	DIGETHER_API void EdgeSpanRect(const FDigetherJihunGridGraphSkeleton& Skeleton, const FDigetherJihunGridGraphSkeletonEdge& Edge,
		EDigetherJihunGridGraphDir Side, int32 OffsetUnits, int32 LengthUnits, int32 Cross0, int32 Cross1, FIntPoint& OutMin, FIntPoint& OutSize);

	DIGETHER_API void LobbyRect(const FDigetherJihunGridGraphSkeleton& Skeleton, const FDigetherJihunGridGraphSkeletonDims& Dims,
		FIntPoint& OutMin, FIntPoint& OutSize);

	// 같은 Rules·Dims·Seed면 항상 같은 결과. 실패 시 OutReason에 이유
	DIGETHER_API bool BuildSkeleton(const FDigetherJihunGridGraphRules& Rules, const FDigetherJihunGridGraphSkeletonDims& Dims,
		int32 Seed, FDigetherJihunGridGraphSkeleton& Out, FString& OutReason);
}
