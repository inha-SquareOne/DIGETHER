#pragma once

#include "CoreMinimal.h"

// 순수 계산 데이터, Actor·입력·네트워크·물리 처리와 분리
namespace DigetherSoil
{
	struct FDigetherSoilGrid
	{
		// 셀 개수와 표본 간격(cm), 표본 좌표는 0~Cells 양끝 포함
		FIntVector Cells = FIntVector(8, 8, 8);
		double SpacingCm = 50.0;
		// 축·표본 총수·유한 간격의 CPU 입력 제한 충족 여부
		bool IsValid() const;
		// 유효 격자의 표본 수, 잘못된 격자는 0
		int32 GetSampleCount() const;
		// 유효 표본 좌표 입력 → X 우선 1차원 인덱스, 범위 검사는 호출부 책임
		int32 GetSampleIndex(int32 X, int32 Y, int32 Z) const;
		// 표본 좌표 입력 → Actor 로컬 위치(cm)
		FVector GetSamplePosition(int32 X, int32 Y, int32 Z) const;
	};

	// PMC 전달용 계산 결과, UObject·충돌 데이터 소유 없음
	struct FDigetherSoilSurface
	{
		// 로컬 정점(cm)과 삼각형당 인덱스 3개, 현재 면별 정점 분리
		TArray<FVector> Vertices;
		TArray<int32> Indices;
		// 정점별 공기 방향 음영 노멀과 XY 투영 UV, 충돌 형상과 별개
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
	};

	// 밀도 계산 결과, 실제 메시·충돌 적용 결과와 분리
	enum class EDigetherSoilCarveResult : uint8
	{
		InvalidInput,
		NoChange,
		Changed
	};

	// Air/Solid는 정상 무표면, Surface는 경계 존재, Failed는 적용 금지
	enum class EDigetherSoilRangeState : uint8
	{
		Failed,
		Air,
		Solid,
		Surface
	};
	// 전체 밀도·[CellMin, CellMax) 입력 → 표면 분류와 OutSurface, 실패 시 출력 보존
	// 전역 좌표 사용, 내부 청크 경계에 인공 공기층 추가 금지
	EDigetherSoilRangeState BuildSurfaceRange(
		const FDigetherSoilGrid& Grid,
		const TArray<uint8>& Density,
		const FIntVector& CellMin,
		const FIntVector& CellMax,
		FDigetherSoilSurface& OutSurface);

	// 외곽 공기층은 변경 금지, 표면 기준값은 128
	// Grid 입력 → 초기 밀도 출력, 실패 시 OutDensity 보존
	bool Initialize(const FDigetherSoilGrid& Grid, TArray<uint8>& OutDensity);
	// 전체 밀도 입력 → OutSurface, 정상 빈 결과도 true, 실패 시 출력 보존
	bool BuildSurface(const FDigetherSoilGrid& Grid, const TArray<uint8>& Density, FDigetherSoilSurface& OutSurface);
	// 로컬 중심·월드 반경(cm)·보호 상자·양수 스케일 입력 → Density 감소와 로컬 변경 범위 출력
	// 로컬 보호 영역에 한 셀 여유 적용
	// OutChangedBounds는 이웃 표면을 포함한 변경 범위이며 붕괴 구역 판정이 아님
	EDigetherSoilCarveResult Carve(
		const FDigetherSoilGrid& Grid,
		TArray<uint8>& Density,
		const FVector& LocalPoint,
		double Radius,
		const TArray<FBox>& ProtectedBoxes,
		FBox& OutChangedBounds,
		const FVector& WorldScale = FVector::OneVector);
	// Transform·월드점 입력 → OutLocal(cm), 유효 변환일 때만 true
	// 이동·회전·양수 비균일 스케일 허용, 0과 음수 반전은 거절
	bool WorldToLocal(const FTransform& Transform, const FVector& WorldPoint, FVector& OutLocal);
} // namespace DigetherSoil
