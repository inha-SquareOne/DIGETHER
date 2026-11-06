#include "DigetherSoilField.h"

namespace DigetherSoil
{
bool FGrid::IsValid() const
{
	return Cells.X >= 4 && Cells.Y >= 4 && Cells.Z >= 4 && Cells.X <= 64 && Cells.Y <= 64 && Cells.Z <= 64 &&
		int64(Cells.X + 1) * (Cells.Y + 1) * (Cells.Z + 1) <= 32768 && FMath::IsFinite(Spacing) && Spacing >= 2.0 && Spacing <= 200.0;
}
int32 FGrid::Count() const { return IsValid() ? (Cells.X + 1) * (Cells.Y + 1) * (Cells.Z + 1) : 0; }
int32 FGrid::Index(int32 X, int32 Y, int32 Z) const { return (Z * (Cells.Y + 1) + Y) * (Cells.X + 1) + X; }
FVector FGrid::Position(int32 X, int32 Y, int32 Z) const { return FVector(X, Y, Z) * Spacing; }
static bool Boundary(const FGrid& G, int32 X, int32 Y, int32 Z)
{
	return X == 0 || Y == 0 || Z == 0 || X == G.Cells.X || Y == G.Cells.Y || Z == G.Cells.Z;
}
static uint8 Encode(double Distance)
{
	const int32 Magnitude = FMath::RoundToInt(FMath::Min(FMath::Abs(Distance) / 2.0, 127.0));
	return uint8(128 + (Distance < 0 ? -Magnitude : Magnitude));
}
static bool ValidDensity(const FGrid& G, const TArray<uint8>& Density)
{
	if (!G.IsValid() || Density.Num() != G.Count()) { return false; }
	for (int32 Z = 0; Z <= G.Cells.Z; ++Z)
		for (int32 Y = 0; Y <= G.Cells.Y; ++Y)
			for (int32 X = 0; X <= G.Cells.X; ++X)
				if (Boundary(G, X, Y, Z) && Density[G.Index(X,Y,Z)] >= 128) { return false; }
	return true;
}
bool Initialize(const FGrid& G, TArray<uint8>& OutDensity)
{
	if (!G.IsValid()) { return false; }
	TArray<uint8> Values;
	Values.SetNumUninitialized(G.Count());
	for (int32 Z = 0; Z <= G.Cells.Z; ++Z)
		for (int32 Y = 0; Y <= G.Cells.Y; ++Y)
			for (int32 X = 0; X <= G.Cells.X; ++X)
			{
				// Distance to all six sides leaves a negative outer shell and a closed surface.
				const int32 Steps = FMath::Min3(FMath::Min(X, G.Cells.X - X), FMath::Min(Y, G.Cells.Y - Y), FMath::Min(Z, G.Cells.Z - Z));
				Values[G.Index(X,Y,Z)] = Encode((Steps - 0.5) * G.Spacing);
			}
	OutDensity = MoveTemp(Values);
	return true;
}
bool WorldToLocal(const FTransform& Transform, const FVector& WorldPoint, FVector& OutLocal)
{
	if (Transform.ContainsNaN() || WorldPoint.ContainsNaN() || !Transform.GetRotation().IsNormalized() ||
		!Transform.GetScale3D().Equals(FVector::OneVector, 1.e-6)) { return false; }
	const FVector Local = Transform.InverseTransformPosition(WorldPoint);
	if (Local.ContainsNaN()) { return false; }
	OutLocal = Local;
	return true;
}
ECarveResult Carve(const FGrid& G, TArray<uint8>& Density, const FVector& Point, double Radius,
	const TArray<FBox>& ProtectedBoxes, FBox& ChangedBounds)
{
	ChangedBounds = FBox(ForceInit);
	if (!ValidDensity(G, Density) || Point.ContainsNaN() || !FMath::IsFinite(Radius)) { return ECarveResult::InvalidInput; }
	const FBox Domain(FVector::ZeroVector, G.Position(G.Cells.X, G.Cells.Y, G.Cells.Z));
	if (
		Radius < G.Spacing * 0.5 || Radius > Domain.GetSize().Size() || !Domain.IsInsideOrOn(Point)) { return ECarveResult::InvalidInput; }
	for (const FBox& Box : ProtectedBoxes)
	{
		if (!Box.IsValid || Box.Min.ContainsNaN() || Box.Max.ContainsNaN() ||
			Box.Min.X > Box.Max.X || Box.Min.Y > Box.Max.Y || Box.Min.Z > Box.Max.Z) { return ECarveResult::InvalidInput; }
	}
	const FVector Center(FMath::RoundToDouble(Point.X), FMath::RoundToDouble(Point.Y), FMath::RoundToDouble(Point.Z));
	for (int32 Z = 1; Z < G.Cells.Z; ++Z)
		for (int32 Y = 1; Y < G.Cells.Y; ++Y)
			for (int32 X = 1; X < G.Cells.X; ++X)
			{
				const FVector P = G.Position(X,Y,Z);
				bool bProtected = false;
				for (const FBox& Box : ProtectedBoxes) { bProtected |= Box.ExpandBy(G.Spacing).IsInsideOrOn(P); }
				if (bProtected) { continue; }
				const uint8 Value = Encode(FVector::Distance(P, Center) - Radius);
				uint8& Old = Density[G.Index(X,Y,Z)];
				if (Value < Old) { Old = Value; ChangedBounds += P; }
			}
	if (!ChangedBounds.IsValid) { return ECarveResult::NoChange; }
	ChangedBounds = ChangedBounds.ExpandBy(G.Spacing).Overlap(Domain);
	return ECarveResult::Changed;
}
// Surface extraction and UE winding adapted from DigetherDainField / prototype.
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

bool BuildSurface(const FGrid& Grid, const TArray<uint8>& Density, FSurface& OutSurface)
{
	if (!ValidDensity(Grid, Density)) { return false; }
	FSurface Result;
	// The same body diagonal and face diagonals are used in every cell.
	static constexpr int32 Corners[8][3] = { {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0}, {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1} };
	static constexpr int32 Tets[6][4] = { {0,1,2,6}, {0,2,3,6}, {0,3,7,6}, {0,7,4,6}, {0,4,5,6}, {0,5,1,6} };
	for (int32 Z = 0; Z < Grid.Cells.Z; ++Z)
		for (int32 Y = 0; Y < Grid.Cells.Y; ++Y)
			for (int32 X = 0; X < Grid.Cells.X; ++X)
			{
				FVector P[8]; int32 D[8];
				for (int32 I = 0; I < 8; ++I)
				{
					const int32 CX = X + Corners[I][0], CY = Y + Corners[I][1], CZ = Z + Corners[I][2];
					P[I] = Grid.Position(CX, CY, CZ); D[I] = static_cast<int32>(Density[Grid.Index(CX, CY, CZ)]) - 128;
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
	// Lighting only: interpolate the outward density gradient. Geometry/collision stay identical.
    if (!Result.Vertices.IsEmpty())
    {
        TArray<FVector> Gradients;
        Gradients.SetNumUninitialized(Grid.Count());
        for (int32 Z=0; Z<=Grid.Cells.Z; ++Z)
            for (int32 Y=0; Y<=Grid.Cells.Y; ++Y)
                for (int32 X=0; X<=Grid.Cells.X; ++X)
                {
                    const int32 X0=FMath::Max(0,X-1), X1=FMath::Min(Grid.Cells.X,X+1);
                    const int32 Y0=FMath::Max(0,Y-1), Y1=FMath::Min(Grid.Cells.Y,Y+1);
                    const int32 Z0=FMath::Max(0,Z-1), Z1=FMath::Min(Grid.Cells.Z,Z+1);
                    Gradients[Grid.Index(X,Y,Z)]=FVector(
                        double(int32(Density[Grid.Index(X0,Y,Z)])-Density[Grid.Index(X1,Y,Z)])/(X1-X0),
                        double(int32(Density[Grid.Index(X,Y0,Z)])-Density[Grid.Index(X,Y1,Z)])/(Y1-Y0),
                        double(int32(Density[Grid.Index(X,Y,Z0)])-Density[Grid.Index(X,Y,Z1)])/(Z1-Z0));
                }
        for (int32 I=0; I<Result.Vertices.Num(); ++I)
        {
            const FVector P=Result.Vertices[I]/Grid.Spacing;
            const FIntVector Cell(FMath::Clamp(FMath::FloorToInt(P.X),0,Grid.Cells.X-1),
                FMath::Clamp(FMath::FloorToInt(P.Y),0,Grid.Cells.Y-1), FMath::Clamp(FMath::FloorToInt(P.Z),0,Grid.Cells.Z-1));
            const FVector Weight=P-FVector(Cell);
            FVector Normal=FVector::ZeroVector;
            for (int32 Z=0; Z<2; ++Z) for (int32 Y=0; Y<2; ++Y) for (int32 X=0; X<2; ++X)
                Normal += Gradients[Grid.Index(Cell.X+X,Cell.Y+Y,Cell.Z+Z)] *
                    (X ? Weight.X : 1-Weight.X)*(Y ? Weight.Y : 1-Weight.Y)*(Z ? Weight.Z : 1-Weight.Z);
            if (Normal.Normalize()) { Result.Normals[I]=Normal; }
        }
    }
    // Empty geometry is a valid fully excavated volume, not a build failure.
	OutSurface = MoveTemp(Result);
	return true;
}

}




