#include "DigetherJihunSpineEdgeOccupancy.h"

bool FDigetherJihunSpineEdgeOccupancy::IsRectFree(const FIntPoint& Min, const FIntPoint& Size, const int32 Floor, const int32 FloorSpan) const
{
	for (int32 Z = Floor; Z < Floor + FloorSpan; ++Z)
	{
		for (int32 X = Min.X; X < Min.X + Size.X; ++X)
		{
			for (int32 Y = Min.Y; Y < Min.Y + Size.Y; ++Y)
			{
				if (Cells.Contains(FIntVector(X, Y, Z)))
				{
					return false;
				}
			}
		}
	}
	return true;
}

void FDigetherJihunSpineEdgeOccupancy::MarkRect(const FIntPoint& Min, const FIntPoint& Size, const int32 Floor, const int32 FloorSpan, const int32 Owner)
{
	for (int32 Z = Floor; Z < Floor + FloorSpan; ++Z)
	{
		for (int32 X = Min.X; X < Min.X + Size.X; ++X)
		{
			for (int32 Y = Min.Y; Y < Min.Y + Size.Y; ++Y)
			{
				Cells.Add(FIntVector(X, Y, Z), Owner);
			}
		}
	}
}

int32 FDigetherJihunSpineEdgeOccupancy::GetOwner(const FIntVector& Cell) const
{
	const int32* Owner = Cells.Find(Cell);
	return Owner ? *Owner : INDEX_NONE;
}
