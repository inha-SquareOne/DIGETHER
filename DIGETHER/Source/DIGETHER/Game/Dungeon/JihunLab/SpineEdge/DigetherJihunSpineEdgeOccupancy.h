#pragma once

#include "CoreMinimal.h"

// 200cm 칸 점유 기록. 키 = (X, Y, 층), 값 = 배치 인덱스
class DIGETHER_API FDigetherJihunSpineEdgeOccupancy
{
public:
	bool IsRectFree(const FIntPoint& Min, const FIntPoint& Size, int32 Floor, int32 FloorSpan = 1) const;
	void MarkRect(const FIntPoint& Min, const FIntPoint& Size, int32 Floor, int32 FloorSpan, int32 Owner);


	int32 GetOwner(const FIntVector& Cell) const;
	int32 Num() const { return Cells.Num(); }
	void Reset() { Cells.Reset(); }

private:
	TMap<FIntVector, int32> Cells;
};
