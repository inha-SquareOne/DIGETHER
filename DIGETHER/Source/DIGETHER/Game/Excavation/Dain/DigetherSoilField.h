#pragma once

#include "CoreMinimal.h"

// Pure CPU data. No Actor, input, network or physics ownership.
namespace DigetherSoil
{
struct FGrid
{
	FIntVector Cells = FIntVector(8, 8, 8);
	double Spacing = 50.0;
	bool IsValid() const;
	int32 Count() const;
	int32 Index(int32 X, int32 Y, int32 Z) const;
	FVector Position(int32 X, int32 Y, int32 Z) const;
};

struct FSurface
{
	TArray<FVector> Vertices;
	TArray<int32> Indices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
};

enum class ECarveResult : uint8 { InvalidInput, NoChange, Changed };

// Sample bounds include an immutable outer AIR layer; zero surface is value 128.
bool Initialize(const FGrid& Grid, TArray<uint8>& OutDensity);
bool BuildSurface(const FGrid& Grid, const TArray<uint8>& Density, FSurface& OutSurface);
// Protected boxes are local-space volumes. A one-cell halo preserves their surface.
// ChangedBounds includes neighbouring cells whose surface may change (not a collapse zone).
ECarveResult Carve(const FGrid& Grid, TArray<uint8>& Density, const FVector& LocalPoint,
	double Radius, const TArray<FBox>& ProtectedBoxes, FBox& ChangedBounds);
// E1 permits rigid transforms only. Size must be expressed by Grid, not Actor scale.
bool WorldToLocal(const FTransform& Transform, const FVector& WorldPoint, FVector& OutLocal);
}
