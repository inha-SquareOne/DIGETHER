#include "DigetherJihunGridGraphTypes.h"

namespace DigetherJihunGridGraph
{
	namespace
	{
		int32 NormalizeTurns(const int32 QuarterTurns)
		{
			return ((QuarterTurns % 4) + 4) % 4;
		}
	}

	EDigetherJihunGridGraphDir RotateDir(const EDigetherJihunGridGraphDir Dir, const int32 QuarterTurns)
	{
		return static_cast<EDigetherJihunGridGraphDir>((static_cast<int32>(Dir) + NormalizeTurns(QuarterTurns)) % 4);
	}

	EDigetherJihunGridGraphDir OppositeDir(const EDigetherJihunGridGraphDir Dir)
	{
		return RotateDir(Dir, 2);
	}

	FIntPoint DirStep(const EDigetherJihunGridGraphDir Dir)
	{
		switch (Dir)
		{
		case EDigetherJihunGridGraphDir::East:
			return FIntPoint(1, 0);
		case EDigetherJihunGridGraphDir::North:
			return FIntPoint(0, 1);
		case EDigetherJihunGridGraphDir::West:
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

	void GetPlacementRect(
		const FDigetherJihunGridGraphModuleSpec& Spec,
		const FDigetherJihunGridGraphPlacement& Placement,
		FIntPoint& OutMin,
		FIntPoint& OutSize)
	{
		OutMin = Placement.PivotUnits + RotatedMinOffset(Spec.SizeUnits, Placement.QuarterTurns);
		OutSize = RotatedSize(Spec.SizeUnits, Placement.QuarterTurns);
	}

	void GetWorldEdges(
		const FDigetherJihunGridGraphModuleSpec& Spec,
		const FDigetherJihunGridGraphPlacement& Placement,
		const int32 UnitSizeCm,
		TArray<FDigetherJihunGridGraphWorldEdge>& OutEdges)
	{
		const FIntPoint SizeCm = Spec.SizeUnits * UnitSizeCm;
		const FIntPoint PivotCm = Placement.PivotUnits * UnitSizeCm;
		for (const FDigetherJihunGridGraphEdge& Edge : Spec.Edges)
		{
			FIntPoint A;
			FIntPoint B;
			switch (Edge.Side)
			{
			case EDigetherJihunGridGraphDir::North:
				A = FIntPoint(Edge.StartCm, SizeCm.Y);
				B = FIntPoint(Edge.EndCm, SizeCm.Y);
				break;
			case EDigetherJihunGridGraphDir::South:
				A = FIntPoint(Edge.StartCm, 0);
				B = FIntPoint(Edge.EndCm, 0);
				break;
			case EDigetherJihunGridGraphDir::West:
				A = FIntPoint(0, Edge.StartCm);
				B = FIntPoint(0, Edge.EndCm);
				break;
			default:
				A = FIntPoint(SizeCm.X, Edge.StartCm);
				B = FIntPoint(SizeCm.X, Edge.EndCm);
				break;
			}
			A = PivotCm + RotatePointCm(A, Placement.QuarterTurns);
			B = PivotCm + RotatePointCm(B, Placement.QuarterTurns);

			FDigetherJihunGridGraphWorldEdge& World = OutEdges.AddDefaulted_GetRef();
			World.Floor = Placement.Floor + Edge.FloorOffset;
			World.Dir = RotateDir(Edge.Side, Placement.QuarterTurns);
			World.Type = Edge.Type;
			const bool bAlongX = World.Dir == EDigetherJihunGridGraphDir::North || World.Dir == EDigetherJihunGridGraphDir::South;
			World.LineCm = bAlongX ? A.Y : A.X;
			World.StartCm = bAlongX ? FMath::Min(A.X, B.X) : FMath::Min(A.Y, B.Y);
			World.EndCm = bAlongX ? FMath::Max(A.X, B.X) : FMath::Max(A.Y, B.Y);
		}
	}
}
