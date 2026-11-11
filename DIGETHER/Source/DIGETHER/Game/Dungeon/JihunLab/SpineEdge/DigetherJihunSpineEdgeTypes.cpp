#include "DigetherJihunSpineEdgeTypes.h"

namespace DigetherJihunSpineEdge
{
	namespace
	{
		int32 NormalizeTurns(const int32 QuarterTurns)
		{
			return ((QuarterTurns % 4) + 4) % 4;
		}
	}

	EDigetherJihunSpineEdgeDir RotateDir(const EDigetherJihunSpineEdgeDir Dir, const int32 QuarterTurns)
	{
		return static_cast<EDigetherJihunSpineEdgeDir>((static_cast<int32>(Dir) + NormalizeTurns(QuarterTurns)) % 4);
	}

	EDigetherJihunSpineEdgeDir OppositeDir(const EDigetherJihunSpineEdgeDir Dir)
	{
		return RotateDir(Dir, 2);
	}

	FIntPoint DirStep(const EDigetherJihunSpineEdgeDir Dir)
	{
		switch (Dir)
		{
		case EDigetherJihunSpineEdgeDir::East:
			return FIntPoint(1, 0);
		case EDigetherJihunSpineEdgeDir::North:
			return FIntPoint(0, 1);
		case EDigetherJihunSpineEdgeDir::West:
			return FIntPoint(-1, 0);
		default:
			return FIntPoint(0, -1);
		}
	}

	FIntPoint RotateCell(const FIntPoint& LocalCell, const int32 QuarterTurns)
	{
		const int32 I = LocalCell.X;
		const int32 J = LocalCell.Y;
		switch (NormalizeTurns(QuarterTurns))
		{
		case 1:
			return FIntPoint(-J - 1, I);
		case 2:
			return FIntPoint(-I - 1, -J - 1);
		case 3:
			return FIntPoint(J, -I - 1);
		default:
			return LocalCell;
		}
	}

	FIntPoint RotatePointCm(const FIntPoint& LocalCm, const int32 QuarterTurns)
	{
		const int32 X = LocalCm.X;
		const int32 Y = LocalCm.Y;
		switch (NormalizeTurns(QuarterTurns))
		{
		case 1:
			return FIntPoint(-Y, X);
		case 2:
			return FIntPoint(-X, -Y);
		case 3:
			return FIntPoint(Y, -X);
		default:
			return LocalCm;
		}
	}

	FIntPoint RotatedMinOffset(const FIntPoint& SizeUnits, const int32 QuarterTurns)
	{
		switch (NormalizeTurns(QuarterTurns))
		{
		case 1:
			return FIntPoint(-SizeUnits.Y, 0);
		case 2:
			return FIntPoint(-SizeUnits.X, -SizeUnits.Y);
		case 3:
			return FIntPoint(0, -SizeUnits.X);
		default:
			return FIntPoint::ZeroValue;
		}
	}

	FIntPoint RotatedSize(const FIntPoint& SizeUnits, const int32 QuarterTurns)
	{
		return NormalizeTurns(QuarterTurns) % 2 == 0 ? SizeUnits : FIntPoint(SizeUnits.Y, SizeUnits.X);
	}
}
