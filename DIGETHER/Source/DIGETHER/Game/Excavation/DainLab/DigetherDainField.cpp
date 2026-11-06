// Adapted from DIGETHER prototype be0cc70, World/DigetherDainField.
// Reused: density encoding, sphere subtraction, marching tetrahedra, UE winding.
// New: finite 200 cm slab with AIR behind it, bounded brush parameter. Restore omitted.
#include "DigetherDainField.h"

namespace DigetherDainField
{
int32 Index(int32 X, int32 Y, int32 Z) { return (Z * SamplesXY + Y) * SamplesXY + X; }
FVector Position(int32 X, int32 Y, int32 Z)
{
	return FVector((X - CellsXY / 2) * Spacing, (Y - CellsXY / 2) * Spacing, Bottom + Z * Spacing);
}
TArray<uint8> InitialDensity()
{
	TArray<uint8> Values;
	Values.SetNumUninitialized(SampleCount);
	for (int32 Z = 0; Z < SamplesZ; ++Z)
	{
		const uint8 Value = static_cast<uint8>(128 + FMath::Clamp(FMath::RoundToInt(FMath::Min(-(Bottom + Z * Spacing), Bottom + Z * Spacing + 200.f) / 2.f), -127, 127));
		for (int32 Y = 0; Y < SamplesXY; ++Y)
			for (int32 X = 0; X < SamplesXY; ++X) { Values[Index(X, Y, Z)] = Value; }
	}
	return Values;
}
bool Carve(TArray<uint8>& Density, const FVector& LocalPoint, float BrushRadius)
{
	if (!FMath::IsFinite(BrushRadius) || BrushRadius < 50.f || BrushRadius > 125.f || Density.Num() != SampleCount || LocalPoint.ContainsNaN() ||
		FMath::Abs(LocalPoint.X) > CellsXY * Spacing / 2 || FMath::Abs(LocalPoint.Y) > CellsXY * Spacing / 2 ||
		LocalPoint.Z < Bottom || LocalPoint.Z > Bottom + CellsZ * Spacing) { return false; }
	// Quantizing the center makes the stored operation independent of floating point noise.
	const FVector Center(FMath::RoundToInt(LocalPoint.X), FMath::RoundToInt(LocalPoint.Y), FMath::RoundToInt(LocalPoint.Z));
	bool bChanged = false;
	for (int32 Z = 1; Z < SamplesZ; ++Z)
		for (int32 Y = 1; Y < CellsXY; ++Y)
			for (int32 X = 1; X < CellsXY; ++X)
			{
				const float Distance = FVector::Distance(Position(X, Y, Z), Center);
				const int32 Encoded = 128 + FMath::Clamp(FMath::RoundToInt((Distance - BrushRadius) / 2.f), -127, 127);
				uint8& Value = Density[Index(X, Y, Z)];
				if (Encoded < Value) { Value = static_cast<uint8>(Encoded); bChanged = true; }
			}
	return bChanged;
}


static void Triangle(FSurface& Surface, FVector A, FVector B, FVector C, const FVector& AirDirection)
{
	FVector Normal = FVector::CrossProduct(B - A, C - A);
	if (Normal.SizeSquared() < 0.0001) { return; }
	if (FVector::DotProduct(Normal, AirDirection) < 0) { Swap(B, C); Normal = -Normal; }
	Normal.Normalize();
	// ProceduralMesh uses the opposite vertex winding to Chaos face normals.
	// Keep the air-facing shading normal while emitting UE's A,C,B order.
	for (const FVector& P : { A, C, B })
	{
		Surface.Indices.Add(Surface.Vertices.Num());
		Surface.Vertices.Add(P);
		Surface.Normals.Add(Normal);
		Surface.UVs.Add(FVector2D(P.X / 200.f, P.Y / 200.f));
	}
}

bool BuildSurface(const TArray<uint8>& Density, FSurface& OutSurface)
{
	if (Density.Num() != SampleCount) { return false; }
	FSurface Result;
	// The same body diagonal and face diagonals are used in every cell.
	static constexpr int32 Corners[8][3] = { {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0}, {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1} };
	static constexpr int32 Tets[6][4] = { {0,1,2,6}, {0,2,3,6}, {0,3,7,6}, {0,7,4,6}, {0,4,5,6}, {0,5,1,6} };
	for (int32 Z = 0; Z < CellsZ; ++Z)
		for (int32 Y = 0; Y < CellsXY; ++Y)
			for (int32 X = 0; X < CellsXY; ++X)
			{
				FVector P[8]; int32 D[8];
				for (int32 I = 0; I < 8; ++I)
				{
					const int32 CX = X + Corners[I][0], CY = Y + Corners[I][1], CZ = Z + Corners[I][2];
					P[I] = Position(CX, CY, CZ); D[I] = static_cast<int32>(Density[Index(CX, CY, CZ)]) - 128;
				}
				for (const auto& Tet : Tets)
				{
					int32 Solid[4], Air[4], NS = 0, NA = 0; FVector SolidMean = FVector::ZeroVector, AirMean = FVector::ZeroVector;
					for (const int32 I : Tet)
					{
						if (D[I] > 0) { Solid[NS++] = I; SolidMean += P[I]; }
						else { Air[NA++] = I; AirMean += P[I]; }
					}
					if (NS == 0 || NA == 0) { continue; }
					const FVector TowardsAir = AirMean / NA - SolidMean / NS;
					const auto Edge = [&P, &D](int32 S, int32 A) { return FMath::Lerp(P[S], P[A], static_cast<double>(D[S]) / (D[S] - D[A])); };
					if (NS == 1) { Triangle(Result, Edge(Solid[0], Air[0]), Edge(Solid[0], Air[1]), Edge(Solid[0], Air[2]), TowardsAir); }
					else if (NA == 1) { Triangle(Result, Edge(Solid[0], Air[0]), Edge(Solid[1], Air[0]), Edge(Solid[2], Air[0]), TowardsAir); }
					else
					{
						const FVector A = Edge(Solid[0], Air[0]), B = Edge(Solid[0], Air[1]);
						const FVector C = Edge(Solid[1], Air[0]), Dp = Edge(Solid[1], Air[1]);
						Triangle(Result, A, B, C, TowardsAir); Triangle(Result, B, Dp, C, TowardsAir);
					}
				}
			}
	if (Result.Indices.IsEmpty()) { return false; }
	OutSurface = MoveTemp(Result);
	return true;
}
uint32 Hash(const TArray<uint8>& Density)
{
	uint32 Value = 2166136261u;
	for (uint8 B : Density) { Value = (Value ^ B) * 16777619u; }
	return Value;
}
bool IsSolid(const TArray<uint8>& Density, const FVector& LocalPoint)
{
	if (Density.Num() != SampleCount || LocalPoint.ContainsNaN()) { return false; }
	const FVector Grid = (LocalPoint - Position(0, 0, 0)) / Spacing;
	if (Grid.X < 0 || Grid.Y < 0 || Grid.Z < 0 || Grid.X > CellsXY || Grid.Y > CellsXY || Grid.Z > CellsZ) { return false; }
	const int32 X = FMath::Clamp(FMath::RoundToInt(Grid.X), 0, CellsXY), Y = FMath::Clamp(FMath::RoundToInt(Grid.Y), 0, CellsXY), Z = FMath::Clamp(FMath::RoundToInt(Grid.Z), 0, CellsZ);
	return Density[Index(X, Y, Z)] > 128;
}
}
